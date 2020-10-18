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
	// std::map<std::string, std::shared_ptr<Mesh>> meshes_;
	std::unique_ptr<SceneManager> scene_manager_;
	RenderPass alley_render_pass_;
	RenderPass rain_render_pass_;
	// Pipeline alley_graphics_pipeline_;
	Pipeline rain_graphics_pipeline_;

	// options
	
};

// Implementation

bool RainyAlley::loadAssets() {
	graphics_command_buffers_ = vulkan_backend_.createPrimaryCommandBuffers(vulkan_backend_.getSwapChainSize());
	
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

	rain_drops_emitter_ = ParticleEmitter::createParticleEmitter("rain_drops_emitter", &vulkan_backend_);
	rain_drops_emitter_->createParticles(1000, "shaders/rainfall_cp.spv");

	auto extent = vulkan_backend_.getSwapChainExtent();
	scene_manager_ = SceneManager::create(&vulkan_backend_);
	scene_manager_->setCameraProperties(90.0f, extent.width / (float)extent.height, 0.1f, 10.0f);
	scene_manager_->setCameraPosition(glm::vec3(-1.0f, -1.0f, 0.0f));
	scene_manager_->setCameraTarget(glm::vec3(2.0f, 2.0f, 2.0f));

	imgui_renderer_ = ImGuiRenderer::create(&vulkan_backend_);
	imgui_renderer_->setUp(window_);

#ifndef NDEBUG
	vulkan_backend_.enableTimestampQueries(2);
#endif

	return true;
}

bool RainyAlley::setupScene() {
	DescriptorPoolConfig pool_config;
	pool_config.uniform_buffers_count = 2 * vulkan_backend_.getSwapChainSize();
	pool_config.image_samplers_count = 2 * vulkan_backend_.getSwapChainSize();
	pool_config.storage_texel_buffers_count = 1;
	vulkan_backend_.createDescriptorPool(pool_config);

	RenderPassConfig render_pass_config;
	render_pass_config.name = "Rain Drops Pass";
	render_pass_config.msaa_samples = vulkan_backend_.getMaxMSAASamples();
	render_pass_config.store_depth = false;
	render_pass_config.enable_overlays = true;

	rain_render_pass_ = vulkan_backend_.createRenderPass(render_pass_config);

	if (rain_render_pass_.vk_render_pass == VK_NULL_HANDLE) {
		return false;
	}

	if (!createGraphicsPipeline()) {
		return false;
	}
	if (!imgui_renderer_->createGraphicsPipeline(rain_render_pass_, 1)) {
		return false;
	}

	drawUi();

	return true;
}

void RainyAlley::cleanupSwapChainAssets() {
	imgui_renderer_->cleanupGraphicsPipeline();

	scene_manager_->deleteUniformBuffer();
	
	rain_drops_emitter_->deleteUniformBuffer();

	vulkan_backend_.destroyRenderPass(rain_render_pass_);
	vulkan_backend_.destroyPipeline(rain_graphics_pipeline_);
}

void RainyAlley::cleanup() {
	cleanupSwapChainAssets();
	imgui_renderer_->shutDown();
	vulkan_backend_.freeCommandBuffers(graphics_command_buffers_);
	rain_drops_emitter_.reset();
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
	return true;
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
	config.render_pass = rain_render_pass_;
	config.subpass_number = 0;

	rain_graphics_pipeline_ = vulkan_backend_.createGraphicsPipeline(config);

	scene_manager_->createUniformBuffer();
	scene_manager_->createDescriptorSets(rain_graphics_pipeline_.vk_descriptor_set_layouts);
	scene_manager_->updateDescriptorSets(rain_graphics_pipeline_.descriptor_metadata);

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

	vulkan_backend_.resetTimestampQueries(command_buffers[0]);

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = rain_render_pass_.vk_render_pass;
	render_pass_info.framebuffer = rain_render_pass_.swap_chain_framebuffers[swapchain_image];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = vulkan_backend_.getSwapChainExtent();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(command_buffers[0], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rain_graphics_pipeline_.vk_pipeline);

	VkDeviceSize offsets[] = { 0 };
	auto& scene_descriptors = scene_manager_->getDescriptorSets();

	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rain_graphics_pipeline_.vk_pipeline_layout, SCENE_UNIFORM_SET_ID, 1, &scene_descriptors[swapchain_image], 0, nullptr);
	
	auto& particles_descriptors = rain_drops_emitter_->getGraphicsDescriptorSets();
	vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rain_graphics_pipeline_.vk_pipeline_layout, MODEL_UNIFORM_SET_ID, 1, &particles_descriptors[swapchain_image], 0, nullptr);

	vkCmdBindVertexBuffers(command_buffers[0], 0, 1, &rain_drops_emitter_->getVertexBuffer().vk_buffer, offsets);
	
	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0); // does nothing if not in debug

	vkCmdDraw(command_buffers[0], rain_drops_emitter_->getVertexCount(), 1, 0, 0);
	
	vulkan_backend_.writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1); // does nothing if not in debug

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

void RainyAlley::drawUi() {

	imgui_renderer_->beginFrame();

	ImGui::Begin("Options");                   

	
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
