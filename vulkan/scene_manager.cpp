/*
* scene_manager.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "scene_manager.hpp"
#include "vulkan_backend.hpp"
#include "texture.hpp"
#include "static_mesh.hpp"
#include "shader_module.hpp"
#include "pipelines/graphics_pipeline.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_USE_CPP14 
#define TINYGLTF_NO_STB_IMAGE_WRITE 
#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gltf = tinygltf;

namespace {
    // gltf helper functions
    void processMeshNode(SceneManager* manager, gltf::Model& model, const gltf::Node& node, const glm::mat4& parent_transform, std::vector<Vertex>& vertex_buffer, std::vector<uint32_t>& index_buffer) {
        auto& gltf_mesh = model.meshes[node.mesh];
        auto static_mesh = manager->addObject(node.name);
        static_mesh->setTransform(parent_transform);

        for (auto& p : gltf_mesh.primitives) {
            auto material = manager->getMaterial(p.material);
            uint32_t vertex_start = static_cast<uint32_t>(vertex_buffer.size());
            uint32_t index_start = static_cast<uint32_t>(index_buffer.size());
            uint32_t index_count = 0;
            uint32_t vertex_count = 0;
            bool has_indices = p.indices > -1;

            const float* buffer_pos = nullptr;
            const float* buffer_normals = nullptr;
            const float* buffer_tangents = nullptr;
            const float* buffer_uv = nullptr;
            int pos_stride;
            int norm_stride;
            int tan_stride;
            int uv_stride;

            const gltf::Accessor& pos_accessor = model.accessors[p.attributes.find("POSITION")->second];
            const gltf::BufferView& pos_view = model.bufferViews[pos_accessor.bufferView];
            buffer_pos = reinterpret_cast<const float*>(&(model.buffers[pos_view.buffer].data[pos_accessor.byteOffset + pos_view.byteOffset]));
            vertex_count = static_cast<uint32_t>(pos_accessor.count);
            pos_stride = pos_accessor.ByteStride(pos_view) ? (pos_accessor.ByteStride(pos_view) / sizeof(float)) : 3;

            if (p.attributes.find("TEXCOORD_0") != p.attributes.end()) {
                // slight simplification: assume all textures share the same uv space
                const tinygltf::Accessor& uv_accessor = model.accessors[p.attributes.find("TEXCOORD_0")->second];
                const tinygltf::BufferView& uv_view = model.bufferViews[uv_accessor.bufferView];
                buffer_uv = reinterpret_cast<const float*>(&(model.buffers[uv_view.buffer].data[uv_accessor.byteOffset + uv_view.byteOffset]));
                uv_stride = uv_accessor.ByteStride(uv_view) ? (uv_accessor.ByteStride(uv_view) / sizeof(float)) : 2;
            }

            if (p.attributes.find("NORMAL") != p.attributes.end()) {
                const gltf::Accessor& norm_accessor = model.accessors[p.attributes.find("NORMAL")->second];
                const gltf::BufferView& norm_view = model.bufferViews[norm_accessor.bufferView];
                buffer_normals = reinterpret_cast<const float*>(&(model.buffers[norm_view.buffer].data[norm_accessor.byteOffset + norm_view.byteOffset]));
                norm_stride = norm_accessor.ByteStride(norm_view) ? (norm_accessor.ByteStride(norm_view) / sizeof(float)) : 3;
            }

            if (p.attributes.find("TANGENT") != p.attributes.end()) {
                const gltf::Accessor& tan_accessor = model.accessors[p.attributes.find("TANGENT")->second];
                const gltf::BufferView& tan_view = model.bufferViews[tan_accessor.bufferView];
                buffer_tangents = reinterpret_cast<const float*>(&(model.buffers[tan_view.buffer].data[tan_accessor.byteOffset + tan_view.byteOffset]));
                tan_stride = tan_accessor.ByteStride(tan_view) ? (tan_accessor.ByteStride(tan_view) / sizeof(float)) : 4;
            }

            for (size_t v = 0; v < pos_accessor.count; v++) {
                auto gltf_pos = glm::make_vec3(&buffer_pos[v * pos_stride]);
                auto gltf_norm = glm::make_vec3(&buffer_normals[v * norm_stride]);
                Vertex vert{};
                vert.pos[0] = -gltf_pos[2];  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                vert.pos[1] = gltf_pos[0];
                vert.pos[2] = gltf_pos[1];
                vert.normal[0] = -gltf_norm[2];
                vert.normal[1] = gltf_norm[0];
                vert.normal[2] = gltf_norm[1];
                vert.tex_coord = buffer_uv ? glm::make_vec2(&buffer_uv[v * uv_stride]) : glm::vec3(0.0f);
                
                if (buffer_tangents != nullptr) {
                    auto gltf_tan = glm::make_vec4(&buffer_tangents[v * tan_stride]);
                    vert.tangent[0] = -gltf_tan[2];
                    vert.tangent[1] = gltf_tan[0];
                    vert.tangent[2] = gltf_tan[1];
                    vert.tangent[3] = gltf_tan[3];
                } else {
                    vert.tangent = glm::vec4(0.0f);
                }

                vertex_buffer.push_back(vert);
            }

            if (has_indices)
            {
                const gltf::Accessor& accessor = model.accessors[p.indices > -1 ? p.indices : 0];
                const gltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
                const gltf::Buffer& buffer = model.buffers[buffer_view.buffer];

                index_count = static_cast<uint32_t>(accessor.count);
                const void* data_ptr = &(buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
                switch (accessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const uint32_t* buf = static_cast<const uint32_t*>(data_ptr);
                        for (size_t index = 0; index < accessor.count; index++) {
                            index_buffer.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const uint16_t* buf = static_cast<const uint16_t*>(data_ptr);
                        for (size_t index = 0; index < accessor.count; index++) {
                            index_buffer.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const uint8_t* buf = static_cast<const uint8_t*>(data_ptr);
                        for (size_t index = 0; index < accessor.count; index++) {
                            index_buffer.push_back(buf[index]);
                        }
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                }
            }

            auto& surface = static_mesh->addSurface();
            surface.vertex_count = vertex_count;
            surface.index_count = index_count;
            surface.vertex_start = vertex_start;
            surface.index_start = index_start;
            surface.material_weak = material;
        }
    }

    void processCameraNode(SceneManager* manager, gltf::Model& model, const gltf::Node& node, const glm::mat4& parent_transform) {
        auto& camera = model.cameras[node.camera];

        if (camera.type == "perspective") {
            // TODO: can't really use the aspect ratio saved in the gltf as it depends on the current frame buffer.
            //manager->setCameraTransform(parent_transform);
            //manager->setCameraProperties(glm::degrees(camera.perspective.yfov), camera.perspective.aspectRatio, camera.perspective.znear, camera.perspective.zfar);
        }
    }

    void processLightNode(SceneManager* manager, gltf::Model& model, const gltf::Node& node, const glm::mat4& parent_transform) {
        // TODO
    }

    void processNode(SceneManager* manager, gltf::Model& model, const gltf::Node& node, const glm::mat4& parent_transform, std::vector<Vertex>& vertex_buffer, std::vector<uint32_t>& index_buffer) {
        auto local_transform = glm::mat4(1.0f);
        if (node.matrix.size() == 16) {
            local_transform = glm::make_mat4x4(node.matrix.data());
        } else {
            glm::mat4 translation = glm::mat4(1.0f);
            glm::mat4 rotation = glm::mat4(1.0f);
            glm::mat4 scale = glm::mat4(1.0f);
            if (node.translation.size() == 3) {
                auto gltf_trans = glm::make_vec3(node.translation.data());
                auto trans = glm::vec3(0.0f);
                trans[0] = float(-gltf_trans[2]);  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                trans[1] = float(gltf_trans[0]);
                trans[2] = float(gltf_trans[1]);
                translation = glm::translate(translation, trans);
            }
            if (node.rotation.size() == 4) {
                auto gltf_rot = glm::make_quat(node.rotation.data());
                glm::quat rot;
                rot.x = float(-gltf_rot.z);  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                rot.y = float(gltf_rot.x);
                rot.z = float(gltf_rot.y);
                rot.w = float(gltf_rot.w);
                rotation = glm::mat4(rot);
            }
            if (node.scale.size() == 3) {
                auto gltf_scale = glm::make_vec3(node.scale.data());
                auto sc = glm::vec3(0.0f);
                sc[0] = float(gltf_scale[2]);
                sc[1] = float(gltf_scale[0]);
                sc[2] = float(gltf_scale[1]);
                scale = glm::scale(scale, sc);
            }

            local_transform = translation * rotation * scale;
        }

        auto node_transform = parent_transform * local_transform;

        if (node.mesh > -1) {
            processMeshNode(manager, model, node, node_transform, vertex_buffer, index_buffer);
        }

        if (node.camera > -1) {
            processCameraNode(manager, model, node, node_transform);
        }

        if (!node.children.empty()) {
            for (auto c : node.children) {
                processNode(manager, model, model.nodes[c], node_transform, vertex_buffer, index_buffer);
            }
        }
    }

    float getGlobalScaleFactor(gltf::Model& model) {
        auto& scene = model.scenes[0];
        auto& node = model.nodes[scene.nodes[0]];

        if (node.scale.size() == 3) {
            auto gltf_scale = glm::make_vec3((node.scale.data()));
            auto sc = glm::vec3(0.0f);
            sc[0] = float(gltf_scale[2]);
            sc[1] = float(gltf_scale[0]);
            sc[2] = float(gltf_scale[1]);

            if (std::abs(sc[0] - sc[1]) > 1e-6 || std::abs(sc[1] - sc[2]) > 1e-6) {
                std::cerr << "Error: non-uniform scaling is not supported!" << std::endl;
            }

            return sc[0];
        }

        return 1.0f;
    }

}

std::unique_ptr<SceneManager> SceneManager::create(VulkanBackend* backend) {
    return std::make_unique<SceneManager>(backend);
}

SceneManager::SceneManager(VulkanBackend* backend) :
	backend_(backend) {

}

SceneManager::~SceneManager() {
    cleanupSwapChainAssets();
    backend_->destroyBuffer(scene_index_buffer_);
    backend_->destroyBuffer(scene_vertex_buffer_);
    for (auto& mat : materials_) {
        backend_->destroyUniformBuffer(mat->material_uniform);
    }
    materials_.clear();
    meshes_.clear();
    textures_.clear();
    if (shadows_enabled_) {
        backend_->destroyRenderPass(shadow_map_render_pass_);
        backend_->destroyUniformBuffer(shadow_map_data_buffer_);
        shadow_map_pipeline_.reset();
    }
    vk_descriptor_sets_.clear();
    backend_->freeCommandBuffers(command_buffers_);
}

bool SceneManager::loadFromGlb(const std::string& file_path) {
    gltf::TinyGLTF gltf_loader;
    gltf::Model gltf_model;
    std::string errors;
    std::string warnings;

    bool result = gltf_loader.LoadBinaryFromFile(&gltf_model, &errors, &warnings, file_path);

    if (!warnings.empty()) {
        std::cout << "[SceneManager] Got warnings while loading glb file " << file_path << ":\n\n" << warnings << std::endl;
    }
    if (!errors.empty()) {
        std::cerr << "[SceneManager] Got errors while loading glb file " << file_path << ":\n\n" << errors << std::endl;
    }
    if (!result) {
        std::cerr << "[SceneManager] Failed to parse glb file " << file_path << std::endl;
        return false;
    }

    // global buffers

    // load all textures
    for (auto& gltf_tex : gltf_model.textures) {
        auto& gltf_image = gltf_model.images[gltf_tex.source];
        auto texture = backend_->createTexture(gltf_image.name);
        texture->loadImageRGBA(gltf_image.width, gltf_image.height, gltf_image.component, true, gltf_image.image);
        texture->createSampler();
        textures_.push_back(std::move(texture));
    }

    // load all materials (with limited material support)
    for (auto& gltf_mat : gltf_model.materials) {
        auto material = std::make_shared<Material>();
        material->material_data.emissive_factor[0] = static_cast<float>(gltf_mat.emissiveFactor[0]);
        material->material_data.emissive_factor[1] = static_cast<float>(gltf_mat.emissiveFactor[1]);
        material->material_data.emissive_factor[2] = static_cast<float>(gltf_mat.emissiveFactor[2]);

        // only support one texture per type for now
        auto& pbr_metal_rough = gltf_mat.pbrMetallicRoughness;

        if (pbr_metal_rough.baseColorTexture.index > -1) {
            material->material_data.diffuse_idx = pbr_metal_rough.baseColorTexture.index;
        }
        if (pbr_metal_rough.metallicRoughnessTexture.index > -1) {
            material->material_data.metal_rough_idx = pbr_metal_rough.metallicRoughnessTexture.index;
        } else {
            material->material_data.metallic_factor = static_cast<float>(pbr_metal_rough.metallicFactor);
            material->material_data.roughness_factor = static_cast<float>(pbr_metal_rough.roughnessFactor);
        }
        if (gltf_mat.normalTexture.index > -1) {
            material->material_data.normal_idx = gltf_mat.normalTexture.index;
        }
        if (gltf_mat.emissiveTexture.index > -1) {
            material->material_data.emissive_idx = gltf_mat.emissiveTexture.index;
        }

        material->material_uniform = backend_->createUniformBuffer<MaterialData>("material_" + std::to_string(materials_.size()));
        for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
            backend_->updateBuffer<MaterialData>(material->material_uniform.buffers[i], { material->material_data });
        }
       
        materials_.push_back(std::move(material));
    }
    
    // store the entire scene in one big buffer, individual meshes will be accessed via offsets
    std::vector<Vertex> vertex_buffer;
    std::vector<uint32_t> index_buffer;

    // load all meshes for one scene. only handles static meshes, no animations, no skins
    // flatten the scene graph into a list of mesh nodes
    gltf_scale_factor_ = getGlobalScaleFactor(gltf_model);
    setLightPosition(scene_data_.light_position);

    auto& scene = gltf_model.scenes[0];
    for (auto n : scene.nodes) {
        auto& gltf_node = gltf_model.nodes[n];
        glm::mat4 transform = glm::mat4(1.0f);
        processNode(this, gltf_model, gltf_node, transform, vertex_buffer, index_buffer);
    }

    scene_vertex_buffer_ = backend_->createVertexBuffer<Vertex>("scene_manager_vb", vertex_buffer, false);
    scene_index_buffer_ = backend_->createIndexBuffer<uint32_t>("scene_manager_ib", index_buffer, false);

    return true;
}

std::shared_ptr<StaticMesh> SceneManager::addObject(const std::string& name) {
    meshes_.push_back(backend_->createStaticMesh(name));
    return meshes_.back();
}

std::shared_ptr<StaticMesh> SceneManager::getObject(const std::string& name) const {
    for (auto& mesh : meshes_) {
        if (mesh->getName() == name) {
            return mesh;
        }
    }
    return std::shared_ptr<StaticMesh>();
}

std::shared_ptr<StaticMesh> SceneManager::getObjectByIndex(uint32_t idx) const {
    if (idx < meshes_.size()) {
        return meshes_[idx];
    }
    return std::shared_ptr<StaticMesh>();
}

void SceneManager::setCameraProperties(float fov_deg, float aspect_ratio, float z_near, float z_far) {
    scene_data_.proj = glm::perspective(glm::radians(fov_deg), aspect_ratio, z_near, z_far);
}

void SceneManager::setCameraPosition(const glm::vec3& pos) {
    camera_position_ = pos;
    updateCameraTransform();
}

void SceneManager::setCameraTarget(const glm::vec3& target) {
    camera_look_at_ = target;
    follow_target_ = true;
    updateCameraTransform();
}

void SceneManager::setCameraTransform(const glm::mat4 transform) {
    camera_transform_ = transform;
}

void SceneManager::setLightPosition(const glm::vec3& pos) {
    // need the light position to be consistent with the scene scale factor or it
    // will be off once it goes through the model matrix
    scene_data_.light_position = glm::vec4(pos / gltf_scale_factor_, 1.0f);
}

void SceneManager::setLightColour(const glm::vec4& colour, float intensity) {
    scene_data_.light_intensity = colour * intensity;
}

void SceneManager::setAmbientColour(const glm::vec4& colour, float intensity) {
    scene_data_.ambient_intensity = colour * intensity;
}

void SceneManager::enableShadows() {
    shadows_enabled_ = true;
}

DescriptorPoolConfig SceneManager::getDescriptorsCount(uint32_t expected_pipelines_count) const {
    DescriptorPoolConfig config;
    for (auto& mesh : meshes_) {
        auto mesh_config = mesh->getDescriptorsCount();
        config = config + mesh_config;
    }

    config.uniform_buffers_count += 1;
    config.image_storage_buffers_count += 1;
    config.image_samplers_count += uint32_t(textures_.size());

    config = config * expected_pipelines_count;
    
    if (shadows_enabled_) {
        config.uniform_buffers_count += 2;
        config.image_samplers_count += 1;
    }

    return config;
}

std::shared_ptr<StaticMesh> SceneManager::getMeshByIndex(uint32_t idx) {
    if (idx < meshes_.size()) {
        return meshes_[idx];
    }
    return std::shared_ptr<StaticMesh>();
}

std::shared_ptr<StaticMesh> SceneManager::getMeshByName(const std::string& name) {
    for (auto& m : meshes_) {
        if (m->getName() == name) {
            return m;
        }
    }
    return std::shared_ptr<StaticMesh>();
}

std::shared_ptr<Texture> SceneManager::getTexture(uint32_t idx) {
    if (idx < textures_.size()) {
        return textures_[idx];
    }
    return std::shared_ptr<Texture>();
}

std::shared_ptr<Material> SceneManager::getMaterial(uint32_t idx) {
    if (idx < materials_.size()) {
        return materials_[idx];
    }
    return std::shared_ptr<Material>();
}

bool SceneManager::createGraphicsPipeline(const std::string& program_name, RenderPass& render_pass, uint32_t subpass_number) { 
    auto vertex_shader_name = program_name + "_vs";
    auto fragment_shader_name = program_name + "_fs";

    vertex_shader_ = backend_->createShaderModule(vertex_shader_name);
	vertex_shader_->loadSpirvShader(std::string("shaders/") + vertex_shader_name + ".spv");

	if (!vertex_shader_->isVertexFormatCompatible(Vertex::getFormatInfo())) {
		std::cerr << "Vertex format is not compatible with pipeline input for " << vertex_shader_->getName() << std::endl;
		return false;
	}

	fragment_shader_ = backend_->createShaderModule(fragment_shader_name);
	fragment_shader_->loadSpirvShader(std::string("shaders/") + fragment_shader_name + ".spv");

	if (!vertex_shader_->isValid() || !fragment_shader_->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

    command_buffers_ = backend_->createSecondaryCommandBuffers(backend_->getSwapChainSize());
    if (command_buffers_.empty()) {
        return false;
    }

    scene_subpass_number_ = subpass_number;

    scene_graphics_pipeline_ = backend_->createGraphicsPipeline(program_name);

    GraphicsPipelineConfig config;
	config.vertex = vertex_shader_;
	config.fragment = fragment_shader_;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.cullBackFace = false;
	config.vertex_buffer_binding_desc = vertex_shader_->getInputBindingDescription();
	config.vertex_buffer_attrib_desc =vertex_shader_->getInputAttributes();
	config.render_pass = render_pass;
	config.subpass_number = scene_subpass_number_;

	if (scene_graphics_pipeline_->buildPipeline(config)) {
        createUniforms();
        createSceneDescriptorSets();
        createGeometryDescriptorSets();
        return true;
    }

	return false;
}

void SceneManager::prepareForRendering() {
    if (shadows_enabled_) {
        renderStaticShadowMap();
    }

    updateDescriptorSets();
}

void SceneManager::update() {
    scene_data_.view = lookAtMatrix();

    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<SceneData>(scene_data_buffer_.buffers[i], { scene_data_ });
    }

    for (auto& mesh : meshes_) {
        mesh->update();
    }
}

RecordCommandsResult SceneManager::renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info) {
    std::vector<VkCommandBuffer> command_buffers = { command_buffers_[swapchain_image] };
    backend_->resetCommandBuffers(command_buffers);

    VkCommandBufferInheritanceInfo inherit_info{};
    inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit_info.renderPass = render_pass_info.renderPass;
    inherit_info.subpass = scene_subpass_number_;
    inherit_info.framebuffer = render_pass_info.framebuffer;
    inherit_info.occlusionQueryEnable = VK_FALSE;
    inherit_info.queryFlags = 0;
    inherit_info.pipelineStatistics = 0;

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inherit_info;

    if (vkBeginCommandBuffer(command_buffers[0], &begin_info) != VK_SUCCESS) {
        std::cerr << "[Particle Emitter] Failed to begin recording command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    bindSceneDescriptors(command_buffers[0], *scene_graphics_pipeline_, swapchain_image);

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, scene_graphics_pipeline_->handle());

	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 2); // does nothing if not in debug

	drawGeometry(command_buffers[0], scene_graphics_pipeline_->layout(), swapchain_image);

	backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 3); // does nothing if not in debug

    if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to record command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    return makeRecordCommandsResult(true, command_buffers);
}

void SceneManager::cleanupSwapChainAssets() {
    deleteUniforms();
    vk_descriptor_sets_.clear();
    if (scene_graphics_pipeline_) {
        scene_graphics_pipeline_.reset();
    }
}

void SceneManager::createUniforms() {
    scene_data_buffer_ = backend_->createUniformBuffer<SceneData>("scene_data"); // the buffer lifecycle is managed by the backend

    for (auto& mesh : meshes_) {
        mesh->createUniformBuffer();
    }

    auto extent = backend_->getSwapChainExtent();
    // create an image buffer to store per-fragment depth and normal information that can be shared across pipelines
    scene_depth_buffer_ = backend_->createTexture("scene_depth_buffer_storage");
    scene_depth_buffer_->createDepthStorageImage(extent.width, extent.height, true /*as rgba*/);
}

