/*
* vulkna_tutorial.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"
#include "shader_module.hpp"
#include "texture.hpp"
#include "particle_emitter.hpp"
#include "scene_manager.hpp"
#include "imgui_renderer.hpp"

#include <chrono>

// Declarations

class RainyAlley : public VulkanApp {
public:
	RainyAlley() {
		setWindowTitle("Rainy Alley");
	}

private:
	virtual bool loadAssets() final;
	virtual bool setupScene() final;
	virtual bool createGraphicsPipeline() final;
	virtual RecordCommandsResult recordCommands(uint32_t swapchain_image) final;
	virtual void updateScene() final;
	virtual void cleanupSwapChainAssets() final;
	virtual void cleanup() final;

	bool createAlleyGraphicsPipeline();
	bool createRainDropsPipeline();
	void drawUi();

	std::unique_ptr<ImGuiRenderer> imgui_renderer_;
	std::vector<VkCommandBuffer> graphics_command_buffers_;

	std::map<std::string, std::shared_ptr<ShaderModule>> shaders_;
	std::shared_ptr<ParticleEmitter> rain_drops_emitter_;
	std::unique_ptr<SceneManager> scene_manager_;
	RenderPass render_pass_;
	Pipeline alley_graphics_pipeline_;
	Pipeline rain_graphics_pipeline_;

	// options
	float camera_fov_deg_ = 45.0f;
	
};

// Implementation

bool RainyAlley::loadAssets() {
	graphics_command_buffers_ = vulkan_backend_.createPrimaryCommandBuffers(vulkan_backend_.getSwapChainSize());
	
	auto alley_vertex_shader = vulkan_backend_.createShaderModule("alley_vs");
	alley_vertex_shader->loadSpirvShader("shaders/alley_vs.spv");

	if (!alley_vertex_shader->isVertexFormatCompatible(Vertex::getFormatInfo())) {
		std::cerr << "Vertex format is not compatible with pipeline input for " << alley_vertex_shader->getName() << std::endl;
		return false;
	}

	auto alley_fragment_shader = vulkan_backend_.createShaderModule("alley_fs");
	alley_fragment_shader->loadSpirvShader("shaders/alley_fs.spv");

	if (!alley_vertex_shader->isValid() || !alley_fragment_shader->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

	shaders_[alley_vertex_shader->getName()] = std::move(alley_vertex_shader);
	shaders_[alley_fragment_shader->getName()] = std::move(alley_fragment_shader);

	auto rain_vertex_shader = vulkan_backend_.createShaderModule("rain_drops_vs");
	rain_vertex_shader->loadSpirvShader("shaders/rain_drops_vs.spv");

	if (!rain_vertex_shader->isVertexFormatCompatible(ParticleVertex::getFormatInfo())) {
		std::cerr << "ParticleVertex format is not compatible with pipeline input for " << rain_vertex_shader->getName() << std::endl;
		return false;
	}

	auto rain_geometry_shader = vulkan_backend_.createShaderModule("rain_drops_gm");
	rain_geometry_shader->loadSpirvShader("shaders/rain_drops_gm.spv");

	auto rain_fragment_shader = vulkan_backend_.createShaderModule("rain_drops_fs");
	rain_fragment_shader->loadSpirvShader("shaders/rain_drops_fs.spv");

	if (!rain_vertex_shader->isValid() || !rain_geometry_shader->isValid() || !rain_fragment_shader->isValid()) {
		std::cerr << "Failed to validate rain drops shaders!" << std::endl;
		return false;
	}

	shaders_[rain_vertex_shader->getName()] = std::move(rain_vertex_shader);
	shaders_[rain_geometry_shader->getName()] = std::move(rain_geometry_shader);
	shaders_[rain_fragment_shader->getName()] = std::move(rain_fragment_shader);

	ParticleEmitterConfig emitter_config;
	emitter_config.name = "rain_drops_emitter";
	emitter_config.starting_transform = glm::identity<glm::mat4>();
	emitter_config.min_box_extent = glm::vec3(-10.0f, -8.0, 0.0f);
	emitter_config.max_box_extent = glm::vec3(2.0f, 8.0, 12.0f);
	emitter_config.min_starting_velocity = glm::vec3(0.0f, 0.0f, -10.0f);
	emitter_config.max_starting_velocity = glm::vec3(0.0f, 0.0f, 0.0f);

#ifndef NDEBUG
	vulkan_backend_.enableTimestampQueries(8);
	emitter_config.profile = true;
	emitter_config.start_query_num = 0;
	emitter_config.stop_query_num = 1;
#endif
	 
	rain_drops_emitter_ = ParticleEmitter::createParticleEmitter(emitter_config, &vulkan_backend_);
	rain_drops_emitter_->createParticles(1000, "shaders/rainfall_cp.spv");

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_ = SceneManager::create(&vulkan_backend_);
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 1000.0f);
	scene_manager_->setCameraPosition(glm::vec3(-10.0f, 0.0f, 4.0f));
	scene_manager_->setCameraTarget(glm::vec3(0.0f, 0.0f, 2.0f));

	scene_manager_->loadFromGlb("meshes/alley.glb");

	imgui_renderer_ = ImGuiRenderer::create(&vulkan_backend_);
	imgui_renderer_->setUp(window_);

	return true;
}

bool RainyAlley::setupScene() {
	DescriptorPoolConfig pool_config;

	auto scene_pool = scene_manager_->getDescriptorsCount();
	auto emitter_pool = rain_drops_emitter_->getDescriptorsCount();
	auto ui_pool = imgui_renderer_->getDescriptorsCount();
	
	pool_config.uniform_buffers_count = scene_pool.uniform_buffers_count + emitter_pool.uniform_buffers_count + ui_pool.uniform_buffers_count;
	pool_config.image_samplers_count = scene_pool.image_samplers_count + emitter_pool.image_samplers_count + ui_pool.image_samplers_count;
	pool_config.storage_texel_buffers_count = emitter_pool.storage_texel_buffers_count;

	pool_config.uniform_buffers_count *= vulkan_backend_.getSwapChainSize();
	pool_config.image_samplers_count *= vulkan_backend_.getSwapChainSize();
	
	vulkan_backend_.createDescriptorPool(pool_config);

	RenderPassConfig render_pass_config;
	render_pass_config.name = "Main Pass";
	render_pass_config.msaa_samples = vulkan_backend_.getMaxMSAASamples();
	render_pass_config.store_depth = false;
	
	SubpassConfig alley_subpass;
	alley_subpass.use_colour_attachment = true;
	alley_subpass.use_depth_stencil_attachemnt = true;
	alley_subpass.src_dependency = SubpassConfig::Dependency::NONE;
	alley_subpass.dst_dependency = SubpassConfig::Dependency::COLOUR_ATTACHMENT;

	SubpassConfig rain_subpass;
	rain_subpass.use_colour_attachment = true;
	rain_subpass.use_depth_stencil_attachemnt = true;
	rain_subpass.src_dependency = SubpassConfig::Dependency::COLOUR_ATTACHMENT;
	rain_subpass.dst_dependency = SubpassConfig::Dependency::COLOUR_ATTACHMENT;

	SubpassConfig ui_subpass;
	ui_subpass.use_colour_attachment = true;
	ui_subpass.use_depth_stencil_attachemnt = false;
	ui_subpass.src_dependency = SubpassConfig::Dependency::COLOUR_ATTACHMENT;
	ui_subpass.dst_dependency = SubpassConfig::Dependency::NONE;
 
	render_pass_config.subpasses = { alley_subpass, rain_subpass, ui_subpass };

	render_pass_ = vulkan_backend_.createRenderPass(render_pass_config);

	if (render_pass_.vk_render_pass == VK_NULL_HANDLE) {
		return false;
	}

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_->setCameraProperties(camera_fov_deg_, extent.width / (float)extent.height, 0.1f, 1000.0f);

	if (!createGraphicsPipeline()) {
		return false;
	}
	if (!imgui_renderer_->createGraphicsPipeline(render_pass_, 2)) {
		return false;
	}

	drawUi();

	return true;
}

void RainyAlley::cleanupSwapChainAssets() {
	imgui_renderer_->cleanupGraphicsPipeline();

	scene_manager_->deleteUniformBuffer();
	
	rain_drops_emitter_->deleteUniformBuffer();

	vulkan_backend_.destroyRenderPass(render_pass_);
	vulkan_backend_.destroyPipeline(alley_graphics_pipeline_);
	vulkan_backend_.destroyPipeline(rain_graphics_pipeline_);
}

void RainyAlley::cleanup() {
	cleanupSwapChainAssets();
	imgui_renderer_->shutDown();
	vulkan_backend_.freeCommandBuffers(graphics_command_buffers_);
	rain_drops_emitter_.reset();
	scene_manager_.reset();
	shaders_.clear();
}

void RainyAlley::updateScene() {
	static auto time_last_call = std::chrono::steady_clock::now();

	auto time_now = std::chrono::steady_clock::now();
	auto delta_time_s = std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_last_call).count() * 1e-6f;
	time_last_call = time_now;

	scene_manager_->update();

	auto result = rain_drops_emitter_->update(delta_time_s);

	// dispatch is handled here instead of within rain_drops_emitter_
	// so that it's possible to stack multiple compute buffers in one queue.
	if (std::get<0>(result)) {
		vulkan_backend_.submitComputeCommands(std::get<1>(result));
	}

	drawUi();
}

bool RainyAlley::createGraphicsPipeline() {
	if (!createAlleyGraphicsPipeline()) {
		return false;
	}
	if (!createRainDropsPipeline()) {
		return false;
	}
	if (!rain_drops_emitter_->createComputePipeline()) {
		return false;
	}

	return true;
}

bool RainyAlley::createAlleyGraphicsPipeline() {
	GraphicsPipelineConfig config;
	config.name = "Alley Geometry";
	config.vertex = shaders_["alley_vs"];
	config.fragment = shaders_["alley_fs"];
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertex_buffer_binding_desc = shaders_["alley_vs"]->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = shaders_["alley_vs"]->getInputAttributes();
	config.render_pass = render_pass_;
	config.subpass_number = 0;

	alley_graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	scene_manager_->createUniformBuffer();
	scene_manager_->createDescriptorSets(alley_graphics_pipeline_.vk_descriptor_set_layouts);
	scene_manager_->updateDescriptorSets(alley_graphics_pipeline_.descriptor_metadata);

	return alley_graphics_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

bool RainyAlley::createRainDropsPipeline() {
	GraphicsPipelineConfig config;
	config.name = "Rain Drops GP";
	config.vertex = shaders_["rain_drops_vs"];
	config.geometry = shaders_["rain_drops_gm"];
	config.fragment = shaders_["rain_drops_fs"];
	config.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	config.vertex_buffer_binding_desc = shaders_["rain_drops_vs"]->getInputBindingDescription();
	config.vertex_buffer_attrib_desc = shaders_["rain_drops_vs"]->getInputAttributes();
	config.render_pass = render_pass_;
	config.subpass_number = 1;
	config.enableDepthTesting = true;
	config.enableTransparency = true;

	rain_graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	rain_drops_emitter_->createUniformBuffer();
	rain_drops_emitter_->createGraphicsDescriptorSets(rain_graphics_pipeline_.vk_descriptor_set_layouts);
	rain_drops_emitter_->updateGraphicsDescriptorSets(rain_graphics_pipeline_.descriptor_metadata);

	return rain_graphics_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

RecordCommandsResult RainyAlley::recordCommands(uint32_t swapchain_image) {
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

	VkDeviceSize offsets[] = { 0 };
	auto& scene_descriptors = scene_manager_->getDescriptorSets();
	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, alley_graphics_pipeline_.vk_pipeline_layout, SCENE_UNIFORM_SET_ID, 1, &scene_descriptors[swapchain_image], 0, nullptr);

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, alley_graphics_pipeline_.vk_pipeline);

	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 2); // does nothing if not in debug

	scene_manager_->drawGeometry(command_buffers[0], alley_graphics_pipeline_.vk_pipeline_layout, swapchain_image);

	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 3); // does nothing if not in debug

	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rain_graphics_pipeline_.vk_pipeline);

	auto& particles_descriptors = rain_drops_emitter_->getGraphicsDescriptorSets();
	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rain_graphics_pipeline_.vk_pipeline_layout, MODEL_UNIFORM_SET_ID, 1, &particles_descriptors[swapchain_image], 0, nullptr);

	vkCmdBindVertexBuffers(command_buffers[0], 0, 1, &rain_drops_emitter_->getVertexBuffer().vk_buffer, offsets);
	
	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 4); // does nothing if not in debug

	vkCmdDraw(command_buffers[0], rain_drops_emitter_->getVertexCount(), 1, 0, 0);
	
	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 5); // does nothing if not in debug

	// register UI overlay commands
	vkCmdNextSubpass(command_buffers[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	
	ImGuiProfileConfig ui_profile_config = { true, 6, 7 };
	auto commands = imgui_renderer_->recordCommands(swapchain_image, render_pass_info, ui_profile_config);
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(80 * high_dpi_scale, 80 * high_dpi_scale), ImVec2(100 * high_dpi_scale, 150 * high_dpi_scale));

	ImGui::Begin("Options");                   
	
	ImGui::End();

	auto extent = vulkan_backend_.getSwapChainExtent();
	auto stats_width = 220 * high_dpi_scale;
	auto stats_pos = extent.width - stats_width - 50;
	ImGui::SetNextWindowPos(ImVec2(stats_pos, 10));
	ImGui::SetNextWindowSizeConstraints(ImVec2(stats_width, 120 * high_dpi_scale), ImVec2(stats_width, 150 * high_dpi_scale));

	ImGui::Begin("Stats");

	ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Rain update time: %.4f ms", time_to_exec_compute);
	ImGui::Text("Alley draw time: %.4f ms", time_to_draw_geometry);
	ImGui::Text("Rain draw time: %.4f ms", time_to_draw_particles);
	ImGui::Text("UI draw time: %.4f ms", time_to_draw_ui);

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
