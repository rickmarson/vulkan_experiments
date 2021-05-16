/*
* static_mesh.cpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "static_mesh.hpp"
#include "vulkan_backend.hpp"
#include "texture.hpp"

std::shared_ptr<StaticMesh> StaticMesh::createStaticMesh(const std::string& name, VulkanBackend* backend) {
    return std::make_shared<StaticMesh>(name, backend);
}

StaticMesh::StaticMesh(const std::string& name, VulkanBackend* backend) :
    name_(name),
    backend_(backend) {
    model_data_.transform_matrix = glm::identity<glm::mat4>();
}

StaticMesh::~StaticMesh() {
    surfaces_.clear();
    vk_descriptor_sets_.clear();
}

StaticMesh::Surface& StaticMesh::addSurface() {
    surfaces_.emplace_back();
    return surfaces_.back();
}

void StaticMesh::setTransform(const glm::mat4& transform) {
    model_data_.transform_matrix = transform;
}

void StaticMesh::update() {
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<ModelData>(uniform_buffer_.buffers[i], { model_data_ });
    }
}

void StaticMesh::createUniformBuffer() {
    uniform_buffer_ = backend_->createUniformBuffer<ModelData>(name_ + "_model_data"); // the buffer lifecycle is managed by the backend
}

void StaticMesh::deleteUniformBuffer() {
    backend_->destroyUniformBuffer(uniform_buffer_);
}

DescriptorPoolConfig StaticMesh::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.uniform_buffers_count = uint32_t(surfaces_.size() * 1 + 1);

    return config;
}

void StaticMesh::createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
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

    for (auto& surface : surfaces_) {
        surface.createDescriptorSets(backend_, descriptor_set_layouts);
    }
}

void StaticMesh::updateDescriptorSets(const DescriptorSetMetadata& metadata, bool with_material) {
    const auto& bindings = metadata.set_bindings.find(MODEL_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(uniform_buffer_, vk_descriptor_sets_, bindings.find(MODEL_DATA_BINDING_NAME)->second);

    if (with_material) {
        for (auto& surface : surfaces_) {
            surface.updateDescriptorSets(backend_, metadata);
        }
    }
}

void StaticMesh::drawGeometry(VkCommandBuffer& cmd_buffer, VkPipelineLayout pipeline_layout, uint32_t swapchain_index, bool with_material) {
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, MODEL_UNIFORM_SET_ID, 1, &vk_descriptor_sets_[swapchain_index], 0, nullptr);

    for (auto& surface : surfaces_) {
        if (with_material) {
            vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, SURFACE_UNIFORM_SET_ID, 1, &surface.vk_descriptor_sets[swapchain_index], 0, nullptr);
        }
        vkCmdDrawIndexed(cmd_buffer, surface.index_count, 1, surface.index_start, surface.vertex_start, 0);
    }
}

void StaticMesh::Surface::createDescriptorSets(VulkanBackend* backend, const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(SURFACE_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(backend->getSwapChainSize(), layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = backend->getDescriptorPool();
    alloc_info.descriptorSetCount = backend->getSwapChainSize();
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(backend->getSwapChainSize());
    if (vkAllocateDescriptorSets(backend->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets = std::move(layout_descriptor_sets);
}

void StaticMesh::Surface::updateDescriptorSets(VulkanBackend* backend, const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(SURFACE_UNIFORM_SET_ID)->second;
    auto material = material_weak.lock();
    backend->updateDescriptorSets(material->material_uniform, vk_descriptor_sets, bindings.find(SURFACE_MATERIAL_BINDING_NAME)->second);
}
