#pragma once
#include <assert.h>
#include <stdio.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define NOMINMAX
#if _WIN32
#include <windows.h>
#endif
#include <vector>
#include <algorithm>
#include <fstream>

#define MIN_IMAGE_COUNT 3

// Vulkan assert macro for all vkCreate functions
#define VK_CHECK(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS); \
	} while (0)

#define VK_CHECK_SWAPCHAIN(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS || result_ == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) \
	} while (0)
