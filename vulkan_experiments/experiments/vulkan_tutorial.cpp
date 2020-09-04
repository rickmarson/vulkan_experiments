/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "file_system.hpp"

// Declarations

class VulkanTutorial : public VulkanApp {
public:


private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createGraphicsPipeline() final;
	virtual bool recordCommands() final;
	virtual void updateScene() final;

	RenderPass render_pass_;
	GraphicsPipeline graphics_pipeline_;
};

// Implementation

bool VulkanTutorial::loadAssets() {
	return true;
}

bool VulkanTutorial::setupScene() {
	if (!createGraphicsPipeline()) {
		return false;
	}
	if (!recordCommands()) {
		return false;
	}

	return true;
}

void VulkanTutorial::updateScene() {

}

bool VulkanTutorial::createGraphicsPipeline() {
	render_pass_ = vulkan_backend_.createRenderPass("Main Pass");

	GraphicsPipelineConfig config;
	config.name = "Solid Geometry";
	config.vert_code = readFile("shaders/tutorial_vs.spv");
	config.frag_code = readFile("shaders/tutorial_fs.spv");
	config.render_pass = render_pass_;

	graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	return graphics_pipeline_.vk_graphics_pipeline != VK_NULL_HANDLE;
}

bool VulkanTutorial::recordCommands() {
	auto& command_buffers = vulkan_backend_.getCommandBuffers();

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0; // Optional
		begin_info.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
			std::cerr << "Failed to begin recording command buffer!" << std::endl;
			return false;
		}

		VkRenderPassBeginInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass_.vk_render_pass;
		render_pass_info.framebuffer = render_pass_.swap_chain_framebuffers[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = vulkan_backend_.getSwapChainExtent();

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clearColor;

		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_graphics_pipeline);

		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(command_buffers[i]);
		if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
			std::cerr << "Failed to record command buffer!" << std::endl;
			return false;
		}
	}

	return true;
}

// Entry point

int main(int argc, char** argv) {
	VulkanTutorial app;
	if (!app.setup()) {
		return -1;
	}
	if (!app.run()) {
		return -1;
	}
	return 0;
}
