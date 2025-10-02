#include "common.h"
#include "device.h"
#include "swapchain.h"
#include "program.h"
#include <fast_obj.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

#define DEVICE_COUNT 16
#define MAX_FRAMES_IN_FLIGHT 2

#define VK_CHECK_FORCE(call) \
	do \
	{ \
		VkResult result_ = call; \
		if (result_ != VK_SUCCESS) \
		{ \
			fprintf(stderr, "%s:%d: %s failed with error %d\n", __FILE__, __LINE__, #call, result_); \
			abort(); \
		} \
	} while (0)

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

struct alignas(16) Camera{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};  

struct Image{
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory memory;
};

// const std::vector<Vertex> vertices = {
//     {{0.0f,-0.5f, 0.5f},{1.0f,0.0f,0.0f}},
//     {{0.5f,0.5f, 0.0f},{0.0f,1.0f,0.0f}},
//     {{-0.5f,0.5f, 0.5f},{0.0f,0.0f,1.0f}},
// };

// const std::vector<uint16_t> indices = {
//     0,1,2,2,3,0
// };

struct VertexHash{
    size_t operator()(const Vertex& ver) const noexcept{
        auto h1 = std::hash<float>{}(ver.pos.x);
        auto h2 = std::hash<float>{}(ver.pos.y);
        auto h3 = std::hash<float>{}(ver.pos.z);

        auto h4 = std::hash<float>{}(ver.color.x);
        auto h5 = std::hash<float>{}(ver.color.y);
        auto h6 = std::hash<float>{}(ver.color.z);

        auto h7 = std::hash<float>{}(ver.texCoord.x);
        auto h8 = std::hash<float>{}(ver.texCoord.y);

        size_t seed = 0;
        auto hashCombine = [&seed](size_t h) {
            seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        hashCombine(h1);
        hashCombine(h2);
        hashCombine(h3);
        hashCombine(h4);
        hashCombine(h5);
        hashCombine(h6);
        hashCombine(h7);
        hashCombine(h8);

        return seed;
    }
};

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

void createImageViews(VkDevice device, Swapchain swapchain, VkFormat format, std::vector<VkImageView> &imageViews){
    imageViews.resize(swapchain.imageCount);

    for(size_t i=0;i<swapchain.images.size();i++){
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain.images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]));
    }
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

bool loadModel(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, const char* path){
    fastObjMesh* obj = fast_obj_read(path);
    if(!obj){
        printf("failed to load\n");
        return false;
    }

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices;

    // Vertex : Vec2 pos & Vec3 color
    for(unsigned int i=0;i<obj->face_count; i++){
        for(unsigned int j=0;j<obj->face_vertices[i]-2;j++){
            fastObjIndex indexX = obj->indices[indexOffset];
            fastObjIndex indexY = obj->indices[indexOffset+j+1];
            fastObjIndex indexZ = obj->indices[indexOffset+j+2];

            fastObjIndex triIdx[3] = {indexX,indexY,indexZ};

            for(unsigned k=0;k<3;k++){
                float px = obj->positions[3*triIdx[k].p+0];
                float py = obj->positions[3*triIdx[k].p+1];
                float pz = obj->positions[3*triIdx[k].p+2];

                glm::vec3 pos = {px,py,pz};
                glm::vec3 color = {1.0f,1.0f,0.0f};
                glm::vec2 texCoord = {0.0f,0.0f};

                if(triIdx[k].t != 0){
                    float u = obj->texcoords[2*triIdx[k].t+0];
                    float v = obj->texcoords[2*triIdx[k].t+1];
                    texCoord = {u,v};
                }

                Vertex vert = {pos,color,texCoord};

                if(!uniqueVertices.contains(vert)){
                    uint32_t idx = static_cast<uint32_t>(vertices.size());
                    uniqueVertices[vert] = idx;
                    vertices.push_back(vert); 
                }

                indices.push_back(uniqueVertices[vert]);
            }
        }
        indexOffset += obj->face_vertices[i];
    }

    return true;
}

VkSampler createSampler(VkDevice device, VkFilter filter, VkSamplerMipmapMode mipmapMode,
     VkSamplerAddressMode addressMode, VkSamplerReductionModeEXT reductionMode){
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = filter;
	createInfo.minFilter = filter;
	createInfo.mipmapMode = mipmapMode;
	createInfo.addressModeU = addressMode;
	createInfo.addressModeV = addressMode;
	createInfo.addressModeW = addressMode;
	createInfo.minLod = 0;
	createInfo.maxLod = 16.f;
	createInfo.anisotropyEnable = mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.maxAnisotropy = mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR ? 4.f : 1.f;

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT)
	{
		createInfoReduction.reductionMode = reductionMode;

		createInfo.pNext = &createInfoReduction;
	}

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

