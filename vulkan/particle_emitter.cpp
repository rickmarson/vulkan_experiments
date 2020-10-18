/*
* mesh.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "particle_emitter.hpp"
#include "texture.hpp"
#include "vulkan_backend.hpp"
#include "shader_module.hpp"

#include <random>

std::shared_ptr<ParticleEmitter> ParticleEmitter::createParticleEmitter(const std::string& name, VulkanBackend* backend) {
    return std::make_shared<ParticleEmitter>(name, backend);
}

ParticleEmitter::ParticleEmitter(const std::string& name, VulkanBackend* backend) :
    name_(name),
    backend_(backend) {
    model_data_.transform_matrix = glm::identity<glm::mat4>();
}

ParticleEmitter::~ParticleEmitter() {
    vk_descriptor_sets_graphics_.clear();
    vk_descriptor_sets_compute_.clear();
    backend_->destroyPipeline(compute_pipeline_);
    backend_->destroyBuffer(vertex_buffer_);
    backend_->freeCommandBuffers(compute_command_buffers_);
    texture_.reset();
    compute_shader_.reset();
}

bool ParticleEmitter::createParticles(uint32_t count, const std::string& shader_file) {
    global_state_pc_.particles_count = count;
    
    const float max_x = 8.0f;
    const float max_y = 8.0f;
    const float max_z = 5.0f;
    const float min_z = 0.0f;
    const float max_speed_z = 10.0f;

    std::random_device r;

    std::default_random_engine e(r());
    std::uniform_real_distribution<float> uniform_dist_x(-max_x, max_x);
    std::uniform_real_distribution<float> uniform_dist_y(-max_y, max_y);
    std::uniform_real_distribution<float> uniform_dist_z(min_z, max_z);
    std::uniform_real_distribution<float> uniform_dist_vel_z(0.0f, max_speed_z);
   
    std::vector<ParticleVertex> particles(count);
    for (uint32_t i = 0; i < count; ++i) {
        particles[i] = { 
            glm::vec4(uniform_dist_x(e), uniform_dist_y(e), uniform_dist_z(e), 1.0),
            glm::vec4(0.0f, 0.0f, uniform_dist_vel_z(e), 1.0)
        };
    }

    vertex_buffer_ = backend_->createVertexBuffer<ParticleVertex>(name_ + "_particles", particles, false /*host_visible*/, true /*compute_visible*/);
    
    // because this vertex buffer is also accessible from the compute pipeline as a storage texel buffer, we need an additional buffer view
    if (!backend_->createBufferView(vertex_buffer_, VK_FORMAT_R32G32B32A32_SFLOAT)) {
        return false;
    }
    
    // create a compute pipeline for this emitter 
    compute_shader_ = backend_->createShaderModule(name_ + "_compute_shader");
    compute_shader_->loadSpirvShader(shader_file);

    compute_command_buffers_ = backend_->createPrimaryCommandBuffers(1);

    return vertex_buffer_.vk_buffer != VK_NULL_HANDLE && vertex_buffer_.vk_buffer_view != VK_NULL_HANDLE;
}

void ParticleEmitter::setTransform(const glm::mat4& transform) {
    model_data_.transform_matrix = transform;
}

RecordCommandsResult ParticleEmitter::update(float delta_time_s) {
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<ModelData>(uniform_buffer_.buffers[i], { model_data_ });
    }

    global_state_pc_.delta_time_s = delta_time_s;

    // record compute commands now as they don't depend on the swapchain
    return recordComputeCommands();
}

void ParticleEmitter::createUniformBuffer() {
    uniform_buffer_ = backend_->createUniformBuffer<ModelData>(name_ + "_model_data"); // the buffer lifecycle is managed by the backend
}

void ParticleEmitter::deleteUniformBuffer() {
    backend_->destroyUniformBuffer(uniform_buffer_);
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

bool ParticleEmitter::createComputePipeline() {
    if (compute_pipeline_.vk_pipeline != VK_NULL_HANDLE) {
        backend_->destroyPipeline(compute_pipeline_);
    }

    ComputePipelineConfig config;
    config.name = name_ + "_cp";
    config.compute = compute_shader_;
    compute_pipeline_ = backend_->createComputePipeline(config);

    createComputeDescriptorSets(compute_pipeline_.vk_descriptor_set_layouts);
    updateComputeDescriptorSets(compute_pipeline_.descriptor_metadata);

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
        std::cerr << "Failed to begin recording compute command buffer for particle emitter " << name_ << std::endl;
        return makeRecordCommandsResult(false, compute_command_buffers_);
    }

    vkCmdBindPipeline(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.vk_pipeline);
    vkCmdBindDescriptorSets(compute_command_buffers_[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.vk_pipeline_layout, COMPUTE_PARTICLE_BUFFER_SET_ID, 1, &vk_descriptor_sets_compute_[0], 0, nullptr);
    
    vkCmdPushConstants(compute_command_buffers_[0], compute_pipeline_.vk_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesGlobalState), &global_state_pc_);

    vkCmdDispatch(compute_command_buffers_[0], global_state_pc_.particles_count / 32 + 1, 1, 1);

    if (vkEndCommandBuffer(compute_command_buffers_[0]) != VK_SUCCESS) {
        std::cerr << "Failed to record compute command buffer for particle emitter " << name_ << std::endl;
        return makeRecordCommandsResult(false, compute_command_buffers_);
    }

    return makeRecordCommandsResult(true, compute_command_buffers_);
}

void ParticleEmitter::createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(1, layout);
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
    backend_->updateDescriptorSets(uniform_buffer_, vk_descriptor_sets_graphics_, bindings.find(MODEL_DATA_BINDING_NAME)->second);
    // getDiffuseTexture()->updateDescriptorSets(vk_descriptor_sets_, bindings.find(DIFFUSE_SAMPLER_BINDING_NAME)->second);
}

void ParticleEmitter::updateComputeDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(COMPUTE_PARTICLE_BUFFER_SET_ID)->second;
    backend_->updateDescriptorSets(vertex_buffer_, vk_descriptor_sets_compute_, bindings.find(COMPUTE_PARTICLE_BUFFER_BINDING_NAME)->second);
}
