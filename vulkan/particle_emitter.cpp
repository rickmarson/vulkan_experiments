/*
* mesh.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "particle_emitter.hpp"
#include "texture.hpp"
#include "vulkan_backend.hpp"
#include "shader_module.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include <random>

std::shared_ptr<ParticleEmitter> ParticleEmitter::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<ParticleEmitter>(config, backend);
}

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    config_(config),
    backend_(backend) {
    model_data_.transform_matrix = config.starting_transform;
}

ParticleEmitter::~ParticleEmitter() {
    vk_descriptor_sets_graphics_.clear();
    vk_descriptor_sets_compute_.clear();
    backend_->destroyPipeline(compute_pipeline_);
    backend_->destroyBuffer(particle_buffer_);
    backend_->destroyBuffer(particle_respawn_buffer_);
    backend_->freeCommandBuffers(compute_command_buffers_);
    texture_.reset();
    compute_shader_.reset();
}

bool ParticleEmitter::createParticles(uint32_t count, const std::string& shader_file) {
    global_state_pc_.particles_count = count;
    
    glm::vec3 origin;
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(config_.starting_transform, scale, rotation, origin, skew, perspective);

    const float min_x = origin[0] + config_.min_box_extent[0];
    const float min_y = origin[1] + config_.min_box_extent[1];
    const float min_z = origin[2] + config_.min_box_extent[2];
    const float max_x = origin[0] + config_.max_box_extent[0];
    const float max_y = origin[1] + config_.max_box_extent[1];
    const float max_z = origin[2] + config_.max_box_extent[2];

    const float min_speed_x = config_.min_starting_velocity[0];
    const float min_speed_y = config_.min_starting_velocity[1];
    const float min_speed_z = config_.min_starting_velocity[2];
    const float max_speed_x = config_.max_starting_velocity[0];
    const float max_speed_y = config_.max_starting_velocity[1];
    const float max_speed_z = config_.max_starting_velocity[2];

    std::random_device r;

    std::default_random_engine e(r());
    std::uniform_real_distribution<float> uniform_dist_x(min_x, max_x);
    std::uniform_real_distribution<float> uniform_dist_y(min_y, max_y);
    std::uniform_real_distribution<float> uniform_dist_z(min_z, max_z);
    std::uniform_real_distribution<float> uniform_dist_vel_x(min_speed_x, max_speed_x);
    std::uniform_real_distribution<float> uniform_dist_vel_y(min_speed_y, max_speed_y);
    std::uniform_real_distribution<float> uniform_dist_vel_z(min_speed_z, max_speed_z);
   
    std::vector<ParticleVertex> particles(count);
    for (uint32_t i = 0; i < count; ++i) {
        particles[i] = { 
            glm::vec4(uniform_dist_x(e), uniform_dist_y(e), uniform_dist_z(e), 0.0),   // w holds a collision flag
            glm::vec4(uniform_dist_vel_x(e), uniform_dist_vel_y(e), uniform_dist_vel_z(e), 2.0)  // w holds the lifetime after collision, in s 
        };
    }

    particle_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particles", particles, false /*host_visible*/, true /*compute_visible*/);
    
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }
    
    particle_respawn_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particles_respawn", particles, false /*host_visible*/, true /*compute_visible*/);

    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_respawn_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    // create a compute pipeline for this emitter 
    compute_shader_ = backend_->createShaderModule(config_.name + "_compute_shader");
    compute_shader_->loadSpirvShader(shader_file);

    compute_command_buffers_ = backend_->createPrimaryCommandBuffers(1);

    return particle_buffer_.vk_buffer != VK_NULL_HANDLE && particle_buffer_.vk_buffer_view != VK_NULL_HANDLE;
}

void ParticleEmitter::setTransform(const glm::mat4& transform) {
    model_data_.transform_matrix = transform;
}

