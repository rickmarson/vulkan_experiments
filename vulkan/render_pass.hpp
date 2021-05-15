/*
* render_pass.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "common_definitions.hpp"
#include <optional>

class VulkanBackend;

struct SubpassConfig {
    enum class DependencyType { NONE = 0, COLOUR_ATTACHMENT, FRAGMENT_SHADER, EARLY_FRAGMENT_TESTS, LATE_FRAGMENT_TESTS };
    struct Dependency {
        int32_t src_subpass = 0;  // -1 -> external
        int32_t dst_subpass = 0;
        DependencyType src_dependency = DependencyType::NONE;
        DependencyType dst_dependency = DependencyType::NONE;
    };
    bool use_colour_attachment = false;
    bool use_depth_stencil_attachemnt = false;
    std::list<Dependency> dependencies;
};

struct RenderPassConfig {
    std::optional<VkExtent2D> framebuffer_size = std::nullopt;
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    bool offscreen = false;
    bool has_colour = true;
    bool has_depth = true;
    bool store_depth = false;
    std::vector<SubpassConfig> subpasses;
};

class RenderPass final {
  public:
    ~RenderPass();

    bool buildRenderPass(const RenderPassConfig& config);

    const std::string& name() const { return name_; }
    const VkSampleCountFlagBits msaaSamples() const { return msaa_samples_; } 
    const VkViewport viewport() const { return viewport_; }
    const VkRect2D scissor() const { return scissor_; }
    const VkRenderPass handle() const { return vk_render_pass_; }
    std::shared_ptr<Texture> colourAttachment() const { return colour_attachment_; }
    std::shared_ptr<Texture> depthAttachment() const { return depth_attachment_; }
    const std::vector<VkFramebuffer> framebuffers() const { return framebuffers_; }

  private:
    friend class VulkanBackend;

    RenderPass(VkDevice device, VulkanBackend* backend, const std::string& name) :
        name_(name),
        backend_(backend),
        device_(device) {}

    std::string name_;
    VulkanBackend* backend_ = nullptr;
    VkDevice device_;
    VkSampleCountFlagBits msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    VkViewport viewport_ = {0, 0, 0, 0, 0, 1};
    VkRect2D scissor_ = {0, 0};
    VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;

    std::shared_ptr<Texture> colour_attachment_;
    std::shared_ptr<Texture> depth_attachment_;
    std::vector<VkFramebuffer> framebuffers_;
};