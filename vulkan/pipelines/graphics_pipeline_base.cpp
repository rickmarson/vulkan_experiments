/*
* grahics_pipeline_base.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "graphics_pipeline_base.hpp"
#include "../render_pass.hpp"

bool GraphicsPipelineBase::buildPipeline(const FixedFunctionConfig& config, 
                              const GraphicsPipelineLayoutInfo& layout_info,
                              std::vector<VkPipelineShaderStageCreateInfo>& shader_stages) {

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &config.vertex_buffer_binding_desc;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertex_buffer_attrib_desc.size());
    vertex_input_info.pVertexAttributeDescriptions = config.vertex_buffer_attrib_desc.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = config.topology;
    input_assembly.primitiveRestartEnable = config.enablePrimitiveRestart ? VK_TRUE : VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &config.render_pass->viewport();
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &config.render_pass->scissor();

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.showWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = config.cullBackFace ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.rasterizationSamples = config.render_pass->msaaSamples();
    multisampling.minSampleShading = 0.2f;
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = config.enableTransparency ? VK_TRUE : VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; 
    color_blend_attachment.srcAlphaBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; 
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; 

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f; // Optional
    color_blending.blendConstants[1] = 0.0f; // Optional
    color_blending.blendConstants[2] = 0.0f; // Optional
    color_blending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = config.enableDepthTesting ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = config.enableDepthTesting ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = config.enableStencilTest ? VK_TRUE : VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

    std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.pNext = nullptr;
    dynamic_state_info.flags = 0;
    dynamic_state_info.dynamicStateCount = config.dynamicStates ? 2 : 0;
    dynamic_state_info.pDynamicStates = config.dynamicStates ? dynamic_states.data() : nullptr;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(layout_info.descriptors_set_layouts_aux.size());
    pipeline_layout_info.pSetLayouts = layout_info.descriptors_set_layouts_aux.data();
    pipeline_layout_info.pushConstantRangeCount = uint32_t(layout_info.push_constants_array.size());
    pipeline_layout_info.pPushConstantRanges = layout_info.push_constants_array.size() > 0 ? layout_info.push_constants_array.data() : nullptr;

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline layout!" << std::endl;
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = config.render_pass->handle();
    pipeline_info.subpass = config.subpass_number;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional
    
    VkPipeline vk_pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &vk_pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create Graphics pipeline!" << std::endl;
        return false;
    }

    vk_pipeline_layout_ = pipeline_layout;
    vk_pipeline_ = vk_pipeline;
    vk_descriptor_set_layouts_ = std::move(layout_info.descriptors_set_layouts);
    descriptor_metadata_ = std::move(layout_info.pipeline_descriptor_metadata);
    push_constants_ = std::move(layout_info.push_constants_map);

    return true;
}
