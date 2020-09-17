/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class Texture;
class VulkanBackend;

class Mesh {
public:
	static std::shared_ptr<Mesh> createMesh(const std::string& name, VulkanBackend* backend);

	explicit Mesh(const std::string& name, VulkanBackend* backend);
	~Mesh();

	const std::string& getName() const { return name_; }
	bool loadObjModel(const std::string& obj_file_path);

	const Buffer& getVertexBuffer() const { return vertex_buffer_; }
	const Buffer& getIndexBuffer() const { return index_buffer_; }
	uint32_t getVertexCount() const { return vertex_count_; }
	uint32_t getIndexCount() const { return index_count_; }

	std::shared_ptr<Texture> getDiffuseTexture() { return textures_["diffuse_colour"]; }

private:
	std::string name_;
	
	glm::mat4 model_matrix_;

	VulkanBackend* backend_;
	Buffer vertex_buffer_;
	Buffer index_buffer_;
	std::map<std::string, std::shared_ptr<Texture>> textures_;

	uint32_t vertex_count_;
	uint32_t index_count_;
};