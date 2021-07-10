/*
* rain_emtter_vb.cpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "rain_emitter_pr.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"
#include "../pipelines/compute_pipeline.hpp"
#include "../pipelines/graphics_pipeline.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace {

struct ParticleCompute {
    glm::vec4 pos;
    glm::vec4 vel;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleCompute, pos),  offsetof(ParticleCompute, vel) };
        return { sizeof(ParticleCompute) , offsets };
    }
};

struct ParticleVertex {
    glm::vec4 pos;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleVertex, pos) };
        return { sizeof(ParticleVertex) , offsets };
    }
};

const uint32_t PARTICLES_UNIFORM_SET_ID = 0;
const std::string PARTICLES_TEXTURE_ATLAS_BINDING_NAME = "texture_atlas";

const uint32_t COMPUTE_PARTICLE_BUFFER_SET_ID = 0;
const std::string COMPUTE_PARTICLE_BUFFER_BINDING_NAME = "particle_buffer";
const std::string COMPUTE_RESPAWN_BUFFER_BINDING_NAME = "respawn_buffer";
const std::string COMPUTE_VERTEX_BUFFER_BINDING_NAME = "vertex_buffer";

const uint32_t COMPUTE_CAMERA_SET_ID = 1; 
const std::string CAMERA_BINDING_NAME = "camera"; 

}

std::shared_ptr<RainEmitterPR> RainEmitterPR::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterPR>(config, backend);
}

RainEmitterPR::RainEmitterPR(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterPR::~RainEmitterPR() {
    backend_->destroyBuffer(particle_vertex_buffer_);
    backend_->destroyBuffer(particle_index_buffer_);
}

bool RainEmitterPR::createAssets(std::vector<Particle>& particles) {
    auto src = (ParticleCompute*)particles.data();
    std::vector<ParticleCompute> particles_compute(src, src + particles.size());

    particle_buffer_ = backend_->createStorageBuffer<ParticleCompute>(config_.name + "_particles", particles_compute, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, false /*host_visible*/); 
    if (!backend_->createBufferView(particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_respawn_buffer_ = backend_->createStorageBuffer<ParticleCompute>(config_.name + "_particles_respawn", particles_compute, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, false /*host_visible*/);
    if (!backend_->createBufferView(particle_respawn_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    // pre-initialise the vertex and index buffers containing all particles
    std::vector<ParticleVertex> particle_vertices(particles.size()*4);
    index_count_ = uint32_t(particles.size()*5);

    std::vector<uint32_t> particle_indices(index_count_);

    for (size_t i = 0, idx = 0; i < particle_indices.size() - 5; i = i + 5, idx = idx + 4) {
        particle_indices[i] = uint32_t(idx);
        particle_indices[i + 1] = uint32_t(idx + 1);
        particle_indices[i + 2] = uint32_t(idx + 2);
        particle_indices[i + 3] = uint32_t(idx + 3);
        particle_indices[i + 4] = 0xFFFFFFFF;
    }

    particle_vertex_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particle_verts", particle_vertices, false /*host_visible*/, true /*compute_visible*/);
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_vertex_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_index_buffer_ = backend_->createIndexBuffer<uint32_t>(config_.name + "_particle_idx", particle_indices, false);

    if (!config_.texture_atlas.empty()) {
        texture_atlas_ = backend_->createTexture(config_.name + "_texture_atlas");
        texture_atlas_->loadImageRGBA(config_.texture_atlas);
        texture_atlas_->createSampler();
    }

    // create a compute pipeline for this emitter 
    compute_shader_ = backend_->createShaderModule(config_.name + "_compute_shader");
    compute_shader_->loadSpirvShader("shaders/rainfall_pr_cp.spv");

    compute_command_buffers_ = backend_->createPrimaryCommandBuffers(1);

    // graphics pipeline assets
    vertex_shader_ = backend_->createShaderModule("rain_drops_pr_vs");
	vertex_shader_->loadSpirvShader("shaders/rain_drops_pr_vs.spv");

	if (!vertex_shader_->isVertexFormatCompatible(ParticleVertex::getFormatInfo())) {
		std::cerr << "ParticleVertex format is not compatible with pipeline input for " << vertex_shader_->getName() << std::endl;
		return false;
	}

	fragment_shader_ = backend_->createShaderModule("rain_drops_pr_fs");
	fragment_shader_->loadSpirvShader("shaders/rain_drops_pr_fs.spv");

	if (!vertex_shader_->isValid() || !fragment_shader_->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

    graphics_command_buffers_ = backend_->createSecondaryCommandBuffers(backend_->getSwapChainSize());
    if (graphics_command_buffers_.empty()) {
        return false;
    }

   return true;
}

RecordCommandsResult RainEmitterPR::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
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

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->handle());

    uint32_t offset = swapchain_image;
    vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->layout(), PARTICLES_UNIFORM_SET_ID, 1, &vk_descriptor_sets_graphics_[offset], 0, nullptr);

    vkCmdPushConstants(command_buffers[0], graphics_pipeline_->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &compute_camera_.proj_matrix);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffers[0], 0, 1, &particle_vertex_buffer_.vk_buffer, offsets);
    vkCmdBindIndexBuffer(command_buffers[0], particle_index_buffer_.vk_buffer, 0, VK_INDEX_TYPE_UINT32);

	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 4); // does nothing if not in debug

    vkCmdDrawIndexed(command_buffers[0], index_count_, 1, 0, 0, 0);
	
	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 5); // does nothing if not in debug

     if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to record command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    return makeRecordCommandsResult(true, command_buffers);
}

