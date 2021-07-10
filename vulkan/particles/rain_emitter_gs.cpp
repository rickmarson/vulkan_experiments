/*
* rain_emtter_gs.cpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "rain_emitter_gs.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"
#include "../pipelines/compute_pipeline.hpp"
#include "../pipelines/graphics_pipeline.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace {

struct ParticleVertex {
    glm::vec4 pos;
    glm::vec4 vel;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleVertex, pos),  offsetof(ParticleVertex, vel) };
        return { sizeof(ParticleVertex) , offsets };
    }
};

struct SplashHint {
    glm::vec3 position;
    float lifetime = 0.f;
    glm::vec3 normal;
    float initial_speed = 0.f;
};

const uint32_t PARTICLES_UNIFORM_SET_ID = 1;
const std::string PARTICLES_TEXTURE_ATLAS_BINDING_NAME = "texture_atlas";

const uint32_t COMPUTE_PARTICLE_BUFFER_SET_ID = 0;
const std::string COMPUTE_PARTICLE_BUFFER_BINDING_NAME = "particle_buffer";
const std::string COMPUTE_RESPAWN_BUFFER_BINDING_NAME = "respawn_buffer";

const uint32_t COMPUTE_CAMERA_SET_ID = 1;  // camera / scene related data passed to compute shaders
const std::string CAMERA_BINDING_NAME = "camera"; 

const uint32_t COMPUTE_COLLISION_HINTS_SET_ID = 2;
const std::string COMPUTE_INDIRECT_DISPATCH_CMD_NAME = "splashes_dispatch";
const std::string COMPUTE_INDIRECT_DRAW_CMD_NAME = "splashes_draw";

const uint32_t VIEW_PROJ_SET_ID = SCENE_UNIFORM_SET_ID;  // a subset of SceneData for pipelines that don't need lighting

const uint32_t COMPUTE_SPLASH_SET_ID = 0;
const std::string COMPUTE_SPLASH_HINT_NAME = "splashes";
const std::string COMPUTE_SPLASH_PARTICLE_BUFFER_NAME = "particles";

}

std::shared_ptr<RainEmitterGS> RainEmitterGS::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterGS>(config, backend);
}

RainEmitterGS::RainEmitterGS(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterGS::~RainEmitterGS() {
  
}

bool RainEmitterGS::createAssets(std::vector<Particle>& particles) {
    auto src = (ParticleVertex*)particles.data();
    std::vector<ParticleVertex> particles_vertices(src, src+particles.size());

    particle_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particles", particles_vertices, false /*host_visible*/, true /*compute_visible*/);
    
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_respawn_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particles_respawn", particles_vertices, false /*host_visible*/, true /*compute_visible*/);

    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_respawn_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    // collision hints buffer
    std::vector<SplashHint> splash_hints(particles.size(), SplashHint{});
    hit_buffer_ = backend_->createStorageBuffer<SplashHint>(config_.name + "_collision_hints", splash_hints, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false /*host_visible*/);

    // secondary particle buffer for splash animations
    std::vector<ParticleVertex> splash_vertices(particles.size(), ParticleVertex{});
    collision_particle_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_splash_particles", splash_vertices, false /*host_visible*/, true /*compute_visible*/);

    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(collision_particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    if (!config_.texture_atlas.empty()) {
        texture_atlas_ = backend_->createTexture(config_.name + "_texture_atlas");
        texture_atlas_->loadImageRGBA(config_.texture_atlas);
        texture_atlas_->createSampler();
    }

    // create a compute pipeline for the raindrops
    compute_shader_ = backend_->createShaderModule(config_.name + "_raindrops");
    compute_shader_->loadSpirvShader("shaders/rainfall_geom_cp.spv");

    // create a compute pipeline for the splashes
    collision_compute_shader_ = backend_->createShaderModule(config_.name + "_splashes");
    collision_compute_shader_->loadSpirvShader("shaders/splash_cp.spv");

    compute_command_buffers_ = backend_->createPrimaryCommandBuffers(1);

    // graphics pipeline assets
    vertex_shader_ = backend_->createShaderModule("rain_drops_geom_vs");
	vertex_shader_->loadSpirvShader("shaders/rain_drops_geom_vs.spv");

	if (!vertex_shader_->isVertexFormatCompatible(ParticleVertex::getFormatInfo())) {
		std::cerr << "ParticleVertex format is not compatible with pipeline input for " << vertex_shader_->getName() << std::endl;
		return false;
	}

	geometry_shader_ = backend_->createShaderModule("rain_drops_geom_gm");
	geometry_shader_->loadSpirvShader("shaders/rain_drops_geom_gm.spv");

	fragment_shader_ = backend_->createShaderModule("rain_drops_geom_fs");
	fragment_shader_->loadSpirvShader("shaders/rain_drops_geom_fs.spv");

	if (!vertex_shader_->isValid() || !geometry_shader_->isValid() || !fragment_shader_->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

    graphics_command_buffers_ = backend_->createSecondaryCommandBuffers(backend_->getSwapChainSize());
    if (graphics_command_buffers_.empty()) {
        return false;
    }
    return true;
}

RecordCommandsResult RainEmitterGS::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
    std::vector<VkCommandBuffer> command_buffers = { graphics_command_buffers_[swapchain_image] };
    backend_->resetCommandBuffers(command_buffers);

    VkCommandBufferInheritanceInfo inherit_info{};
    inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit_info.renderPass = render_pass_info.renderPass;
    inherit_info.subpass = config_.subpass_number;
    inherit_info.framebuffer = render_pass_info.framebuffer;
    inherit_info.occlusionQueryEnable = VK_FALSE;
    inherit_info.queryFlags = 0;
    inherit_info.pipelineStatistics = 0;

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inherit_info;

    if (vkBeginCommandBuffer(command_buffers[0], &begin_info) != VK_SUCCESS) {
        std::cerr << "[Particle Emitter] Failed to begin recording command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    uint32_t offset = swapchain_image;
	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->layout(), SCENE_UNIFORM_SET_ID, 1, &vk_descriptor_sets_graphics_[offset], 0, nullptr);
    offset = backend_->getSwapChainSize() + offset;
    vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->layout(), PARTICLES_UNIFORM_SET_ID, 1, &vk_descriptor_sets_graphics_[offset], 0, nullptr);

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->handle());

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffers[0], 0, 1, &particle_buffer_.vk_buffer, offsets);
	
	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 4); // does nothing if not in debug

	vkCmdDraw(command_buffers[0], global_state_pc_.particles_count, 1, 0, 0);
	
	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 5); // does nothing if not in debug

     if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to record command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    return makeRecordCommandsResult(true, command_buffers);
}

