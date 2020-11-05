/*
* texture.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/


#pragma once

#include "common_definitions.hpp"

class VulkanBackend;

class Texture {
public:
    Texture(const std::string& name, VkDevice device, VulkanBackend* backend);
    ~Texture();

    static std::shared_ptr<Texture> createTexture(const std::string& name, VkDevice device, VulkanBackend* backend);

    const std::string& getName() const { return name_; }
    void loadImageRGBA(const std::string& src_image_path, bool generateMipMaps = true);
    void loadImageRGBA(uint32_t width, uint32_t height, uint32_t channels, bool genMipMaps, const std::vector<unsigned char>& pixels);
    void createColourAttachment(uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits num_samples);
    void createDepthStencilAttachment(uint32_t width, uint32_t height, VkSampleCountFlagBits num_samples);
    void createDepthStorageImage(uint32_t width, uint32_t height);
    bool isValid() const { return vk_image_ != VK_NULL_HANDLE && vk_memory_ != VK_NULL_HANDLE && vk_image_view_ != VK_NULL_HANDLE; }

    void createSampler();
    bool hasValidSampler() const { return vk_sampler_ != VK_NULL_HANDLE; }
    void updateDescriptorSets(std::vector<VkDescriptorSet>& descriptor_sets, uint32_t binding_point);

    VkFormat getFormat() const { return vk_format_; }
    VkImage getImage() const { return vk_image_; }
    VkImageView getImageView() const { return vk_image_view_; }

private:
    bool createImage();
    void cleanup();

    void transitionImageLayout(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkImageLayout old_layout, VkImageLayout new_layout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipMaps();

    std::string name_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t channels_ = 0;
    uint32_t mip_levels_ = 1;
    
    VulkanBackend* backend_;

    VkDevice device_;
    VkImage vk_image_ = VK_NULL_HANDLE;
    VkFormat vk_format_ = VK_FORMAT_UNDEFINED;
    VkImageLayout vk_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDescriptorType vk_descriptor_type_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    VkImageTiling vk_tiling_ = VK_IMAGE_TILING_MAX_ENUM;
    VkMemoryPropertyFlags vk_mem_props_ = VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
    VkImageUsageFlags vk_usage_flags_ = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
    VkSampleCountFlagBits vk_num_samples_ = VK_SAMPLE_COUNT_1_BIT;
    VkDeviceMemory vk_memory_ = VK_NULL_HANDLE;
    VkImageView vk_image_view_ = VK_NULL_HANDLE;
    VkSampler vk_sampler_ = VK_NULL_HANDLE;
};