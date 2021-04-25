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
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <array>
#include <utility>
#include <vector>
#include <list>
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

struct MaterialData { 
    alignas(16) glm::vec3 emissive_factor = glm::vec3(-1.0f);
    alignas(4)  float metallic_factor = -1.0f;
    alignas(16) float roughness_factor = -1.0f;

    alignas(4) int diffuse_idx = -1;
    alignas(4) int metal_rough_idx = -1;
    alignas(4) int normal_idx = -1;
    alignas(4) int emissive_idx = -1;
};

struct Material {
    MaterialData material_data;
    UniformBuffer material_uniform;
};

struct DescriptorPoolConfig {
    uint32_t uniform_buffers_count = 0;
    uint32_t image_samplers_count = 0;
    uint32_t storage_texel_buffers_count = 0;
    uint32_t image_storage_buffers_count = 0;
    uint32_t max_sets = 0;

    DescriptorPoolConfig& operator +(const DescriptorPoolConfig& other) {
        uniform_buffers_count += other.uniform_buffers_count;
        image_samplers_count += other.image_samplers_count;
        storage_texel_buffers_count += other.storage_texel_buffers_count;
        image_storage_buffers_count += other.image_storage_buffers_count;
        return *this;
    }

    DescriptorPoolConfig& operator *(uint32_t multiplier) {
        uniform_buffers_count *= multiplier;
        image_samplers_count *= multiplier;
        storage_texel_buffers_count *= multiplier;
        image_storage_buffers_count *= multiplier;
        return *this;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct RenderPass {
    std::string name;
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    VkViewport viewport = {0, 0, 0, 0, 0, 1};
    VkRect2D scissor = {0, 0};
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;

    std::shared_ptr<Texture> colour_attachment;
    std::shared_ptr<Texture> depth_attachment;
    std::vector<VkFramebuffer> framebuffers;
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
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 tex_coord;

    static VertexFormatInfo getFormatInfo() {
        std::vector<size_t> offsets = { offsetof(Vertex, pos), offsetof(Vertex, normal), offsetof(Vertex, tangent), offsetof(Vertex, tex_coord) };
        return { sizeof(Vertex) , offsets };
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && normal == other.normal && tangent == other.tangent && tex_coord == other.tex_coord;
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
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    glm::vec4 light_position = glm::vec4(0.0f);
    glm::vec4 light_intensity = glm::vec4(1.0f);
    glm::vec4 ambient_intensity = glm::vec4(0.0f);
};

struct ViewProj {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
};

const uint32_t SCENE_UNIFORM_SET_ID = 0;  // all scene-wide uniforms (lights, camera, etc.)
const uint32_t VIEW_PROJ_SET_ID = SCENE_UNIFORM_SET_ID;  // a subset of SceneData for pipelines that don't need lighting
const std::string SCENE_DATA_BINDING_NAME = "scene";  // holds scene-wide information (view, projection, lights, etc..)
const std::string VIEW_PROJ_BINDING_NAME = "view_proj"; 
const std::string SCENE_TEXTURES_ARRAY = "scene_textures"; // global binding point holding all textures in the scene
const std::string SCENE_DEPTH_BUFFER_STORAGE = "scene_depth_buffer";  // texel storage buffers used to store / load depth info across pipelines

struct ShadowMapData {
    glm::mat4 light_view = glm::mat4(1.0f);
    glm::mat4 shadow_proj = glm::mat4(1.0f);
};

// bindings for shadow-map lookup on the lit pipeline(s)
const uint32_t SHADOW_MAP_SET_ID = 3;
const std::string SHADOW_MAP_PROJ_NAME = "shadow_map_data";
const std::string SHADOW_MAP_NAME = "shadow_map";

// bindings on the shadow map generation pipeline
const uint32_t SHADOW_MAP_DATA_UNIFORM_SET_ID = 0;  
const std::string SHADOW_MAP_DATA_BINDING_NAME = "shadow";
// ----

struct ModelData {
    glm::mat4 transform_matrix;
};

const uint32_t MODEL_UNIFORM_SET_ID = 1;  // all uniforms that apply to one object (rotation, translation, etc...)
const std::string MODEL_DATA_BINDING_NAME = "model";  // holds model-specific numeric data (model transform, etc...)

const uint32_t SURFACE_UNIFORM_SET_ID = 2;  // all samplers that apply to one surface (one object can have multiple surfaces)
const std::string SURFACE_MATERIAL_BINDING_NAME = "material";

const uint32_t PARTICLES_UNIFORM_SET_ID = 1;
const std::string PARTICLES_TEXTURE_ATLAS_BINDING_NAME = "texture_atlas";

const uint32_t COMPUTE_PARTICLE_BUFFER_SET_ID = 0;
const std::string COMPUTE_PARTICLE_BUFFER_BINDING_NAME = "particle_buffer";
const std::string COMPUTE_RESPAWN_BUFFER_BINDING_NAME = "respawn_buffer";

const uint32_t COMPUTE_CAMERA_SET_ID = 1;  // camera / scene related data passed to compute shaders
const std::string CAMERA_BINDING_NAME = "camera"; 
// SCENE_DEPTH_BUFFER_STORAGE is also part of this set at binding 1, 1

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