void RainEmitterGS::createUniformBuffers() {
    compute_camera_buffer_ = backend_->createUniformBuffer<CameraData>(config_.name + "_compute_camera", 1);
    graphics_view_proj_buffer_ = backend_->createUniformBuffer<ViewProj>(config_.name + "_graphics_vp");
}

DescriptorPoolConfig RainEmitterGS::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 2;
    config.image_samplers_count = 1;
    config.storage_texel_buffers_count = 2;
    config.image_storage_buffers_count = 1;
    return config;
}

bool  RainEmitterGS::createGraphicsPipeline(const RenderPass& render_pass, uint32_t subpass_number) {
    GraphicsPipelineConfig config;
	config.vertex = vertex_shader_;
	config.geometry = geometry_shader_;
	config.fragment = fragment_shader_;
	config.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	config.vertex_buffer_binding_desc = vertex_shader_->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = vertex_shader_->getInputAttributes();
	config.render_pass = &render_pass;
	config.subpass_number = 1;
	config.enableDepthTesting = true;
	config.enableTransparency = true;

	graphics_pipeline_ = backend_->createGraphicsPipeline("Rain Drops GP");

    if (graphics_pipeline_->buildPipeline(config)) {
        createUniformBuffers();
        createGraphicsDescriptorSets();
        updateGraphicsDescriptorSets();
        return true;
    }
	
	return false;
}

