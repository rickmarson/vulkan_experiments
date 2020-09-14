/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "shader_module.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <chrono>
#include <map>
#include <iostream>

// Declarations
struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VertexFormatInfo getFormatInfo() {
		std::vector<size_t> offsets = { offsetof(Vertex, pos), offsetof(Vertex, color) };
		return { sizeof(Vertex) , offsets };
	}
};

struct ModelViewProj {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};


const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
	0, 1, 2, 2, 3, 0
};


class VulkanTutorial : public VulkanApp {
public:


private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createBuffers() final;
	virtual bool createGraphicsPipeline() final;
	virtual bool recordCommands() final;
	virtual void updateScene() final;
	virtual void cleanup() final;

	Buffer vertex_buffer_;
	Buffer index_buffer_;
	std::map<std::string, std::shared_ptr<ShaderModule>> shaders_;
	std::map<std::string, UniformBuffer> uniform_buffers_;
	RenderPass render_pass_;
	GraphicsPipeline graphics_pipeline_;
	ModelViewProj mvp_;
};

// Implementation

bool VulkanTutorial::loadAssets() {
	auto vertex_shader = vulkan_backend_.createShaderModule("vertex");
	vertex_shader->loadSpirvShader("shaders/tutorial_vs.spv");

	if (!vertex_shader->isVertexFormatCompatible(Vertex::getFormatInfo())) {
		std::cerr << "Requested Vertex format is not compatible with pipeline input!" << std::endl;
		return false;
	}

	auto fragment_shader = vulkan_backend_.createShaderModule("fragment");
	fragment_shader->loadSpirvShader("shaders/tutorial_fs.spv");

	shaders_[vertex_shader->getName()] = std::move(vertex_shader);
	shaders_[fragment_shader->getName()] = std::move(fragment_shader);

;	if (!createBuffers()) {
		return false;
	}

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

void VulkanTutorial::cleanup() {
	shaders_.clear();
}

void VulkanTutorial::updateScene() {
	static auto start_time = std::chrono::high_resolution_clock::now();

	auto current_time = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
	
	auto extent = vulkan_backend_.getSwapChainExtent();
	mvp_.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mvp_.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mvp_.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);

	mvp_.proj[1][1] *= -1;  // y is inverted in vulkan w.r.t. opengl

	auto& command_buffers = vulkan_backend_.getCommandBuffers();
	for (size_t i = 0; i < command_buffers.size(); i++) {
		vulkan_backend_.updateBuffer<ModelViewProj>(uniform_buffers_["vertex_0"].buffers[i], { mvp_ });
	}
}

bool VulkanTutorial::createBuffers() {
	vertex_buffer_ = vulkan_backend_.createVertexBuffer<Vertex>("triangle_vertices", vertices);
	index_buffer_ = vulkan_backend_.createIndexBuffer("triangle_indices", indices);
	return vertex_buffer_.vk_buffer != VK_NULL_HANDLE && index_buffer_.vk_buffer != VK_NULL_HANDLE;
}

bool VulkanTutorial::createGraphicsPipeline() {
	const auto& layout_set = shaders_["vertex"]->getDescriptorSetLayouts();
	auto buffer_name = shaders_["vertex"]->getName() + "_" + std::to_string(layout_set[0].id);
	// we still need to know the underlying C++ type to bind to the buffer
	uniform_buffers_[buffer_name] = vulkan_backend_.createUniformBuffer<ModelViewProj>(buffer_name, layout_set[0].layout);

	render_pass_ = vulkan_backend_.createRenderPass("Main Pass");

	GraphicsPipelineConfig config;
	config.name = "Solid Geometry";
	config.vertex = shaders_["vertex"];
	config.fragment = shaders_["fragment"];
	config.vertex_buffer_binding_desc = shaders_["vertex"]->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = shaders_["vertex"]->getInputAttributes();
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

		VkBuffer vertex_buffers[] = { vertex_buffer_.vk_buffer };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(command_buffers[i], index_buffer_.vk_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_pipeline_layout, 0, 1, &uniform_buffers_["vertex_0"].descriptors[i], 0, nullptr);

		vkCmdDrawIndexed(command_buffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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
