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

class RainEmitterPR : public ParticleEmitterBase {
public:
	static std::shared_ptr<RainEmitterPR> createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend);
	explicit RainEmitterPR(const ParticleEmitterConfig& config, VulkanBackend* backend);
	virtual ~RainEmitterPR();
	
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

	Buffer particle_vertex_buffer_;
	Buffer particle_index_buffer_;
	uint32_t index_count_;
};