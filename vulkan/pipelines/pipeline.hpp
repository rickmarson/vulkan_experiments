/*
* grahics_pipeline_base.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#pragma once

#include "../common_definitions.hpp"

class ShaderModule;

using BindingsMap = std::map<std::string, uint32_t>;
struct DescriptorSetMetadata {
    std::map<uint32_t, BindingsMap> set_bindings;
};

using PushConstantsMap = std::map<std::string, VkPushConstantRange>;

enum class PipelineType { 
    UNKNOWN = -1, 
    GRAPHICS, 
    GRAPHICS_MESH, 
    COMPUTE,
    RAYTRACING 
};

class VulkanPipeline {
 public:

    virtual ~VulkanPipeline() {
        if (isValid()) {
            for (auto& descr_set_layout : vk_descriptor_set_layouts_) {
                vkDestroyDescriptorSetLayout(device_, descr_set_layout.second, nullptr);
            }
            
            vkDestroyPipeline(device_, vk_pipeline_, nullptr);
            vkDestroyPipelineLayout(device_, vk_pipeline_layout_, nullptr);
            vk_descriptor_set_layouts_.clear();
        }
    }

    bool isValid() const { return vk_pipeline_ != VK_NULL_HANDLE; }
    const std::string& name() const { return name_; }
    const PipelineType& type() const { return pipeline_type_; }
    const VkPipelineLayout& layout() const { return vk_pipeline_layout_; }
    const VkPipeline& handle() const { return vk_pipeline_; }
    const  std::map<uint32_t, VkDescriptorSetLayout>& descriptorSets() const { return vk_descriptor_set_layouts_; }
    const DescriptorSetMetadata& descriptorMetadata() const { return descriptor_metadata_; }
    const PushConstantsMap& pushConstants() const { return push_constants_; }

protected:
    VulkanPipeline(VkDevice device, const std::string& name) :
        device_(device),
        name_(name) {}

    std::string name_;
    PipelineType pipeline_type_ = PipelineType::UNKNOWN;

    VkDevice device_;
    VkPipelineLayout vk_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline vk_pipeline_ = VK_NULL_HANDLE;
    std::map<uint32_t, VkDescriptorSetLayout> vk_descriptor_set_layouts_;

    DescriptorSetMetadata descriptor_metadata_;
    PushConstantsMap push_constants_;
};