void SceneManager::deleteUniforms() {
    backend_->destroyUniformBuffer(scene_data_buffer_);

    for (auto& mesh : meshes_) {
        mesh->deleteUniformBuffer();
    }

    scene_depth_buffer_.reset();
    vk_descriptor_sets_.clear();

    if (shadows_enabled_) {
        backend_->destroyRenderPass(shadow_map_render_pass_);
	    backend_->destroyUniformBuffer(shadow_map_data_buffer_);
        shadow_map_pipeline_.reset();
        vk_shadow_descriptor_sets_.clear();
    }
}

void SceneManager::createSceneDescriptorSets() {
    const auto& layout = scene_graphics_pipeline_->descriptorSets().find(SCENE_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), layout);

    if (shadows_enabled_) {
        const auto& shadow_layout = scene_graphics_pipeline_->descriptorSets().find(SHADOW_MAP_SET_ID)->second;
        for (uint32_t i = 0; i < backend_->getSwapChainSize(); ++i) {
            layouts.push_back(shadow_layout);
        }
    }

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = uint32_t(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    vk_descriptor_sets_.resize(layouts.size());
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, vk_descriptor_sets_.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }
}

void SceneManager::updateSceneDescriptorSets() {
    const auto& bindings = scene_graphics_pipeline_->descriptorMetadata().set_bindings.find(SCENE_UNIFORM_SET_ID)->second;
    auto first = vk_descriptor_sets_.begin();
    auto last = vk_descriptor_sets_.begin() + backend_->getSwapChainSize();
    auto scene_descriptors = std::vector<VkDescriptorSet>(first, last);
    backend_->updateDescriptorSets(scene_data_buffer_, scene_descriptors, bindings.find(SCENE_DATA_BINDING_NAME)->second);

    if (bindings.find(SCENE_TEXTURES_ARRAY) != bindings.end()) {
        auto textures_binding_point = bindings.find(SCENE_TEXTURES_ARRAY)->second;

        std::vector<VkDescriptorImageInfo> image_infos;
        for (auto& texture : textures_) {
            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = texture->getImageLayout();
            image_info.imageView = texture->getImageView();
            image_info.sampler = texture->getImageSampler();

            image_infos.push_back(image_info);
        }

        for (size_t i = 0; i < scene_descriptors.size(); i++) {
            VkWriteDescriptorSet descriptor_write{};
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = scene_descriptors[i];
            descriptor_write.dstBinding = textures_binding_point;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.descriptorCount = static_cast<uint32_t>(image_infos.size());
            descriptor_write.pImageInfo = image_infos.data();

            vkUpdateDescriptorSets(backend_->getDevice(), 1, &descriptor_write, 0, nullptr);
        }
    }

    if (bindings.find(SCENE_DEPTH_BUFFER_STORAGE) != bindings.end()){
        scene_depth_buffer_->updateDescriptorSets(scene_descriptors, bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
    }

    if (shadows_enabled_) {
        const auto& shadow_bindings = scene_graphics_pipeline_->descriptorMetadata().set_bindings.find(SHADOW_MAP_SET_ID)->second;
        first = vk_descriptor_sets_.begin() + backend_->getSwapChainSize();
        last = vk_descriptor_sets_.end();
        auto shadow_descriptors = std::vector<VkDescriptorSet>(first, last);
        backend_->updateDescriptorSets(shadow_map_data_buffer_, shadow_descriptors, shadow_bindings.find(SHADOW_MAP_PROJ_NAME)->second);
        shadow_map_render_pass_.depth_attachment->updateDescriptorSets(shadow_descriptors, shadow_bindings.find(SHADOW_MAP_NAME)->second);
    }
}

void SceneManager::createGeometryDescriptorSets() {
    // all pipelines that want to draw the scene geometry need to bind the same sets, so we only need to
    // generate them only once and they will be compatible with all pipelines
    for (auto& mesh : meshes_) {
        mesh->createDescriptorSets(scene_graphics_pipeline_->descriptorSets());
    }
}

void SceneManager::updateGeometryDescriptorSets(const DescriptorSetMetadata& metadata, bool with_material) {
    for (auto& mesh : meshes_) {
        mesh->updateDescriptorSets(metadata, with_material);
    }
}


void SceneManager::updateDescriptorSets() {
    updateSceneDescriptorSets();
    updateGeometryDescriptorSets(scene_graphics_pipeline_->descriptorMetadata(), true);
}

void SceneManager::bindSceneDescriptors(VkCommandBuffer& cmd_buffer, const GraphicsPipeline& pipeline, uint32_t swapchain_index) {
    uint32_t scene_data_offset = swapchain_index;
	// scene data
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), SCENE_UNIFORM_SET_ID, 1, &vk_descriptor_sets_[scene_data_offset], 0, nullptr);
	if (shadows_enabled_) {
        // shadow map data
        uint32_t shadow_map_offset = backend_->getSwapChainSize() + scene_data_offset;
	    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), SHADOW_MAP_SET_ID, 1, &vk_descriptor_sets_[shadow_map_offset], 0, nullptr);
    }
}

