#include <assert.h>
#include <stdio.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <windows.h>

#include <iostream>
#include <vector>
#include <algorithm>

#define API_VERSION VK_API_VERSION_1_0
#define DEVICE_COUNT 16

// Vulkan assert macro for all vkCreate functions 
#define VK_CHECK(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS); \
	} while (0)

VkInstance createInstance(){
    // TODO: add vkEnumerateInstanceVersion to check if API VERSION_1_1 or eventually VERSION_1_4
    // TODO: add verifaction that layers are supported before enabling them
    // TODO: add verifaction that extensions are supported before enabling them

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = API_VERSION;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    #ifdef _DEBUG   
        const char* debugLayers[] = {
            "VK_LAYER_KHRONOS_validation"
        };
        createInfo.ppEnabledLayerNames = debugLayers;
        createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]); // trick to get the size of a C-array divide memory size by 1 memory unit
    #endif
    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions+glfwExtensionCount);
    #ifdef _DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.pNext = nullptr;

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window){
    VkSurfaceKHR surface = 0;
    VK_CHECK(glfwCreateWindowSurface(instance,window,nullptr,&surface));
    return surface;
}

uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice){
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, 0);

    std::vector<VkQueueFamilyProperties> queues(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

    for(uint32_t i=0; i<queueCount;i++){
        if(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            return i;
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

bool supportsPresentation(VkPhysicalDevice physicalDevice, uint32_t index, VkSurfaceKHR surface){
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice,index,surface,&presentSupport);
    return presentSupport;
}

VkPhysicalDevice selectPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount, VkSurfaceKHR surface){
    VkPhysicalDevice discreteGPU = 0;
    VkPhysicalDevice defaultGPU = 0;
    const char* nGPU = getenv("NGPU"); // user preferred device

    for(uint32_t i=0;i<physicalDeviceCount;i++){
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[i],&properties);

        if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;

        printf("GPU%d: %s (Vulkan 1.%d)\n",i,properties.deviceName,VK_VERSION_MINOR(properties.apiVersion));

        uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevices[i]);
        if(familyIndex == VK_QUEUE_FAMILY_IGNORED) continue;

        if(!supportsPresentation(physicalDevices[i], familyIndex,surface)) continue;

        if(properties.apiVersion < API_VERSION) continue;

        if(nGPU && atoi(nGPU) == i) 
            discreteGPU = physicalDevices[i];

        if(!discreteGPU && properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            discreteGPU = physicalDevices[i];
        
        if(!defaultGPU)
            defaultGPU = physicalDevices[i];
    }
    
    VkPhysicalDevice result = discreteGPU ? discreteGPU : defaultGPU;

    if(result){
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(result,&properties);
        printf("Selected GPU %s\n",properties.deviceName);
    }else{
        printf("Error: compatible GPU not found\n");
    }
    return result;
}

// TODO: Add logical device creation (VkDevice)

int main(){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800,600,"Cambion",nullptr,nullptr);
    printf("begin\n");
    VkInstance instance = createInstance();
    VkSurfaceKHR surface = createSurface(instance, window);

    VkPhysicalDevice physicalDevices[DEVICE_COUNT];
    uint32_t physicalDeviceCount = DEVICE_COUNT;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = selectPhysicalDevice(physicalDevices,physicalDeviceCount,surface);

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}