RecordCommandsResult RainEmitterGS::recordComputeCommands() {
    backend_->waitComputeQueueIdle();  // prevents the buffers being reset while in use. possibly overkill

    backend_->resetCommandBuffers(compute_command_buffers_);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(compute_command_buffers_[0], &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording compute command buffer for particle emitter " << config_.name << std::endl;
        return makeRecordCommandsResult(false, compute_command_buffers_);
    }

    if (config_.profile) {
        backend_->resetTimestampQueries(compute_command_buffers_[0], config_.start_query_num, 2);
    }

    // reset the indirect command buffers
    VkBufferCopy region {0, 0, sizeof(VkDispatchIndirectCommand)};
    vkCmdCopyBuffer(compute_command_buffers_[0], dispatch_indirect_cmds_reset_.vk_buffer, dispatch_indirect_cmds_.vk_buffer, 1, &region);

    VkBufferMemoryBarrier reset_barrier {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        dispatch_indirect_cmds_.vk_buffer,
        0,
        VK_WHOLE_SIZE,
    };
    vkCmdPipelineBarrier(compute_command_buffers_[0], VK_PIPELINE_STAGE_TRANSFER_BIT , VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,1, &reset_barrier, 0, nullptr );

    // run the rain particles update
    vkCmdBindPipeline(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->handle());
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->layout(), COMPUTE_PARTICLE_BUFFER_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID], 0, nullptr);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->layout(), COMPUTE_CAMERA_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID], 0, nullptr);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->layout(), COMPUTE_COLLISION_HINTS_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_COLLISION_HINTS_SET_ID], 0, nullptr);

    vkCmdPushConstants(compute_command_buffers_[0], compute_pipeline_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesGlobalState), &global_state_pc_);

    if (config_.profile) {
        backend_->writeTimestampQuery(compute_command_buffers_[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, config_.start_query_num);
    }

    vkCmdDispatch(compute_command_buffers_[0], global_state_pc_.particles_count / 32 + 1, 1, 1);

    // synchronise the dispatch indirect command & splash hints buffers
    VkBufferMemoryBarrier dispatch_cmd_barrier = reset_barrier;
    dispatch_cmd_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    dispatch_cmd_barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    dispatch_cmd_barrier.buffer = dispatch_indirect_cmds_.vk_buffer;
    vkCmdPipelineBarrier(compute_command_buffers_[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &dispatch_cmd_barrier, 0, nullptr );

    VkBufferMemoryBarrier hints_barrier = reset_barrier;
    hints_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    hints_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    hints_barrier.buffer = hit_buffer_.vk_buffer;
    vkCmdPipelineBarrier(compute_command_buffers_[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &hints_barrier, 0, nullptr );

    // run the splash updates
    vkCmdBindPipeline(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, collision_compute_pipeline_->handle());
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, collision_compute_pipeline_->layout(), COMPUTE_SPLASH_SET_ID, 1, &vk_descriptor_sets_collision_compute_[COMPUTE_SPLASH_SET_ID], 0, nullptr);

    vkCmdDispatchIndirect(compute_command_buffers_[0], dispatch_indirect_cmds_.vk_buffer, 0);

    if (config_.profile) {
        backend_->writeTimestampQuery(compute_command_buffers_[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, config_.stop_query_num);
    }

    if (vkEndCommandBuffer(compute_command_buffers_[0]) != VK_SUCCESS) {
        std::cerr << "Failed to record compute command buffer for particle emitter " << config_.name << std::endl;
        return makeRecordCommandsResult(false, compute_command_buffers_);
    }

    return makeRecordCommandsResult(true, compute_command_buffers_);
}

void RainEmitterGS::createComputeDescriptorSets() {
    {
        auto& descriptor_set_layouts = compute_pipeline_->descriptorSets();
        const auto& particle_buffers_layout = descriptor_set_layouts.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
        const auto& camera_layout = descriptor_set_layouts.find(COMPUTE_CAMERA_SET_ID)->second;
        const auto& splash_hints_layout = descriptor_set_layouts.find(COMPUTE_COLLISION_HINTS_SET_ID)->second;

        std::vector<VkDescriptorSetLayout> layouts = { particle_buffers_layout, camera_layout, splash_hints_layout };
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = backend_->getDescriptorPool();
        alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        alloc_info.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> layout_descriptor_sets(layouts.size());
        if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
            std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
            return;
        }

        vk_descriptor_sets_compute_ = std::move(layout_descriptor_sets);
    }

    if (collision_compute_pipeline_) {
        auto& descriptor_set_layouts = collision_compute_pipeline_->descriptorSets();
        const auto& splash_buffer_layout = descriptor_set_layouts.find(COMPUTE_SPLASH_SET_ID)->second;

        std::vector<VkDescriptorSetLayout> layouts = { splash_buffer_layout };
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = backend_->getDescriptorPool();
        alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        alloc_info.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> layout_descriptor_sets(layouts.size());
        if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
            std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
            return;
        }

        vk_descriptor_sets_collision_compute_ = std::move(layout_descriptor_sets);
    }
}


