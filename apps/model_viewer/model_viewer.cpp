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
#include "imgui_renderer.hpp"

#include <chrono>

// Declarations

class ModelViewer : public VulkanApp {
public:
	ModelViewer() {
		setWindowTitle("Model Viewer");
	}

private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createGraphicsPipeline() final;
	virtual RecordCommandsResult recordCommands(uint32_t swapchain_image) final;
	virtual void updateScene() final;
	virtual void cleanupSwapChainAssets() final;
	virtual void cleanup() final;

	void drawUi();

	std::unique_ptr<ImGuiRenderer> imgui_renderer_;
	std::vector<VkCommandBuffer> main_command_buffers_;

	std::map<std::string, std::shared_ptr<ShaderModule>> shaders_;
	std::map<std::string, std::shared_ptr<Mesh>> meshes_;
	std::unique_ptr<SceneManager> scene_manager_;
	RenderPass render_pass_;
	GraphicsPipeline graphics_pipeline_;

	// options
	bool turntable_on_ = false;
	std::chrono::time_point<std::chrono::steady_clock> animation_start_time_;
	float rot_angle_x_ = 0.0f;
	float rot_angle_y_ = 0.0f;
	float rot_angle_z_ = 45.0f;
};

// Implementation

bool ModelViewer::loadAssets() {
	main_command_buffers_ = vulkan_backend_.createPrimaryCommandBuffers(vulkan_backend_.getSwapChainSize());

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
	scene_manager_ = SceneManager::create(&vulkan_backend_);
	scene_manager_->setCameraProperties(45.0f, extent.width / (float)extent.height, 0.1f, 10.0f);
	scene_manager_->setCameraPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	scene_manager_->setCameraTarget(glm::vec3(2.0f, 2.0f, 2.0f));

	imgui_renderer_ = ImGuiRenderer::create(&vulkan_backend_);
	imgui_renderer_->setUp(window_);

	return true;
}

bool ModelViewer::setupScene() {
	vulkan_backend_.createDescriptorPool(2, 2);

	RenderPassConfig render_pass_config;
	render_pass_config.name = "Main Pass";
	render_pass_config.msaa_samples = vulkan_backend_.getMaxMSAASamples();
	render_pass_config.store_depth = false;
	render_pass_config.enable_overlays = true;

	render_pass_ = vulkan_backend_.createRenderPass(render_pass_config);

	if (render_pass_.vk_render_pass == VK_NULL_HANDLE) {
		return false;
	}

	if (!createGraphicsPipeline()) {
		return false;
	}
	if (!imgui_renderer_->createGraphicsPipeline(render_pass_, 1)) {
		return false;
	}

	drawUi();

	return true;
}

void ModelViewer::cleanupSwapChainAssets() {
	imgui_renderer_->cleanupGraphicsPipeline();

	scene_manager_->deleteUniformBuffer();

	for (auto& mesh : meshes_) {
		mesh.second->deleteUniformBuffer();
	}
	
	vulkan_backend_.destroyRenderPass(render_pass_);
	vulkan_backend_.destroyGraphicsPipeline(graphics_pipeline_);
}

void ModelViewer::cleanup() {
	cleanupSwapChainAssets();
	imgui_renderer_->shutDown();
	vulkan_backend_.freeCommandBuffers(main_command_buffers_);
	meshes_.clear();
	shaders_.clear();
}

void ModelViewer::updateScene() {
	scene_manager_->update();

	auto& mesh = meshes_["viking_room"];

	auto final_angle_z = rot_angle_z_;

	if (turntable_on_) {
		auto current_time = std::chrono::steady_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - animation_start_time_).count();
		final_angle_z *= glm::radians(90.0f * time);
	}

	auto rotation_matrix = 
		glm::rotate(glm::mat4(1.0f), glm::radians(final_angle_z), glm::vec3(0.0f, 0.0f, 1.0f)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rot_angle_y_), glm::vec3(0.0f, 1.0f, 0.0f)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rot_angle_x_), glm::vec3(1.0f, 0.0f, 0.0f));
	
	mesh->setTransform(rotation_matrix);

	mesh->update();

	drawUi();
}

