/*
* vulkna_tutorial.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "shader_module.hpp"
#include "texture.hpp"
#include "scene_manager.hpp"
#include "imgui_renderer.hpp"
#include "particles/rain_emitter_gs.hpp"
#include "particles/rain_emitter_pr.hpp"
#include "particles/rain_emitter_inst.hpp"
#include "particles/rain_emitter_mesh.hpp"
#include "render_pass.hpp"

#include <chrono>

// Declarations
const char* const kEmitterTypes[] = {
	"Geometry Shader",
	"Primitive Restart",
	"Instancing",
	"Mesh",
	0
};

enum EmitterType : int {
	GEOMETRY_SHADER,
	PRIMITIVE_RESTART,
	INSTANCING,
	MESH
};


class RainyAlley : public VulkanApp {
public:
	RainyAlley() {
		setWindowTitle("Rainy Alley");
	}

private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createGraphicsPipeline() final;
	virtual RecordCommandsResult renderFrame(uint32_t swapchain_image) final;
	virtual void updateScene() final;
	virtual void cleanupSwapChainAssets() final;
	virtual void cleanup() final;

	void updateDescriptorSets();
	void drawUi();

	std::unique_ptr<ImGuiRenderer> imgui_renderer_;
	std::vector<VkCommandBuffer> graphics_command_buffers_;

	std::shared_ptr<ParticleEmitterBase> rain_drops_emitter_;
	std::unique_ptr<SceneManager> scene_manager_;
	std::unique_ptr<RenderPass> render_pass_;

	// options
	float camera_fov_deg_ = 45.0f;
	int number_of_particles_ = 1000;
	float lifetime_after_collision_ = 0.25f;
	EmitterType selected_emitter_type_ = GEOMETRY_SHADER;
};

// Implementation

bool RainyAlley::loadAssets() {
	graphics_command_buffers_ = vulkan_backend_.createPrimaryCommandBuffers(vulkan_backend_.getSwapChainSize());
	
#ifndef NDEBUG
	vulkan_backend_.enableTimestampQueries(8);
#endif

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_ = SceneManager::create(&vulkan_backend_);
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 1000.0f);
	scene_manager_->setCameraPosition(glm::vec3(-10.0f, 0.0f, 4.0f));
	scene_manager_->setCameraTarget(glm::vec3(0.0f, 0.0f, 2.0f));

	auto light_pos = glm::vec3(0.4f, -0.1f, 5.7f);
	scene_manager_->setLightPosition(light_pos);
	scene_manager_->setLightColour(glm::vec4(1.0f, 0.971f, 0.492f, 1.0f), 4.0f);
	scene_manager_->setAmbientColour(glm::vec4(0.02f));
	scene_manager_->enableShadows();

	scene_manager_->loadFromGlb("meshes/alley.glb");

	imgui_renderer_ = ImGuiRenderer::create(&vulkan_backend_);
	imgui_renderer_->setUp(window_);

	return true;
}

bool RainyAlley::setupScene() {
	ParticleEmitterConfig emitter_config;
	emitter_config.name = "rain_drops_emitter";
	emitter_config.starting_transform = glm::identity<glm::mat4>();
	emitter_config.min_box_extent = glm::vec3(-10.0f, -8.0, 15.0f);
	emitter_config.max_box_extent = glm::vec3(2.0f, 8.0, 20.0f);
	emitter_config.min_starting_velocity = glm::vec3(0.0f, 0.0f, -10.0f);
	emitter_config.max_starting_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	emitter_config.lifetime_after_collision = lifetime_after_collision_;
	emitter_config.texture_atlas = "textures/rain_drops.png";
	emitter_config.subpass_number = 1;
#ifndef NDEBUG
	emitter_config.profile = true;
	emitter_config.start_query_num = 0;
	emitter_config.stop_query_num = 1;
#endif

	switch (selected_emitter_type_) {
		case GEOMETRY_SHADER:
			rain_drops_emitter_ = RainEmitterGS::createParticleEmitter(emitter_config, &vulkan_backend_);
			break;
		case PRIMITIVE_RESTART:
			rain_drops_emitter_ = RainEmitterPR::createParticleEmitter(emitter_config, &vulkan_backend_);
			break;
		case INSTANCING:
			rain_drops_emitter_ = RainEmitterInst::createParticleEmitter(emitter_config, &vulkan_backend_);
			break;
		case MESH:
			rain_drops_emitter_ = RainEmitterMesh::createParticleEmitter(emitter_config, &vulkan_backend_);
			break;
	}

	DescriptorPoolConfig pool_config;

	auto scene_pool = scene_manager_->getDescriptorsCount(2);
	auto emitter_pool = rain_drops_emitter_->getDescriptorsCount();
	auto ui_pool = imgui_renderer_->getDescriptorsCount();
	
	pool_config = scene_pool + emitter_pool + ui_pool;

	pool_config.uniform_buffers_count *= vulkan_backend_.getSwapChainSize();
	pool_config.image_samplers_count *= vulkan_backend_.getSwapChainSize();
	pool_config.image_storage_buffers_count *= vulkan_backend_.getSwapChainSize();
	
	vulkan_backend_.createDescriptorPool(pool_config);

	rain_drops_emitter_->createParticles(number_of_particles_);

	RenderPassConfig render_pass_config;
	render_pass_config.msaa_samples = vulkan_backend_.getMaxMSAASamples();

	SubpassConfig alley_subpass;
	alley_subpass.use_colour_attachment = true;
	alley_subpass.use_depth_stencil_attachemnt = true;
	SubpassConfig::Dependency subpass_dependency;
	subpass_dependency.src_subpass = -1;
	subpass_dependency.dst_subpass = 0;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::NONE;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	alley_subpass.dependencies.push_back(subpass_dependency);

	SubpassConfig rain_subpass;
	rain_subpass.use_colour_attachment = true;
	rain_subpass.use_depth_stencil_attachemnt = true;
	subpass_dependency.src_subpass = 0;
	subpass_dependency.dst_subpass = 1;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	rain_subpass.dependencies.push_back(subpass_dependency);

	SubpassConfig ui_subpass;
	ui_subpass.use_colour_attachment = true;
	ui_subpass.use_depth_stencil_attachemnt = false;
	subpass_dependency.src_subpass = 1;
	subpass_dependency.dst_subpass = 2;
	subpass_dependency.src_dependency = SubpassConfig::DependencyType::COLOUR_ATTACHMENT;
	subpass_dependency.dst_dependency = SubpassConfig::DependencyType::NONE;
	ui_subpass.dependencies.push_back(subpass_dependency);
 
	render_pass_config.subpasses = { alley_subpass, rain_subpass, ui_subpass };

	render_pass_ = vulkan_backend_.createRenderPass("Main Pass");

	if (!render_pass_->buildRenderPass(render_pass_config)) {
		return false;
	}

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 1000.0f);

	if (!createGraphicsPipeline()) {
		return false;
	}

	scene_manager_->prepareForRendering();

	drawUi();

	return true;
}

void RainyAlley::cleanupSwapChainAssets() {
	rain_drops_emitter_.reset();
	scene_manager_->cleanupSwapChainAssets();
	imgui_renderer_->cleanupGraphicsPipeline();
	render_pass_.reset();
}

void RainyAlley::cleanup() {
	cleanupSwapChainAssets();
	imgui_renderer_->shutDown();
	vulkan_backend_.freeCommandBuffers(graphics_command_buffers_);
	rain_drops_emitter_.reset();
	scene_manager_.reset();
}

void RainyAlley::updateScene() {
	static auto time_last_call = std::chrono::steady_clock::now();

	auto time_now = std::chrono::steady_clock::now();
	auto delta_time_s = std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_last_call).count() * 1e-6f;
	time_last_call = time_now;

	scene_manager_->update();

	auto result = rain_drops_emitter_->update(delta_time_s, scene_manager_->getSceneData());

	// dispatch is handled here instead of within rain_drops_emitter_
	// so that it's possible to stack multiple compute buffers in one queue.
	if (std::get<0>(result)) {
		vulkan_backend_.submitComputeCommands(std::get<1>(result));
	}

	drawUi();
}

bool RainyAlley::createGraphicsPipeline() {
	if (!scene_manager_->createGraphicsPipeline("alley", *render_pass_, 0)) {
		return false;
	}
	if (!rain_drops_emitter_->createGraphicsPipeline(*render_pass_, 1)) {
		return false;
	}
	if (!imgui_renderer_->createGraphicsPipeline(*render_pass_, 2)) {
		return false;
	}
	if (!rain_drops_emitter_->createComputePipeline(scene_manager_->getSceneDepthBuffer())) {
		return false;
	}

	return true;
}

RecordCommandsResult RainyAlley::renderFrame(uint32_t swapchain_image) {
	auto& main_command_buffer = graphics_command_buffers_[swapchain_image];

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

	vulkan_backend_.resetTimestampQueries(command_buffers[0], 2, 6);

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_->handle();
	render_pass_info.framebuffer = render_pass_->framebuffers()[swapchain_image];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = vulkan_backend_.getSwapChainExtent();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(command_buffers[0], &render_pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	
	ProfileConfig scene_profile_config = { true, 2, 3 };
	auto scene_commands = scene_manager_->renderFrame(swapchain_image, render_pass_info, scene_profile_config);
	auto success = std::get<0>(scene_commands);

	if (success) {
		auto scene_cmd_buffers = std::get<1>(scene_commands);
		vkCmdExecuteCommands(command_buffers[0], static_cast<uint32_t>(scene_cmd_buffers.size()), scene_cmd_buffers.data());
	}

	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	auto rain_commands = rain_drops_emitter_->renderFrame(swapchain_image, render_pass_info);
	success = std::get<0>(rain_commands);

	if (success) {
		auto rain_cmd_buffers = std::get<1>(rain_commands);
		vkCmdExecuteCommands(command_buffers[0], static_cast<uint32_t>(rain_cmd_buffers.size()), rain_cmd_buffers.data());
	}

	// register UI overlay commands
	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	
	ProfileConfig ui_profile_config = { true, 6, 7 };
	auto ui_commands = imgui_renderer_->renderFrame(swapchain_image, render_pass_info, ui_profile_config);
	success = std::get<0>(ui_commands);

	if (success) {
		auto ui_cmd_buffers = std::get<1>(ui_commands);
		vkCmdExecuteCommands(command_buffers[0], static_cast<uint32_t>(ui_cmd_buffers.size()), ui_cmd_buffers.data());
	}
	
	vkCmdEndRenderPass(command_buffers[0]);
	if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
		std::cerr << "Failed to record command buffer!" << std::endl;
		return makeRecordCommandsResult(false, command_buffers);
	}

	return makeRecordCommandsResult(true, command_buffers);
}

void RainyAlley::drawUi() {
	static auto time_to_exec_compute = 0.0f;
	static auto time_to_draw_geometry = 0.0f;
	static auto time_to_draw_particles = 0.0f;
	static auto time_to_draw_ui = 0.0f;

	auto vulkan_stats = vulkan_backend_.retrieveTimestampQueries();
	if (!vulkan_stats.empty()) {
		time_to_exec_compute = vulkan_stats[1] - vulkan_stats[0];
		time_to_draw_geometry = vulkan_stats[3] - vulkan_stats[2];
		time_to_draw_particles = vulkan_stats[5] - vulkan_stats[4];
		time_to_draw_ui = vulkan_stats[7] - vulkan_stats[6];
	}

	imgui_renderer_->beginFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	const auto high_dpi_scale = imgui_renderer_->getHighDpiScale();
	ImGui::SetNextWindowSizeConstraints(ImVec2(300 * high_dpi_scale, 100 * high_dpi_scale), ImVec2(450 * high_dpi_scale, 150 * high_dpi_scale));

	ImGui::Begin("Options");

	int available_emitters = 3;
	if (vulkan_backend_.meshShaderSupported()) {
		++available_emitters;
	}

	if (ImGui::Combo("Emitter Type", (int*)&selected_emitter_type_, kEmitterTypes, available_emitters)) {
		force_recreate_swapchain_ = true;
	}  
	
	ImGui::Text("Particles: ");
	ImGui::SameLine();
	ImGui::PushItemWidth(240);
	ImGui::SliderInt("##Particles", &number_of_particles_, 1000, 10000);
	ImGui::PopItemWidth();
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		force_recreate_swapchain_ = true;
	}
	
	ImGui::Text("Lifetime after collision: ");
	ImGui::SameLine();
	ImGui::PushItemWidth(80);
	ImGui::InputFloat("##Lifetime", &lifetime_after_collision_, 0, 0, "%.3f");
	ImGui::PopItemWidth();
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		force_recreate_swapchain_ = true;
	}

	ImGui::End();

	auto extent = vulkan_backend_.getSwapChainExtent();
	auto stats_width = 220 * high_dpi_scale;
	auto stats_pos = extent.width - stats_width - 50;
	ImGui::SetNextWindowPos(ImVec2(stats_pos, 10));
	ImGui::SetNextWindowSizeConstraints(ImVec2(stats_width, 120 * high_dpi_scale), ImVec2(stats_width, 150 * high_dpi_scale));

	ImGui::Begin("Stats");
	
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.35f, 0.35f, 1.0f));

	ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Rain update time: %.4f ms", time_to_exec_compute);
	ImGui::Text("Alley draw time: %.4f ms", time_to_draw_geometry);
	ImGui::Text("Rain draw time: %.4f ms", time_to_draw_particles);
	ImGui::Text("UI draw time: %.4f ms", time_to_draw_ui);

	ImGui::PopStyleColor();

	ImGui::End();

	imgui_renderer_->endFrame();
}

// Entry point

int main(int argc, char** argv) {
	RainyAlley app;
	if (!app.setup()) {
		return -1;
	}
	if (!app.run()) {
		return -1;
	}
	return 0;
}
