#include "RHI.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace rhi
{
struct Context
{
    uint64_t frameCount = 0;

    VkDevice device = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties2 properties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceVulkan11Properties properties_1_1 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
    VkPhysicalDeviceVulkan12Properties properties_1_2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceVulkan13Properties properties_1_3 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR raytracingProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceFeatures2KHR features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
    VkPhysicalDeviceVulkan11Features features_1_1 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features features_1_2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features features_1_3 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR raytracingQueryFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VmaAllocator allocator = VK_NULL_HANDLE;
} s_ctx;

struct ResourceManager
{
    uint64_t frameCount = 0;

    std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> destroyerImages;
    std::deque<std::pair<VkImageView, uint64_t>> destroyerImageviews;
    std::deque<std::pair<std::pair<VkBuffer, VmaAllocation>, uint64_t>> destroyerBuffers;
    std::deque<std::pair<VkSampler, uint64_t>> destroyerSamplers;
    std::deque<std::pair<VkDescriptorPool, uint64_t>> destroyerDescriptorPools;
    std::deque<std::pair<VkDescriptorSetLayout, uint64_t>> destroyerDescriptorSetLayouts;
    std::deque<std::pair<VkDescriptorUpdateTemplate, uint64_t>> destroyerDescriptorUpdateTemplates;
    std::deque<std::pair<VkShaderModule, uint64_t>> destroyerShaderModules;
    std::deque<std::pair<VkPipelineLayout, uint64_t>> destroyerPipelineLayouts;
    std::deque<std::pair<VkPipeline, uint64_t>> destroyerPipelines;
    std::deque<std::pair<VkQueryPool, uint64_t>> destroyerQueryPools;
    std::deque<std::pair<VkAccelerationStructureKHR, uint64_t>> destroyerAccelerationStructures;
} s_resMgr;

static bool IsLayerSupported(const char* required, const std::vector<VkLayerProperties>& available)
{
    for (const VkLayerProperties& availableLayer : available)
    {
        if (strcmp(availableLayer.layerName, required) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool IsExtensionSupported(const char* required, const std::vector<VkExtensionProperties>& available)
{
    for (const VkExtensionProperties& available_extension : available)
    {
        if (strcmp(available_extension.extensionName, required) == 0)
        {
            return true;
        }
    }
    return false;
}

#ifdef VK_DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCB(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                        void* userData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        LOGW("Validation Verbose %s: %s\n", callbackData->pMessageIdName, callbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOGW("Validation Info %s: %s\n", callbackData->pMessageIdName, callbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOGW("Validation Warning %s: %s\n", callbackData->pMessageIdName, callbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOGE("Validation Error %s: %s\n", callbackData->pMessageIdName, callbackData->pMessage);
    }
    return VK_FALSE;
}
#endif

void Startup()
{
    VK_ASSERT(volkInitialize());

    uint32_t numInstanceAvailableLayers;
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&numInstanceAvailableLayers, nullptr));
    std::vector<VkLayerProperties> instanceSupportedLayers(numInstanceAvailableLayers);
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&numInstanceAvailableLayers, instanceSupportedLayers.data()));

    uint32_t numInstanceAvailableExtensions;
    VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceAvailableExtensions, nullptr));
    std::vector<VkExtensionProperties> instanceSupportedExtensions(numInstanceAvailableExtensions);
    VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceAvailableExtensions, instanceSupportedExtensions.data()));

    std::vector<const char*> instanceRequiredLayers;
    std::vector<const char*> instanceRequiredExtensions;
    std::vector<const char*> instanceLayers;
    std::vector<const char*> instanceExtensions;

#ifdef VK_DEBUG
    instanceRequiredLayers.push_back("VK_LAYER_KHRONOS_validation");
    instanceRequiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    instanceRequiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_WIN32_KHR
    instanceRequiredExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    for (auto it = instanceRequiredLayers.begin(); it != instanceRequiredLayers.end(); ++it)
    {
        if (IsLayerSupported(*it, instanceSupportedLayers))
        {
            instanceLayers.push_back(*it);
        }
    }

    for (auto it = instanceRequiredExtensions.begin(); it != instanceRequiredExtensions.end(); ++it)
    {
        if (IsExtensionSupported(*it, instanceSupportedExtensions))
        {
            instanceExtensions.push_back(*it);
        }
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName = "vulkan";
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = (uint32_t)instanceLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    VK_ASSERT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_ctx.instance));

    volkLoadInstance(s_ctx.instance);

#ifdef VK_DEBUG
    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCreateInfo.pfnUserCallback = debugUtilsMessengerCB;
    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(s_ctx.instance, &messengerCreateInfo, nullptr, &s_ctx.debugMessenger));
