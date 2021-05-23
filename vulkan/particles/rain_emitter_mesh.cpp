/*
* rain_emtter_mesh.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "rain_emitter_mesh.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"
#include "../pipelines/compute_pipeline.hpp"
#include "../pipelines/mesh_pipeline.hpp"

namespace {
struct ParticleCompute {
    glm::vec4 pos;
    glm::vec4 vel;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleCompute, pos),  offsetof(ParticleCompute, vel) };
        return { sizeof(ParticleCompute) , offsets };
    }

    bool operator==(const ParticleCompute& other) const {
        return pos == other.pos && vel == other.vel;
    }
};

const uint32_t PARTICLES_UNIFORM_SET_ID = 1;
const std::string PARTICLES_TEXTURE_ATLAS_BINDING_NAME = "texture_atlas";

const uint32_t COMPUTE_PARTICLE_BUFFER_SET_ID = 0;
const std::string COMPUTE_PARTICLE_BUFFER_BINDING_NAME = "particle_buffer";
const std::string COMPUTE_RESPAWN_BUFFER_BINDING_NAME = "respawn_buffer";

const uint32_t COMPUTE_CAMERA_SET_ID = 1; 
const std::string CAMERA_BINDING_NAME = "camera"; 

}

std::shared_ptr<RainEmitterMesh> RainEmitterMesh::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterMesh>(config, backend);
}

RainEmitterMesh::RainEmitterMesh(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterMesh::~RainEmitterMesh() {

}

bool RainEmitterMesh::createAssets(std::vector<Particle>& particles) {
    auto src = (ParticleCompute*)particles.data();
    std::vector<ParticleCompute> particles_vertices(src, src+particles.size());

    particle_buffer_ = backend_->createStorageTexelBuffer<ParticleCompute>(config_.name + "_particles", particles_vertices, false /*host_visible*/);
    if (!backend_->createBufferView(particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_respawn_buffer_ = backend_->createStorageTexelBuffer<ParticleCompute>(config_.name + "_particles_respawn", particles_vertices, false /*host_visible*/);
    if (!backend_->createBufferView(particle_respawn_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    if (!config_.texture_atlas.empty()) {
        texture_atlas_ = backend_->createTexture(config_.name + "_texture_atlas");
        texture_atlas_->loadImageRGBA(config_.texture_atlas);
        texture_atlas_->createSampler();
    }

    // create a compute pipeline for this emitter 
    compute_shader_ = backend_->createShaderModule(config_.name + "_compute_shader");
    compute_shader_->loadSpirvShader("shaders/rainfall_geom_cp.spv");

    compute_command_buffers_ = backend_->createPrimaryCommandBuffers(1);

    // graphics pipeline assets
    mesh_shader_ = backend_->createShaderModule("rain_drops_mesh_ms");
	mesh_shader_->loadSpirvShader("shaders/rain_drops_mesh_ms.spv");

	fragment_shader_ = backend_->createShaderModule("rain_drops_inst_fs");
	fragment_shader_->loadSpirvShader("shaders/rain_drops_inst_fs.spv");

	if (!mesh_shader_->isValid() || !fragment_shader_->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

    graphics_command_buffers_ = backend_->createSecondaryCommandBuffers(backend_->getSwapChainSize());
    if (graphics_command_buffers_.empty()) {
        return false;
    }
    return true;
}

struct Pc {
    glm::mat4 view;
    glm::mat4 proj;
    uint32_t  particles_count;
};

RecordCommandsResult RainEmitterMesh::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
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

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_->handle());

    uint32_t offset = swapchain_image;
    vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_->layout(), PARTICLES_UNIFORM_SET_ID, 1, &vk_descriptor_sets_graphics_[offset], 0, nullptr);
    offset += backend_->getSwapChainSize();
    vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_->layout(), COMPUTE_PARTICLE_BUFFER_SET_ID, 1, &vk_descriptor_sets_graphics_[offset], 0, nullptr);

    Pc pc = {compute_camera_.view_matrix, compute_camera_.proj_matrix, global_state_pc_.particles_count};

    vkCmdPushConstants(command_buffers[0], mesh_pipeline_->layout(), VK_SHADER_STAGE_MESH_BIT_NV, 0, sizeof(Pc), &pc);

	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 4); // does nothing if not in debug

    VkDrawMeshTasksNV(command_buffers[0], global_state_pc_.particles_count, 0);
	
	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 5); // does nothing if not in debug

     if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to record command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    return makeRecordCommandsResult(true, command_buffers);
}

