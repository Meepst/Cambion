#include "arena.h"
#include "common.h"
#include "device.h"
#include "swapchain.h"
#include "program.h"
#include "model.h"

#define _Debug

#define DEVICE_COUNT 16
#define MAX_FRAMES_IN_FLIGHT 2

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

// const std::vector<Vertex> vertices = {
//     {{0.0f,-0.5f, 0.5f},{1.0f,0.0f,0.0f}},
//     {{0.5f,0.5f, 0.0f},{0.0f,1.0f,0.0f}},
//     {{-0.5f,0.5f, 0.5f},{0.0f,0.0f,1.0f}},
// };

// const std::vector<uint16_t> indices = {
//     0,1,2,2,3,0
// };

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

void createImageViews(Allocator& allocator, VkDevice device, Swapchain swapchain, VkFormat format, DynArray<VkImageView> &imageViews){
    imageViews.resize(allocator, swapchain.imageCount);

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

int main(int argc, char *argv[]){
    Allocator arena = arenaNew((uint64_t)1 << 31);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800,600,"Cambion",nullptr,nullptr);
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
    createSwapchain(arena, swapchain, physicalDevice, device, surface, familyIndex, window, swapchainFormat, VK_NULL_HANDLE);

    DynArray<VkImageView> swapchainImageViews;
    createImageViews(arena, device, swapchain, swapchainFormat, swapchainImageViews);

    const VkFormat vertBufferFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
    };

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

    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        imageAvailableSemaphores[i] = createSemaphore(device);
        renderFinishedSemaphores[i] = createSemaphore(device);
        inFlightFences[i] = createFence(device);
    }

    // TODO: make scene or model header
    DynArray<Vertex> vertices;
    DynArray<uint32_t> indices;
    assert(loadModel(arena, vertices, indices, "assets/crocodile/crocodile.obj"));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

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

    VkClearColorValue clearColor = {0.3f,0.6f,0.6f,1.0f};

    uint32_t currentFrame = 0;
    uint64_t alloc_frame_start = ((Arena*)arena.data)->last;
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

        VkImageMemoryBarrier2 barrierBegin{};
        barrierBegin.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrierBegin.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrierBegin.srcAccessMask = 0;
        barrierBegin.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrierBegin.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrierBegin.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierBegin.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        barrierBegin.image = swapchain.images[imageIndex];
        barrierBegin.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrierBegin.subresourceRange.baseMipLevel = 0;
        barrierBegin.subresourceRange.levelCount = 1;
        barrierBegin.subresourceRange.baseArrayLayer = 0;
        barrierBegin.subresourceRange.layerCount = 1;

        VkDependencyInfo depInfoBegin{};
        depInfoBegin.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfoBegin.imageMemoryBarrierCount = 1;
        depInfoBegin.pImageMemoryBarriers = &barrierBegin;

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

        VkImageMemoryBarrier2 barrierEnd = barrierBegin;
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

        arenaSetLast(arena, alloc_frame_start);
    }

    vkDeviceWaitIdle(device);

    destroyBuffer(indexBuffer, device);
    destroyBuffer(vertexBuffer, device);
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i],nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    // for(auto framebuffer : framebuffers)
    //    vkDestroyFramebuffer(device, framebuffer, nullptr);

    for(int i = 0; i < swapchainImageViews.size(); ++i) {
        VkImageView view = swapchainImageViews[i];
        vkDestroyImageView(device, view, nullptr);
    }

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