bool ModelViewer::createGraphicsPipeline() {
	GraphicsPipelineConfig config;
	config.name = "Solid Geometry";
	config.vertex = shaders_["vertex"];
	config.fragment = shaders_["fragment"];
	config.vertex_buffer_binding_desc = shaders_["vertex"]->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = shaders_["vertex"]->getInputAttributes();
	config.render_pass = render_pass_;
	config.subpass_number = 0;

	graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	scene_manager_->createUniformBuffer();
	scene_manager_->createDescriptorSets(graphics_pipeline_.vk_descriptor_set_layouts);
	scene_manager_->updateDescriptorSets(graphics_pipeline_.descriptor_metadata);

	auto& mesh = meshes_["viking_room"];

	mesh->createUniformBuffer();
	mesh->createDescriptorSets(graphics_pipeline_.vk_descriptor_set_layouts);
	mesh->updateDescriptorSets(graphics_pipeline_.descriptor_metadata);

	return graphics_pipeline_.vk_graphics_pipeline != VK_NULL_HANDLE;
}

RecordCommandsResult ModelViewer::recordCommands(uint32_t swapchain_image) {
	auto& main_command_buffer = main_command_buffers_[swapchain_image];

	// we might need to combine multiple command buffers in one frame in the future
	std::vector<VkCommandBuffer> command_buffers = { main_command_buffer };
	vulkan_backend_.resetCommandBuffers(command_buffers);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(command_buffers[0], &begin_info) != VK_SUCCESS) {
		std::cerr << "Failed to begin recording command buffer!" << std::endl;
		return makeRecordCommandsResult(false, command_buffers);
	}

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_.vk_render_pass;
	render_pass_info.framebuffer = render_pass_.swap_chain_framebuffers[swapchain_image];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = vulkan_backend_.getSwapChainExtent();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(command_buffers[0], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_graphics_pipeline);

	VkDeviceSize offsets[] = { 0 };
	auto& scene_descriptors = scene_manager_->getDescriptorSets();

	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_pipeline_layout, SCENE_UNIFORM_SET_ID, 1, &scene_descriptors[swapchain_image], 0, nullptr);

	auto& mesh = meshes_["viking_room"];
	auto& mesh_descriptors = mesh->getDescriptorSets();

	vkCmdBindVertexBuffers(command_buffers[0], 0, 1, &mesh->getVertexBuffer().vk_buffer, offsets);
	vkCmdBindIndexBuffer(command_buffers[0], mesh->getIndexBuffer().vk_buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.vk_pipeline_layout, MODEL_UNIFORM_SET_ID, 1, &mesh_descriptors[swapchain_image], 0, nullptr);

	vkCmdDrawIndexed(command_buffers[0], mesh->getIndexCount(), 1, 0, 0, 0);

	// register UI overlay commands
	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	
	auto commands = imgui_renderer_->recordCommands(swapchain_image, render_pass_info);
	auto success = std::get<0>(commands);

	if (success) {
		auto ui_cmd_buffers = std::get<1>(commands);
		vkCmdExecuteCommands(command_buffers[0], static_cast<uint32_t>(ui_cmd_buffers.size()), ui_cmd_buffers.data());
	}
	
	vkCmdEndRenderPass(command_buffers[0]);
	if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
		std::cerr << "Failed to record command buffer!" << std::endl;
		return makeRecordCommandsResult(false, command_buffers);
	}

	return makeRecordCommandsResult(true, command_buffers);
}

void ModelViewer::drawUi() {
	imgui_renderer_->beginFrame();

	static char turn_table_label[] = "Toggle Turn Table";
	static char reset_button_label[] = "Reset";

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	const auto high_dpi_scale = imgui_renderer_->getHighDpiScale();
	ImGui::SetNextWindowSize(ImVec2(270 * high_dpi_scale, 220 * high_dpi_scale));

	ImGui::Begin("Options");                   

	if (ImGui::Checkbox(turn_table_label, &turntable_on_)) {
		if (turntable_on_) {
			animation_start_time_ = std::chrono::steady_clock::now();
		}
	}

	ImGui::SliderFloat("X Rotation", &rot_angle_x_, -90.0f, 90.0f); 
	ImGui::SliderFloat("Y Rotation", &rot_angle_y_, -90.0f, 90.0f);
	ImGui::SliderFloat("Z Rotation", &rot_angle_z_, -180.0f, 180.0f);
	
	if (ImGui::Button(reset_button_label)) {
		rot_angle_x_ = 0.0f;
		rot_angle_y_ = 0.0f;
		rot_angle_z_ = 45.0f;
		turntable_on_ = false;
	}

	ImGui::NewLine();
	ImGui::Separator();
	ImGui::NewLine();

	ImGui::Text("Stats:");
	ImGui::NewLine();

	ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

	ImGui::End();

	imgui_renderer_->endFrame();
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
