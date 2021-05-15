/*
* grahics_pipeline.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#pragma once

#include "../common_definitions.hpp"
#include "graphics_pipeline_base.hpp"

class VulkanBackend;
class ShaderModule;


struct GraphicsPipelineConfig : public FixedFunctionConfig  {
    std::shared_ptr<ShaderModule> vertex;
    std::shared_ptr<ShaderModule> geometry;
    std::shared_ptr<ShaderModule> fragment;
    struct TessellationShaders {
        std::shared_ptr<ShaderModule> control;
        std::shared_ptr<ShaderModule> evaluation;

        operator bool() const { return control && evaluation; }
    }tessellation;
};

class GraphicsPipeline : public GraphicsPipelineBase {
 public:
    virtual ~GraphicsPipeline() = default;

    bool buildPipeline(const GraphicsPipelineConfig& config);

 private:
    friend class VulkanBackend;
    
    GraphicsPipeline(VkDevice device, const std::string& name) : 
            GraphicsPipelineBase(device, name) {
                pipeline_type_ = PipelineType::GRAPHICS;
            }

    bool assembleGraphicsPipelineLayoutInfo(const GraphicsPipelineConfig& config, GraphicsPipelineLayoutInfo& layout_info);
};
