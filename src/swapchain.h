#pragma once
#include "common.h"

enum SwapchainStatus{
    Swapchain_Ready,
    Swapchain_Resized,
    Swapchain_NotReady,
};

struct Swapchain{
    VkSwapchainKHR swapchain;
    DynArray<VkImage> images;
    uint32_t width, height;
    uint32_t imageCount;
};

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

VkPresentModeKHR getPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

void createSwapchain(Allocator& allocator, Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device,
     VkSurfaceKHR surface, uint32_t familyIndex, GLFWwindow* window, VkFormat format, VkSwapchainKHR oldSwapchain);

SwapchainStatus updateSwapchain(Allocator& allocator, Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device,
    VkSurfaceKHR surface, uint32_t familyIndex, GLFWwindow* window, VkFormat format);
