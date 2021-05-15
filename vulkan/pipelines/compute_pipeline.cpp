/*
* compute_pipeline.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "compute_pipeline.hpp"
#include "../shader_module.hpp"

bool ComputePipeline::buildPipeline(const ComputePipelineConfig& config) {
    if (!config.compute) {
        return false;
    }

    DescriptorSetMetadata pipeline_descriptor_metadata;
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> layout_bindings_by_set;
    const auto& compute_layouts = config.compute->getDescriptorSetLayouts();
    for (const auto& layout : compute_layouts) {
        if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
            layout_bindings_by_set[layout.id] = layout.layout_bindings;
        }
        else {
            auto& bindings_array = layout_bindings_by_set[layout.id];
            bindings_array.insert(bindings_array.begin(), layout.layout_bindings.begin(), layout.layout_bindings.end());
        }
    }
    const auto& compute_descriptor_metadata = config.compute->getDescriptorsMetadata();
    for (const auto& meta : compute_descriptor_metadata.set_bindings) {
        pipeline_descriptor_metadata.set_bindings[meta.first] = meta.second;
    }

    // create descriptor layouts for all sets of binding points in the pipeline
    std::map<uint32_t, VkDescriptorSetLayout> descriptors_set_layouts;
    std::map<uint32_t, std::vector< VkDescriptorSet>> descriptor_sets;
    for (auto& set : layout_bindings_by_set) {
        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(set.second.size());
        layout_create_info.pBindings = set.second.data();

        VkDescriptorSetLayout descriptor_set_layout;
        if (vkCreateDescriptorSetLayout(device_, &layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            std::cerr << "Failed to create compute pipeline descriptor set layout!" << std::endl;
            return false;
        }

        descriptors_set_layouts[set.first] = descriptor_set_layout;
    }

    // auxiliary array to make sure the layouts are ordered and contiguous in memory
    std::vector<VkDescriptorSetLayout> descriptors_set_layouts_aux;
    for (auto& layout : descriptors_set_layouts) {
        descriptors_set_layouts_aux.push_back(layout.second);
    }

    // assemble push constants 
    std::vector<VkPushConstantRange> push_constants_array;
    auto& compute_push_constants = config.compute->getPushConstants();
    PushConstantsMap push_constants_map;
    for (auto& pc : compute_push_constants) {
        push_constants_array.push_back(pc.push_constant_range);
        push_constants_map.insert({ pc.name, pc.push_constant_range });
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptors_set_layouts_aux.size());
    pipeline_layout_info.pSetLayouts = descriptors_set_layouts_aux.data();
    pipeline_layout_info.pushConstantRangeCount = uint32_t(push_constants_array.size());
    pipeline_layout_info.pPushConstantRanges = push_constants_array.size() > 0 ? push_constants_array.data() : nullptr;

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline layout!" << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo compute_shader_stage_info{};
    compute_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_shader_stage_info.module = config.compute->getShader();
    compute_shader_stage_info.pName = "main";

    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.stage = compute_shader_stage_info;
    create_info.layout = pipeline_layout;
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = -1;

    VkPipeline vk_pipeline;
    if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &create_info, nullptr, &vk_pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create Compute pipeline!" << std::endl;
        return false;
    }

    vk_pipeline_layout_ = pipeline_layout;
    vk_pipeline_ = vk_pipeline;
    vk_descriptor_set_layouts_ = std::move(descriptors_set_layouts);
    descriptor_metadata_ = std::move(pipeline_descriptor_metadata);
    push_constants_ = std::move(push_constants_map);
   
   return true;
}
