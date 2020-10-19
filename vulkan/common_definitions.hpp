/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <array>
#include <utility>
#include <vector>
#include <string>
#include <cstring>  // for memcpy in gcc
#include <functional>
#include <memory>
#include <iostream>

using VertexFormatInfo = std::pair<size_t, std::vector<size_t>>;
class Texture;

struct Buffer {
    std::string name;
    bool host_visible = false;
    size_t buffer_size = 0;

    VkBufferUsageFlags type = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_buffer_memory = VK_NULL_HANDLE;
    VkBufferView vk_buffer_view = VK_NULL_HANDLE; // optional
};

struct UniformBuffer {
    std::string name;
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
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;

    std::shared_ptr<Texture> colour_attachment;
    std::shared_ptr<Texture> depth_attachment;
    std::vector<VkFramebuffer> swap_chain_framebuffers;
};

using RecordCommandsResult = std::tuple<bool, std::vector<VkCommandBuffer>>;
inline RecordCommandsResult makeRecordCommandsResult(bool success, std::vector<VkCommandBuffer>& command_buffers) {
    return std::make_tuple(success, command_buffers);
}

using BindingsMap = std::map<std::string, uint32_t>;
struct DescriptorSetMetadata {
    std::map<uint32_t, BindingsMap> set_bindings;
};

using PushConstantsMap = std::map<std::string, VkPushConstantRange>;

enum class PipelineType { UNKNOWN = -1, GRAPHICS, COMPUTE };
struct Pipeline {
    std::string name;
    PipelineType type = PipelineType::UNKNOWN;

    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    std::map<uint32_t, VkDescriptorSetLayout> vk_descriptor_set_layouts;

    DescriptorSetMetadata descriptor_metadata;
    PushConstantsMap push_constants;
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

struct ParticleVertex {
    glm::vec4 pos;
    glm::vec4 vel;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ParticleVertex, pos),  offsetof(ParticleVertex, vel) };
        return { sizeof(ParticleVertex) , offsets };
    }

    bool operator==(const ParticleVertex& other) const {
        return pos == other.pos && vel == other.vel;
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

const uint32_t COMPUTE_PARTICLE_BUFFER_SET_ID = 0;
const std::string COMPUTE_PARTICLE_BUFFER_BINDING_NAME = "particle_buffer";
const std::string COMPUTE_RESPAWN_BUFFER_BINDING_NAME = "respawn_buffer";

struct ParticlesGlobalState {
    uint32_t particles_count = 0;
    float delta_time_s = 0.0f;
}; // push constant
const std::string COMPUTE_PARTICLES_GLOBAL_STATE_PC = "GlobalState";

struct UiTransform {
    glm::vec2 scale;
    glm::vec2 translate;
}; // push constant
const std::string UI_TRANSFORM_PUSH_CONSTANT = "UiTransform";

const uint32_t UI_UNIFORM_SET_ID = 0;
const std::string UI_TEXTURE_SAMPLER_BINDING_NAME = "fonts_sampler"; 
