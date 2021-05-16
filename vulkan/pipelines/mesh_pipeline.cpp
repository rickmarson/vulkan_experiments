/*
* mesh_pipeline.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "mesh_pipeline.hpp"
#include "../shader_module.hpp"

bool MeshPipeline::buildPipeline(const MeshPipelineConfig& config) {
    GraphicsPipelineLayoutInfo layout_info;
    if (!assembleMeshPipelineLayoutInfo(config, layout_info)) {
        return false;
    }
    
    VkPipelineShaderStageCreateInfo mesh_shader_stage_info{};
    mesh_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    mesh_shader_stage_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
    mesh_shader_stage_info.module = config.mesh->getShader();
    mesh_shader_stage_info.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = { mesh_shader_stage_info };

      if (config.task) {
        VkPipelineShaderStageCreateInfo task_shader_stage_info{};
        task_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        task_shader_stage_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        task_shader_stage_info.module = config.task->getShader();
        task_shader_stage_info.pName = "main";

        shader_stages.push_back(task_shader_stage_info);
    }

    if (config.fragment) {
        VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = config.fragment->getShader();
        frag_shader_stage_info.pName = "main";

        shader_stages.push_back(frag_shader_stage_info);
    }
    
    return GraphicsPipelineBase::buildPipeline(config, layout_info, shader_stages);
}

bool MeshPipeline::assembleMeshPipelineLayoutInfo(const MeshPipelineConfig& config, GraphicsPipelineLayoutInfo& layout_info) {
    // assemble layout information from all shaders
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> layout_bindings_by_set;

    const auto& mesh_layouts = config.mesh->getDescriptorSetLayouts();
    for (const auto& layout : mesh_layouts) {
        if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
            layout_bindings_by_set[layout.id] = layout.layout_bindings;
        }
        else {
            auto& bindings_array = layout_bindings_by_set[layout.id];
            bindings_array.insert(bindings_array.begin(), layout.layout_bindings.begin(), layout.layout_bindings.end());
        }
    }
    const auto& mesh_descriptor_metadata = config.mesh->getDescriptorsMetadata();
    for (const auto& meta : mesh_descriptor_metadata.set_bindings) {
        layout_info.pipeline_descriptor_metadata.set_bindings[meta.first] = meta.second;
    }

    if (config.task) {
        const auto& task_layouts = config.task->getDescriptorSetLayouts();
        for (const auto& layout : task_layouts) {
            if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
                layout_bindings_by_set[layout.id] = layout.layout_bindings;
            }
            else {
                auto& bindings_array = layout_bindings_by_set[layout.id];
                bindings_array.insert(bindings_array.end(), layout.layout_bindings.begin(), layout.layout_bindings.end());
            }
        }
        const auto& task_descriptor_metadata = config.task->getDescriptorsMetadata();
        for (const auto& meta : task_descriptor_metadata.set_bindings) {
            auto& bindings_map = layout_info.pipeline_descriptor_metadata.set_bindings[meta.first];
            for (const auto& src_binding : meta.second) {
                bindings_map.insert(src_binding);
            }
        }
    }

    if (config.fragment) {
        const auto& fragment_layouts = config.fragment->getDescriptorSetLayouts();
        for (const auto& layout : fragment_layouts) {
            if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
                layout_bindings_by_set[layout.id] = layout.layout_bindings;
            }
            else {
                auto& bindings_array = layout_bindings_by_set[layout.id];
                bindings_array.insert(bindings_array.end(), layout.layout_bindings.begin(), layout.layout_bindings.end());
            }
        }
        const auto& fragment_descriptor_metadata = config.fragment->getDescriptorsMetadata();
        for (const auto& meta : fragment_descriptor_metadata.set_bindings) {
            auto& bindings_map = layout_info.pipeline_descriptor_metadata.set_bindings[meta.first];
            for (const auto& src_binding : meta.second) {
                bindings_map.insert(src_binding);
            }
        }
    }

    // create descriptor layouts for all sets of binding points in the pipeline
    std::map<uint32_t, std::vector< VkDescriptorSet>> descriptor_sets;
    for (auto& set : layout_bindings_by_set) {
        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(set.second.size());
        layout_create_info.pBindings = set.second.data();

        VkDescriptorSetLayout descriptor_set_layout;
        if (vkCreateDescriptorSetLayout(device_, &layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline descriptor set layout!" << std::endl;
            return false;
        }

        layout_info.descriptors_set_layouts[set.first] = descriptor_set_layout;
    }

    // auxiliary array to make sure the layouts are ordered and contiguous in memory
    for (auto& layout : layout_info.descriptors_set_layouts) {
        layout_info.descriptors_set_layouts_aux.push_back(layout.second);
    }

    // assemble push constants  
    auto compare = [](const PushConstantBlock& a, const PushConstantBlock& b) { return a.name < b.name; };
    std::set<PushConstantBlock, decltype(compare)> push_constants_temp(compare);

    const auto& mesh_push_constants = config.mesh->getPushConstants();

    for (const auto& pc : mesh_push_constants) {
        push_constants_temp.insert(pc);
    }

    if (config.task) {
        const auto& task_push_constants = config.task->getPushConstants();
        for (const auto& pc : task_push_constants) {
            auto dst_iter = push_constants_temp.find(pc);
            if (dst_iter == push_constants_temp.end()) {
                push_constants_temp.insert(pc);
            }
            else {
                auto new_block = *(dst_iter);
                new_block.push_constant_range.stageFlags |= pc.push_constant_range.stageFlags;
                push_constants_temp.erase(dst_iter);
                push_constants_temp.insert(new_block);
            }
        }
    }

    if (config.fragment) {
        const auto& fragment_push_constants = config.fragment->getPushConstants();
        for (const auto& pc : fragment_push_constants) {
            auto dst_iter = push_constants_temp.find(pc);
            if (dst_iter == push_constants_temp.end()) {
                push_constants_temp.insert(pc);
            }
            else {
                auto new_block = *(dst_iter);
                new_block.push_constant_range.stageFlags |= pc.push_constant_range.stageFlags;
                push_constants_temp.erase(dst_iter);
                push_constants_temp.insert(new_block);
            }
        }
    }

    for (auto& pc : push_constants_temp) {
        layout_info.push_constants_array.push_back(pc.push_constant_range);
        layout_info.push_constants_map.insert({ pc.name, pc.push_constant_range });
    }
    push_constants_temp.clear();

    return true;
}
