/*
* texture.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "texture.hpp"
#include "vulkan_backend.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
        vkDestroySampler(device_, vk_sampler_, nullptr);
        vkDestroyImageView(device_, vk_image_view_, nullptr);
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }
}

void Texture::loadImageRGBA(const std::string& src_image_path) {
    int width, height, channels;
    stbi_uc* stb_pixels = stbi_load(src_image_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!stb_pixels) {
        std::cerr << "Failed to load texture image!" << std::endl;
        return;
    }

    std::vector<stbi_uc> pixels;

    if (channels == 3) {
        pixels.resize(width * height * 4, 0);
        for (size_t px = 0; px < width * height * 3; ++px) {
            pixels[px] = stb_pixels[px];
            pixels[px+1] = stb_pixels[px+1];
            pixels[px+2] = stb_pixels[px+2];
            pixels[px+3] = 255;
        }
        channels = 4;
    }
    else {
        pixels = std::vector<stbi_uc>(stb_pixels, stb_pixels + width * height * channels);
    }

    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    channels_ = static_cast<uint32_t>(channels);
    VkDeviceSize image_size = static_cast<uint64_t>(width)* height* channels;
    vk_format_ = VK_FORMAT_R8G8B8A8_SRGB;
    vk_usage_flags_ = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    Buffer staging_buffer = backend_->createBuffer<stbi_uc>("image_staging_buffer", pixels, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true, false);
    backend_->updateBuffer<stbi_uc>(staging_buffer, pixels);

    pixels.clear();

    if (!createImage()) {
        vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        stbi_image_free(stb_pixels);
        return;
    }

    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(staging_buffer.vk_buffer, vk_image_, width_, height_);
    transitionImageLayout(vk_image_, vk_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk_image_view_ = backend_->createImageView(vk_image_, vk_format_);

    if (vk_image_view_ == VK_NULL_HANDLE) {
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }

    vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
    vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
    stbi_image_free(stb_pixels);
}

bool Texture::createImage() {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width_;
    image_info.extent.height = height_;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = vk_format_;
    image_info.tiling =  vk_tiling_;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = vk_usage_flags_;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
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

void Texture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer command_buffer = backend_->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
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
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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
    sampler_info.maxLod = 0.0f;

    if (vkCreateSampler(device_, &sampler_info, nullptr, &vk_sampler_) != VK_SUCCESS) {
        std::cerr << "Failed to create texture sampler!" << std::endl;
    }
}

void Texture::updateDescriptorSets(std::vector<VkDescriptorSet>& descriptor_sets, uint32_t binding_point) {
    for (size_t i = 0; i < backend_->getSwapChainSize(); i++) {
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = vk_image_view_;
        image_info.sampler = vk_sampler_;

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = binding_point;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = nullptr; // Optional
        descriptor_write.pImageInfo = &image_info; 
        descriptor_write.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
    }
}
