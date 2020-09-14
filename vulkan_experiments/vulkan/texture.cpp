/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "texture.hpp"
#include "vulkan_backend.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>


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
        vkDestroyImage(device_, vk_image_, nullptr);
        vkFreeMemory(device_, vk_memory_, nullptr);
    }
}

void Texture::loadImageRGBA(const std::string& src_image_path) {
    int width, height, channels;
    stbi_uc* stb_pixels = stbi_load("textures/texture.jpg", &width, &height, &channels, STBI_rgb_alpha);
    VkDeviceSize image_size = static_cast<uint64_t>(width) * height * channels;

    if (!stb_pixels) {
        std::cerr << "failed to load texture image!" << std::endl;
        return;
    }

    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    channels_ = static_cast<uint32_t>(channels);
    vk_format_ = VK_FORMAT_R8G8B8A8_SRGB;
    vk_usage_flags_ = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vk_tiling_ = VK_IMAGE_TILING_OPTIMAL;
    vk_mem_props_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    std::vector<stbi_uc> pixels(stb_pixels, stb_pixels + image_size);
    stbi_image_free(stb_pixels);

    Buffer staging_buffer = backend_->createBuffer<stbi_uc>("image_staging_buffer", pixels, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true, false);
    backend_->updateBuffer<stbi_uc>(staging_buffer, pixels);

    pixels.clear();

    if (!createImage()) {
        vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
        if (vk_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, vk_image_, nullptr);
        }
        return;
    }
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

    if (vkCreateImage(device_, &image_info, nullptr, &vk_image_) != VK_SUCCESS) {
        std::cerr << "Failed to create image!" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, vk_image_, &mem_reqs);

    VkDeviceMemory memory = backend_->allocateDeviceMemory(mem_reqs, vk_mem_props_);

    if (memory == VK_NULL_HANDLE) {
        std::cerr << "Failed to allocate image memory!" << std::endl;
        return false;
    }

    vkBindImageMemory(device_, vk_image_, vk_memory_, 0);
}