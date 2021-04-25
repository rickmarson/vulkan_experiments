/*
* rain_emitter_vb.hpp
*
*  simulates rainfall and splashes.
*  all particle quads are packed into a single vertex buffer, updated in compute and drawn with primitive restart
*
* Copyright (C) 2021 Riccardo Marson
*/


#pragma once

#include "particle_emitter_base.hpp"

class RainEmitterVB : public ParticleEmitterBase {
public:
	static std::shared_ptr<RainEmitterVB> createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend);
	explicit RainEmitterVB(const ParticleEmitterConfig& config, VulkanBackend* backend);
	virtual ~RainEmitterVB();
	
	virtual RecordCommandsResult renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) override;

	virtual DescriptorPoolConfig getDescriptorsCount() const override;
	virtual bool createGraphicsPipeline(RenderPass& render_pass, uint32_t subpass_number) override;

private:
	virtual bool createAssets(std::vector<ParticleVertex>& particles) override;
	virtual void createUniformBuffers() override;
	virtual void createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) override;
	virtual void updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata) override;
	virtual void createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) override;
	virtual void updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer) override;
	virtual RecordCommandsResult recordComputeCommands() override;
};