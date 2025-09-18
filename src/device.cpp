#include "device.h"

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

VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t index){
    float queuePriorities[]={1.0};
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = index;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queuePriorities;

    // TODO: make vector later
    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.pNext = nullptr; // will change later to include features

    VkDevice device = 0;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &device));

    return device;
}