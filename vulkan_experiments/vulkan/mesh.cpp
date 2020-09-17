/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "mesh.hpp"
#include "texture.hpp"
#include "vulkan_backend.hpp"

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
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

}

Mesh::~Mesh() {
    textures_.clear();
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
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.tex_coord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
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