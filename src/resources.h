#pragma once

#include "common.h"

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

struct Image{
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory memory;
};

uint32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProperties,
    uint32_t memoryTypeBits, VkMemoryPropertyFlags flags);

void createBuffer(Buffer &result, VkDevice device, const VkPhysicalDeviceMemoryProperties& memoryProperties,
    size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags);

void destroyBuffer(const Buffer& buffer, VkDevice device);

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, uint32_t mipLevel,
    uint32_t levelCount);

void createImage(Image &result, VkDevice device, const VkPhysicalDeviceMemoryProperties &MemoryProperties,
    uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage);

void destroyImage(const Image& image, VkDevice device);

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex);

void createCommandBuffer(VkDevice device, VkCommandPool commandPool,VkCommandBuffer &commandBuffer);

VkSemaphore createSemaphore(VkDevice device);

VkFence createFence(VkDevice device);

VkSampler createSampler(VkDevice device, VkFilter filter, VkSamplerMipmapMode mipmapMode,
     VkSamplerAddressMode addressMode);

uint32_t getImageMipLevels(uint32_t width, uint32_t height);
