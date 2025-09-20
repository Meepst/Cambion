#include "common.h"
#include "device.h"
#include "swapchain.h"

#define _Debug

#include <iostream>
#include <array>

#define DEVICE_COUNT 16
#define MAX_FRAMES_IN_FLIGHT 2

// TODO: need to add input binding descriptors
struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }
};

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

const std::vector<Vertex> vertices = {
    {{0.0f,-0.5f},{1.0f,0.0f,0.0f}},
    {{0.5f,0.5f},{0.0f,1.0f,0.0f}},
    {{-0.5f,0.5f},{0.0f,0.0f,1.0f}},
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

std::vector<char> readShader(const std::string& filename){
    // open file and keep into in byte code / binary form
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    assert(file.is_open());

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    // fill a new buffer with the binary data
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code){
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

    return shaderModule;
}


void createGraphicsPipeline(VkDevice device, Swapchain swapchain,
     VkPipeline &pipeline, VkPipelineLayout &pipelineLayout, VkRenderPass renderPass){
    auto vertShaderCode = readShader("./src/shaders/vert.spv");
    auto fragShaderCode = readShader("./src/shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    std::vector<VkDynamicState> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,  
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(
        attributeDescriptions.size()
    ); 
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); 

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchain.width;
    viewport.height = (float) swapchain.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = swapchain.width;
    scissor.extent.height = swapchain.height;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; 
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
         &pipelineInfo, nullptr, &pipeline));

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

VkRenderPass createRenderPass(VkDevice device, VkFormat format){
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));

    return renderPass;
}

VkPipelineLayout createPipelineLayout(VkDevice device){
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    return pipelineLayout;
}

void createFramebuffers(VkDevice device, VkRenderPass renderpass,
     Swapchain swapchain, std::vector<VkImageView> views,std::vector<VkFramebuffer> &framebuffers){
    framebuffers.resize(views.size());

    for(size_t i=0; i<views.size();i++){
        VkImageView attachments[]={
            views[i],
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain.width;
        framebufferInfo.height = swapchain.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device,&framebufferInfo,nullptr,&framebuffers[i]));
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

// TODO: this needs to be removed and placed in main, but specific steps we can abstract
void recordCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer,
    VkRenderPass renderpass, std::vector<VkFramebuffer> framebuffers, Swapchain swapchain,
    uint32_t frameIndex, VkPipeline graphicsPipeline, Buffer &buffer){
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderpass;
    renderPassInfo.framebuffer = framebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0,0};
    renderPassInfo.renderArea.extent.width = swapchain.width;
    renderPassInfo.renderArea.extent.height = swapchain.height;

    VkClearValue clearColor = {{{8.0f,4.0f,1.0f,1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkBuffer buffers[] = {buffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer,0,1,buffers,offsets);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.width);
    viewport.height = static_cast<float>(swapchain.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0,0};
    scissor.extent.width = swapchain.width;
    scissor.extent.height = swapchain.height;
    vkCmdSetScissor(commandBuffer,0,1,&scissor);

    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
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


int main(){
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

    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    VkDevice device = createDevice(instance, physicalDevice, familyIndex);

    VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);
    
    Swapchain swapchain;
    createSwapchain(swapchain, physicalDevice, device, surface, familyIndex, window, swapchainFormat, VK_NULL_HANDLE);

    std::vector<VkImageView> swapchainImageViews;
    createImageViews(device, swapchain, swapchainFormat, swapchainImageViews);
    VkPipelineLayout pipelineLayout = createPipelineLayout(device);

    VkRenderPass renderPass = createRenderPass(device, swapchainFormat);
   
    VkPipeline graphicsPipeline = 0;
    createGraphicsPipeline(device, swapchain, graphicsPipeline,
        pipelineLayout, renderPass);
    
    std::vector<VkFramebuffer> framebuffers;
    createFramebuffers(device, renderPass, swapchain,swapchainImageViews,
        framebuffers);
   
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
   
    VkQueue graphicsQueue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &graphicsQueue);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    Buffer vertexBuffer{};
    createBuffer(vertexBuffer, device, memoryProperties, vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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
        
        recordCommandBuffer(device, commandBuffers[currentFrame], renderPass, framebuffers, swapchain, imageIndex, graphicsPipeline,vertexBuffer);

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

    destroyBuffer(vertexBuffer, device);
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i],nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    for(auto framebuffer : framebuffers)
       vkDestroyFramebuffer(device, framebuffer, nullptr);

    for(auto view : swapchainImageViews)
        vkDestroyImageView(device, view, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}