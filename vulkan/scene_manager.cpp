/*
* scene_manager.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "scene_manager.hpp"
#include "vulkan_backend.hpp"

std::unique_ptr<SceneManager> SceneManager::create(VulkanBackend* backend) {
    return std::make_unique<SceneManager>(backend);
}

SceneManager::SceneManager(VulkanBackend* backend) :
	backend_(backend) {

}

SceneManager::~SceneManager() {
    vk_descriptor_sets_.clear();
}

void SceneManager::setCameraProperties(float fov, float aspect_ratio, float z_near, float z_far) {
    scene_data_.proj = glm::perspective(glm::radians(fov), aspect_ratio, z_near, z_far);
    scene_data_.proj[1][1] *= -1;  // y is inverted in vulkan w.r.t. opengl
}

void SceneManager::setCameraPosition(const glm::vec3& pos) {
    camera_position_ = pos;
}

void SceneManager::setCameraTarget(const glm::vec3& target) {
    camera_look_at_ = target;
}

void SceneManager::update() {
    scene_data_.view = glm::lookAt(camera_look_at_, camera_position_, camera_up_);

    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        backend_->updateBuffer<SceneData>(uniform_buffer_.buffers[i], { scene_data_ });
    }
}

void SceneManager::createUniformBuffer() {
    uniform_buffer_ = backend_->createUniformBuffer<SceneData>("scene_data"); // the buffer lifecycle is managed by the backend
}

void SceneManager::deleteUniformBuffer() {
    backend_->destroyUniformBuffer(uniform_buffer_);
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
}

void SceneManager::updateDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(SCENE_UNIFORM_SET_ID)->second;
    backend_->updateDescriptorSets(uniform_buffer_, vk_descriptor_sets_, bindings.find(SCENE_DATA_BINDING_NAME)->second);
}
