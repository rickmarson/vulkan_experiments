/*
* shader_module.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

struct SpvReflectShaderModule;

struct DescriptorSetLayouts {
	uint32_t id;
	VkDescriptorSetLayoutCreateInfo create_info;
	VkDescriptorSetLayout layout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
};

struct PushConstantBlock {
	std::string name;
	VkPushConstantRange push_constant_range;
};

class ShaderModule {
public:
	explicit ShaderModule(const std::string& name, VkDevice device);
	~ShaderModule();

	static std::shared_ptr<ShaderModule> createShaderModule(const std::string& name, VkDevice device);

	void loadSpirvShader(const std::string& spirv_file_path);
	bool isValid() const { return vk_shader_ != VK_NULL_HANDLE; }
	const std::string& getName() const { return name_; }
	VkShaderStageFlags getShaderStage() const { return vk_shader_stage_; }
	VkShaderModule getShader() const { return vk_shader_; };
	const std::vector<DescriptorSetLayouts>& getDescriptorSetLayouts() const { return layout_sets_; }
	const std::vector<PushConstantBlock>& getPushConstants() const { return push_constants_; }
	VkVertexInputBindingDescription getInputBindingDescription() const { return input_binding_description_; }  
	const std::vector<VkVertexInputAttributeDescription>& getInputAttributes() { return input_attributes_; }
	bool isVertexFormatCompatible(const VertexFormatInfo& format_info) const;
	const DescriptorSetMetadata& getDescriptorsMetadata() const { return descriptors_metadata_; }

private:	
	void extractUniformBufferLayouts(SpvReflectShaderModule& reflect_module);
	void extractPushConstants(SpvReflectShaderModule& reflect_module);
	void extractInputVariables(SpvReflectShaderModule& reflect_module);
	void cleanup();

	std::string name_;
	VkDevice device_;
	VkShaderStageFlags vk_shader_stage_ = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	VkShaderModule vk_shader_ = VK_NULL_HANDLE;
	std::vector<DescriptorSetLayouts> layout_sets_;
	std::vector<PushConstantBlock> push_constants_;
	VkVertexInputBindingDescription input_binding_description_ = {};
	std::vector<VkVertexInputAttributeDescription> input_attributes_;
	DescriptorSetMetadata descriptors_metadata_;
};