void SceneManager::drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index, bool with_material) {
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene_vertex_buffer_.vk_buffer, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, scene_index_buffer_.vk_buffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto& mesh : meshes_) {
        mesh->drawGeometry(cmd_buffer, pipeline_layout, swapchain_index, with_material);
    }
}

namespace {
    glm::mat4 calcWorldTransform(const glm::vec3& position, glm::vec3 target = glm::vec3(0.f), glm::vec3 up_ = glm::vec3(0.f, 0.f, 1.f), bool follow_target = false) {
        glm::vec3 forward;
        if (follow_target) {
            forward = glm::normalize(target - position);
        } else {
            forward = glm::normalize(target); // interpret target as the forward vector
        }

        glm::vec3 right = glm::normalize(glm::cross(forward, up_));
        glm::vec3 up = glm::cross(right, forward);
        
        glm::mat4 world_tranform(1.f);
        // rotation
        world_tranform[0][0] = forward[0];
        world_tranform[1][0] = right[0];
        world_tranform[2][0] = up[0];
        world_tranform[0][1] = forward[1];
        world_tranform[1][1] = right[1];
        world_tranform[2][1] = up[1];
        world_tranform[0][2] = forward[2];
        world_tranform[1][2] = right[2];
        world_tranform[2][2] = up[2];

        // translation
        world_tranform[3][0] = position[0];
        world_tranform[3][1] = position[1];
        world_tranform[3][2] = position[2];

        // scale
        world_tranform[0][3] = 0.0f;
        world_tranform[1][3] = 0.0f;
        world_tranform[2][3] = 0.0f;
        world_tranform[3][3] = 1.0f;

        return world_tranform;
    }
}