RecordCommandsResult ParticleEmitter::update(float delta_time_s,  const SceneData& scene_data) {
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<ModelData>(graphics_uniform_buffer_.buffers[i], { model_data_ });
    }

    compute_camera_.view_matrix = scene_data.view;
    compute_camera_.proj_matrix = scene_data.proj;
    compute_camera_.framebuffer_size = { backend_->getSwapChainExtent().width, backend_->getSwapChainExtent().height };
    backend_->updateBuffer<CameraData>(compute_camera_buffer_.buffers[0], { compute_camera_ });
    global_state_pc_.delta_time_s = delta_time_s;

    // record compute commands now as they don't depend on the swapchain
    return recordComputeCommands();
}

void ParticleEmitter::createUniformBuffers() {
    graphics_uniform_buffer_ = backend_->createUniformBuffer<ModelData>(config_.name + "_model_data"); 
    compute_camera_buffer_ = backend_->createUniformBuffer<CameraData>(config_.name + "_compute_camera", 1);
}

void ParticleEmitter::deleteUniformBuffers() {
    backend_->destroyUniformBuffer(graphics_uniform_buffer_);
    backend_->destroyUniformBuffer(compute_camera_buffer_);
}

DescriptorPoolConfig ParticleEmitter::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 2;
    config.storage_texel_buffers_count = 2;
    config.image_storage_buffers_count = 1;
    return config;
}

void ParticleEmitter::createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(MODEL_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = backend_->getSwapChainSize();
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(backend_->getSwapChainSize());
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets_graphics_ = std::move(layout_descriptor_sets);
}

bool ParticleEmitter::createComputePipeline(std::shared_ptr<Texture>& scene_depth_buffer) {
    if (compute_pipeline_.vk_pipeline != VK_NULL_HANDLE) {
        backend_->destroyPipeline(compute_pipeline_);
    }

    ComputePipelineConfig config;
    config.name = config_.name + "_cp";
    config.compute = compute_shader_;
    compute_pipeline_ = backend_->createComputePipeline(config);

    createComputeDescriptorSets(compute_pipeline_.vk_descriptor_set_layouts);
    updateComputeDescriptorSets(compute_pipeline_.descriptor_metadata, scene_depth_buffer);

    return compute_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

RecordCommandsResult ParticleEmitter::recordComputeCommands() {
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

    vkCmdBindPipeline(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.vk_pipeline);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.vk_pipeline_layout, COMPUTE_PARTICLE_BUFFER_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID], 0, nullptr);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.vk_pipeline_layout, COMPUTE_CAMERA_SET_ID, 1, &vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID], 0, nullptr);

    vkCmdPushConstants(compute_command_buffers_[0], compute_pipeline_.vk_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesGlobalState), &global_state_pc_);

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

void ParticleEmitter::createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
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

void ParticleEmitter::updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(MODEL_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(graphics_uniform_buffer_, vk_descriptor_sets_graphics_, bindings.find(MODEL_DATA_BINDING_NAME)->second);
    // getDiffuseTexture()->updateDescriptorSets(vk_descriptor_sets_, bindings.find(DIFFUSE_SAMPLER_BINDING_NAME)->second);
}

void ParticleEmitter::updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer) {
    const auto& particles_bindings = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    auto particles_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_PARTICLE_BUFFER_SET_ID]};
    backend_->updateDescriptorSets(particle_buffer_, particles_set, particles_bindings.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
    backend_->updateDescriptorSets(particle_respawn_buffer_, particles_set, particles_bindings.find(COMPUTE_RESPAWN_BUFFER_BINDING_NAME)->second);
    const auto& camera_bindings = metadata.set_bindings.find(COMPUTE_CAMERA_SET_ID)->second;
    auto camera_set = std::vector<VkDescriptorSet>{vk_descriptor_sets_compute_[COMPUTE_CAMERA_SET_ID]};
    backend_->updateDescriptorSets(compute_camera_buffer_, camera_set, camera_bindings.find(CAMERA_BINDING_NAME)->second);
    scene_depth_buffer->updateDescriptorSets(camera_set, camera_bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
}
