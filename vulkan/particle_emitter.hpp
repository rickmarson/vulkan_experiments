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

class ParticleEmitter {
public:
	static std::shared_ptr<ParticleEmitter> createParticleEmitter(const std::string& name, VulkanBackend* backend);

	explicit ParticleEmitter(const std::string& name, VulkanBackend* backend);
	~ParticleEmitter();

	const std::string& getName() const { return name_; }
	
	void setTransform(const glm::mat4& transform);
	RecordCommandsResult update(float delta_time_s);

	bool createParticles(uint32_t count, const std::string& shader_file);
	const Buffer& getVertexBuffer() const { return vertex_buffer_; }
	uint32_t getVertexCount() const { return global_state_pc_.particles_count; }
	
	void createUniformBuffer();
	void deleteUniformBuffer();
	void createGraphicsDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateGraphicsDescriptorSets(const DescriptorSetMetadata& metadata);
	std::vector<VkDescriptorSet>& getGraphicsDescriptorSets() { return vk_descriptor_sets_graphics_; }
	
	std::shared_ptr<Texture> getTexture() { return texture_; }
	const UniformBuffer& getUniformBuffer() const { return uniform_buffer_; }

	bool createComputePipeline();

private:
	void createComputeDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateComputeDescriptorSets(const DescriptorSetMetadata& metadata);
	RecordCommandsResult recordComputeCommands();

	std::string name_;

	VulkanBackend* backend_;
	Buffer vertex_buffer_;
	std::shared_ptr<Texture> texture_;

	ModelData model_data_;
	UniformBuffer uniform_buffer_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_graphics_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_compute_;

	ParticlesGlobalState global_state_pc_;

	std::shared_ptr<ShaderModule> compute_shader_;
	Pipeline compute_pipeline_;
	std::vector<VkCommandBuffer> compute_command_buffers_;
};