/*
* rain_emitter_gs.hpp
*
*  simulates rainfall and splashes, using a geometry shader for quad expansion
*
* Copyright (C) 2021 Riccardo Marson
*/


#pragma once

#include "particle_emitter_base.hpp"

class RainEmitterGS : public ParticleEmitterBase {
public:
	static std::shared_ptr<RainEmitterGS> createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend);
	explicit RainEmitterGS(const ParticleEmitterConfig& config, VulkanBackend* backend);
	virtual ~RainEmitterGS();
	
	virtual RecordCommandsResult renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) override;

	virtual DescriptorPoolConfig getDescriptorsCount() const override;
	virtual bool createGraphicsPipeline(const RenderPass& render_pass, uint32_t subpass_number) override;

private:
	virtual bool createAssets(std::vector<Particle>& particles) override;
	virtual void createUniformBuffers() override;
	virtual void createGraphicsDescriptorSets() override;
	virtual void updateGraphicsDescriptorSets() override;
	virtual void createComputeDescriptorSets() override;
	virtual void updateComputeDescriptorSets(std::shared_ptr<Texture>& scene_depth_buffer) override;
	virtual RecordCommandsResult recordComputeCommands() override;
};