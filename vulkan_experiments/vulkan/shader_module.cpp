/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "shader_module.hpp"
#include "file_system.hpp"

#include "spirv_reflect.h"

#include <algorithm>
#include <iostream>

// utility functions

namespace {
    uint32_t FormatSize(VkFormat format)
    {
        uint32_t result = 0;
        switch (format) {
        case VK_FORMAT_UNDEFINED: result = 0; break;
        case VK_FORMAT_R4G4_UNORM_PACK8: result = 1; break;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_R5G6B5_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_B5G6R5_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: result = 2; break;
        case VK_FORMAT_R8_UNORM: result = 1; break;
        case VK_FORMAT_R8_SNORM: result = 1; break;
        case VK_FORMAT_R8_USCALED: result = 1; break;
        case VK_FORMAT_R8_SSCALED: result = 1; break;
        case VK_FORMAT_R8_UINT: result = 1; break;
        case VK_FORMAT_R8_SINT: result = 1; break;
        case VK_FORMAT_R8_SRGB: result = 1; break;
        case VK_FORMAT_R8G8_UNORM: result = 2; break;
        case VK_FORMAT_R8G8_SNORM: result = 2; break;
        case VK_FORMAT_R8G8_USCALED: result = 2; break;
        case VK_FORMAT_R8G8_SSCALED: result = 2; break;
        case VK_FORMAT_R8G8_UINT: result = 2; break;
        case VK_FORMAT_R8G8_SINT: result = 2; break;
        case VK_FORMAT_R8G8_SRGB: result = 2; break;
        case VK_FORMAT_R8G8B8_UNORM: result = 3; break;
        case VK_FORMAT_R8G8B8_SNORM: result = 3; break;
        case VK_FORMAT_R8G8B8_USCALED: result = 3; break;
        case VK_FORMAT_R8G8B8_SSCALED: result = 3; break;
        case VK_FORMAT_R8G8B8_UINT: result = 3; break;
        case VK_FORMAT_R8G8B8_SINT: result = 3; break;
        case VK_FORMAT_R8G8B8_SRGB: result = 3; break;
        case VK_FORMAT_B8G8R8_UNORM: result = 3; break;
        case VK_FORMAT_B8G8R8_SNORM: result = 3; break;
        case VK_FORMAT_B8G8R8_USCALED: result = 3; break;
        case VK_FORMAT_B8G8R8_SSCALED: result = 3; break;
        case VK_FORMAT_B8G8R8_UINT: result = 3; break;
        case VK_FORMAT_B8G8R8_SINT: result = 3; break;
        case VK_FORMAT_B8G8R8_SRGB: result = 3; break;
        case VK_FORMAT_R8G8B8A8_UNORM: result = 4; break;
        case VK_FORMAT_R8G8B8A8_SNORM: result = 4; break;
        case VK_FORMAT_R8G8B8A8_USCALED: result = 4; break;
        case VK_FORMAT_R8G8B8A8_SSCALED: result = 4; break;
        case VK_FORMAT_R8G8B8A8_UINT: result = 4; break;
        case VK_FORMAT_R8G8B8A8_SINT: result = 4; break;
        case VK_FORMAT_R8G8B8A8_SRGB: result = 4; break;
        case VK_FORMAT_B8G8R8A8_UNORM: result = 4; break;
        case VK_FORMAT_B8G8R8A8_SNORM: result = 4; break;
        case VK_FORMAT_B8G8R8A8_USCALED: result = 4; break;
        case VK_FORMAT_B8G8R8A8_SSCALED: result = 4; break;
        case VK_FORMAT_B8G8R8A8_UINT: result = 4; break;
        case VK_FORMAT_B8G8R8A8_SINT: result = 4; break;
        case VK_FORMAT_B8G8R8A8_SRGB: result = 4; break;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_SINT_PACK32: result = 4; break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: result = 4; break;
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: result = 4; break;
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: result = 4; break;
        case VK_FORMAT_R16_UNORM: result = 2; break;
        case VK_FORMAT_R16_SNORM: result = 2; break;
        case VK_FORMAT_R16_USCALED: result = 2; break;
        case VK_FORMAT_R16_SSCALED: result = 2; break;
        case VK_FORMAT_R16_UINT: result = 2; break;
        case VK_FORMAT_R16_SINT: result = 2; break;
        case VK_FORMAT_R16_SFLOAT: result = 2; break;
        case VK_FORMAT_R16G16_UNORM: result = 4; break;
        case VK_FORMAT_R16G16_SNORM: result = 4; break;
        case VK_FORMAT_R16G16_USCALED: result = 4; break;
        case VK_FORMAT_R16G16_SSCALED: result = 4; break;
        case VK_FORMAT_R16G16_UINT: result = 4; break;
        case VK_FORMAT_R16G16_SINT: result = 4; break;
        case VK_FORMAT_R16G16_SFLOAT: result = 4; break;
        case VK_FORMAT_R16G16B16_UNORM: result = 6; break;
        case VK_FORMAT_R16G16B16_SNORM: result = 6; break;
        case VK_FORMAT_R16G16B16_USCALED: result = 6; break;
        case VK_FORMAT_R16G16B16_SSCALED: result = 6; break;
        case VK_FORMAT_R16G16B16_UINT: result = 6; break;
        case VK_FORMAT_R16G16B16_SINT: result = 6; break;
        case VK_FORMAT_R16G16B16_SFLOAT: result = 6; break;
        case VK_FORMAT_R16G16B16A16_UNORM: result = 8; break;
        case VK_FORMAT_R16G16B16A16_SNORM: result = 8; break;
        case VK_FORMAT_R16G16B16A16_USCALED: result = 8; break;
        case VK_FORMAT_R16G16B16A16_SSCALED: result = 8; break;
        case VK_FORMAT_R16G16B16A16_UINT: result = 8; break;
        case VK_FORMAT_R16G16B16A16_SINT: result = 8; break;
        case VK_FORMAT_R16G16B16A16_SFLOAT: result = 8; break;
        case VK_FORMAT_R32_UINT: result = 4; break;
        case VK_FORMAT_R32_SINT: result = 4; break;
        case VK_FORMAT_R32_SFLOAT: result = 4; break;
        case VK_FORMAT_R32G32_UINT: result = 8; break;
        case VK_FORMAT_R32G32_SINT: result = 8; break;
        case VK_FORMAT_R32G32_SFLOAT: result = 8; break;
        case VK_FORMAT_R32G32B32_UINT: result = 12; break;
        case VK_FORMAT_R32G32B32_SINT: result = 12; break;
        case VK_FORMAT_R32G32B32_SFLOAT: result = 12; break;
        case VK_FORMAT_R32G32B32A32_UINT: result = 16; break;
        case VK_FORMAT_R32G32B32A32_SINT: result = 16; break;
        case VK_FORMAT_R32G32B32A32_SFLOAT: result = 16; break;
        case VK_FORMAT_R64_UINT: result = 8; break;
        case VK_FORMAT_R64_SINT: result = 8; break;
        case VK_FORMAT_R64_SFLOAT: result = 8; break;
        case VK_FORMAT_R64G64_UINT: result = 16; break;
        case VK_FORMAT_R64G64_SINT: result = 16; break;
        case VK_FORMAT_R64G64_SFLOAT: result = 16; break;
        case VK_FORMAT_R64G64B64_UINT: result = 24; break;
        case VK_FORMAT_R64G64B64_SINT: result = 24; break;
        case VK_FORMAT_R64G64B64_SFLOAT: result = 24; break;
        case VK_FORMAT_R64G64B64A64_UINT: result = 32; break;
        case VK_FORMAT_R64G64B64A64_SINT: result = 32; break;
        case VK_FORMAT_R64G64B64A64_SFLOAT: result = 32; break;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: result = 4; break;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: result = 4; break;

        default:
            break;
        }
        return result;
    }
}

