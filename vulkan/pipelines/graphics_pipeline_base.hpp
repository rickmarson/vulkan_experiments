/*
* grahics_pipeline_base.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/
#pragma once

#include "pipeline.hpp"

struct FixedFunctionConfig {  
    VkPrimitiveTopology topology;
    VkVertexInputBindingDescription vertex_buffer_binding_desc;
    std::vector<VkVertexInputAttributeDescription> vertex_buffer_attrib_desc;

    // fixed function options
    bool cullBackFace = true;
    bool enableDepthTesting = true;
    bool enableStencilTest = false;
    bool enableTransparency = false;
    bool showWireframe = false;
    bool dynamicStates = false;
    bool enablePrimitiveRestart = false;

    RenderPass render_pass;
    uint32_t subpass_number = 0;
};

struct GraphicsPipelineLayoutInfo {
        std::map<uint32_t, VkDescriptorSetLayout> descriptors_set_layouts;
        std::vector<VkDescriptorSetLayout> descriptors_set_layouts_aux;  // contiguous memory block for passing to vulkan 
        std::vector<VkPushConstantRange> push_constants_array;
        DescriptorSetMetadata pipeline_descriptor_metadata;
        PushConstantsMap push_constants_map;
};

class GraphicsPipelineBase : public VulkanPipeline {
    public:
        virtual ~GraphicsPipelineBase() = default;

    protected:
        GraphicsPipelineBase(VkDevice device, const std::string& name) : 
            VulkanPipeline(device, name) {}

        bool buildPipeline(const FixedFunctionConfig& config, 
                                      const GraphicsPipelineLayoutInfo& layout_info,
                                      std::vector<VkPipelineShaderStageCreateInfo>& shader_stages);
};
