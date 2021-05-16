/*
* mesh_pipeline.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#pragma once

#include "../common_definitions.hpp"
#include "graphics_pipeline_base.hpp"

class VulkanBackend;
class ShaderModule;


struct MeshPipelineConfig : public FixedFunctionConfig  {
    std::shared_ptr<ShaderModule> task;
    std::shared_ptr<ShaderModule> mesh;
    std::shared_ptr<ShaderModule> fragment;
};

class MeshPipeline : public GraphicsPipelineBase {
 public:
    virtual ~MeshPipeline() = default;

    bool buildPipeline(const MeshPipelineConfig& config);

 private:
    friend class VulkanBackend;
    
    MeshPipeline(VkDevice device, const std::string& name) : 
            GraphicsPipelineBase(device, name) {
                pipeline_type_ = PipelineType::GRAPHICS_MESH;
            }

    bool assembleMeshPipelineLayoutInfo(const MeshPipelineConfig& config, GraphicsPipelineLayoutInfo& layout_info);
};