void SceneManager::updateCameraTransform() {    
    if (follow_target_) {
        camera_forward_ = glm::normalize(camera_look_at_ - camera_position_);
    }

    camera_transform_ = calcWorldTransform(camera_position_, camera_forward_, camera_up_);
}

glm::mat4 SceneManager::lookAtMatrix() const {
    return glm::inverse(camera_transform_);
}

glm::mat4 SceneManager::lightViewMatrix() const {
    glm::vec3 world_light_position = scene_data_.light_position * gltf_scale_factor_;
    glm::mat4 light_transform = calcWorldTransform(world_light_position, glm::vec3(0.f), glm::vec3(0.01f, 0.f, 0.99f), true);
    return glm::inverse(light_transform);
}

glm::mat4 SceneManager::shadowMapProjection() const {
    const float ar = shadow_map_width_ / (float)shadow_map_height_;
    return glm::perspective(glm::radians(120.0f), ar, 0.1f, 1000.0f);
}

void SceneManager::setupShadowMapAssets() {
    shadow_map_data_.light_view = lightViewMatrix();
    shadow_map_data_.shadow_proj = shadowMapProjection();

    shadow_map_data_buffer_ = backend_->createUniformBuffer<ShadowMapData>("shadow_map_data", backend_->getSwapChainSize());
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
       backend_->updateBuffer<ShadowMapData>(shadow_map_data_buffer_.buffers[i], { shadow_map_data_ });
    }

    RenderPassConfig render_pass_config;
	render_pass_config.name = "Shadow Map Pass";
    render_pass_config.framebuffer_size = {shadow_map_width_, shadow_map_height_};
    render_pass_config.offscreen = true;
    render_pass_config.has_colour = false;
    render_pass_config.has_depth = true;
	render_pass_config.store_depth = true;
	
	SubpassConfig subpass;
	subpass.use_colour_attachment = false;
	subpass.use_depth_stencil_attachemnt = true;
    SubpassConfig::Dependency subpass_dependency;
	subpass_dependency.src_subpass = -1;
	subpass_dependency.dst_subpass = 0;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::FRAGMENT_SHADER;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::EARLY_FRAGMENT_TESTS;
	subpass.dependencies.push_back(subpass_dependency);

    subpass_dependency.src_subpass = 0;
	subpass_dependency.dst_subpass = -1;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::LATE_FRAGMENT_TESTS;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::FRAGMENT_SHADER;
	subpass.dependencies.push_back(subpass_dependency);
    
    render_pass_config.subpasses = { subpass };

	shadow_map_render_pass_ = backend_->createRenderPass(render_pass_config);

    auto vertex_shader = backend_->createShaderModule("shadow_map_vs");
	vertex_shader->loadSpirvShader("shaders/shadow_map_vs.spv");

	if (!vertex_shader->isVertexFormatCompatible(Vertex::getFormatInfo())) {
		std::cerr << "Vertex format is not compatible with pipeline input for " << vertex_shader->getName() << std::endl;
		return;
	}

    GraphicsPipelineConfig config;
	config.vertex = vertex_shader;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.cullBackFace = true;
	config.vertex_buffer_binding_desc = vertex_shader->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = vertex_shader->getInputAttributes();
	config.render_pass = shadow_map_render_pass_;
	config.subpass_number = 0;

	shadow_map_pipeline_ = backend_->createGraphicsPipeline( "Shadow Map Generation");

    if (!shadow_map_pipeline_->buildPipeline(config)) {
        std::cerr << " " << std::endl;
        return;
    }
}

