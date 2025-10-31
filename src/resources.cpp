#include "resources.h"

uint32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProperties,
    uint32_t memoryTypeBits, VkMemoryPropertyFlags flags){
    for(uint32_t i =0; i<memoryProperties.memoryTypeCount; i++){
        // memoryTypeBits is a bitmask
        // its an unsigned 32 bit value and each bit is a "memory type index"
        // we shift left i amount of types to check our current memory index properties
        // if it returns 0 that memory type is not available for us
        // if true then we determine if that index has the property flags that we want
        if((memoryTypeBits & (1 << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags){
            return i; // return the hopefully valid memory index
        }
    }

    // if not found force an assert and return max int
    assert(!"Unable to find compatible memory type");
    return ~0u;
}

void createBuffer(Buffer &result, VkDevice device, const VkPhysicalDeviceMemoryProperties& memoryProperties,
    size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags){
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    uint32_t memoryTypeIndex = selectMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryFlags);
    assert(memoryTypeIndex != ~0u); // if uint max returned no memory available

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkMemoryAllocateFlagsInfo flagInfo{};
    flagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

    if(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT){
        allocInfo.pNext = &flagInfo;
        flagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        flagInfo.deviceMask = 1;
    }

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &memory));
    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

    void* data = 0;
    if(memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT){
        VK_CHECK(vkMapMemory(device, memory, 0, size, 0, &data));
    }

    result.buffer = buffer;
    result.memory = memory;
    result.data = data;
    result.size = size;
}

void destroyBuffer(const Buffer& buffer, VkDevice device){
    vkDestroyBuffer(device, buffer.buffer, 0);
    vkFreeMemory(device, buffer.memory, 0);
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, uint32_t mipLevel,
    uint32_t levelCount){
    VkImageAspectFlags aspectMask = (format == VK_FORMAT_D32_SFLOAT) ?
        VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspectMask;
	createInfo.subresourceRange.baseMipLevel = mipLevel;
	createInfo.subresourceRange.levelCount = levelCount;
	createInfo.subresourceRange.layerCount = 1;
    createInfo.image = image;

    VkImageView view = 0;
	VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));

	return view;
}

void createImage(Image &result, VkDevice device, const VkPhysicalDeviceMemoryProperties &MemoryProperties,
uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage){
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = {width, height, 1};
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = usage;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VK_CHECK_FORCE(vkCreateImage(device, &createInfo, 0, &image));

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(device, image, &MemoryRequirements);

    uint32_t memoryTypeIndex = selectMemoryType(MemoryProperties, MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    assert(memoryTypeIndex != ~0u);
    std::cout << "memIndex: " << memoryTypeIndex << std::endl;
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = MemoryRequirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

    VK_CHECK(vkBindImageMemory(device, image, memory, 0));

    result.image = image;
    result.imageView = createImageView(device, image, format, 0, mipLevels);
    result.memory = memory;
}

void destroyImage(const Image& image, VkDevice device){
    vkDestroyImageView(device, image.imageView, 0);
    vkDestroyImage(device, image.image, 0);
    vkFreeMemory(device, image.memory, 0);
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex){
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = familyIndex;

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

    return commandPool;
}

void createCommandBuffer(VkDevice device, VkCommandPool commandPool,VkCommandBuffer &commandBuffer){
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
}

VkSemaphore createSemaphore(VkDevice device){
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore = 0;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

    return semaphore;
}

VkFence createFence(VkDevice device){
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // change later

    VkFence fence = 0;
    VK_CHECK(vkCreateFence(device, &createInfo, 0, &fence));

    return fence;
}

VkSampler createSampler(VkDevice device, VkFilter filter, VkSamplerMipmapMode mipmapMode,
     VkSamplerAddressMode addressMode){
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = filter;
	createInfo.minFilter = filter;
	createInfo.mipmapMode = mipmapMode;
	createInfo.addressModeU = addressMode;
	createInfo.addressModeV = addressMode;
	createInfo.addressModeW = addressMode;
	createInfo.minLod = 0.f;
	createInfo.maxLod = 11.f;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR ? 4.f : 1.f;

	VkSampler sampler = 0;
	VK_CHECK(vkCreateSampler(device, &createInfo, 0, &sampler));
	return sampler;
}

uint32_t getImageMipLevels(uint32_t width, uint32_t height){
    uint32_t result =1;
    while(width>1||height>1){
        result++;
        width /=2;
        height /=2;
    }

    return result;
}
