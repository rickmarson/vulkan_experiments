/*
* mesh.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class Texture;
class VulkanBackend;
class ShaderModule;

struct ParticleEmitterConfig {
	std::string name;
	glm::mat4 starting_transform;
	glm::vec3 min_box_extent;
	glm::vec3 max_box_extent;
	glm::vec3 min_starting_velocity;
	glm::vec3 max_starting_velocity;
	float lifetime_after_collision = 0.0f; // s

	std::string texture_atlas;

	// profiler settings
	bool profile = false;
	uint32_t start_query_num = 0;
	uint32_t stop_query_num = 0;
};

class ParticleEmitter {
public:
	static std::shared_ptr<ParticleEmitter> createParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend);

	explicit ParticleEmitter(const ParticleEmitterConfig& config, VulkanBackend* backend);
	~ParticleEmitter();

	const std::string& getName() const { return config_.name; }
	
	void setTransform(const glm::mat4& transform);
	RecordCommandsResult update(float delta_time_s, const SceneData& scene_data);

	bool createParticles(uint32_t count, const std::string& shader_file);
	const Buffer& getVertexBuffer() const { return particle_buffer_; }
	uint32_t getVertexCount() const { return global_state_pc_.particles_count; }
	
	void createUniformBuffers();
	void deleteUniformBuffers();
	DescriptorPoolConfig getDescriptorsCount() const;
	void createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata);
	std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() { return vk_descriptor_sets_graphics_; }
	
	std::shared_ptr<Texture> getTexture() { return texture_atlas_; }
	const UniformBuffer& getUniformBuffer() const { return graphics_uniform_buffer_; }

	bool createComputePipeline(std::shared_ptr<Texture>& scene_depth_buffer);

private:
	void createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateComputeDescriptorSets(const DescriptorSetMetadata& metadata, std::shared_ptr<Texture>& scene_depth_buffer);
	RecordCommandsResult recordComputeCommands();

	ParticleEmitterConfig config_;
	glm::mat4 transform_;

	VulkanBackend* backend_;
	Buffer particle_buffer_;
	Buffer particle_respawn_buffer_;
	std::shared_ptr<Texture> texture_atlas_;

	UniformBuffer graphics_uniform_buffer_; // unused for now
	std::vector<VkDescriptorSet> vk_descriptor_sets_graphics_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_compute_;

	struct CameraData {
		glm::mat4 view_matrix; 
		glm::mat4 proj_matrix;
		glm::ivec2 framebuffer_size;
	} compute_camera_;  
	UniformBuffer compute_camera_buffer_;

	ParticlesGlobalState global_state_pc_;

	std::shared_ptr<ShaderModule> compute_shader_;
	Pipeline compute_pipeline_;
	std::vector<VkCommandBuffer> compute_command_buffers_;
};