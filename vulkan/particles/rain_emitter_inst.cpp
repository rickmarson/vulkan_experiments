/*
* rain_emtter_inst.cpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "rain_emitter_inst.hpp"
#include "../texture.hpp"
#include "../vulkan_backend.hpp"
#include "../shader_module.hpp"

#include <glm/gtx/matrix_decompose.hpp>


std::shared_ptr<RainEmitterInst> RainEmitterInst::createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend) {
    return std::make_shared<RainEmitterInst>(config, backend);
}

RainEmitterInst::RainEmitterInst(const ParticleEmitterConfig& config, VulkanBackend* backend) :
    ParticleEmitterBase(config, backend) {

}

RainEmitterInst::~RainEmitterInst() {
  
}

bool RainEmitterInst::createAssets(std::vector<ParticleVertex>& particles) {
   return false;
}

RecordCommandsResult RainEmitterInst::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
    
    return makeRecordCommandsResult(true, graphics_command_buffers_);
}

void RainEmitterInst::createUniformBuffers() {
    
}

DescriptorPoolConfig RainEmitterInst::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = 2;
    config.image_samplers_count = 1;
    config.storage_texel_buffers_count = 2;
    config.image_storage_buffers_count = 1;
    return config;
}

bool  RainEmitterInst::createGraphicsPipeline(RenderPass& render_pass, uint32_t subpass_number) {
    
	return graphics_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

RecordCommandsResult RainEmitterInst::recordComputeCommands() {

    return makeRecordCommandsResult(false, compute_command_buffers_);
}

void RainEmitterInst::createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    
}

void RainEmitterInst::createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
   
}

void RainEmitterInst::updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata) {

}

void RainEmitterInst::updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer) {
   
}
