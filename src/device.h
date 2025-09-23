#pragma once
#include "common.h"

#define API_VERSION VK_API_VERSION_1_4

// Vulkan assert macro for all vkCreate functions 
#define VK_CHECK(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS); \
	} while (0)

VkInstance createInstance();

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window);

uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice);

bool supportsPresentation(VkPhysicalDevice physicalDevice, uint32_t index, VkSurfaceKHR surface);

VkPhysicalDevice selectPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount, VkSurfaceKHR surface);

VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t index, bool raytracingSupported, bool unifiedlayoutSupported);