void SceneManager::createShadowMapDescriptors() {
    const auto& layout = shadow_map_pipeline_->descriptorSets().find(SHADOW_MAP_DATA_UNIFORM_SET_ID)->second;
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet layout_descriptor_set;
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, &layout_descriptor_set) != VK_SUCCESS) {
        std::cerr << "Failed to allocate shadow map pipeline descriptor set!" << std::endl;
        return;
    }

    vk_shadow_descriptor_sets_.push_back(layout_descriptor_set);
}

void SceneManager::renderStaticShadowMap() {
    setupShadowMapAssets();
    createShadowMapDescriptors();

    update();  // needed to load all the mesh uniforms

    const auto& bindings = shadow_map_pipeline_->descriptorMetadata().set_bindings.find(SHADOW_MAP_DATA_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(shadow_map_data_buffer_, vk_shadow_descriptor_sets_, bindings.find(SHADOW_MAP_DATA_BINDING_NAME)->second);
    updateGeometryDescriptorSets(shadow_map_pipeline_->descriptorMetadata(), false /*no material*/);

    auto cmd_buffer = backend_->beginSingleTimeCommands();

    VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = shadow_map_render_pass_.vk_render_pass;
	render_pass_info.framebuffer = shadow_map_render_pass_.framebuffers[0]; //
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = { shadow_map_width_, shadow_map_height_ };

	std::array<VkClearValue, 1> clear_values{};
	clear_values[0].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_->handle());

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_->layout(), SHADOW_MAP_DATA_UNIFORM_SET_ID, 1, &vk_shadow_descriptor_sets_[0], 0, nullptr);

	drawGeometry(cmd_buffer, shadow_map_pipeline_->layout(), 0, false /*no material*/);

    vkCmdEndRenderPass(cmd_buffer);

    backend_->endSingleTimeCommands(cmd_buffer);
}