void RainEmitterMesh::createUniformBuffers() {
    compute_camera_buffer_ = backend_->createUniformBuffer<CameraData>(config_.name + "_compute_camera", 1);
}

DescriptorPoolConfig RainEmitterMesh::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 1;
    config.image_samplers_count = 1;
    config.storage_texel_buffers_count = 2 + 1 * backend_->getSwapChainSize();
    config.image_storage_buffers_count = 1;
    return config;
}

bool RainEmitterMesh::createGraphicsPipeline(const RenderPass& render_pass, uint32_t subpass_number) {
    MeshPipelineConfig config;
	config.mesh = mesh_shader_;
	config.fragment = fragment_shader_;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    config.has_vertex_assembly_stage = false;
	config.render_pass = &render_pass;
	config.subpass_number = 1;
	config.enableDepthTesting = true;
	config.enableTransparency = true;

	mesh_pipeline_ = backend_->createMeshPipeline("Rain Drops MP");

    if (mesh_pipeline_->buildPipeline(config)) {
        createUniformBuffers();
        createGraphicsDescriptorSets(mesh_pipeline_->descriptorSets());
        updateGraphicsDescriptorSets(mesh_pipeline_->descriptorMetadata());
        return true;
    }
	
	return false;
}

RecordCommandsResult RainEmitterMesh::recordComputeCommands() {
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

void RainEmitterMesh::createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
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

void RainEmitterMesh::createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& particles_layout = descriptor_set_layouts.find(PARTICLES_UNIFORM_SET_ID)->second;
    const auto& particle_buffers_layout = descriptor_set_layouts.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;

    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), particles_layout);
    layouts.insert(layouts.end(), backend_->getSwapChainSize(), particle_buffers_layout);

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

    vk_descriptor_sets_graphics_ = std::move(layout_descriptor_sets);
}

void RainEmitterMesh::updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& particles_bindings = metadata.set_bindings.find(PARTICLES_UNIFORM_SET_ID)->second;
    auto first = vk_descriptor_sets_graphics_.begin();
    auto last = vk_descriptor_sets_graphics_.begin() + backend_->getSwapChainSize();
    auto paritcles_descriptors = std::vector<VkDescriptorSet>(first, last);
    texture_atlas_->updateDescriptorSets(paritcles_descriptors, particles_bindings.find(PARTICLES_TEXTURE_ATLAS_BINDING_NAME)->second);

    const auto& particle_instances = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    first = vk_descriptor_sets_graphics_.begin() + backend_->getSwapChainSize();
    last = vk_descriptor_sets_graphics_.end();
    auto particle_instances_descriptors = std::vector<VkDescriptorSet>(first, last);
    backend_->updateDescriptorSets(particle_buffer_, particle_instances_descriptors, particle_instances.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
}

void RainEmitterMesh::updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer) {
    const auto& particles_bindings = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    auto particles_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID]};
    backend_->updateDescriptorSets(particle_buffer_, particles_set, particles_bindings.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
    backend_->updateDescriptorSets(particle_respawn_buffer_, particles_set, particles_bindings.find(COMPUTE_RESPAWN_BUFFER_BINDING_NAME)->second);
    const auto& camera_bindings = metadata.set_bindings.find(COMPUTE_CAMERA_SET_ID)->second;
    auto camera_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID]};
    backend_->updateDescriptorSets(compute_camera_buffer_, camera_set, camera_bindings.find(CAMERA_BINDING_NAME)->second);
    scene_depth_buffer->updateDescriptorSets(camera_set, camera_bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
}
