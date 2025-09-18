#include <assert.h>
#include <stdio.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define NOMINMAX
#include <windows.h>
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