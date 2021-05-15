/*
* compute_pipeline.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/
#pragma once

#include "../common_definitions.hpp"
#include "pipeline.hpp"

class VulkanBackend;
class ShaderModule;

struct ComputePipelineConfig {
    std::shared_ptr<ShaderModule> compute;
};

class ComputePipeline : public VulkanPipeline {
 public:
    virtual ~ComputePipeline() = default;

    bool buildPipeline(const ComputePipelineConfig& config);

 private:
    friend class VulkanBackend;
    
    ComputePipeline(VkDevice device, const std::string& name) : 
            VulkanPipeline(device, name) {
               pipeline_type_ = PipelineType::COMPUTE;
            }
};
