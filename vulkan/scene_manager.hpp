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
	std::shared_ptr<StaticMesh> getObject(const std::string& name) const;
	std::shared_ptr<StaticMesh> getObjectByIndex(uint32_t idx) const;

	void setFollowTarget(bool should_follow) { follow_target_ = should_follow; }
	void setCameraProperties(float fov_deg, float aspect_ratio, float z_near, float z_far);
	void setCameraPosition(const glm::vec3& pos);
	void setCameraTarget(const glm::vec3& target);
	void setCameraTransform(const glm::mat4 transform);

	void setLightPosition(const glm::vec3& pos);
	void setLightColour(const glm::vec4& colour, float intensity = 1.0f);
	void setAmbientColour(const glm::vec4& colour, float intensity = 1.0f);
	void enableShadows();
	
	const SceneData& getSceneData() const { return scene_data_; }

	DescriptorPoolConfig getDescriptorsCount(uint32_t expected_pipelines_count) const;
	std::shared_ptr<StaticMesh> getMeshByIndex(uint32_t idx);
	std::shared_ptr<StaticMesh> getMeshByName(const std::string& name);
	std::shared_ptr<Texture> getTexture(uint32_t idx);
	std::shared_ptr<Material> getMaterial(uint32_t idx);

	void update();

	void createUniforms();
	void deleteUniforms();
	void createDescriptorSets(const std::string& pipeline_name, const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateDescriptorSets(const std::string& pipeline_name, const DescriptorSetMetadata& metadata);
	void createGeometryDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateGeometryDescriptorSets(const DescriptorSetMetadata& metadata, bool with_material = true);
	std::vector<VkDescriptorSet>& getDescriptorSets(const std::string& pipeline_name);
	std::shared_ptr<Texture>& getSceneDepthBuffer() { return scene_depth_buffer_; }

	void bindSceneDescriptors(VkCommandBuffer& cmd_buffer, const Pipeline& pipeline, uint32_t swapchain_index);  // these can be shared across multiple pipelines that need scene-level data
	void drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index, bool with_material = true);
	void renderStaticShadowMap();

private:
	void updateCameraTransform();
	glm::mat4 lookAtMatrix() const;
	glm::mat4 lightViewMatrix() const;
	glm::mat4 shadowMapProjection() const;
	void setupShadowMapAssets();
	void createShadowMapDescriptors();

	VulkanBackend* backend_;
	SceneData scene_data_;
	UniformBuffer scene_data_buffer_;
	std::shared_ptr<Texture> scene_depth_buffer_;
	std::map<std::string, std::vector<VkDescriptorSet>> vk_descriptor_sets_;

	Buffer scene_vertex_buffer_;
	Buffer scene_index_buffer_;
	std::vector<std::shared_ptr<Texture>> textures_;
	std::vector<std::shared_ptr<Material>> materials_;
	std::vector<std::shared_ptr<StaticMesh>> meshes_;

	float gltf_scale_factor_ = 1.0f;
	glm::vec3 camera_position_ = { 0.0f, 0.0f, 0.0f };
	glm::vec3 camera_forward_ = { 1.0f, 0.0f, 0.0f };
	glm::vec3 camera_up_ = { 0.0f, 0.0f, 1.0f };
	glm::vec3 camera_look_at_;
	glm::mat4 camera_transform_ = glm::mat4(1.0f);
	bool follow_target_ = false;

	bool shadows_enabled_ = false;
	const uint32_t shadow_map_width_ = 2048;
	const uint32_t shadow_map_height_ = 2048;
	RenderPass shadow_map_render_pass_;
	Pipeline shadow_map_pipeline_;
	ShadowMapData shadow_map_data_;
	UniformBuffer shadow_map_data_buffer_;
	std::vector<VkDescriptorSet> vk_shadow_descriptor_sets_;
};
