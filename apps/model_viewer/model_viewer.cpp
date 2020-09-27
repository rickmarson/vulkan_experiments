/*
* vulkna_tutorial.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "shader_module.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include "scene_manager.hpp"

#include <chrono>

// Declarations

class ModelViewer : public VulkanApp {
public:


private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createGraphicsPipeline() final;
	virtual bool recordCommands() final;
	virtual void updateScene() final;
	virtual void cleanupSwapChainAssets() final;
	virtual void cleanup() final;

	std::map<std::string, std::shared_ptr<ShaderModule>> shaders_;
	std::map<std::string, std::shared_ptr<Mesh>> meshes_;
	std::unique_ptr<SceneManager> scene_manager_;
	RenderPass render_pass_;
	GraphicsPipeline graphics_pipeline_;
};

// Implementation

bool ModelViewer::loadAssets() {
	auto vertex_shader = vulkan_backend_.createShaderModule("vertex");
	vertex_shader->loadSpirvShader("shaders/model_viewer_vs.spv");

	if (!vertex_shader->isVertexFormatCompatible(Vertex::getFormatInfo())) {
		std::cerr << "Requested Vertex format is not compatible with pipeline input!" << std::endl;
		return false;
	}

	auto fragment_shader = vulkan_backend_.createShaderModule("fragment");
	fragment_shader->loadSpirvShader("shaders/model_viewer_fs.spv");
	
	if (!vertex_shader->isValid() || !fragment_shader->isValid()) {
		std::cerr << "Failed to validate shaders!" << std::endl;
		return false;
	}

	shaders_[vertex_shader->getName()] = std::move(vertex_shader);
	shaders_[fragment_shader->getName()] = std::move(fragment_shader);

	auto mesh = vulkan_backend_.createMesh("viking_room");
	if (!mesh->loadObjModel("meshes/viking_room.obj")) {
		return false;
	}

	meshes_[mesh->getName()] = std::move(mesh);

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_ = std::make_unique<SceneManager>(&vulkan_backend_);
	scene_manager_->setCameraProperties(45.0f, extent.width / (float)extent.height, 0.1f, 10.0f);
	scene_manager_->setCameraPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	scene_manager_->setCameraTarget(glm::vec3(2.0f, 2.0f, 2.0f));

	return true;
}

bool ModelViewer::setupScene() {
	if (!createGraphicsPipeline()) {
		return false;
	}
	if (!recordCommands()) {
		return false;
	}

	return true;
}

void ModelViewer::cleanupSwapChainAssets() {
	scene_manager_->deleteUniformBuffer();

	for (auto& mesh : meshes_) {
		mesh.second->deleteUniformBuffer();
	}
	
	vulkan_backend_.destroyRenderPass(render_pass_);
	vulkan_backend_.destroyGraphicsPipeline(graphics_pipeline_);
}

void ModelViewer::cleanup() {
	cleanupSwapChainAssets();
	meshes_.clear();
	shaders_.clear();
}

void ModelViewer::updateScene() {
	static auto start_time = std::chrono::high_resolution_clock::now();

	auto current_time = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

	scene_manager_->update();

	auto& mesh = meshes_["viking_room"];

	mesh->setTransform(
		glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f))
	);

	mesh->update();
}

bool ModelViewer::createGraphicsPipeline() {
	render_pass_ = vulkan_backend_.createRenderPass("Main Pass", vulkan_backend_.getMaxMSAASamples());

	GraphicsPipelineConfig config;
	config.name = "Solid Geometry";
	config.vertex = shaders_["vertex"];
	config.fragment = shaders_["fragment"];
	config.vertex_buffer_binding_desc = shaders_["vertex"]->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = shaders_["vertex"]->getInputAttributes();
	config.render_pass = render_pass_;

	graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	vulkan_backend_.createDescriptorPool(2, 1);

	scene_manager_->createUniformBuffer();
	scene_manager_->createDescriptorSets(graphics_pipeline_.vk_descriptor_set_layouts);
	scene_manager_->updateDescriptorSets(graphics_pipeline_.descriptor_metadata);

	auto& mesh = meshes_["viking_room"];

	mesh->createUniformBuffer();
	mesh->createDescriptorSets(graphics_pipeline_.vk_descriptor_set_layouts);
	mesh->updateDescriptorSets(graphics_pipeline_.descriptor_metadata);

	return graphics_pipeline_.vk_graphics_pipeline != VK_NULL_HANDLE;
}

bool ModelViewer::recordCommands() {
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

		std::array<VkClearValue, 2> clear_values{};
		clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_values[1].depthStencil = { 1.0f, 0 };

		render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
		render_pass_info.pClearValues = clear_values.data();

		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_graphics_pipeline);

		VkDeviceSize offsets[] = { 0 };
		auto& scene_descriptors = scene_manager_->getDescriptorSets();

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_pipeline_layout, SCENE_UNIFORM_SET_ID, 1, &scene_descriptors[i], 0, nullptr);

		auto& mesh = meshes_["viking_room"];
		auto& mesh_descriptors = mesh->getDescriptorSets();

		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &mesh->getVertexBuffer().vk_buffer, offsets);
		vkCmdBindIndexBuffer(command_buffers[i], mesh->getIndexBuffer().vk_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_pipeline_layout, MODEL_UNIFORM_SET_ID, 1, &mesh_descriptors[i], 0, nullptr);

		vkCmdDrawIndexed(command_buffers[i], mesh->getIndexCount(), 1, 0, 0, 0);

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
	ModelViewer app;
	if (!app.setup()) {
		return -1;
	}
	if (!app.run()) {
		return -1;
	}
	return 0;
}
