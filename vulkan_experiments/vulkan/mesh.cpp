/*
* mesh.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "mesh.hpp"
#include "texture.hpp"
#include "vulkan_backend.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#ifdef TINYOBJLOADER_USE_DOUBLE
#undef TINYOBJLOADER_USE_DOUBLE
#endif
#include <tiny_obj_loader.h>


namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.tex_coord) << 1);
        }
    };
}

std::shared_ptr<Mesh> Mesh::createMesh(const std::string& name, VulkanBackend* backend) {
    return std::make_shared<Mesh>(name, backend);
}

Mesh::Mesh(const std::string& name, VulkanBackend* backend) :
    name_(name),
    backend_(backend) {
    model_data_.transform_matrix = glm::identity<glm::mat4>();
}

Mesh::~Mesh() {
    textures_.clear();
    vk_descriptor_sets_.clear();
}

bool Mesh::loadObjModel(const std::string& obj_file_path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, obj_file_path.c_str())) {
        std::cerr << "Failed to load Obj file " << obj_file_path << std::endl << "\t\t" << warn + err << std::endl;
        return false;
    }

    // assume 1 material for now
    if (materials.size() != 1) {
        std::cerr << "Multiple materials are not yet supported!" << std::endl;
    }

    // load textures
    auto& diffuse_texture_path = materials[0].diffuse_texname;
    
    auto texture = backend_->createTexture("diffuse_colour");
    texture->loadImageRGBA(diffuse_texture_path);

    if (!texture->isValid()) {
        std::cerr << "Failed to validate texture " << diffuse_texture_path << "!" << std::endl;
        return false;
    }

    texture->createSampler();

    textures_[texture->getName()] = std::move(texture);

    // process triangles
    glm::vec3 diffuse_colour = { materials[0].diffuse[0], materials[0].diffuse[1], materials[0].diffuse[2] };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3.0f * index.vertex_index + 0.0f],
                attrib.vertices[3.0f * index.vertex_index + 1.0f],
                attrib.vertices[3.0f * index.vertex_index + 2.0f]
            };

            vertex.tex_coord = {
                attrib.texcoords[2.0f * index.texcoord_index + 0.0f],
                1.0f - attrib.texcoords[2.0f * index.texcoord_index + 1.0f]
            };

            vertex.color = diffuse_colour;

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    vertex_count_ = static_cast<uint32_t>(vertices.size());
    index_count_ = static_cast<uint32_t>(indices.size());

    vertex_buffer_ = backend_->createVertexBuffer<Vertex>(name_ + "_vertices", vertices);
    index_buffer_ = backend_->createIndexBuffer(name_ + "_indices", indices);
    return vertex_buffer_.vk_buffer != VK_NULL_HANDLE && index_buffer_.vk_buffer != VK_NULL_HANDLE;
}

void Mesh::setTransform(const glm::mat4& transform) {
    model_data_.transform_matrix = transform;
}

void Mesh::update() {
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<ModelData>(uniform_buffer_.buffers[i], { model_data_ });
    }
}

void Mesh::createUniformBuffer() {
    uniform_buffer_ = backend_->createUniformBuffer<ModelData>(name_ + "_model_data"); // the buffer lifecycle is managed by the backend
}

void Mesh::createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(MODEL_UNIFORM_SET_ID)->second;
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
}

void Mesh::updateDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(MODEL_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(uniform_buffer_, vk_descriptor_sets_, bindings.find(MODEL_DATA_BINDING_NAME)->second);
    getDiffuseTexture()->updateDescriptorSets(vk_descriptor_sets_, bindings.find(DIFFUSE_SAMPLER_BINDING_NAME)->second);
}
