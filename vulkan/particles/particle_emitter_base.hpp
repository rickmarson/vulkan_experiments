/*
* particle_emitte_base.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "../common_definitions.hpp"
#include "pipelines/pipeline.hpp"

class Texture;
class VulkanBackend;
class ShaderModule;
class ComputePipeline;
class GraphicsPipeline;
class RenderPass;

struct ParticleEmitterConfig {
	std::string name;
	glm::mat4 starting_transform;
	glm::vec3 min_box_extent;
	glm::vec3 max_box_extent;
	glm::vec3 min_starting_velocity;
	glm::vec3 max_starting_velocity;
	float lifetime_after_collision = 0.0f; // s

	// graphics settings
	std::string texture_atlas;
	uint32_t subpass_number = 0;

	// profiler settings
	bool profile = false;
	uint32_t start_query_num = 0;
	uint32_t stop_query_num = 0;
};

class ParticleEmitterBase {
public:
	virtual ~ParticleEmitterBase();

	const std::string& getName() const { return config_.name; }
	
	void setTransform(const glm::mat4& transform);
	RecordCommandsResult update(float delta_time_s, const SceneData& scene_data);
	virtual RecordCommandsResult renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) = 0;

	bool createParticles(uint32_t count);
	bool createComputePipeline(std::shared_ptr<Texture>& scene_depth_buffer);

	virtual DescriptorPoolConfig getDescriptorsCount() const = 0;
	virtual bool createGraphicsPipeline(const RenderPass& render_pass, uint32_t subpass_number) = 0;

protected:
	explicit ParticleEmitterBase(const ParticleEmitterConfig& config, VulkanBackend* backend);

	virtual bool createAssets(std::vector<Particle>& particles) = 0;
	virtual void createUniformBuffers() = 0;
	virtual void createGraphicsDescriptorSets() = 0;
	virtual void updateGraphicsDescriptorSets() = 0;
	virtual void createComputeDescriptorSets() = 0;
	virtual void updateComputeDescriptorSets(std::shared_ptr<Texture>& scene_depth_buffer) = 0;
	virtual RecordCommandsResult recordComputeCommands() = 0;

	std::shared_ptr<Texture> getTexture() { return texture_atlas_; }

	bool setupIndirectBuffers();

	ParticleEmitterConfig config_;
	glm::mat4 transform_;

	VulkanBackend* backend_;
	Buffer particle_buffer_;
	Buffer particle_respawn_buffer_;
	Buffer hit_buffer_;
	Buffer collision_particle_buffer_;   // used for spawning additional particles at the collision points
	Buffer dispatch_indirect_cmds_;
	Buffer dispatch_indirect_cmds_reset_;
	Buffer draw_indirect_cmds_;
	Buffer draw_indirect_cmds_reset_;
	std::shared_ptr<Texture> texture_atlas_;

	std::shared_ptr<ShaderModule> compute_shader_;
	std::shared_ptr<ShaderModule> collision_compute_shader_;

	std::shared_ptr<ShaderModule> vertex_shader_;
	std::shared_ptr<ShaderModule> geometry_shader_;
	std::shared_ptr<ShaderModule> fragment_shader_;

	std::shared_ptr<ShaderModule> collision_vertex_shader_;
	std::shared_ptr<ShaderModule> collision_fragment_shader_;

	std::vector<VkDescriptorSet> vk_descriptor_sets_compute_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_collision_compute_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_graphics_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_collision_graphics_;

	struct CameraData {
		glm::mat4 view_matrix; 
		glm::mat4 proj_matrix;
		glm::ivec2 framebuffer_size;
	} compute_camera_;  
	UniformBuffer compute_camera_buffer_;
	UniformBuffer graphics_view_proj_buffer_;

	ParticlesGlobalState global_state_pc_;

	std::unique_ptr<ComputePipeline> compute_pipeline_;
	std::unique_ptr<GraphicsPipeline> graphics_pipeline_;
	std::unique_ptr<ComputePipeline> collision_compute_pipeline_;
	std::unique_ptr<GraphicsPipeline> collision_graphics_pipeline_;
	std::vector<VkCommandBuffer> compute_command_buffers_;
	std::vector<VkCommandBuffer> graphics_command_buffers_; 
};