#endif

    // Selected physical device
    uint32_t numGpus = 0;
    VK_ASSERT(vkEnumeratePhysicalDevices(s_ctx.instance, &numGpus, nullptr));
    std::vector<VkPhysicalDevice> gpus(numGpus);
    VK_ASSERT(vkEnumeratePhysicalDevices(s_ctx.instance, &numGpus, gpus.data()));
    for (auto& gpu : gpus)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(gpu, &props);
        LOGI("Found Vulkan GPU: %s\n", props.deviceName);
        LOGI("API: %u.%u.%u\n",
             VK_VERSION_MAJOR(props.apiVersion),
             VK_VERSION_MINOR(props.apiVersion),
             VK_VERSION_PATCH(props.apiVersion));
        LOGI("Driver: %u.%u.%u\n",
             VK_VERSION_MAJOR(props.driverVersion),
             VK_VERSION_MINOR(props.driverVersion),
             VK_VERSION_PATCH(props.driverVersion));
    }
    s_ctx.physicalDevice = gpus.front();

    s_ctx.properties2.pNext = &s_ctx.properties_1_1;
    s_ctx.properties_1_1.pNext = &s_ctx.properties_1_2;
    s_ctx.properties_1_2.pNext = &s_ctx.properties_1_3;
    void** properties_chain = &s_ctx.properties_1_3.pNext;

    s_ctx.features_1_3.dynamicRendering = true;
    s_ctx.features_1_3.synchronization2 = true;
    s_ctx.features_1_3.maintenance4 = true;
    s_ctx.features2.pNext = &s_ctx.features_1_1;
    s_ctx.features_1_1.pNext = &s_ctx.features_1_2;
    s_ctx.features_1_2.pNext = &s_ctx.features_1_3;
    void** features_chain = &s_ctx.features_1_3.pNext;

    uint32_t numDeviceAvailableExtensions = 0;
    VK_ASSERT(vkEnumerateDeviceExtensionProperties(s_ctx.physicalDevice, nullptr, &numDeviceAvailableExtensions, nullptr));
    std::vector<VkExtensionProperties> deviceAvailableExtensions(numDeviceAvailableExtensions);
    VK_ASSERT(vkEnumerateDeviceExtensionProperties(s_ctx.physicalDevice, nullptr, &numDeviceAvailableExtensions, deviceAvailableExtensions.data()));

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, deviceAvailableExtensions))
    {
        deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        *features_chain = &s_ctx.accelerationStructureFeatures;
        features_chain = &s_ctx.accelerationStructureFeatures.pNext;
        *properties_chain = &s_ctx.accelerationStructureProperties;
        properties_chain = &s_ctx.accelerationStructureProperties.pNext;

        if(IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, deviceAvailableExtensions))
        {
            deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            *features_chain = &s_ctx.raytracingFeatures;
            features_chain = &s_ctx.raytracingFeatures.pNext;
            *properties_chain = &s_ctx.raytracingProperties;
            properties_chain = &s_ctx.raytracingProperties.pNext;
        }

        if(IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME, deviceAvailableExtensions))
        {
            deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            *features_chain = &s_ctx.raytracingQueryFeatures;
            features_chain = &s_ctx.raytracingQueryFeatures.pNext;
        }
    }

    vkGetPhysicalDeviceFeatures2(s_ctx.physicalDevice, &s_ctx.features2);
    vkGetPhysicalDeviceProperties2(s_ctx.physicalDevice, &s_ctx.properties2);

    uint32_t numQueueFamilies;
    vkGetPhysicalDeviceQueueFamilyProperties(s_ctx.physicalDevice, &numQueueFamilies, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(s_ctx.physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

    const float graphicsQueuePrio = 0.0f;
    const float copyQueuePrio = 0.1f;
    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t copyFamily = UINT32_MAX;
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
    {
        const auto& properties = queueFamilyProperties[i];
        const VkQueueFlags flags = properties.queueFlags;

        if (copyFamily == UINT32_MAX &&
            (flags & VK_QUEUE_TRANSFER_BIT) &&
            !(flags & VK_QUEUE_GRAPHICS_BIT) &&
            !(flags & VK_QUEUE_COMPUTE_BIT))
        {
            copyFamily = i;
        }

        if (graphicsFamily == UINT32_MAX && (flags & VK_QUEUE_GRAPHICS_BIT))
        {
            graphicsFamily = i;
        }

        if (graphicsFamily != UINT32_MAX && copyFamily != UINT32_MAX)
        {
            break;
        }
    }

    if (copyFamily == UINT32_MAX)
    {
        copyFamily = graphicsFamily;
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.resize(2);

    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = graphicsFamily;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &graphicsQueuePrio;

    queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[1].queueFamilyIndex = copyFamily;
    queueCreateInfos[1].queueCount = 1;
    queueCreateInfos[1].pQueuePriorities = &copyQueuePrio;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &s_ctx.features2;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 2;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    VK_ASSERT(vkCreateDevice(s_ctx.physicalDevice, &deviceCreateInfo, nullptr, &s_ctx.device));

    // Initialize vma
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
    vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = s_ctx.physicalDevice;
    allocatorInfo.device = s_ctx.device;
    allocatorInfo.instance = s_ctx.instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    if (s_ctx.features_1_2.bufferDeviceAddress)
    {
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    VK_ASSERT(vmaCreateAllocator(&allocatorInfo, &s_ctx.allocator));
}

void Shutdown()
{
#ifdef VK_DEBUG
    if (s_ctx.debugMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(s_ctx.instance, s_ctx.debugMessenger, nullptr);
        s_ctx.debugMessenger = VK_NULL_HANDLE;
    }
#endif
    vmaDestroyAllocator(s_ctx.allocator);
    vkDestroyDevice(s_ctx.device, nullptr);
    vkDestroyInstance(s_ctx.instance, nullptr);
}
} // namespace rhi