// ShaderModule

ShaderModule::ShaderModule(const std::string& name, VkDevice device) :
    name_(name),
    device_(device)
{

}

ShaderModule::~ShaderModule() { 
    if (isValid()) {
        cleanup();
    }
}

std::shared_ptr<ShaderModule> ShaderModule::createShaderModule(const std::string& name, VkDevice device) {
    return std::make_shared<ShaderModule>(name, device);
}

void ShaderModule::loadSpirvShader(const std::string& spirv_file_path) {
    // create shader module
    auto code = readFile(spirv_file_path);
    if (code.empty()) {
        return;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device_, &create_info, nullptr, &vk_shader_) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module!" << std::endl;
        return;
    }

    SpvReflectShaderModule reflect_module = {};
    SpvReflectResult result = spvReflectCreateShaderModule(code.size(), code.data(), &reflect_module);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        std::cerr << "Failed to create SPIRV Reflect module!" << std::endl;
        cleanup();
        return;
    }

    vk_shader_stage_ = static_cast<VkShaderStageFlagBits>(reflect_module.shader_stage);

    extractUniformBufferLayouts(reflect_module);

    if (vk_shader_stage_ == VK_SHADER_STAGE_VERTEX_BIT) {
        extractInputVariables(reflect_module);
    }
}

