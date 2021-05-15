/*
* render_pass.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "render_pass.hpp"
#include "texture.hpp"
#include "vulkan_backend.hpp"

RenderPass::~RenderPass() {
    for (auto& framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();
    colour_attachment_.reset();
    depth_attachment_.reset();

    vkDestroyRenderPass(device_, vk_render_pass_, nullptr);
}

bool RenderPass::buildRenderPass(const RenderPassConfig& config) {
    // create multisampled colour attachment
    VkExtent2D extent;
    if (config.framebuffer_size.has_value()) {
        extent = config.framebuffer_size.value();
    } else {
        extent = backend_->getSwapChainExtent();
    }
    
    std::vector<VkAttachmentDescription> attachments;

    std::shared_ptr<Texture> colour_texture;
    uint32_t colour_attachment_idx = ~0;
    VkAttachmentReference color_attachment_ref{};
    if (config.has_colour) {
        // create colour attachment
        colour_texture = Texture::createTexture(name_ + "_colour_attachment", device_, backend_);
        colour_texture->createColourAttachment(extent.width, extent.height, backend_->swap_chain_image_format_, config.msaa_samples);

        VkAttachmentDescription color_attachment{};
        color_attachment.format =  backend_->swap_chain_image_format_;
        color_attachment.samples = config.msaa_samples;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = colour_texture->getImageLayout();
        color_attachment.finalLayout = colour_texture->getImageLayout();

        attachments.push_back(color_attachment);
        colour_attachment_idx = 0;
        color_attachment_ref.attachment = colour_attachment_idx;
        color_attachment_ref.layout = colour_texture->getImageLayout();
    }
    
    std::shared_ptr<Texture> depth_texture;
    uint32_t depth_attachment_idx = ~0;
    VkAttachmentReference depth_attachment_ref{};
    if (config.has_depth) {
        // create depth stencil attachment
        depth_texture = Texture::createTexture(name_ + "_depth_attachment", device_, backend_);
        depth_texture->createDepthStencilAttachment(extent.width, extent.height, config.msaa_samples, config.store_depth);
        
        VkAttachmentDescription depth_attachment{};
        depth_attachment.format = depth_texture->getFormat();
        depth_attachment.samples = config.msaa_samples;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = config.store_depth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = depth_texture->getImageLayout();
        depth_attachment.finalLayout = config.store_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : depth_texture->getImageLayout();

        attachments.push_back(depth_attachment);
        depth_attachment_idx = colour_attachment_idx == 0 ? 1 : 0;
        depth_attachment_ref.attachment = depth_attachment_idx;
        depth_attachment_ref.layout = depth_texture->getImageLayout();

        if (config.store_depth) {
            depth_texture->updateImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        }
    }

    uint32_t resolve_attachment_idx = ~0;
    VkAttachmentReference color_attachment_resolve_ref{};
    if (config.msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        VkAttachmentDescription color_attachment_resolve{};
        color_attachment_resolve.format =  backend_->swap_chain_image_format_;
        color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments.push_back(color_attachment_resolve);
        if (depth_attachment_idx >= 0) {
            resolve_attachment_idx = depth_attachment_idx + 1;
        } else if (colour_attachment_idx >= 0) {
            resolve_attachment_idx = colour_attachment_idx + 1;
        } else {
            resolve_attachment_idx = 0;
        }
        
        color_attachment_resolve_ref.attachment = resolve_attachment_idx;
        color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> subpass_dependencies;
    auto subpass_count = config.subpasses.size();
    subpasses.resize(subpass_count);

    for (auto i = 0; i < subpass_count; ++i) {
        auto& sub = config.subpasses[i];
        subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if (sub.use_colour_attachment && config.has_colour) {
            subpasses[i].colorAttachmentCount = 1;
            subpasses[i].pColorAttachments = &color_attachment_ref;
        } else {
            subpasses[i].colorAttachmentCount = 0;
            subpasses[i].pColorAttachments = nullptr;
        }
        
        if (sub.use_depth_stencil_attachemnt && config.has_depth) {
            subpasses[i].pDepthStencilAttachment = &depth_attachment_ref;
        } else {
            subpasses[i].pDepthStencilAttachment = nullptr;
        }

        if (i == config.subpasses.size() - 1 && config.msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
            subpasses[i].pResolveAttachments = &color_attachment_resolve_ref;
        } else {
            subpasses[i].pResolveAttachments = nullptr;
        }

        for (auto& dep : sub.dependencies) {      
            uint32_t src_subpass = dep.src_subpass >= 0 ? dep.src_subpass : VK_SUBPASS_EXTERNAL;
            uint32_t dst_subpass = dep.dst_subpass >= 0 ? dep.dst_subpass : VK_SUBPASS_EXTERNAL;

            VkPipelineStageFlags src_stage;
            VkPipelineStageFlags dst_stage;
            VkAccessFlags src_access;
            VkAccessFlags dst_access;
            VkDependencyFlags flags = 0;

            switch (dep.src_dependency) {
                case SubpassConfig::DependencyType::NONE:
                    src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    src_access = 0;
                    break;
                case SubpassConfig::DependencyType::COLOUR_ATTACHMENT:
                    src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;
                case SubpassConfig::DependencyType::FRAGMENT_SHADER:
                    src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    src_access = VK_ACCESS_SHADER_READ_BIT;
                    break;
                case SubpassConfig::DependencyType::LATE_FRAGMENT_TESTS:
                    src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    break;
            }

            switch (dep.dst_dependency) {
                case SubpassConfig::DependencyType::NONE:
                    dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;
                case SubpassConfig::DependencyType::COLOUR_ATTACHMENT:
                    dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;
                case SubpassConfig::DependencyType::FRAGMENT_SHADER:
                    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    dst_access = VK_ACCESS_SHADER_READ_BIT;
                    flags = VK_DEPENDENCY_BY_REGION_BIT;
                    break;
                case SubpassConfig::DependencyType::EARLY_FRAGMENT_TESTS:
                    dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    flags = VK_DEPENDENCY_BY_REGION_BIT;
                    break;
            }
        
            VkSubpassDependency dependency;
            dependency.srcSubpass = src_subpass;
            dependency.dstSubpass = dst_subpass;
            dependency.srcStageMask = src_stage;
            dependency.srcAccessMask = src_access;
            dependency.dstStageMask = dst_stage;
            dependency.dstAccessMask = dst_access;
            dependency.dependencyFlags = flags;
            subpass_dependencies.push_back(dependency);
        }
    }

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());;
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
    render_pass_info.pSubpasses = subpasses.data();
    render_pass_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
    render_pass_info.pDependencies = subpass_dependencies.data();

    VkRenderPass vk_render_pass;
    if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &vk_render_pass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass!" << std::endl;
        return false;
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    msaa_samples_ = config.msaa_samples;
    viewport_ = viewport;
    scissor_ = scissor;
    vk_render_pass_ = vk_render_pass;
    colour_attachment_ = std::move(colour_texture);
    depth_attachment_ = std::move(depth_texture);

    // create the framebuffers for this render pass
    if (!config.offscreen) {
        framebuffers_.resize( backend_->swap_chain_image_views_.size());

        for (size_t i = 0; i <  backend_->swap_chain_image_views_.size(); i++) {
            std::vector<VkImageView> framebuffer_attachments;
            if (config.has_colour) {
                framebuffer_attachments.push_back(colour_attachment_->getImageView());
            }
            if (config.has_depth) {
                framebuffer_attachments.push_back(depth_attachment_->getImageView());
            }
            framebuffer_attachments.push_back(backend_->swap_chain_image_views_[i]); 

            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = vk_render_pass_;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(framebuffer_attachments.size());
            framebuffer_info.pAttachments = framebuffer_attachments.data();
            framebuffer_info.width = extent.width;
            framebuffer_info.height = extent.height;
            framebuffer_info.layers = 1;

            if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
                std::cerr << "Failed to create framebuffer!" << std::endl;
                // TODO cleanup
                return false;
            }
        }
    } else {
        framebuffers_.resize(1);

        std::vector<VkImageView> framebuffer_attachments;
        if (config.has_colour) {
            framebuffer_attachments.push_back(colour_attachment_->getImageView());
        }
        if (config.has_depth) {
            framebuffer_attachments.push_back(depth_attachment_->getImageView());
        }
      
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk_render_pass_;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(framebuffer_attachments.size());
        framebuffer_info.pAttachments = framebuffer_attachments.data();
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[0]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer!" << std::endl;
            // TODO cleanup
            return false;
        }
    }

    return true;
}
