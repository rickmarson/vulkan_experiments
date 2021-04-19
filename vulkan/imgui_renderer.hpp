/*
* imgui_renderer.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include "common_definitions.hpp"
#include "shader_module.hpp"

#include <imgui.h>

class VulkanBackend;
class Texture;

using GLFWWindowHandle = void*;

struct ImGuiProfileConfig {
	bool profile_draw = false;
	uint32_t start_query_num = 0;
	uint32_t stop_query_num = 0;
};

class ImGuiRenderer {
public:
	static std::unique_ptr<ImGuiRenderer> create(VulkanBackend* backend);

	explicit ImGuiRenderer(VulkanBackend* backend);

	bool setUp(GLFWWindowHandle window);
	void shutDown();

	float getHighDpiScale() const { return high_dpi_scale_; }

	bool createGraphicsPipeline(RenderPass& render_pass, uint32_t subpass_number);
	void cleanupGraphicsPipeline();
	DescriptorPoolConfig getDescriptorsCount() const;

	void beginFrame();
	void endFrame();
	RecordCommandsResult renderFrame(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info, const ImGuiProfileConfig& profile_config);

private:
	void InitImGui(GLFWWindowHandle window);
	void InitVulkanAssets();
	void uploadFonts();

	void createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts);
	void updateDescriptorSets(const DescriptorSetMetadata& metadata);
	void createBuffers();
	void updateBuffers();

	float high_dpi_scale_ = 1.0f;

	VulkanBackend* vulkan_backend_ = nullptr;
	Buffer vertex_buffer_;
	Buffer index_buffer_;
	uint32_t max_vertex_count_ = 0;
	uint32_t max_index_count_ = 0;
	
	std::shared_ptr<ShaderModule> imgui_vertex_shader_;
	std::shared_ptr<ShaderModule> imgui_fragment_shader_;
	std::shared_ptr<Texture> fonts_texture_;

	UiTransform ui_transform_push_constant_;
	std::vector<VkDescriptorSet> vk_descriptor_sets_;

	Pipeline ui_pipeline_;
	uint32_t subpass_number_ = 0;
	std::vector<VkCommandBuffer> vk_drawing_buffers_;  // these are secondary buffers for recording UI drawing commands
};
