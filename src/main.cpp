#include "arena.h"
#include "common.h"
#include "containers.h"
#include "primitives.h"
#include "resources.h"
#include "device.h"
#include "swapchain.h"
#include "program.h"
#include "model.h"
#include "vulkan/vulkan_core.h"

#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

//#define _Debug

#define DEVICE_COUNT 16
#define MAX_FRAMES_IN_FLIGHT 2

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

struct alignas(16) UniformBufferObject{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

void transitionImageLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels){
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT: VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_GENERAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL){
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR){
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    }else{
        assert(!"Unknown image layout transition\n");
    }

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

bool loadTexture(Image& image, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const char* path, uint32_t mipLevels){
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if(!pixels){
        printf("failed to load image: %s\n", path);
        return false;
    }
    VkDeviceSize imageSize = texWidth*texHeight*4; // number of channels RGBA, should change per texture format

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    Buffer stagingBuffer;
    createBuffer(stagingBuffer, device, memoryProperties, imageSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(stagingBuffer.data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBuffer.memory);

    stbi_image_free(pixels);

    mipLevels = getImageMipLevels(texWidth, texHeight);
    createImage(image, device, memoryProperties, texWidth, texHeight, mipLevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT  | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkCommandBuffer commandBuffer = 0;
    createCommandBuffer(device, commandPool, commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    printf("Before transition !\n");
    transitionImageLayout(device, commandBuffer, queue, image.image, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};

    printf("Before image copy !\n");
    vkCmdCopyBufferToImage(commandBuffer,stagingBuffer.buffer, image.image
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier2 mipMapBarrier{};
    mipMapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    mipMapBarrier.image = image.image;
    mipMapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    mipMapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    mipMapBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    mipMapBarrier.subresourceRange.baseArrayLayer = 0;
    mipMapBarrier.subresourceRange.layerCount = 1;
    mipMapBarrier.subresourceRange.levelCount = 1;

    VkDependencyInfo depInfo{};

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;
    printf("Mip Levels: %u", mipLevels);
    for(uint32_t i=1;i<mipLevels;i++){
        mipMapBarrier.subresourceRange.baseMipLevel = i-1;
        mipMapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        mipMapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        mipMapBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        mipMapBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        mipMapBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        mipMapBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &mipMapBarrier;
        vkCmdPipelineBarrier2(commandBuffer, &depInfo);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        mipMapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        mipMapBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mipMapBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        mipMapBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        mipMapBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        mipMapBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &mipMapBarrier;
        vkCmdPipelineBarrier2(commandBuffer, &depInfo);

        if(mipWidth > 1) mipWidth/=2;
        if(mipHeight>1) mipHeight/=2;
    }

    mipMapBarrier.subresourceRange.baseMipLevel = mipLevels-1;
    mipMapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    mipMapBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    mipMapBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    mipMapBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    mipMapBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    mipMapBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &mipMapBarrier;
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo,0);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    destroyBuffer(stagingBuffer, device);

    return true;
}

int main(int argc, char *argv[]){
    Allocator arena = arenaNew((uint64_t)1 << 31);

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
    createSwapchain(arena, swapchain, physicalDevice, device, surface, familyIndex, window, swapchainFormat, VK_NULL_HANDLE);

    DynArray<VkImageView> swapchainImageViews;

    swapchainImageViews.resize(arena, swapchain.imageCount);
    for(int i=0; i<swapchain.imageCount;i++)
        swapchainImageViews[i] = createImageView(device,swapchain.images[i],swapchainFormat,0,1);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    Image depthImage = {};
    createImage(depthImage,device, memoryProperties,swapchain.width,swapchain.height,1,
        depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    const VkFormat vertBufferFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
    };

    VkPipelineRenderingCreateInfo vertBufferInfo{};
    vertBufferInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    vertBufferInfo.colorAttachmentCount = 1;
    vertBufferInfo.pColorAttachmentFormats = vertBufferFormats;
    vertBufferInfo.depthAttachmentFormat = depthFormat;

    ShaderSet shaders;
    bool result = loadShaders(shaders, argv[0], "spirv/");
    assert(result);

    // VkDescriptorSetLayout texSetLayout = createDescriptorArrayLayout(device);
    // Program mainProgram = createProgram(device, VK_PIPELINE_BIND_POINT_GRAPHICS, {&shaders["vertexshader.vert"],&shaders["fragshader.frag"]},sizeof(UniformBufferObject),texSetLayout);
    Program mainProgram = createSimpleProgram(device, VK_PIPELINE_BIND_POINT_GRAPHICS,{&shaders["vertexshader.vert"],&shaders["fragshader.frag"]},sizeof(UniformBufferObject));
    VkPipeline graphicsPipeline = createGraphicsPipeline(device, VK_NULL_HANDLE, vertBufferInfo, mainProgram,{});

    VkCommandPool commandPool = createCommandPool(device, familyIndex);

    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    for(size_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        createCommandBuffer(device, commandPool, commandBuffers[i]);
    }



    DynArray<VkSemaphore> renderFinishedSemaphores;
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        imageAvailableSemaphores[i] = createSemaphore(device);
        inFlightFences[i] = createFence(device);
    }

    renderFinishedSemaphores.resize(arena, swapchain.imageCount);
    for(int i=0;i<swapchain.imageCount;i++){
        renderFinishedSemaphores[i] = createSemaphore(device);
    }

   // TODO: make scene or model header
    std::vector<Mesh> refMeshes;
    std::vector<Material> refMaterial;
    Model model(refMeshes,refMaterial);
    assert(loadModel(arena, model, "assets/viking_house/viking.obj"));
    std::cout << model.meshes[3].materialID << std::endl;

    VkDeviceSize totalVertSize = 0;
    VkDeviceSize totalIndexSize = 0;

    for(size_t i = 0; i<model.meshes.size();i++){
        totalVertSize += sizeof(Vertex)*model.meshes[i].vertices.size();
        totalIndexSize += sizeof(uint32_t)*model.meshes[i].indices.size();
    }

    Buffer vertexBuffer{};
    createBuffer(vertexBuffer, device, memoryProperties, totalVertSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Buffer indexBuffer{};
    createBuffer(indexBuffer, device, memoryProperties, totalIndexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset = 0;

    DynArray<VkDeviceSize> vertexOffsets;
    DynArray<VkDeviceSize> indexOffsets;
    for(uint32_t i=0; i<model.meshes.size();i++){
        vertexOffsets.push(arena,vertexOffset);
        indexOffsets.push(arena,indexOffset);

        memcpy((uint8_t*)vertexBuffer.data+vertexOffset,model.meshes[i].vertices.data(),model.meshes[i].vertices.size()*sizeof(Vertex));
        memcpy((uint8_t*)indexBuffer.data+indexOffset,model.meshes[i].indices.data(),model.meshes[i].indices.size()*sizeof(uint32_t));

        vertexOffset += sizeof(Vertex)*model.meshes[i].vertices.size();
        indexOffset += sizeof(uint32_t)*model.meshes[i].indices.size();
    }

    vkUnmapMemory(device, vertexBuffer.memory);
    vkUnmapMemory(device, indexBuffer.memory);

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapchain.width / (float)swapchain.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    Buffer uniformBuffer{};
    createBuffer(uniformBuffer, device, memoryProperties, sizeof(ubo),
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(uniformBuffer.data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uniformBuffer.memory);


    VkClearColorValue clearColor = {0.3f,0.6f,0.6f,1.0f};

    VkCommandPool initCommandPool = createCommandPool(device, familyIndex);

    VkCommandBuffer initCommandBuffer = 0;
    createCommandBuffer(device, initCommandPool, initCommandBuffer);

    Image textureImage;
    const char* texturePath = "assets/viking_house/viking_room.png";
    uint32_t texMipLevels = 0;
    loadTexture(textureImage, device, physicalDevice, initCommandPool, graphicsQueue, texturePath, texMipLevels);

    vkDestroyCommandPool(device, initCommandPool, 0);


    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSampler textureSampler = 0;
    textureSampler = createSampler(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,VK_SAMPLER_ADDRESS_MODE_REPEAT);

    printf("Descript! \n");
    VkDescriptorPoolSize descPoolSize{};
    descPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descPoolSize.descriptorCount = 10;
    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = &descPoolSize;
    descPoolInfo.maxSets = 10;

    VkDescriptorPool descPool = 0;
    VK_CHECK(vkCreateDescriptorPool(device,&descPoolInfo,0,&descPool));

    VkDescriptorSetAllocateInfo descAllocInfo{};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = descPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &mainProgram.setLayout;

    VkDescriptorSet descSet = 0;
    VK_CHECK(vkAllocateDescriptorSets(device, &descAllocInfo,&descSet));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = textureSampler;
    imageInfo.imageView = textureImage.imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descWrite{};
    descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet = descSet;
    descWrite.dstBinding = 0;
    descWrite.dstArrayElement = 0;
    descWrite.descriptorCount = 1;
    descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device,1,&descWrite,0,0);

    uint32_t currentFrame = 0;
    uint64_t alloc_frame_start = ((Arena*)arena.data)->last;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        SwapchainStatus swapchainStatus = updateSwapchain(arena,swapchain,physicalDevice,device,surface,familyIndex,window,swapchainFormat);
        if(swapchainStatus == Swapchain_NotReady) continue;
        if(swapchainStatus == Swapchain_Resized){
            printf("Swapchain: %dx%d\n",swapchain.width,swapchain.height);
            // later on rebuild gbuffer and depth target images here
            for(int i=0;i<swapchainImageViews.size();i++){
                if(swapchainImageViews[i])
                    vkDestroyImageView(device,swapchainImageViews[i],0);
                swapchainImageViews[i] = createImageView(device,swapchain.images[i],swapchainFormat,0,1);
            }
            destroyImage(depthImage,device);
            createImage(depthImage,device, memoryProperties,swapchain.width,swapchain.height,1,
                depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        }


        VK_CHECK(vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        VkResult acquireStatus = vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE, &imageIndex);

        if(acquireStatus == VK_ERROR_OUT_OF_DATE_KHR) continue;
        assert(acquireStatus == VK_SUCCESS || acquireStatus == VK_SUBOPTIMAL_KHR || acquireStatus == VK_ERROR_OUT_OF_DATE_KHR);

        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame],&beginInfo));
        // should move this to an indepent swapchain recreate buffer sequence/area
        transitionImageLayout(device, commandBuffers[currentFrame], graphicsQueue,
            depthImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            1);


        transitionImageLayout(device,commandBuffers[currentFrame],graphicsQueue,
            swapchain.images[imageIndex],swapchainFormat,VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1);


        // add rendering info
        //need to change this from hard coded value later
        VkRenderingAttachmentInfo renderAttachment{};
        renderAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderAttachment.imageView = swapchainImageViews[imageIndex];
        renderAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        renderAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        renderAttachment.clearValue.color = clearColor;

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthImage.imageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f,0};

        VkRenderingInfo passInfo{};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        passInfo.renderArea.extent.width = swapchain.width;
        passInfo.renderArea.extent.height = swapchain.height;
        passInfo.layerCount = 1;
        passInfo.colorAttachmentCount = 1;
        passInfo.pColorAttachments = &renderAttachment;
        passInfo.pDepthAttachment = &depthAttachment; // need to add later

        vkCmdBeginRendering(commandBuffers[currentFrame], &passInfo);
        vkCmdBindPipeline(commandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsPipeline);

        vkCmdBindDescriptorSets(commandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
            mainProgram.layout,0,1,&descSet,0,nullptr);

        VkViewport viewport = { 0, 0, float(swapchain.width), float(swapchain.height), 0, 1 };
		VkRect2D scissor = { { 0, 0 }, { uint32_t(swapchain.width), uint32_t(swapchain.height) } };

		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        vkCmdSetCullMode(commandBuffers[currentFrame], VK_CULL_MODE_NONE);
        vkCmdSetDepthBias(commandBuffers[currentFrame], 0.0,0.0, 1.0);

        //vkCmdBindDescriptorSets(commandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,
           //mainProgram.layout,0,1,&textureSet.second,0,nullptr);

        VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
        for(size_t i=0;i<model.meshes.size();i++){

            vkCmdPushConstants(commandBuffers[currentFrame],mainProgram.layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(ubo),&ubo);
            auto& mesh = model.meshes[i];

            VkDeviceSize vertexOffset = vertexOffsets[i];
            VkDeviceSize indexOffset = indexOffsets[i];

            vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, &vertexOffset);
            vkCmdBindIndexBuffer(commandBuffers[currentFrame],indexBuffer.buffer,indexOffset,VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffers[currentFrame],mesh.indices.size(),1,0,0,0);
        }
        vkCmdEndRendering(commandBuffers[currentFrame]);

        transitionImageLayout(device, commandBuffers[currentFrame], graphicsQueue,
            swapchain.images[imageIndex], swapchainFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);

        vkEndCommandBuffer(commandBuffers[currentFrame]);

        VkSemaphoreSubmitInfo waitSemaphoreInfo{};
        waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfo.semaphore = imageAvailableSemaphores[currentFrame];
        waitSemaphoreInfo.value = 0;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitSemaphoreInfo.deviceIndex = 0;

        VkCommandBufferSubmitInfo cmdBuffInfo{};
        cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdBuffInfo.commandBuffer = commandBuffers[currentFrame];
        cmdBuffInfo.deviceMask = 1;

        VkSemaphoreSubmitInfo signalSemaphoreInfo{};
        signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfo.semaphore = renderFinishedSemaphores[imageIndex];
        signalSemaphoreInfo.value = 0;
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        signalSemaphoreInfo.deviceIndex = 0;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &cmdBuffInfo;

        VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];

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
    vkDestroySampler(device, textureSampler, nullptr);

    destroyBuffer(indexBuffer, device);
    destroyBuffer(vertexBuffer, device);
    destroyBuffer(uniformBuffer, device);

    destroyImage(depthImage, device);


    vkDestroyDescriptorPool(device, descPool,0);

    // vkDestroyDescriptorPool(device, textureSet.first, nullptr);
    // vkDestroyDescriptorSetLayout(device, texSetLayout, nullptr);

    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    for(int i=0;i<swapchain.imageCount;i++)
        vkDestroySemaphore(device,renderFinishedSemaphores[i],nullptr);

    destroyImage(textureImage, device);

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
