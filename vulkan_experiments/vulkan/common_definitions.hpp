/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <map>
#include <unordered_map>
#include <set>
#include <array>
#include <utility>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <iostream>

using VertexFormatInfo = std::pair<size_t, std::vector<size_t>>;

struct Buffer {
    std::string name;
    size_t key = 0;

    VkBufferUsageFlags type = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_buffer_memory = VK_NULL_HANDLE;
};

struct UniformBuffer {
    std::string name;
    size_t key = 0;
    size_t buffer_size = 0;

    std::vector<Buffer> buffers;  // one per command buffer / swap chain image
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static VertexFormatInfo getFormatInfo() {
		std::vector<size_t> offsets = { offsetof(Vertex, pos), offsetof(Vertex, color), offsetof(Vertex, tex_coord) };
		return { sizeof(Vertex) , offsets };
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
	}
};