void ShaderModule::extractUniformBufferLayouts(SpvReflectShaderModule& reflect_module) {
    uint32_t count = 0;
    SpvReflectResult result = spvReflectEnumerateDescriptorSets(&reflect_module, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        std::cerr << "Failed to enumerate descriptor sets!" << std::endl;
        cleanup();
        return;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&reflect_module, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        std::cerr << "Failed to extract descriptor sets!" << std::endl;
        cleanup();
        return;
    }

    layout_sets_.resize(sets.size());

    for (size_t i = 0; i < sets.size(); ++i) {
        const SpvReflectDescriptorSet& src_set = *(sets[i]);

        DescriptorSetLayouts& vk_set = layout_sets_[i];
        vk_set.layout_bindings.resize(src_set.binding_count);
        
        for (uint32_t j = 0; j < src_set.binding_count; ++j) {
            const SpvReflectDescriptorBinding& src_binding = *(src_set.bindings[j]);

            VkDescriptorSetLayoutBinding& vk_layout_binding = vk_set.layout_bindings[j];
            vk_layout_binding.binding = src_binding.binding;
            vk_layout_binding.descriptorType = static_cast<VkDescriptorType>(src_binding.descriptor_type);
            vk_layout_binding.descriptorCount = 1;
            for (uint32_t k = 0; k < src_binding.array.dims_count; ++k) {
                vk_layout_binding.descriptorCount *= src_binding.array.dims[k];
            }
            vk_layout_binding.stageFlags = vk_shader_stage_;
        }
        vk_set.id = src_set.set;
        vk_set.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        vk_set.create_info.bindingCount = src_set.binding_count;
        vk_set.create_info.pBindings = vk_set.layout_bindings.data();
    }
}

void ShaderModule::extractInputVariables(SpvReflectShaderModule& reflect_module) {
    uint32_t count = 0;
    SpvReflectResult result = spvReflectEnumerateInputVariables(&reflect_module, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        std::cerr << "Failed to enumerate input variables!" << std::endl;
        cleanup();
        return;
    }

    std::vector<SpvReflectInterfaceVariable*> input_vars(count);
    result = spvReflectEnumerateInputVariables(&reflect_module, &count, input_vars.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        std::cerr << "Failed to extract input variables!" << std::endl;
        cleanup();
        return;
    }

    input_binding_description_.binding = 0;
    input_binding_description_.stride = 0;  // computed below
    input_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    input_attributes_.resize(input_vars.size(), VkVertexInputAttributeDescription{});

    for (size_t i = 0; i < input_vars.size(); ++i) {
        const SpvReflectInterfaceVariable& src_var = *(input_vars[i]);
        VkVertexInputAttributeDescription& attr_desc = input_attributes_[i];
        attr_desc.location = src_var.location;
        attr_desc.binding = input_binding_description_.binding;
        attr_desc.format = static_cast<VkFormat>(src_var.format);
        attr_desc.offset = 0;  // final offset computed below after sorting.
    }

    // Sort attributes by location
    std::sort(std::begin(input_attributes_), std::end(input_attributes_),
        [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
            return a.location < b.location; });
    // Compute final offsets of each attribute, and total vertex stride.
    for (auto& attribute : input_attributes_) {
        uint32_t format_size = FormatSize(attribute.format);
        attribute.offset = input_binding_description_.stride;
        input_binding_description_.stride += format_size;
    }
}

void ShaderModule::cleanup() {
    vkDestroyShaderModule(device_, vk_shader_, nullptr);
    vk_shader_ = VK_NULL_HANDLE;
    /*for (auto& set : layout_sets_) {
        vkDestroyDescriptorSetLayout(device_, set.layout, nullptr);
    }*/
    layout_sets_.clear();
}

bool ShaderModule::isVertexFormatCompatible(const VertexFormatInfo& format_info) const {
    if (format_info.first != input_binding_description_.stride) {
        return false;
    }
    if (format_info.second.size() != input_attributes_.size()) {
        return false;
    }
    for (size_t i = 0; i < format_info.second.size(); ++i) {
        if (format_info.second[i] != input_attributes_[i].offset) {
            return false;
        }
    }
    return true;
}