VkImageMemoryBarrier2 createImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
    VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask,
    VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount){
    VkImageMemoryBarrier2 result{};
    result.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    result.srcAccessMask = srcAccessMask;
    result.dstStageMask = dstStageMask;
    result.dstAccessMask = dstAccessMask;
    result.oldLayout = oldLayout;
    result.newLayout = newLayout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    result.subresourceRange.aspectMask = aspectMask;
	result.subresourceRange.baseMipLevel = baseMipLevel;
	result.subresourceRange.levelCount = levelCount;
	result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

int main(int argc, char *argv[]){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH,WINDOW_HEIGHT,"Cambion",nullptr,nullptr);
    assert(window);

    VkInstance instance = createInstance();
    VkSurfaceKHR surface = createSurface(instance, window);
    
    VkPhysicalDevice physicalDevices[DEVICE_COUNT];
    uint32_t physicalDeviceCount = DEVICE_COUNT;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = selectPhysicalDevice(physicalDevices,physicalDeviceCount,surface);

    bool raytracingSupported = false;
    bool unifiedlayoutsSupported = false;

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, 0));

    std::vector<VkExtensionProperties> extensionsCheck(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, extensionsCheck.data()));

    for(auto &ext : extensionsCheck){
        raytracingSupported = raytracingSupported || strcmp(ext.extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0;
		unifiedlayoutsSupported = unifiedlayoutsSupported || strcmp(ext.extensionName, "VK_KHR_unified_image_layouts") == 0;
    }

    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    VkDevice device = createDevice(instance, physicalDevice, familyIndex, raytracingSupported, unifiedlayoutsSupported);

    VkQueue graphicsQueue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &graphicsQueue);

    VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);
    
    Swapchain swapchain;
    createSwapchain(swapchain, physicalDevice, device, surface, familyIndex, window, swapchainFormat, VK_NULL_HANDLE);

    std::vector<VkImageView> swapchainImageViews;
    createImageViews(device, swapchain, swapchainFormat, swapchainImageViews);

    const VkFormat vertBufferFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
    };

    VkSampler textureSampler = createSampler(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);

    VkPipelineRenderingCreateInfo vertBufferInfo{};
    vertBufferInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    vertBufferInfo.colorAttachmentCount = 1;
    vertBufferInfo.pColorAttachmentFormats = vertBufferFormats;
    vertBufferInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    ShaderSet shaders;
    bool result = loadShaders(shaders, argv[0], "spirv/");
    assert(result);

    Program mainProgram = createProgram(device, VK_PIPELINE_BIND_POINT_GRAPHICS, {&shaders["vertexshader.vert"],&shaders["fragshader.frag"]},0,0);
    
    VkPipeline graphicsPipeline = createGraphicsPipeline(device, VK_NULL_HANDLE, vertBufferInfo, mainProgram,{});
 
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    for(size_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++){ 
        createCommandBuffer(device, commandPool, commandBuffers[i]);
    }

    VkCommandPool initCommandPool = createCommandPool(device, familyIndex);
    VkCommandBuffer initCommandBuffer = 0;
    createCommandBuffer(device, initCommandPool, initCommandBuffer);
   
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        imageAvailableSemaphores[i] = createSemaphore(device);
        renderFinishedSemaphores[i] = createSemaphore(device);
        inFlightFences[i] = createFence(device);
    }
    
 
    // TODO: make scene or model header
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    assert(loadModel(vertices, indices, "assets/crocodile/crocodile.obj"));


    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f),(float)WINDOW_WIDTH/(float)WINDOW_HEIGHT,0.1f,100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(4,3,3),glm::vec3(0,0,0),glm::vec3(0,1,0));
    Camera camera{model, view, proj};



    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    // Buffer imageBuffer{};
    // createBuffer(imageBuffer, device, memoryProperties, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // TODO: figure out how to upload data 
    Buffer vertexBuffer{};
    createBuffer(vertexBuffer, device, memoryProperties, vertices.size()*sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    memcpy(vertexBuffer.data, vertices.data(), sizeof(Vertex)*vertices.size());
    vkUnmapMemory(device, vertexBuffer.memory);

    Buffer indexBuffer{};
    createBuffer(indexBuffer, device, memoryProperties, indices.size()*sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(indexBuffer.data, indices.data(), sizeof(uint32_t)*indices.size());
    vkUnmapMemory(device, indexBuffer.memory);

    Buffer cameraBuffer{};
    createBuffer(cameraBuffer, device, memoryProperties, sizeof(camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    memcpy(cameraBuffer.data, &camera, sizeof(camera));
    vkUnmapMemory(device, cameraBuffer.memory);

    printf("Number of vertices: %u Number of Indices: %u\n", vertices.size(),indices.size());
    VkClearColorValue clearColor = {0.3f,0.6f,0.6f,1.0f};

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("assets/crocodile/crocodile_diff.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    assert(pixels);
    
    VkDeviceSize imageSize = texWidth*texHeight*4;
    uint32_t miplevels = getImageMipLevels(texWidth, texHeight);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = cameraBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(cameraBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Image textureImage{};
    createImage(textureImage, device, memoryProperties, texWidth, texHeight, miplevels,
        VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    
    memcpy(textureImage.memory, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, textureImage.memory);
    
    stbi_image_free(pixels);

    vkBeginCommandBuffer(initCommandBuffer, &beginInfo);
    VkImageMemoryBarrier2 initBarrier = createImageBarrier(textureImage.image,VK_PIPELINE_STAGE_2_NONE,
        0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, VK_IMAGE_ASPECT_COLOR_BIT,0,1);
    
    VkDependencyInfo depInfoEnd{};
    depInfoEnd.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfoEnd.imageMemoryBarrierCount = 1;
    depInfoEnd.pImageMemoryBarriers = &initBarrier;

    vkCmdPipelineBarrier2(initCommandBuffer, &depInfoEnd);
    
    vkEndCommandBuffer(initCommandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &initCommandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &initCommandBuffer);


    uint32_t currentFrame = 0;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
    
        vkQueueWaitIdle(graphicsQueue);
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE, &imageIndex);
        
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
    
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame],&beginInfo));
        
        vkCmdBindPipeline(commandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsPipeline);

        VkImageMemoryBarrier2 imageBarrierBegin = createImageBarrier(swapchain.images[imageIndex],VK_PIPELINE_STAGE_2_NONE,
        0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, VK_IMAGE_ASPECT_COLOR_BIT,0,1);

        VkDependencyInfo depInfoBegin{};
        depInfoBegin.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfoBegin.imageMemoryBarrierCount = 1;
        depInfoBegin.pImageMemoryBarriers = &imageBarrierBegin;
        
        vkCmdPipelineBarrier2(commandBuffers[currentFrame], &depInfoBegin);
        
        // add rendering info
        //need to change this from hard coded value later
        VkRenderingAttachmentInfo vertBufferAttachment{};
        vertBufferAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        vertBufferAttachment.imageView = swapchainImageViews[imageIndex];
        vertBufferAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        vertBufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        vertBufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        vertBufferAttachment.clearValue.color = clearColor;
        

        VkRenderingInfo passInfo{};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        passInfo.renderArea.extent.width = swapchain.width;
        passInfo.renderArea.extent.height = swapchain.height;
        passInfo.layerCount = 1;
        passInfo.colorAttachmentCount = 1;
        passInfo.pColorAttachments = &vertBufferAttachment;
        passInfo.pDepthAttachment = VK_NULL_HANDLE; // need to add later

        vkCmdBeginRendering(commandBuffers[currentFrame], &passInfo);

        VkViewport viewport = { 0, 0, float(swapchain.width), float(swapchain.height), 0, 1 };
		VkRect2D scissor = { { 0, 0 }, { uint32_t(swapchain.width), uint32_t(swapchain.height) } };

		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        vkCmdSetCullMode(commandBuffers[currentFrame], VK_CULL_MODE_NONE);
        vkCmdSetDepthBias(commandBuffers[currentFrame], 0.0,0.0, 1.0);

        VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers[currentFrame],0,1,vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(commandBuffers[currentFrame],static_cast<uint32_t>(indices.size()), 1,0,0,0);
       
        vkCmdEndRendering(commandBuffers[currentFrame]);

        VkImageMemoryBarrier2 barrierEnd = imageBarrierBegin;
        barrierEnd.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrierEnd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrierEnd.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrierEnd.dstAccessMask = 0;
        barrierEnd.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        barrierEnd.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkDependencyInfo depInfoEnd{};
        depInfoEnd.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfoEnd.imageMemoryBarrierCount = 1;
        depInfoEnd.pImageMemoryBarriers = &barrierEnd;

        vkCmdPipelineBarrier2(commandBuffers[currentFrame], &depInfoEnd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkEndCommandBuffer(commandBuffers[currentFrame]);

        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = {swapchain.swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue, &presentInfo);
        
        // always within [0, MAX_FRAMES_IN_FLIGHT]
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);
    vkDestroySampler(device, textureSampler, nullptr);
    destroyBuffer(indexBuffer, device);
    destroyBuffer(vertexBuffer, device);
    destroyBuffer(cameraBuffer, device);
    
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i],nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    // for(auto framebuffer : framebuffers)
    //    vkDestroyFramebuffer(device, framebuffer, nullptr);

    for(auto view : swapchainImageViews)
        vkDestroyImageView(device, view, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    destroyProgram(device, mainProgram);
    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}