void RainEmitterPR::createUniformBuffers() {
    compute_camera_buffer_ = backend_->createUniformBuffer<CameraData>(config_.name + "_compute_camera", 1);
}

DescriptorPoolConfig RainEmitterPR::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 1;
    config.image_samplers_count = 1;
    config.storage_texel_buffers_count = 3;
    config.image_storage_buffers_count = 1;
    return config;
}

bool  RainEmitterPR::createGraphicsPipeline(const RenderPass& render_pass, uint32_t subpass_number) {
    GraphicsPipelineConfig config;
	config.vertex = vertex_shader_;
	config.fragment = fragment_shader_;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	config.vertex_buffer_binding_desc = vertex_shader_->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = vertex_shader_->getInputAttributes();
	config.render_pass = &render_pass;
	config.subpass_number = 1;
	config.enableDepthTesting = true;
	config.enableTransparency = true;
    config.enablePrimitiveRestart = true;

	graphics_pipeline_ = backend_->createGraphicsPipeline("Rain Drops GP");

    if (graphics_pipeline_->buildPipeline(config)) {
        createUniformBuffers();
        createGraphicsDescriptorSets();
        updateGraphicsDescriptorSets();
        return true;
    }
	
	return false;
}

RecordCommandsResult RainEmitterPR::recordComputeCommands() {
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

    vkCmdBindPipeline(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->handle());
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->layout(), COMPUTE_PARTICLE_BUFFER_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID], 0, nullptr);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_->layout(), COMPUTE_CAMERA_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID], 0, nullptr);

    vkCmdPushConstants(compute_command_buffers_[0], compute_pipeline_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesGlobalState), &global_state_pc_);

    if (config_.profile) {
        backend_->writeTimestampQuery(compute_command_buffers_[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, config_.start_query_num);
    }

    vkCmdDispatch(compute_command_buffers_[0], global_state_pc_.particles_count / 32 + 1, 1, 1);

    if (config_.profile) {
        backend_->writeTimestampQuery(compute_command_buffers_[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, config_.stop_query_num);
    }

    if (vkEndCommandBuffer(compute_command_buffers_[0]) != VK_SUCCESS) {
        std::cerr << "Failed to record compute command buffer for particle emitter " << config_.name << std::endl;
        return makeRecordCommandsResult(false, compute_command_buffers_);
    }

    return makeRecordCommandsResult(true, compute_command_buffers_);
}

void RainEmitterPR::createComputeDescriptorSets() {
    auto& descriptor_set_layouts = compute_pipeline_->descriptorSets();
    const auto& particle_buffers_layout = descriptor_set_layouts.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    const auto& camera_layout = descriptor_set_layouts.find(COMPUTE_CAMERA_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts = { particle_buffers_layout, camera_layout };
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

void RainEmitterPR::createGraphicsDescriptorSets() {
    auto& descriptor_set_layouts = graphics_pipeline_->descriptorSets();
    const auto& particles_layout = descriptor_set_layouts.find(PARTICLES_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), particles_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(backend_->getSwapChainSize());
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets_graphics_ = std::move(layout_descriptor_sets);
}

void RainEmitterPR::updateGraphicsDescriptorSets() {
     auto& metadata = graphics_pipeline_->descriptorMetadata();
    const auto& particles_bindings = metadata.set_bindings.find(PARTICLES_UNIFORM_SET_ID)->second;
    texture_atlas_->updateDescriptorSets(vk_descriptor_sets_graphics_, particles_bindings.find(PARTICLES_TEXTURE_ATLAS_BINDING_NAME)->second);
}

void RainEmitterPR::updateComputeDescriptorSets(std::shared_ptr<Texture>& scene_depth_buffer) {
    auto& metadata = compute_pipeline_->descriptorMetadata();
    const auto& particles_bindings = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    auto particles_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID]};
    backend_->updateDescriptorSets(particle_buffer_, particles_set, particles_bindings.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
    backend_->updateDescriptorSets(particle_respawn_buffer_, particles_set, particles_bindings.find(COMPUTE_RESPAWN_BUFFER_BINDING_NAME)->second);
    backend_->updateDescriptorSets(particle_vertex_buffer_, particles_set, particles_bindings.find(COMPUTE_VERTEX_BUFFER_BINDING_NAME)->second);
    const auto& camera_bindings = metadata.set_bindings.find(COMPUTE_CAMERA_SET_ID)->second;
    auto camera_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID]};
    backend_->updateDescriptorSets(compute_camera_buffer_, camera_set, camera_bindings.find(CAMERA_BINDING_NAME)->second);
    scene_depth_buffer->updateDescriptorSets(camera_set, camera_bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
}
