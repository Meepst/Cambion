<<<<<<< HEAD
#include <assert.h>
#include <stdio.h>
=======
#include <vulkan/vulkan.h>
>>>>>>> 979bedd82f6c8cb5c135592946fa0803795b7d6c

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <windows.h>

#include <iostream>
#include <vector>
#include <algorithm>

// Vulkan assert macro for all vkCreate functions 
#define VK_CHECK(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS); \
	} while (0)

VkInstance createInstance(){
    // TODO: add vkEnumerateInstanceVersion to check if API VERSION_1_1 or eventually VERSION_1_4
    // TODO: verify layers are supported before enabling them

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    #ifdef _DEBUG   
        const char* debugLayers[] = {
            "VK_LAYER_LUNARG_standard_validation"
        };
        createInfo.ppEnabledLayerNames = debugLayers;
        createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]); // trick to get the size of a C-array divide memory size by 1 memory unit
    #endif
    
    const char* extensions[] = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    #ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };

    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

int main(){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800,600,"Cambion",nullptr,nullptr);
    printf("begin\n");
    VkInstance instance = createInstance();

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
    }

    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
