/*
* texture.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "texture.hpp"
#include "vulkan_backend.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


namespace {
    bool isFormatSupported(VkPhysicalDevice physical_device, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return true;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return true;
        }

        return false;
    }
}


std::shared_ptr<Texture> Texture::createTexture(const std::string& name, VkDevice device, VulkanBackend* backend) {
	return std::make_shared<Texture>(name, device, backend);
}

Texture::Texture(const std::string& name, VkDevice device, VulkanBackend* backend) :
	name_(name),
    backend_(backend),
	device_(device) { }

Texture::~Texture() {
    cleanup();
}

void Texture::cleanup() {
    if (isValid()) {
        if (vk_sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, vk_sampler_, nullptr);
        }
        vkDestroyImageView(device_, vk_image_view_, nullptr);
        if (vk_sampler_image_view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, vk_sampler_image_view_, nullptr);
        }
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }
}

void Texture::loadImageRGBA(const std::string& src_image_path, bool genMipMaps, bool srgb) {
    int width, height, original_channels;
    stbi_uc* stb_pixels = stbi_load(src_image_path.c_str(), &width, &height, &original_channels, STBI_rgb_alpha);

    if (!stb_pixels) {
        std::cerr << "Failed to load texture image!" << std::endl;
        return;
    }

    uint32_t channels = STBI_rgb_alpha;
    size_t bytes = static_cast<size_t>(width)* height * channels;

    auto pixels = std::vector<stbi_uc>(stb_pixels, stb_pixels + bytes);
    stbi_image_free(stb_pixels);

    loadImageRGBA(static_cast<uint32_t>(width), 
                  static_cast<uint32_t>(height), 
                  static_cast<uint32_t>(channels),
                  genMipMaps,
                  pixels,
                  srgb);
}

void Texture::loadImageRGBA(uint32_t width, uint32_t height, bool genMipMaps, glm::vec4 fill_colour, bool srgb) {
    auto pixels = std::vector<uint8_t>(width * height * 4, 0);
    uint8_t fill_bytes[] = { static_cast<uint8_t>(fill_colour[0] * 255), 
                            static_cast<uint8_t>(fill_colour[1] * 255),
                            static_cast<uint8_t>(fill_colour[2] * 255),
                            static_cast<uint8_t>(fill_colour[3] * 255)};
    size_t offset = 0;
    for (auto px = 0; px < width * height; ++px) {
        std::memcpy(pixels.data() + offset, fill_bytes, 4);
        offset += 4;
    }

    loadImageRGBA(width, 
                  height, 
                  4,
                  genMipMaps,
                  pixels,
                  srgb);
}

void Texture::loadImageRGBA(uint32_t width, uint32_t height, uint32_t channels, bool genMipMaps, const std::vector<unsigned char>& pixels, bool srgb) {
     VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

     if (!isFormatSupported(backend_->getPhysicalDevice(), 
                           format, 
                           VK_IMAGE_TILING_OPTIMAL, 
                           VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        std::cerr << "Error creating texture " << name_ << std::endl;
        std::cerr << (srgb ? "VK_FORMAT_R8G8B8A8_SRGB" : "VK_FORMAT_R8G8B8A8_UNORM") << " texture format is not supported on the selected device!" << std::endl;
        return;
    }
    
    width_ = width;
    height_ = height;
    channels_ = channels;
    mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    vk_format_ = format;
    vk_usage_flags_ = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vk_num_samples_ = VK_SAMPLE_COUNT_1_BIT;

    Buffer staging_buffer = backend_->createBuffer<stbi_uc>("image_staging_buffer", pixels, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true);
    backend_->updateBuffer<stbi_uc>(staging_buffer, pixels);

    if (!createImage()) {
        vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        return;
    }

    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(staging_buffer.vk_buffer, vk_image_, width_, height_);
    
    if (genMipMaps) {
        generateMipMaps();
    }
    else {
        transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    vk_image_view_ = backend_->createImageView(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels_);

    if (vk_image_view_ == VK_NULL_HANDLE) {
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }

    vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
    vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
}

void Texture::createColourAttachment(uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits num_samples, bool enable_sampling) {
    if (!isFormatSupported(backend_->getPhysicalDevice(),
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
        std::cerr << "Error loading texture " << name_ << std::endl;
        std::cerr << "VK_FORMAT_R8G8B8A8_SRGB texture format is not supported on the selected device!" << std::endl;
        return;
    }

    width_ = width;
    height_ = height;
    channels_ = 4;
    mip_levels_ = 1;
    vk_format_ = format;
    vk_usage_flags_ = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vk_num_samples_ = num_samples;

    if (!createImage()) {
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        return;
    }

    vk_image_view_ = backend_->createImageView(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT);

    if (vk_image_view_ == VK_NULL_HANDLE) {
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }

    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (enable_sampling) {
        createSampler();
    }
}

void Texture::createDepthStencilAttachment(uint32_t width, uint32_t height, VkSampleCountFlagBits num_samples, bool enable_sampling) {
    if (!isFormatSupported(backend_->getPhysicalDevice(),
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        std::cerr << "Error creating depth attachment image" << std::endl;
        std::cerr << "VK_FORMAT_D24_UNORM_S8_UINT texture format is not supported on the selected device!" << std::endl;
        return;
    }

    VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (enable_sampling) {
     usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    width_ = width;
    height_ = height;
    channels_ = 1;
    mip_levels_ = 1;
    vk_format_ = VK_FORMAT_D24_UNORM_S8_UINT;
    vk_usage_flags_ = usage_flags;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vk_num_samples_ = num_samples;

    if (!createImage()) {
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        return;
    }

    vk_image_view_ = backend_->createImageView(vk_image_, vk_format_, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    if (vk_image_view_ == VK_NULL_HANDLE) {
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }

    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    if (enable_sampling) {
        createSampler();

        vk_sampler_image_view_ = backend_->createImageView(vk_image_, vk_format_, VK_IMAGE_ASPECT_DEPTH_BIT);

        if (vk_sampler_image_view_ == VK_NULL_HANDLE) {
            vkDestroyImageView(device_, vk_image_view_, nullptr);
            vkDestroyImage(device_, vk_image_, nullptr);
            vkFreeMemory(device_, vk_memory_, nullptr);   
        }
    }
}

void Texture::createDepthStorageImage(uint32_t width, uint32_t height, bool as_rgba32) {
    VkFormat format = as_rgba32 ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R32_SFLOAT;

    if (!isFormatSupported(backend_->getPhysicalDevice(),
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
        std::cerr << "Error creating depth storage image" << std::endl;
        std::cerr << "VK_FORMAT_R32_SFLOAT texture format is not supported as STORAGE_IMAGE on the selected device!" << std::endl;
        return;
    }

    width_ = width;
    height_ = height;
    channels_ = 1;
    mip_levels_ = 1;
    vk_format_ = format;
    vk_usage_flags_ = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vk_descriptor_type_ = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    if (!createImage()) {
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        return;
    }

    vk_image_view_ = backend_->createImageView(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT);

    if (vk_image_view_ == VK_NULL_HANDLE) {
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }

    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    auto cmd_buffer = backend_->beginSingleTimeCommands();
    VkClearColorValue clear_colour{};
    clear_colour.float32[0] = 1.0f;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseArrayLayer = 0;
    range.baseMipLevel = 0;
    range.layerCount = 1;
    range.levelCount = 1;
    vkCmdClearColorImage(cmd_buffer, vk_image_, VK_IMAGE_LAYOUT_GENERAL, &clear_colour, 1, &range);
    backend_->endSingleTimeCommands(cmd_buffer);
}

VkImageView Texture::getSamplerImageView() const {
    if (vk_sampler_image_view_ != VK_NULL_HANDLE) {
        return vk_sampler_image_view_;
    }
    return vk_image_view_;
}

bool Texture::createImage() {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width_;
    image_info.extent.height = height_;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_levels_;
    image_info.arrayLayers = 1;
    image_info.format = vk_format_;
    image_info.tiling =  vk_tiling_;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = vk_usage_flags_;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = vk_num_samples_;
    image_info.flags = 0; // Optional

    VkImageFormatProperties image_format_props{};
    vkGetPhysicalDeviceImageFormatProperties(backend_->physical_device_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D, vk_tiling_, vk_usage_flags_, 0, &image_format_props);

    if (vkCreateImage(device_, &image_info, nullptr, &vk_image_) != VK_SUCCESS) {
        std::cerr << "Failed to create image!" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, vk_image_, &mem_reqs);

    vk_memory_ = backend_->allocateDeviceMemory(mem_reqs, vk_mem_props_);

    if (vk_memory_ == VK_NULL_HANDLE) {
        std::cerr << "Failed to allocate image memory!" << std::endl;
        return false;
    }

    vkBindImageMemory(device_, vk_image_, vk_memory_, 0);

    return true;
}

void Texture::transitionImageLayout(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer command_buffer = backend_->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_flags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels_;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        std::cerr << "Unsupported layout transition!" << std::endl;
        return;
    }

    vkCmdPipelineBarrier(
        command_buffer,
        src_stage, dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    backend_->endSingleTimeCommands(command_buffer);

    vk_layout_ = new_layout;
}

void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer command_buffer = backend_->beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    backend_->endSingleTimeCommands(command_buffer);
}

void Texture::generateMipMaps() {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(backend_->getPhysicalDevice(), vk_format_, &format_properties);

    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        std::cerr << "Texture image format does not support linear blitting!" << std::endl;
        return;
    }

    VkCommandBuffer command_buffer = backend_->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = vk_image_;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = width_;
    int32_t mip_height = height_;

    for (uint32_t i = 1; i < mip_levels_; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mip_width, mip_height, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(command_buffer,
            vk_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vk_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels_ - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    backend_->endSingleTimeCommands(command_buffer);

    vk_layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void Texture::createSampler() {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = static_cast<float>(mip_levels_);

    if (vkCreateSampler(device_, &sampler_info, nullptr, &vk_sampler_) != VK_SUCCESS) {
        std::cerr << "Failed to create texture sampler!" << std::endl;
    }
}

void Texture::updateDescriptorSets(std::vector<VkDescriptorSet>& descriptor_sets, uint32_t binding_point) {
    VkImageView image_view;
    if (vk_sampler_ != VK_NULL_HANDLE && vk_sampler_image_view_ != VK_NULL_HANDLE) {
        image_view = vk_sampler_image_view_;
    } else {
        image_view = vk_image_view_;
    }

    for (size_t i = 0; i < descriptor_sets.size(); i++) {
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = vk_layout_;
        image_info.imageView = image_view;
        image_info.sampler = vk_sampler_;

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = binding_point;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = vk_descriptor_type_;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = nullptr; // Optional
        descriptor_write.pImageInfo = &image_info; 
        descriptor_write.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
    }
}
