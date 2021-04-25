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


std::shared_ptr<RainEmitterVB> RainEmitterVB::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterVB>(config, backend);
}

RainEmitterVB::RainEmitterVB(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterVB::~RainEmitterVB() {
  
}

bool RainEmitterVB::createAssets(std::vector<ParticleVertex>& particles) {
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
