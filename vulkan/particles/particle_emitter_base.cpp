/*
* particle_emitter_base.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "particle_emitter_base.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include <random>

ParticleEmitterBase::ParticleEmitterBase(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    config_(config),
    backend_(backend) {
    transform_ = config.starting_transform;
}

ParticleEmitterBase::~ParticleEmitterBase() {
    vk_descriptor_sets_graphics_.clear();
    vk_descriptor_sets_compute_.clear();
    backend_->destroyPipeline(compute_pipeline_);
    backend_->destroyPipeline(graphics_pipeline_);
    backend_->destroyBuffer(particle_buffer_);
    backend_->destroyBuffer(particle_respawn_buffer_);
    backend_->destroyUniformBuffer(graphics_view_proj_buffer_);
    backend_->destroyUniformBuffer(compute_camera_buffer_);
    backend_->freeCommandBuffers(compute_command_buffers_);
    backend_->freeCommandBuffers(graphics_command_buffers_);
    texture_atlas_.reset();
    compute_shader_.reset();
    vertex_shader_.reset();
    fragment_shader_.reset();
    geometry_shader_.reset();
}

bool ParticleEmitterBase::createParticles(uint32_t count) {
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
            glm::vec4(uniform_dist_vel_x(e), uniform_dist_vel_y(e), uniform_dist_vel_z(e), config_.lifetime_after_collision)  // w holds the lifetime after collision, in s 
        };
    }

    return createAssets(particles);
}

void ParticleEmitterBase::setTransform(const glm::mat4& transform) {
    transform_ = transform;
}

RecordCommandsResult ParticleEmitterBase::update(float delta_time_s,  const SceneData& scene_data) {
    compute_camera_.view_matrix = scene_data.view;
    compute_camera_.proj_matrix = scene_data.proj;
    compute_camera_.framebuffer_size = { backend_->getSwapChainExtent().width, backend_->getSwapChainExtent().height };
    backend_->updateBuffer<CameraData>(compute_camera_buffer_.buffers[0], { compute_camera_ });
    ViewProj view_proj  { scene_data.view, scene_data.proj };
    for (auto& buffer : graphics_view_proj_buffer_.buffers) {
        backend_->updateBuffer<ViewProj>(buffer, {view_proj} );
    }
    global_state_pc_.delta_time_s = delta_time_s;

    // record compute commands now as they don't depend on the swapchain
    return recordComputeCommands();
}

bool ParticleEmitterBase::createComputePipeline(std::shared_ptr<Texture>& scene_depth_buffer) {
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
