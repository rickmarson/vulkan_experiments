/*
* static_mesh.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class Texture;
class VulkanBackend;

class StaticMesh {
public:
	struct Surface {
		~Surface() {
			vk_descriptor_sets.clear();
		}

		uint32_t vertex_start;
		uint32_t vertex_count;
		uint32_t index_start;
		uint32_t index_count;
		std::weak_ptr<Material> material_weak;

		std::vector<VkDescriptorSet> vk_descriptor_sets;

		void createDescriptorSets(VulkanBackend* backend, const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
		void updateDescriptorSets(VulkanBackend* backend, const DescriptorSetMetadata& metadata);
	};

	static std::shared_ptr<StaticMesh> createStaticMesh(const std::string& name, VulkanBackend* backend);

	explicit StaticMesh(const std::string& name, VulkanBackend* backend);
	~StaticMesh();

	const std::string& getName() const { return name_; }
	Surface& addSurface();

	void setTransform(const glm::mat4& transform);
	const glm::mat4& getTransform() const { return model_data_.transform_matrix; }
	void update();

	void createUniformBuffer();
	const UniformBuffer& getUniformBuffer() const { return uniform_buffer_; }
	void deleteUniformBuffer();
	DescriptorPoolConfig getDescriptorsCount() const;
	void createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateDescriptorSets(const DescriptorSetMetadata& metadata);
	std::vector<VkDescriptorSet>& getDescriptorSets() { return vk_descriptor_sets_; }

	void drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index, bool with_material = true);

private:
	std::string name_;

	VulkanBackend* backend_;

	ModelData model_data_;
	UniformBuffer uniform_buffer_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_;

	std::vector<Surface> surfaces_;
};