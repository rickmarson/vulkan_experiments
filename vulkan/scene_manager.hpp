/*
* scene_manager.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class VulkanBackend;

class SceneManager {
public:
	static std::unique_ptr<SceneManager> create(VulkanBackend* backend);

	explicit SceneManager(VulkanBackend* backend);
	~SceneManager();

	void setCameraProperties(float fov, float aspect_ratio, float z_near, float z_far);
	void setCameraPosition(const glm::vec3& pos);
	void setCameraTarget(const glm::vec3& target);

	void update();

	void createUniformBuffer();
	void deleteUniformBuffer();
	void createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateDescriptorSets(const DescriptorSetMetadata& metadata);
	std::vector<VkDescriptorSet>& getDescriptorSets() { return vk_descriptor_sets_; }

private:
	VulkanBackend* backend_;
	SceneData scene_data_;
	UniformBuffer uniform_buffer_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_;

	glm::vec3 camera_position_;
	glm::vec3 camera_look_at_;
	glm::vec3 camera_up_ = { 0.0f, 0.0f, 1.0f };
};
