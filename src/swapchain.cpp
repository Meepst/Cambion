#include "swapchain.h"

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface){
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
    assert(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

    if(formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        return VK_FORMAT_R8G8B8A8_UNORM;

    for(uint32_t i=0;i<formatCount;i++){
        if(formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
            return formats[i].format;
    }

    return formats[0].format;
}

VkPresentModeKHR getPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface){
    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,surface,&presentModeCount,0));
    assert(presentModeCount > 0);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

    for(VkPresentModeKHR mode : presentModes){
        if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
        if(mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

void createSwapchain(Allocator& allocator, Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device,
     VkSurfaceKHR surface, uint32_t familyIndex, GLFWwindow* window, VkFormat format, VkSwapchainKHR oldSwapchain){
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    VkPresentModeKHR presentMode = getPresentMode(physicalDevice, surface);

    VkCompositeAlphaFlagBitsKHR surfaceComposite = (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
        : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = std::max(unsigned(MIN_IMAGE_COUNT), surfaceCapabilities.minImageCount);
    createInfo.imageFormat = format;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = width;
    createInfo.imageExtent.height = height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &familyIndex;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = surfaceComposite;
    createInfo.presentMode = presentMode;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain));

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, 0));

    DynArray<VkImage> images(allocator, imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));

    result.swapchain = swapchain;
    result.images = images;
    result.imageCount = imageCount;
    result.height = height;
    result.width = width;
}

SwapchainStatus updateSwapchain(Allocator& allocator, Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device,
        VkSurfaceKHR surface, uint32_t familyIndex, GLFWwindow* window, VkFormat format){
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    if(width==0||height==0)
        return Swapchain_NotReady;
    if(result.width==width&&result.height==height)
        return Swapchain_Ready;

    Swapchain old = result;
    createSwapchain(allocator, result, physicalDevice, device, surface, familyIndex, window, format, old.swapchain);
    VK_CHECK(vkDeviceWaitIdle(device));

    vkDestroySwapchainKHR(device, old.swapchain, 0);
    return Swapchain_Resized;
}
