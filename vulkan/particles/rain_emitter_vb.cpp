/*
* rain_emtter_vb.cpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "rain_emitter_vb.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"

#include <glm/gtx/matrix_decompose.hpp>

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

struct ParticleVertex {
    glm::vec4 pos;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleVertex, pos),  offsetof(ParticleVertex, vel) };
        return { sizeof(ParticleVertex) , offsets };
    }

    bool operator==(const ParticleVertex& other) const {
        return pos == other.pos;
    }
};


std::shared_ptr<RainEmitterVB> RainEmitterVB::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterVB>(config, backend);
}

RainEmitterVB::RainEmitterVB(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterVB::~RainEmitterVB() {
  
}

bool RainEmitterVB::createAssets(std::vector<Particle>& particles) {
    std::vector<ParticleCompute> particles_compute(particles.begin(), particles.end());

    particle_buffer_ = backend_->createVertexBuffer<ParticleCompute>(config_.name + "_particles", particles_compute, false /*host_visible*/, true /*compute_visible*/);
    
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_respawn_buffer_ = backend_->createVertexBuffer<ParticleCompute>(config_.name + "_particles_respawn", particles_compute, false /*host_visible*/, true /*compute_visible*/);

    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_respawn_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    // pre-initialise the vertex and index buffers containing all particles
    std::vector<ParticleVertex> particle_vertices(particles.size()*4);
    std::vector<uint32_t> particle_indices(particles.size()*5);

    for (auto idx = 0; idx < particle_vertices.size(); idx = idx + 4) {
        particle_vertices[idx] = glm::vec4();
        particle_vertices[idx + 1] = glm::vec4();
        particle_vertices[idx + 2] = glm::vec4();
        particle_vertices[idx + 3] = glm::vec4();
    }

    particle_indices[0] = 0;
    particle_indices[1] = 1;
    particle_indices[2] = 2;
    particle_indices[3] = 3;
    for (auto idx = 4; idx < particle_indices.size(); ++idx) {
        if (idx % 4 == 0) {
            particle_indices[idx] = 0xFFFFFFFF;
        } else {
            particle_indices[idx] = idx - 1;
        }
    }

    particle_vertex_buffer_ = backend_->createVertexBuffer<ParticleVertex>(config_.name + "_particle_verts", particle_vertices, false /*host_visible*/, true /*compute_visible*/);
    
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(particle_vertex_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }

    particle_index_buffer_ = backend_->createIndexBuffer<uint32_t>(config_.name + "_particle_idx", particle_indices, false);

   return false;
}

RecordCommandsResult RainEmitterVB::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
    
    return makeRecordCommandsResult(true, graphics_command_buffers_);
}

void RainEmitterVB::createUniformBuffers() {
    
}

DescriptorPoolConfig RainEmitterVB::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 2;
    config.image_samplers_count = 1;
    config.storage_texel_buffers_count = 2;
    config.image_storage_buffers_count = 1;
    return config;
}

bool  RainEmitterVB::createGraphicsPipeline(RenderPass& render_pass, uint32_t subpass_number) {
    
	return graphics_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

RecordCommandsResult RainEmitterVB::recordComputeCommands() {

    return makeRecordCommandsResult(false, compute_command_buffers_);
}

void RainEmitterVB::createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    
}

void RainEmitterVB::createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
   
}

void RainEmitterVB::updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata) {

}

void RainEmitterVB::updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer) {
   
}
