/*
* vulkna_tutorial.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "shader_module.hpp"
#include "texture.hpp"
#include "static_mesh.hpp"
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
	virtual RecordCommandsResult renderFrame(uint32_t swapchain_image) final;
	virtual void updateScene() final;
	virtual void cleanupSwapChainAssets() final;
	virtual void cleanup() final;

	void drawUi();

	std::unique_ptr<ImGuiRenderer> imgui_renderer_;
	std::vector<VkCommandBuffer> main_command_buffers_;

	std::unique_ptr<SceneManager> scene_manager_;
	RenderPass render_pass_;

	// options
	float camera_fov_deg_ = 45.0f;
	bool turntable_on_ = false;
	bool lock_camera_to_target = true;
	bool update_mesh_transform = false;
	std::chrono::time_point<std::chrono::steady_clock> animation_start_time_;
	glm::mat4 initial_model_transform_ = glm::mat4(1.0f);
	float rot_angle_x_ = 0.0f;
	float rot_angle_y_ = 0.0f;
	float rot_angle_z_ = 0.0f;

	glm::vec3 cam_pos_ = glm::vec3(-3.0f, 0.0f, 1.0f);
};

// Implementation

bool ModelViewer::loadAssets() {
	main_command_buffers_ = vulkan_backend_.createPrimaryCommandBuffers(vulkan_backend_.getSwapChainSize());

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_ = SceneManager::create(&vulkan_backend_);
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 10.0f);
	scene_manager_->setCameraPosition(cam_pos_);
	scene_manager_->setCameraTarget(glm::vec3(0.0f, 0.0f, 0.0f));

	scene_manager_->loadFromGlb("meshes/viking_room.glb");
	initial_model_transform_ = scene_manager_->getObjectByIndex(0)->getTransform();

	imgui_renderer_ = ImGuiRenderer::create(&vulkan_backend_);
	imgui_renderer_->setUp(window_);

#ifndef NDEBUG
	vulkan_backend_.enableTimestampQueries(4);
#endif

	return true;
}

bool ModelViewer::setupScene() {
	DescriptorPoolConfig pool_config;

	auto scene_pool = scene_manager_->getDescriptorsCount(1);
	auto ui_pool = imgui_renderer_->getDescriptorsCount();

	pool_config = scene_pool + ui_pool;
	
	pool_config.uniform_buffers_count *= vulkan_backend_.getSwapChainSize();
	pool_config.image_samplers_count *= vulkan_backend_.getSwapChainSize();
	pool_config.image_storage_buffers_count *= vulkan_backend_.getSwapChainSize();

	vulkan_backend_.createDescriptorPool(pool_config);

	RenderPassConfig render_pass_config;
	render_pass_config.name = "Main Pass";
	render_pass_config.msaa_samples = vulkan_backend_.getMaxMSAASamples();
	
	SubpassConfig model_subpass;
	model_subpass.use_colour_attachment = true;
	model_subpass.use_depth_stencil_attachemnt = true;
	SubpassConfig::Dependency subpass_dependency;
	subpass_dependency.src_subpass = -1;
	subpass_dependency.dst_subpass = 0;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::NONE;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	model_subpass.dependencies.push_back(subpass_dependency);

	SubpassConfig ui_subpass;
	ui_subpass.use_colour_attachment = true;
	ui_subpass.use_depth_stencil_attachemnt = false;
	subpass_dependency.src_subpass = 0;
	subpass_dependency.dst_subpass = 1;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::NONE;
	ui_subpass.dependencies.push_back(subpass_dependency);

	render_pass_config.subpasses = { model_subpass, ui_subpass };

	render_pass_ = vulkan_backend_.createRenderPass(render_pass_config);

	if (render_pass_.vk_render_pass == VK_NULL_HANDLE) {
		return false;
	}

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 10.0f);

	createGraphicsPipeline();
	scene_manager_->prepareForRendering();

	drawUi();

	return true;
}

bool ModelViewer::createGraphicsPipeline() {
	if (!scene_manager_->createGraphicsPipeline("model_viewer", render_pass_, 0)) {
		return false;
	}
	if (!imgui_renderer_->createGraphicsPipeline(render_pass_, 1)) {
		return false;
	}
	return true;
}

void ModelViewer::cleanupSwapChainAssets() {
	imgui_renderer_->cleanupGraphicsPipeline();
	scene_manager_->cleanupSwapChainAssets();
	vulkan_backend_.destroyRenderPass(render_pass_);
}

void ModelViewer::cleanup() {
	cleanupSwapChainAssets();
	imgui_renderer_->shutDown();
	vulkan_backend_.freeCommandBuffers(main_command_buffers_);
	scene_manager_.reset();
}

void ModelViewer::updateScene() {
	auto final_angle_z = rot_angle_z_;

	if (turntable_on_) {
		auto current_time = std::chrono::steady_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - animation_start_time_).count();
		final_angle_z *= glm::radians(90.0f * time);
		update_mesh_transform = true;
	}

	if (update_mesh_transform) {
		auto mesh = scene_manager_->getObjectByIndex(0);  // there's only one mesh

		auto rotation_matrix = initial_model_transform_;
		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(final_angle_z), glm::vec3(0.0f, 0.0f, 1.0f));
		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rot_angle_y_), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rot_angle_x_), glm::vec3(1.0f, 0.0f, 0.0f));

		mesh->setTransform(rotation_matrix);
		update_mesh_transform = false;
	}

	scene_manager_->setFollowTarget(lock_camera_to_target);
	scene_manager_->setCameraPosition(cam_pos_);
	scene_manager_->update();

	drawUi();
}

RecordCommandsResult ModelViewer::renderFrame(uint32_t swapchain_image) {
	auto& main_command_buffer = main_command_buffers_[swapchain_image];

	// might need to combine multiple command buffers in one frame in the future
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

	vulkan_backend_.resetAllTimestampQueries(command_buffers[0]);

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_.vk_render_pass;
	render_pass_info.framebuffer = render_pass_.framebuffers[swapchain_image];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = vulkan_backend_.getSwapChainExtent();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(command_buffers[0], &render_pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	
	auto scene_commands = scene_manager_->renderFrame(swapchain_image, render_pass_info);
	auto success = std::get<0>(scene_commands);

	if (success) {
		auto scene_cmd_buffers = std::get<1>(scene_commands);
		vkCmdExecuteCommands(command_buffers[0], static_cast<uint32_t>(scene_cmd_buffers.size()), scene_cmd_buffers.data());
	}

	// register UI overlay commands
	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	ImGuiProfileConfig ui_profile_config = { true, 2, 3 };
	auto commands = imgui_renderer_->renderFrame(swapchain_image, render_pass_info, ui_profile_config);
	success = std::get<0>(commands);

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
	static auto time_to_draw_geometry = 0.0f;
	static auto time_to_draw_ui = 0.0f;
	
	auto vulkan_stats = vulkan_backend_.retrieveTimestampQueries();
	if (!vulkan_stats.empty()) {
		time_to_draw_geometry = vulkan_stats[1] - vulkan_stats[0];
		time_to_draw_ui = vulkan_stats[3] - vulkan_stats[2];
	}

	imgui_renderer_->beginFrame();

	static char turn_table_label[] = "Toggle Turn Table";
	static char reset_button_label[] = "Reset";

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	const auto high_dpi_scale = imgui_renderer_->getHighDpiScale();
	ImGui::SetNextWindowSizeConstraints(ImVec2(270 * high_dpi_scale, 220 * high_dpi_scale), ImVec2(600 * high_dpi_scale, 600 * high_dpi_scale));

	ImGui::Begin("Options");                   

	ImGui::Text("Rotate Model");

	if (ImGui::Checkbox(turn_table_label, &turntable_on_)) {
		if (turntable_on_) {
			animation_start_time_ = std::chrono::steady_clock::now();
		}
	}

	if (ImGui::SliderFloat("X Rotation", &rot_angle_x_, -90.0f, 90.0f)) update_mesh_transform = true;
	if (ImGui::SliderFloat("Y Rotation", &rot_angle_y_, -90.0f, 90.0f)) update_mesh_transform = true;
	if (ImGui::SliderFloat("Z Rotation", &rot_angle_z_, -180.0f, 180.0f)) update_mesh_transform = true;
	
	ImGui::Separator();
	ImGui::Text("Move Camera");

	ImGui::SliderFloat("X Offset", &cam_pos_[0], -5.0f, 5.0f);
	ImGui::SliderFloat("Y Offset", &cam_pos_[1], -5.0f, 5.0f);
	ImGui::SliderFloat("Z Offset", &cam_pos_[2], -5.0f, 5.0f);
	ImGui::Checkbox("Lock To Target", &lock_camera_to_target);

	ImGui::Separator();

	if (ImGui::Button(reset_button_label)) {
		rot_angle_x_ = 0.0f;
		rot_angle_y_ = 0.0f;
		rot_angle_z_ = 45.0f;
		turntable_on_ = false;
	}

	ImGui::End();

	auto extent = vulkan_backend_.getSwapChainExtent();
	auto stats_width = 200 * high_dpi_scale;
	auto stats_pos = extent.width - stats_width - 50;
	ImGui::SetNextWindowPos(ImVec2(stats_pos, 10));
	ImGui::SetNextWindowSizeConstraints(ImVec2(stats_width, 80 * high_dpi_scale), ImVec2(stats_width, 100 * high_dpi_scale));

	ImGui::Begin("Stats");

	ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Geom. draw time: %.4f ms", time_to_draw_geometry);
	ImGui::Text("UI draw time: %.4f ms", time_to_draw_ui);
	
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
