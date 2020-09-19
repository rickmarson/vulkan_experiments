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

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct RenderPass {
    std::string name;
    size_t key = 0;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swap_chain_framebuffers;
};

using BindingsMap = std::map<std::string, uint32_t>;
struct DescriptorSetMetadata {
    std::map<uint32_t, BindingsMap> set_bindings;
};

// shader interfaces
// these must match the format, names and binding points defined in the shader code

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

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
};

const uint32_t SCENE_UNIFORM_SET_ID = 0;
const std::string SCENE_DATA_BINDING_NAME = "scene";  // holds scene-wide information (view, projection, lights, etc..)

struct ModelData {
    glm::mat4 transform_matrix;
};

const uint32_t MODEL_UNIFORM_SET_ID = 1;
const std::string MODEL_DATA_BINDING_NAME = "model";  // holds model-specific numeric data (model transform, etc...)
const std::string DIFFUSE_SAMPLER_BINDING_NAME = "diffuse_sampler";  // holds the diffuse texture of the surface / mesh being drawn