void RainEmitterGS::updateComputeDescriptorSets(std::shared_ptr<Texture>& scene_depth_buffer) {
    {
        auto& metadata = compute_pipeline_->descriptorMetadata();
        const auto& particles_bindings = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
        auto particles_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID]};
        backend_->updateDescriptorSets(particle_buffer_, particles_set, particles_bindings.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
        backend_->updateDescriptorSets(particle_respawn_buffer_, particles_set, particles_bindings.find(COMPUTE_RESPAWN_BUFFER_BINDING_NAME)->second);
        const auto& camera_bindings = metadata.set_bindings.find(COMPUTE_CAMERA_SET_ID)->second;
        auto camera_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID]};
        backend_->updateDescriptorSets(compute_camera_buffer_, camera_set, camera_bindings.find(CAMERA_BINDING_NAME)->second);
        scene_depth_buffer->updateDescriptorSets(camera_set, camera_bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
        const auto& splash_bindings = metadata.set_bindings.find(COMPUTE_COLLISION_HINTS_SET_ID)->second;
        auto splashes_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_COLLISION_HINTS_SET_ID]};
        backend_->updateDescriptorSets(dispatch_indirect_cmds_, splashes_set, splash_bindings.find(COMPUTE_INDIRECT_DISPATCH_CMD_NAME)->second);
        backend_->updateDescriptorSets(draw_indirect_cmds_, splashes_set, splash_bindings.find(COMPUTE_INDIRECT_DRAW_CMD_NAME)->second);
        backend_->updateDescriptorSets(hit_buffer_, splashes_set, splash_bindings.find(COMPUTE_SPLASH_HINT_NAME)->second);
    }

    if (collision_compute_pipeline_) {
        auto& metadata = collision_compute_pipeline_->descriptorMetadata();
        const auto& splash_bindings = metadata.set_bindings.find(COMPUTE_SPLASH_SET_ID)->second;
        auto splashes_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_collision_compute_[COMPUTE_SPLASH_SET_ID]};
        backend_->updateDescriptorSets(hit_buffer_, splashes_set, splash_bindings.find(COMPUTE_SPLASH_HINT_NAME)->second);
        backend_->updateDescriptorSets(collision_particle_buffer_, splashes_set, splash_bindings.find(COMPUTE_SPLASH_PARTICLE_BUFFER_NAME)->second);
    }
}

void RainEmitterGS::createGraphicsDescriptorSets() {
     auto& descriptor_set_layouts = graphics_pipeline_->descriptorSets();
    const auto& view_proj_layout = descriptor_set_layouts.find(VIEW_PROJ_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), view_proj_layout);

    const auto& particles_layout = descriptor_set_layouts.find(PARTICLES_UNIFORM_SET_ID)->second;
    layouts.insert(layouts.end(), backend_->getSwapChainSize(), particles_layout );

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(backend_->getSwapChainSize() * 2);
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets_graphics_ = std::move(layout_descriptor_sets);
}

void RainEmitterGS::updateGraphicsDescriptorSets() {
    auto& metadata = graphics_pipeline_->descriptorMetadata();
    const auto& view_proj_bindings = metadata.set_bindings.find(VIEW_PROJ_SET_ID)->second;
    auto first = vk_descriptor_sets_graphics_.begin();
    auto last = vk_descriptor_sets_graphics_.begin() + backend_->getSwapChainSize();
    auto view_proj_descriptors = std::vector<VkDescriptorSet>(first, last);
    backend_->updateDescriptorSets(graphics_view_proj_buffer_, view_proj_descriptors, view_proj_bindings.find(VIEW_PROJ_BINDING_NAME)->second);

    const auto& particles_bindings = metadata.set_bindings.find(PARTICLES_UNIFORM_SET_ID)->second;
    first = vk_descriptor_sets_graphics_.begin() + backend_->getSwapChainSize();
    last = vk_descriptor_sets_graphics_.end();
    auto paritcles_descriptors = std::vector<VkDescriptorSet>(first, last);
    texture_atlas_->updateDescriptorSets(paritcles_descriptors, particles_bindings.find(PARTICLES_TEXTURE_ATLAS_BINDING_NAME)->second);
}
