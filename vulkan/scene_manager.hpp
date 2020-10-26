/*
* scene_manager.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class VulkanBackend;
class Texture;
class StaticMesh;

/*
* Lights, Cameras and Static Environment geometry
*/
class SceneManager {
public:
	static std::unique_ptr<SceneManager> create(VulkanBackend* backend);

	explicit SceneManager(VulkanBackend* backend);
	~SceneManager();

	bool loadFromGlb(const std::string& file_path);
	std::shared_ptr<StaticMesh> addObject(const std::string& name);

	void setFollowTarget(bool should_follow) { follow_target_ = should_follow; }
	void setCameraProperties(float fov_deg, float aspect_ratio, float z_near, float z_far);
	void setCameraPosition(const glm::vec3& pos);
	void setCameraTarget(const glm::vec3& target);
	void setCameraTransform(const glm::mat4 transform);

	DescriptorPoolConfig getDescriptorsCount() const;
	std::shared_ptr<StaticMesh> getMeshByIndex(uint32_t idx);
	std::shared_ptr<StaticMesh> getMeshByName(const std::string& name);
	std::shared_ptr<Texture> getTexture(uint32_t idx);
	std::shared_ptr<Material> getMaterial(uint32_t idx);

	void update();

	void createUniformBuffer();
	void deleteUniformBuffer();
	void createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateDescriptorSets(const DescriptorSetMetadata& metadata);
	std::vector<VkDescriptorSet>& getDescriptorSets() { return vk_descriptor_sets_; }

	void drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index);

private:
	glm::mat4 lookAtMatrix() const;

	VulkanBackend* backend_;
	SceneData scene_data_;
	UniformBuffer uniform_buffer_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_;

	Buffer scene_vertex_buffer_;
	Buffer scene_index_buffer_;
	std::vector<std::shared_ptr<Texture>> textures_;
	std::vector<std::shared_ptr<Material>> materials_;
	std::vector<std::shared_ptr<StaticMesh>> meshes_;

	glm::vec3 camera_position_;
	glm::vec3 camera_look_at_;
	glm::vec3 camera_up_ = { 0.0f, 0.0f, 1.0f };
	glm::quat camera_rotation_ = { 1.0, 0.0, 0.0, 0.0 };
	bool follow_target_ = false;
};
