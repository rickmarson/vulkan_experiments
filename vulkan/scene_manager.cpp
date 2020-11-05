/*
* scene_manager.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "scene_manager.hpp"
#include "vulkan_backend.hpp"
#include "texture.hpp"
#include "static_mesh.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_USE_CPP14 
#define TINYGLTF_NO_STB_IMAGE_WRITE 
#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp>

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
            const float* buffer_uv = nullptr;
            int pos_stride;
            int norm_stride;
            int uv_stride;

            const gltf::Accessor& pos_accessor = model.accessors[p.attributes.find("POSITION")->second];
            const gltf::BufferView& pos_view = model.bufferViews[pos_accessor.bufferView];
            buffer_pos = reinterpret_cast<const float*>(&(model.buffers[pos_view.buffer].data[pos_accessor.byteOffset + pos_view.byteOffset]));
            vertex_count = static_cast<uint32_t>(pos_accessor.count);
            pos_stride = pos_accessor.ByteStride(pos_view) ? (pos_accessor.ByteStride(pos_view) / sizeof(float)) : 3;

            if (p.attributes.find("TEXCOORD_0") != p.attributes.end()) {
                const tinygltf::Accessor& uv_accessor = model.accessors[p.attributes.find("TEXCOORD_0")->second];
                const tinygltf::BufferView& uv_view = model.bufferViews[uv_accessor.bufferView];
                buffer_uv = reinterpret_cast<const float*>(&(model.buffers[uv_view.buffer].data[uv_accessor.byteOffset + uv_view.byteOffset]));
                uv_stride = uv_accessor.ByteStride(uv_view) ? (uv_accessor.ByteStride(uv_view) / sizeof(float)) : 2;
            }

            if (p.attributes.find("NORMAL") != p.attributes.end()) {
                // here for reference, not yet used 
                const gltf::Accessor& norm_accessor = model.accessors[p.attributes.find("NORMAL")->second];
                const gltf::BufferView& norm_view = model.bufferViews[norm_accessor.bufferView];
                buffer_normals = reinterpret_cast<const float*>(&(model.buffers[norm_view.buffer].data[norm_accessor.byteOffset + norm_view.byteOffset]));
                norm_stride = norm_accessor.ByteStride(norm_view) ? (norm_accessor.ByteStride(norm_view) / sizeof(float)) : 3;
            }

            for (size_t v = 0; v < pos_accessor.count; v++) {
                auto gltf_pos = glm::make_vec3(&buffer_pos[v * pos_stride]);
                Vertex vert{};
                vert.pos[0] = -gltf_pos[2];  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                vert.pos[1] = gltf_pos[0];
                vert.pos[2] = gltf_pos[1];
                vert.color = glm::vec3(1.0f); // this will need to change
                vert.tex_coord = buffer_uv ? glm::make_vec2(&buffer_uv[v * uv_stride]) : glm::vec3(0.0f);
                
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
                trans[0] = -gltf_trans[2];  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                trans[1] = gltf_trans[0];
                trans[2] = gltf_trans[1];
                translation = glm::translate(translation, trans);
            }
            if (node.rotation.size() == 4) {
                auto gltf_rot = glm::make_quat(node.rotation.data());
                glm::quat rot;
                rot.x = -gltf_rot.z;  // glTF "forward" is -Z, world "forward" is +X. Blender's "forward" is +Y
                rot.y = gltf_rot.x;
                rot.z = gltf_rot.y;
                rot.w = gltf_rot.w;
                rotation = glm::mat4(rot);
            }
            if (node.scale.size() == 3) {
                auto gltf_scale = glm::make_vec3(node.scale.data());
                auto sc = glm::vec3(0.0f);
                sc[0] = gltf_scale[2];
                sc[1] = gltf_scale[0];
                sc[2] = gltf_scale[1];
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

}

std::unique_ptr<SceneManager> SceneManager::create(VulkanBackend* backend) {
    return std::make_unique<SceneManager>(backend);
}

SceneManager::SceneManager(VulkanBackend* backend) :
	backend_(backend) {

}

SceneManager::~SceneManager() {
    backend_->destroyBuffer(scene_index_buffer_);
    backend_->destroyBuffer(scene_vertex_buffer_);
    materials_.clear();
    meshes_.clear();
    textures_.clear();
    vk_descriptor_sets_.clear();
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

        // only support one texture per type for now
        if (gltf_mat.values.find("baseColorTexture") != gltf_mat.values.end()) {
            material->diffuse_texture = textures_[gltf_mat.values["baseColorTexture"].TextureIndex()];
        }
        if (gltf_mat.values.find("normalTexture") != gltf_mat.values.end()) {
            material->normal_texture = textures_[gltf_mat.values["normalTexture"].TextureIndex()];
        }
        if (gltf_mat.values.find("emissiveTexture") != gltf_mat.values.end()) {
            material->occlusion_texture = textures_[gltf_mat.values["emissiveTexture"].TextureIndex()];
        }
        if (gltf_mat.values.find("occlusionTexture") != gltf_mat.values.end()) {
            material->emissive_texture = textures_[gltf_mat.values["occlusionTexture"].TextureIndex()];
        }
        if (gltf_mat.values.find("baseColorFactor") != gltf_mat.values.end()) {
            material->diffuse_factor = glm::make_vec4(gltf_mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (gltf_mat.values.find("emissiveFactor") != gltf_mat.values.end()) {
            material->emissive_factor = glm::make_vec4(gltf_mat.values["emissiveFactor"].ColorFactor().data());
        }

        materials_.push_back(std::move(material));
    }
    
    // store the entire scene in one big buffer, individual meshes will be accessed via offsets
    std::vector<Vertex> vertex_buffer;
    std::vector<uint32_t> index_buffer;

    // load all meshes for one scene. only handles static meshes, no animations, no skins
    // flatten the scene graph into a list of mesh nodes
    auto& scene = gltf_model.scenes[0];
    for (auto n : scene.nodes) {
        auto& gltf_node = gltf_model.nodes[n];
        glm::mat4& transform = glm::identity<glm::mat4>();
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

DescriptorPoolConfig SceneManager::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    for (auto& mesh : meshes_) {
        auto mesh_config = mesh->getDescriptorsCount();
        config.image_samplers_count += mesh_config.image_samplers_count;
        config.uniform_buffers_count += mesh_config.uniform_buffers_count;
        config.storage_texel_buffers_count += mesh_config.storage_texel_buffers_count;
    }

    config.uniform_buffers_count += 1;
    config.image_storage_buffers_count += 1;
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

void SceneManager::update() {
    scene_data_.view = lookAtMatrix();

    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<SceneData>(uniform_buffer_.buffers[i], { scene_data_ });
    }

    for (auto& mesh : meshes_) {
        mesh->update();
    }
}

void SceneManager::createUniforms() {
    uniform_buffer_ = backend_->createUniformBuffer<SceneData>("scene_data"); // the buffer lifecycle is managed by the backend

    for (auto& mesh : meshes_) {
        mesh->createUniformBuffer();
    }

    auto extent = backend_->getSwapChainExtent();
    scene_depth_buffer_ = backend_->createTexture("scene_depth_buffer_storage");
    scene_depth_buffer_->createDepthStorageImage(extent.width, extent.height);
}

void SceneManager::deleteUniforms() {
    backend_->destroyUniformBuffer(uniform_buffer_);

    for (auto& mesh : meshes_) {
        mesh->deleteUniformBuffer();
    }

    scene_depth_buffer_.reset();
}

void SceneManager::createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(SCENE_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend_->getSwapChainSize(), layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = backend_->getSwapChainSize();
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(backend_->getSwapChainSize());
    if (vkAllocateDescriptorSets(backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets_ = std::move(layout_descriptor_sets);

    for (auto& mesh : meshes_) {
        mesh->createDescriptorSets(descriptor_set_layouts);
    }
}

void SceneManager::updateDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(SCENE_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(uniform_buffer_, vk_descriptor_sets_, bindings.find(SCENE_DATA_BINDING_NAME)->second);

    if (bindings.find(SCENE_DEPTH_BUFFER_STORAGE) != bindings.end()){
        scene_depth_buffer_->updateDescriptorSets(vk_descriptor_sets_, bindings.find(SCENE_DEPTH_BUFFER_STORAGE)->second);
    }

    for (auto& mesh : meshes_) {
        mesh->updateDescriptorSets(metadata);
    }
}

void SceneManager::drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index) {
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene_vertex_buffer_.vk_buffer, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, scene_index_buffer_.vk_buffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto& mesh : meshes_) {
        mesh->drawGeometry(cmd_buffer, pipeline_layout, swapchain_index);
    }
}

void SceneManager::updateCameraTransform() {    
    if (follow_target_) {
        camera_forward_ = glm::normalize(camera_look_at_ - camera_position_);
    }

    glm::vec3 right = glm::normalize(glm::cross(camera_forward_, camera_up_));
    glm::vec3 up = glm::cross(right, camera_forward_);

    // rotation
    camera_transform_[0][0] = camera_forward_[0];
    camera_transform_[1][0] = right[0];
    camera_transform_[2][0] = up[0];
    camera_transform_[0][1] = camera_forward_[1];
    camera_transform_[1][1] = right[1];
    camera_transform_[2][1] = up[1];
    camera_transform_[0][2] = camera_forward_[2];
    camera_transform_[1][2] = right[2];
    camera_transform_[2][2] = up[2];

    // translation
    camera_transform_[3][0] = camera_position_[0];
    camera_transform_[3][1] = camera_position_[1];
    camera_transform_[3][2] = camera_position_[2];

    // scale
    camera_transform_[0][3] = 0.0f;
    camera_transform_[1][3] = 0.0f;
    camera_transform_[2][3] = 0.0f;
    camera_transform_[3][3] = 1.0f;
}

glm::mat4 SceneManager::lookAtMatrix() const {
    return glm::inverse(camera_transform_);
}
