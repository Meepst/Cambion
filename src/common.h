#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <spirv-headers/spirv.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#if _WIN32
#include <windows.h>
#endif

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <array>
#include <unordered_map>

#include "alloc.h"
#include "arena.h"
#include "bits.h"
#include "containers.h"

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
