#include "RHI.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define RHI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define RHI_MIN(a, b) (((a) < (b)) ? (a) : (b))

namespace RHI
{
struct StageBufferPool
{
    uint64_t size = 0;
    uint64_t offset = 0;
    Buffer* currentBuffer = nullptr;
};

struct Frame
{
    StageBufferPool stageBufferPool;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    std::vector<CommandBuffer*> cmdBuffers;
    std::vector<CommandBuffer*> submissions;
};

struct Context
{
    uint8_t frameIndex = 0;
    uint64_t frameCount = 0;

    VkDevice device = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;

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

    Frame frames[RHI_MAX_FRAMES_IN_FLIGHT] = {};

    // Present state
    std::vector<Swapchain*> presentSwapchains;
} s_ctx;

struct ResourceManager
{
    uint64_t frameCount = 0;
    std::mutex destroylocker;

    std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> destroyerImages;
    std::deque<std::pair<VkImageView, uint64_t>> destroyerImageViews;
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
    std::deque<std::pair<VkSwapchainKHR, uint64_t>> destroyerSwapchains;
    std::deque<std::pair<VkSurfaceKHR, uint64_t>> destroyerSurfaces;
    std::deque<std::pair<VkSemaphore, uint64_t>> destroyerSemaphores;
} s_resMgr;

static Frame& GetFrame() { return s_ctx.frames[GetFrameIndex()]; }

inline constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

inline constexpr uint64_t AlignTo(uint64_t value, uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

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

void ClearStageBufferPool(uint8_t frameIndex)
{
    Frame& frame = s_ctx.frames[frameIndex];
    frame.stageBufferPool.size = 0;
    frame.stageBufferPool.offset = 0;
    if (frame.stageBufferPool.currentBuffer)
    {
        DestroyBuffer(frame.stageBufferPool.currentBuffer);
        frame.stageBufferPool.currentBuffer = nullptr;
    }
}

void UpdateResourceMgr(uint64_t currentFrameCount, uint8_t maxFramesInFlight)
{
    s_resMgr.frameCount = currentFrameCount;

    auto processDestroyQueue = [&](auto& queue, auto destroyFunc)
    {
        while (!queue.empty())
        {
            if (queue.front().second + maxFramesInFlight < s_resMgr.frameCount)
            {
                auto item = queue.front();
                queue.pop_front();
                destroyFunc(item.first);
            }
            else
            {
                break;
            }
        }
    };

    processDestroyQueue(s_resMgr.destroyerImages,
                        [&](const auto& item)
                        {
                            vmaDestroyImage(s_ctx.allocator, item.first, item.second);
                        });

    processDestroyQueue(s_resMgr.destroyerImageViews,
                        [&](VkImageView imageView)
                        {
                            vkDestroyImageView(s_ctx.device, imageView, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerBuffers,
                        [&](const auto& item)
                        {
                            vmaDestroyBuffer(s_ctx.allocator, item.first, item.second);
                        });

    processDestroyQueue(s_resMgr.destroyerSamplers,
                        [&](VkSampler sampler)
                        {
                            vkDestroySampler(s_ctx.device, sampler, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerDescriptorPools,
                        [&](VkDescriptorPool pool)
                        {
                            vkDestroyDescriptorPool(s_ctx.device, pool, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerDescriptorSetLayouts,
                        [&](VkDescriptorSetLayout layout)
                        {
                            vkDestroyDescriptorSetLayout(s_ctx.device, layout, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerDescriptorUpdateTemplates,
                        [&](VkDescriptorUpdateTemplate template_)
                        {
                            vkDestroyDescriptorUpdateTemplate(s_ctx.device, template_, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerShaderModules,
                        [&](VkShaderModule shaderModule)
                        {
                            vkDestroyShaderModule(s_ctx.device, shaderModule, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerPipelineLayouts,
                        [&](VkPipelineLayout pipelineLayout)
                        {
                            vkDestroyPipelineLayout(s_ctx.device, pipelineLayout, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerPipelines,
                        [&](VkPipeline pipeline)
                        {
                            vkDestroyPipeline(s_ctx.device, pipeline, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerQueryPools,
                        [&](VkQueryPool queryPool)
                        {
                            vkDestroyQueryPool(s_ctx.device, queryPool, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerAccelerationStructures,
                        [&](VkAccelerationStructureKHR accelerationStructure)
                        {
                            vkDestroyAccelerationStructureKHR(s_ctx.device, accelerationStructure, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerSwapchains,
                        [&](VkSwapchainKHR swapchain)
                        {
                            vkDestroySwapchainKHR(s_ctx.device, swapchain, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerSemaphores,
                        [&](VkSemaphore semaphore)
                        {
                            vkDestroySemaphore(s_ctx.device, semaphore, nullptr);
                        });

    processDestroyQueue(s_resMgr.destroyerSurfaces,
                        [&](VkSurfaceKHR surface)
                        {
                            vkDestroySurfaceKHR(s_ctx.instance, surface, nullptr);
                        });
}

void CreateDynamicDescriptorPool(CommandBuffer* cmd)
{
    // Todo: Support ray tracing
    VkDescriptorPoolSize poolSizes[9] = {};
    uint32_t poolSizeCount = 0;
    uint32_t maxSets = cmd->maxSets;

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[1].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    poolSizes[2].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[4].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    poolSizes[5].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[6].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    poolSizes[7].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[7].descriptorCount = RHI_MAX_BINDING_COUNT * maxSets;
    poolSizeCount++;

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.poolSizeCount = poolSizeCount;
    poolCreateInfo.pPoolSizes = poolSizes;
    poolCreateInfo.maxSets = maxSets;
    VK_ASSERT(vkCreateDescriptorPool(s_ctx.device, &poolCreateInfo, nullptr, &cmd->descriptorPool));
}

void DestroyDynamicDescriptorPool(CommandBuffer* cmd)
{
    // Destroy operation may be multithreaded, thus requiring locking.
    s_resMgr.destroylocker.lock();
    s_resMgr.destroyerDescriptorPools.emplace_back(cmd->descriptorPool, s_ctx.frameCount);
    s_resMgr.destroylocker.unlock();
}

VkImageAspectFlags GetAspectMask(VkFormat format)
{
    VkImageAspectFlags result = 0;
    switch (format)
    {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            result = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        case VK_FORMAT_S8_UINT:
            result = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            result = VK_IMAGE_ASPECT_DEPTH_BIT;
            result |= VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        default:
            result = VK_IMAGE_ASPECT_COLOR_BIT;
            break;
    }
    return result;
}

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

    uint32_t queueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
    {
        const auto& properties = queueFamilyProperties[i];
        const VkQueueFlags flags = properties.queueFlags;

        if (queueFamily == UINT32_MAX && (flags & VK_QUEUE_GRAPHICS_BIT))
        {
            queueFamily = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &s_ctx.features2;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    VK_ASSERT(vkCreateDevice(s_ctx.physicalDevice, &deviceCreateInfo, nullptr, &s_ctx.device));

    vkGetDeviceQueue(s_ctx.device, queueFamily, 0, &s_ctx.queue);

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

    for (uint32_t i = 0; i < RHI_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        Frame& frame = s_ctx.frames[i];

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_ASSERT(vkCreateFence(s_ctx.device, &fenceCreateInfo, nullptr, &frame.fence));

        VkCommandPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.queueFamilyIndex = queueFamily;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VK_ASSERT(vkCreateCommandPool(s_ctx.device, &poolCreateInfo, nullptr, &frame.cmdPool));
    }
}

void Shutdown()
{
    vkDeviceWaitIdle(s_ctx.device);

#ifdef VK_DEBUG
    if (s_ctx.debugMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(s_ctx.instance, s_ctx.debugMessenger, nullptr);
        s_ctx.debugMessenger = VK_NULL_HANDLE;
    }
#endif
    for (uint32_t i = 0; i < RHI_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        Frame& frame = s_ctx.frames[i];

        ClearStageBufferPool(i);

        vkDestroyFence(s_ctx.device, frame.fence, nullptr);

        for (auto* cmd : frame.cmdBuffers)
        {
            if (cmd->descriptorPool != VK_NULL_HANDLE)
            {
                DestroyDynamicDescriptorPool(cmd);
            }
            delete cmd;
        }
        frame.cmdBuffers.clear();

        vkDestroyCommandPool(s_ctx.device, frame.cmdPool, nullptr);
    }

    UpdateResourceMgr(~0, 0);

    vmaDestroyAllocator(s_ctx.allocator);
    vkDestroyDevice(s_ctx.device, nullptr);
    vkDestroyInstance(s_ctx.instance, nullptr);
}

uint8_t GetFrameIndex()
{
    return s_ctx.frameIndex;
}

uint8_t GetMaxFramesInFlight()
{
    return RHI_MAX_FRAMES_IN_FLIGHT;
}

void CreateBuffer(const BufferDesc& desc, Buffer*& buffer)
{
    buffer = new Buffer();
    buffer->size = desc.size;
    buffer->memoryUsage = desc.memoryUsage;
    buffer->dynamicBuffer = desc.dynamicBuffer;
    buffer->currentWriteIndex = 0;
    buffer->lastWriteFrame = 0;

    VkDeviceSize allocSize = desc.dynamicBuffer ? desc.size * RHI_MAX_FRAMES_IN_FLIGHT : desc.size;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = allocSize;
    bufferCreateInfo.usage = desc.bufferUsage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (s_ctx.features_1_2.bufferDeviceAddress)
    {
        bufferCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = desc.memoryUsage;
    VK_ASSERT(vmaCreateBuffer(s_ctx.allocator, &bufferCreateInfo, &allocCreateInfo, &buffer->handle, &buffer->allocation, nullptr));

    if (bufferCreateInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer->handle;
        buffer->deviceAddress = vkGetBufferDeviceAddress(s_ctx.device, &addressInfo);
    }
}

void DestroyBuffer(Buffer* buffer)
{
    s_resMgr.destroyerBuffers.emplace_back(std::make_pair(buffer->handle, buffer->allocation), s_ctx.frameCount);
    delete buffer;
}

void* MapMemory(Buffer* buffer)
{
    void* mappedData = nullptr;
    vmaMapMemory(s_ctx.allocator, buffer->allocation, &mappedData);

    if (buffer->dynamicBuffer)
    {
        if (buffer->lastWriteFrame != s_ctx.frameCount)
        {
            buffer->currentWriteIndex = (buffer->currentWriteIndex + 1) % RHI_MAX_FRAMES_IN_FLIGHT;
            buffer->lastWriteFrame = s_ctx.frameCount;
        }
        return static_cast<uint8_t*>(mappedData) + buffer->currentWriteIndex * buffer->size;
    }

    return mappedData;
}

void UnmapMemory(Buffer* buffer)
{
    vmaUnmapMemory(s_ctx.allocator, buffer->allocation);
}

StageAllocation RequestStageBuffer(size_t size)
{
    Frame& frame = GetFrame();
    StageBufferPool& stageBufferPool = frame.stageBufferPool;

    const uint64_t freeSpace = stageBufferPool.size - stageBufferPool.offset;
    if (size > freeSpace || !stageBufferPool.currentBuffer)
    {
        if (stageBufferPool.currentBuffer)
        {
            DestroyBuffer(stageBufferPool.currentBuffer);
        }

        stageBufferPool.size = AlignTo((stageBufferPool.size + size) * 2, 8);
        stageBufferPool.offset = 0;

        BufferDesc bufferDesc = {};
        bufferDesc.size = stageBufferPool.size;
        bufferDesc.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
        CreateBuffer(bufferDesc, stageBufferPool.currentBuffer);
    }

    StageAllocation allocation;
    allocation.buffer = stageBufferPool.currentBuffer;
    allocation.offset = stageBufferPool.offset;
    stageBufferPool.offset += AlignTo(size, 8);
    return allocation;
}

void CreateTexture(const TextureDesc& desc, Texture*& texture)
{
    texture = new Texture();
    texture->width = desc.width;
    texture->height = desc.height;
    texture->depth = desc.depth;
    texture->levels = desc.levels;
    texture->layers = desc.layers;
    texture->format = desc.format;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = desc.imageType;
    imageCreateInfo.format = desc.format;
    imageCreateInfo.extent.width = desc.width;
    imageCreateInfo.extent.height = desc.height;
    imageCreateInfo.extent.depth = desc.depth;
    imageCreateInfo.mipLevels = desc.levels;
    imageCreateInfo.arrayLayers = desc.layers;
    imageCreateInfo.samples = desc.samples;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.flags = 0;
    imageCreateInfo.usage = desc.usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_ASSERT(vmaCreateImage(s_ctx.allocator, &imageCreateInfo, &allocCreateInfo, &texture->handle, &texture->allocation, nullptr));
}

void DestroyTexture(Texture* texture)
{
    s_resMgr.destroyerImages.emplace_back(std::make_pair(texture->handle, texture->allocation), s_ctx.frameCount);
    for (auto view : texture->views)
    {
        s_resMgr.destroyerImageViews.emplace_back(view.handle, s_ctx.frameCount);
    }
    delete texture;
}

int CreateTextureView(Texture* texture, VkImageViewType view_type, VkImageAspectFlags aspect_mask,
                      uint32_t base_level, uint32_t level_count,
                      uint32_t base_layer, uint32_t layer_count)
{
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.flags = 0;
    viewCreateInfo.image = texture->handle;
    viewCreateInfo.subresourceRange.aspectMask = aspect_mask;
    viewCreateInfo.subresourceRange.baseArrayLayer = base_layer;
    viewCreateInfo.subresourceRange.layerCount = layer_count;
    viewCreateInfo.subresourceRange.baseMipLevel = base_level;
    viewCreateInfo.subresourceRange.levelCount = level_count;
    viewCreateInfo.format = texture->format;
    viewCreateInfo.viewType = view_type;

    VkImageView imageView;
    VK_ASSERT(vkCreateImageView(s_ctx.device, &viewCreateInfo, nullptr, &imageView));

    TextureView view = {};
    view.handle = imageView;
    view.subresourceRange = viewCreateInfo.subresourceRange;
    texture->views.push_back(view);
    return int(texture->views.size()) - 1;
}

void CreateSampler(const EzSamplerDesc& desc, Sampler*& sampler)
{
    sampler = new Sampler();

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = desc.magFilter;
    samplerCreateInfo.minFilter = desc.minFilter;
    samplerCreateInfo.mipmapMode = desc.mipmapMode;
    samplerCreateInfo.addressModeU = desc.addressU;
    samplerCreateInfo.addressModeV = desc.addressV;
    samplerCreateInfo.addressModeW = desc.addressW;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.anisotropyEnable = desc.anisotropyEnable;
    samplerCreateInfo.maxAnisotropy = 0.0f;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = desc.compareEnable;
    samplerCreateInfo.compareOp = desc.compareOp;
    VK_ASSERT(vkCreateSampler(s_ctx.device, &samplerCreateInfo, nullptr, &sampler->handle));
}

void DestroySampler(Sampler* sampler)
{
    s_resMgr.destroyerSamplers.emplace_back(sampler->handle, s_ctx.frameCount);
    delete sampler;
}

void CreateShader(void* data, size_t size, Shader*& shader)
{
    shader = new Shader();

    VkShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = size;
    shaderCreateInfo.pCode = static_cast<const uint32_t*>(data);
    VK_ASSERT(vkCreateShaderModule(s_ctx.device, &shaderCreateInfo, nullptr, &shader->handle));

    // Parse shader
    SpvReflectResult reflectResult = spvReflectCreateShaderModule(size, shaderCreateInfo.pCode, &shader->reflect);

    shader->stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader->stageInfo.module = shader->handle;
    shader->stageInfo.pName = "main";
    shader->stageInfo.stage = static_cast<VkShaderStageFlagBits>(shader->reflect.shader_stage);

    // Get descriptor bindings
    uint32_t bindingCount = 0;
    reflectResult = spvReflectEnumerateDescriptorBindings(&shader->reflect, &bindingCount, nullptr);

    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    if (bindingCount > 0)
    {
        reflectResult = spvReflectEnumerateDescriptorBindings(&shader->reflect, &bindingCount, bindings.data());
    }

    // Get push constants
    uint32_t pushCount = 0;
    reflectResult = spvReflectEnumeratePushConstantBlocks(&shader->reflect, &pushCount, nullptr);

    std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
    if (pushCount > 0)
    {
        reflectResult = spvReflectEnumeratePushConstantBlocks(&shader->reflect, &pushCount, pushConstants.data());
    }

    // Process push constants
    for (const auto& pushConstant : pushConstants)
    {
        shader->pushConstants.size = pushConstant->size;
        shader->pushConstants.offset = pushConstant->offset;
        shader->pushConstants.stageFlags = shader->stageInfo.stage;
    }

    // Process descriptor bindings
    shader->layoutBindings.reserve(bindings.size());
    for (const auto& binding : bindings)
    {
        VkDescriptorSetLayoutBinding descriptor = {};
        descriptor.stageFlags = shader->stageInfo.stage;
        descriptor.binding = binding->binding;
        descriptor.descriptorCount = binding->count;
        descriptor.descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);
        shader->layoutBindings.push_back(descriptor);
    }
}

void DestroyShader(Shader* shader)
{
    spvReflectDestroyShaderModule(&shader->reflect);
    s_resMgr.destroyerShaderModules.emplace_back(shader->handle, s_ctx.frameCount);
    delete shader;
}

void CreatePipelineLayout(Pipeline*& pipeline, const std::vector<Shader*>& shaders)
{
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindingMap;

    auto processShader = [&](Shader* shader)
    {
        for (const auto& layoutBinding : shader->layoutBindings)
        {
            auto it = bindingMap.find(layoutBinding.binding);
            if (it != bindingMap.end())
            {
                it->second.stageFlags |= layoutBinding.stageFlags;
            }
            else
            {
                bindingMap[layoutBinding.binding] = layoutBinding;
                pipeline->layoutBindings.push_back(layoutBinding);
            }
        }

        if (shader->pushConstants.size > 0)
        {
            pipeline->pushConstants.offset = RHI_MIN(pipeline->pushConstants.offset, shader->pushConstants.offset);
            pipeline->pushConstants.size = RHI_MAX(pipeline->pushConstants.size, shader->pushConstants.size);
            pipeline->pushConstants.stageFlags |= shader->pushConstants.stageFlags;
        }
    };

    for (size_t i = 0; i < shaders.size(); ++i)
    {
        processShader(shaders[i]);
    }

    for (uint32_t i = 0; i < pipeline->layoutBindings.size(); ++i)
    {
        pipeline->bindingToIndexMap[pipeline->layoutBindings[i].binding] = i;
    }

    std::sort(pipeline->layoutBindings.begin(), pipeline->layoutBindings.end(),
              [](const auto& a, const auto& b)
              {
                  return a.binding < b.binding;
              });

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(pipeline->layoutBindings.size());
    descriptorSetLayoutCreateInfo.pBindings = pipeline->layoutBindings.data();
    VK_ASSERT(vkCreateDescriptorSetLayout(s_ctx.device, &descriptorSetLayoutCreateInfo, nullptr, &pipeline->descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &pipeline->descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = (pipeline->pushConstants.size > 0) ? 1u : 0u;
    pipelineLayoutCreateInfo.pPushConstantRanges = (pipeline->pushConstants.size > 0) ? &pipeline->pushConstants : nullptr;
    VK_ASSERT(vkCreatePipelineLayout(s_ctx.device, &pipelineLayoutCreateInfo, nullptr, &pipeline->pipelineLayout));
}

void CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, Pipeline*& pipeline)
{
    pipeline = new Pipeline();
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::vector<Shader*> shaders = {desc.vertexShader, desc.fragmentShader};
    CreatePipelineLayout(pipeline, shaders);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = pipeline->pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    // Shader stage
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (size_t i = 0; i < shaders.size(); ++i)
    {
        shaderStages.push_back(shaders[i]->stageInfo);
    }
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();

    // Input
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = desc.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;

    std::vector<VkVertexInputBindingDescription> inputBindings;
    std::vector<VkVertexInputAttributeDescription> inputAttributes;

    for (uint32_t bindingIndex = 0; bindingIndex < RHI_MAX_VERTEX_BUFFER_COUNT; bindingIndex++)
    {
        const auto& vertexBinding = desc.vertexLayout.bindings[bindingIndex];
        if (vertexBinding.stride != 0)
        {
            inputBindings.emplace_back();
            auto& inputBinding = inputBindings.back();
            inputBinding.binding = bindingIndex;
            inputBinding.stride = vertexBinding.stride;
            inputBinding.inputRate = vertexBinding.inputRate;
        }
    }

    for (uint32_t attributeIndex = 0; attributeIndex < RHI_MAX_VERTEX_ATTRIB_COUNT; attributeIndex++)
    {
        const auto& vertexAttrib = desc.vertexLayout.attributes[attributeIndex];
        if (vertexAttrib.format != VK_FORMAT_UNDEFINED)
        {
            inputAttributes.emplace_back();
            auto& inputAttribute = inputAttributes.back();
            inputAttribute.location = attributeIndex;
            inputAttribute.binding = vertexAttrib.binding;
            inputAttribute.format = vertexAttrib.format;
            inputAttribute.offset = vertexAttrib.offset;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(inputBindings.size());
    vertexInputCreateInfo.pVertexBindingDescriptions = inputBindings.data();
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(inputAttributes.size());
    vertexInputCreateInfo.pVertexAttributeDescriptions = inputAttributes.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_TRUE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = desc.fillMode;
    rasterizerCreateInfo.lineWidth = 1.0f;
    rasterizerCreateInfo.cullMode = desc.cullMode;
    rasterizerCreateInfo.frontFace = desc.frontFace;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizerCreateInfo.depthBiasClamp = 0.0f;
    rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;

    // MSAA
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = desc.multisampleState.sampleShading ? VK_TRUE : VK_FALSE;
    multisamplingCreateInfo.rasterizationSamples = desc.multisampleState.samples;
    multisamplingCreateInfo.alphaToCoverageEnable = desc.multisampleState.alphaToCoverage;
    multisamplingCreateInfo.alphaToOneEnable = desc.multisampleState.alphaToOne;
    pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;

    // Blend
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {};
    for (uint32_t i = 0; i < RHI_MAX_ATTACHMENT_COUNT; ++i)
    {
        if (!desc.renderPass || !desc.renderPass->desc.colors[i].texture)
            continue;
        
        VkPipelineColorBlendAttachmentState attachment = {};
        attachment.blendEnable = desc.blendState.blendEnable ? VK_TRUE : VK_FALSE;
        attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        attachment.srcColorBlendFactor = desc.blendState.srcColor;
        attachment.dstColorBlendFactor = desc.blendState.dstColor;
        attachment.colorBlendOp = desc.blendState.colorOp;
        attachment.srcAlphaBlendFactor = desc.blendState.srcAlpha;
        attachment.dstAlphaBlendFactor = desc.blendState.dstAlpha;
        attachment.alphaBlendOp = desc.blendState.alphaOp;
        blendAttachments.push_back(attachment);
    }

    VkPipelineColorBlendStateCreateInfo blendingCreateInfo = {};
    blendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendingCreateInfo.logicOpEnable = VK_FALSE;
    blendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    blendingCreateInfo.attachmentCount = blendAttachments.size();
    blendingCreateInfo.pAttachments = blendAttachments.data();
    blendingCreateInfo.blendConstants[0] = 1.0f;
    blendingCreateInfo.blendConstants[1] = 1.0f;
    blendingCreateInfo.blendConstants[2] = 1.0f;
    blendingCreateInfo.blendConstants[3] = 1.0f;
    pipelineCreateInfo.pColorBlendState = &blendingCreateInfo;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = desc.depthState.depthTest ? VK_TRUE : VK_FALSE;
    depthStencilCreateInfo.depthWriteEnable = desc.depthState.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencilCreateInfo.depthCompareOp = desc.depthState.depthFunc;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;

    depthStencilCreateInfo.stencilTestEnable = desc.stencilState.stencilTest ? VK_TRUE : VK_FALSE;

    // Front face stencil operations
    depthStencilCreateInfo.front.compareMask = desc.stencilState.stencilReadMask;
    depthStencilCreateInfo.front.writeMask = desc.stencilState.stencilWriteMask;
    depthStencilCreateInfo.front.reference = 0;
    depthStencilCreateInfo.front.compareOp = desc.stencilState.frontStencilFunc;
    depthStencilCreateInfo.front.passOp = desc.stencilState.frontStencilPassOp;
    depthStencilCreateInfo.front.failOp = desc.stencilState.frontStencilFailOp;
    depthStencilCreateInfo.front.depthFailOp = desc.stencilState.frontStencilDepthFailOp;

    // Back face stencil operations
    depthStencilCreateInfo.back.compareMask = desc.stencilState.stencilReadMask;
    depthStencilCreateInfo.back.writeMask = desc.stencilState.stencilWriteMask;
    depthStencilCreateInfo.back.reference = 0;
    depthStencilCreateInfo.back.compareOp = desc.stencilState.backStencilFunc;
    depthStencilCreateInfo.back.passOp = desc.stencilState.backStencilPassOp;
    depthStencilCreateInfo.back.failOp = desc.stencilState.backStencilFailOp;
    depthStencilCreateInfo.back.depthFailOp = desc.stencilState.backStencilDepthFailOp;

    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;

    // Tessellation
    VkPipelineTessellationStateCreateInfo tessellationCreateInfo = {};
    tessellationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationCreateInfo.patchControlPoints = 3;
    pipelineCreateInfo.pTessellationState = &tessellationCreateInfo;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = nullptr;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = nullptr;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;

    // Dynamic states
    VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;

    std::vector<VkFormat> colorFormats;
    
    if (desc.renderPass)
    {
        for (uint32_t i = 0; i < RHI_MAX_ATTACHMENT_COUNT; ++i)
        {
            if (desc.renderPass->desc.colors[i].texture)
            {
                colorFormats.push_back(desc.renderPass->desc.colors[i].texture->format);
            }
        }
    }
    
    VkPipelineRenderingCreateInfo renderingCreateInfo = {};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
    renderingCreateInfo.pColorAttachmentFormats = colorFormats.data();
    renderingCreateInfo.depthAttachmentFormat = (desc.renderPass && desc.renderPass->desc.depth.texture) ? desc.renderPass->desc.depth.texture->format : VK_FORMAT_UNDEFINED;
    renderingCreateInfo.stencilAttachmentFormat = (desc.renderPass && desc.renderPass->desc.stencil.texture) ? desc.renderPass->desc.stencil.texture->format : VK_FORMAT_UNDEFINED;
    pipelineCreateInfo.pNext = &renderingCreateInfo;

    VK_ASSERT(vkCreateGraphicsPipelines(s_ctx.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline->handle));
}

void CreateComputePipeline(const ComputePipelineDesc& desc, Pipeline*& pipeline)
{
    pipeline = new Pipeline();
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    CreatePipelineLayout(pipeline, {desc.computeShader});

    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage = desc.computeShader->stageInfo;
    pipelineCreateInfo.layout = pipeline->pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    VK_ASSERT(vkCreateComputePipelines(s_ctx.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline->handle));
}

void DestroyPipeline(Pipeline* pipeline)
{
    s_resMgr.destroyerPipelines.emplace_back(pipeline->handle, s_ctx.frameCount);
    s_resMgr.destroyerPipelineLayouts.emplace_back(pipeline->pipelineLayout, s_ctx.frameCount);
    s_resMgr.destroyerDescriptorSetLayouts.emplace_back(pipeline->descriptorSetLayout, s_ctx.frameCount);
    delete pipeline;
}

void CreateRenderPass(const RenderPassDesc& desc, RenderPass*& renderPass)
{
    renderPass = new RenderPass();
    renderPass->desc = desc;
    
    for (uint32_t i = 0; i < RHI_MAX_ATTACHMENT_COUNT; ++i)
    {
        if (desc.colors[i].texture)
        {
            renderPass->width = desc.colors[i].texture->width;
            renderPass->height = desc.colors[i].texture->height;
            break;
        }
    }
    
    if (renderPass->width == 0 && desc.depth.texture)
    {
        renderPass->width = desc.depth.texture->width;
        renderPass->height = desc.depth.texture->height;
    }
    
    if (renderPass->width == 0 && desc.stencil.texture)
    {
        renderPass->width = desc.stencil.texture->width;
        renderPass->height = desc.stencil.texture->height;
    }
}

void DestroyRenderPass(RenderPass* renderPass)
{
    delete renderPass;
}

void CreateSwapchain(void* window, Swapchain*& swapchain)
{
    swapchain = new Swapchain();

#ifdef WIN32
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = ::GetModuleHandle(nullptr);
    surfaceCreateInfo.hwnd = (HWND)window;
    VK_ASSERT(vkCreateWin32SurfaceKHR(s_ctx.instance, &surfaceCreateInfo, nullptr, &swapchain->surface));
#else
    #error "Unsupported platform for Vulkan surface creation"
#endif

    UpdateSwapchain(swapchain);
}

SwapchainStatus UpdateSwapchain(Swapchain* swapchain)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_ctx.physicalDevice, swapchain->surface, &surfaceCapabilities));
    uint32_t newWidth = surfaceCapabilities.currentExtent.width;
    uint32_t newHeight = surfaceCapabilities.currentExtent.height;

    if (newWidth == 0 || newHeight == 0)
    {
        return SwapchainStatus::NotReady;
    }

    if (swapchain->width == newWidth && swapchain->height == newHeight)
    {
        return SwapchainStatus::Ready;
    }

    swapchain->width = newWidth;
    swapchain->height = newHeight;

    VkSwapchainKHR oldHandle = swapchain->handle;
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = swapchain->surface;
    swapchainCreateInfo.minImageCount = RHI_MAX_FRAMES_IN_FLIGHT;
    swapchainCreateInfo.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCreateInfo.imageExtent.width = swapchain->width;
    swapchainCreateInfo.imageExtent.height = swapchain->height;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.oldSwapchain = oldHandle;
    VK_ASSERT(vkCreateSwapchainKHR(s_ctx.device, &swapchainCreateInfo, nullptr, &swapchain->handle));

    VK_ASSERT(vkGetSwapchainImagesKHR(s_ctx.device, swapchain->handle, &swapchain->imageCount, nullptr));
    swapchain->images.resize(swapchain->imageCount);
    VK_ASSERT(vkGetSwapchainImagesKHR(s_ctx.device, swapchain->handle, &swapchain->imageCount, swapchain->images.data()));

    if (oldHandle)
    {
        s_resMgr.destroyerSwapchains.emplace_back(oldHandle, s_ctx.frameCount);
    }
    for (uint32_t i = 0; i < RHI_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (swapchain->acquireSemaphores[i] != VK_NULL_HANDLE)
        {
            s_resMgr.destroyerSemaphores.emplace_back(swapchain->acquireSemaphores[i], s_ctx.frameCount);
        }
        if (swapchain->releaseSemaphores[i] != VK_NULL_HANDLE)
        {
            s_resMgr.destroyerSemaphores.emplace_back(swapchain->releaseSemaphores[i], s_ctx.frameCount);
        }
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < RHI_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VK_ASSERT(vkCreateSemaphore(s_ctx.device, &semaphoreCreateInfo, nullptr, &swapchain->acquireSemaphores[i]));
        VK_ASSERT(vkCreateSemaphore(s_ctx.device, &semaphoreCreateInfo, nullptr, &swapchain->releaseSemaphores[i]));
    }

    return SwapchainStatus::Resized;
}

void DestroySwapchain(Swapchain* swapchain)
{
    s_resMgr.destroyerSurfaces.emplace_back(swapchain->surface, s_ctx.frameCount);
    s_resMgr.destroyerSwapchains.emplace_back(swapchain->handle, s_ctx.frameCount);
    for (uint32_t i = 0; i < RHI_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        s_resMgr.destroyerSemaphores.emplace_back(swapchain->acquireSemaphores[i], s_ctx.frameCount);
        s_resMgr.destroyerSemaphores.emplace_back(swapchain->releaseSemaphores[i], s_ctx.frameCount);
    }
    delete swapchain;
}

CommandBuffer* RequestCommandBuffer()
{
    Frame& frame = GetFrame();

    CommandBuffer* newCmd = new CommandBuffer();
    frame.cmdBuffers.push_back(newCmd);

    VkCommandBufferAllocateInfo cmdInfo = {};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandBufferCount = 1;
    cmdInfo.commandPool = frame.cmdPool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VK_ASSERT(vkAllocateCommandBuffers(s_ctx.device, &cmdInfo, &newCmd->handle));

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmdBeginInfo.pInheritanceInfo = nullptr;
    VK_ASSERT(vkBeginCommandBuffer(newCmd->handle, &cmdBeginInfo));

    return newCmd;
}

void CmdCopyBuffer(CommandBuffer* cmd, Buffer* src, Buffer* dst, VkBufferCopy range)
{
    vkCmdCopyBuffer(cmd->handle, src->handle, dst->handle, 1, &range);
}

void CmdUploadBuffer(CommandBuffer* cmd, Buffer* buffer, uint32_t size, uint32_t offset, void* data)
{
    StageAllocation allocation = RequestStageBuffer(size);

    void* memoryPtr = MapMemory(allocation.buffer);
    memcpy((uint8_t*)memoryPtr + allocation.offset, data, size);
    UnmapMemory(allocation.buffer);

    VkBufferCopy range = {};
    range.size = size;
    range.srcOffset = allocation.offset;
    range.dstOffset = offset;
    CmdCopyBuffer(cmd, allocation.buffer, buffer, range);
}

void CmdCopyBufferToTexture(CommandBuffer* cmd, Buffer* src, Texture* dst, uint32_t srcOffset, const TextureRegion& region)
{
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = srcOffset;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = region.mipLevel;
    copyRegion.imageSubresource.baseArrayLayer = region.baseArrayLayer;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset.x = static_cast<int32_t>(region.x);
    copyRegion.imageOffset.y = static_cast<int32_t>(region.y);
    copyRegion.imageOffset.z = static_cast<int32_t>(region.z);
    copyRegion.imageExtent.width = region.width;
    copyRegion.imageExtent.height = region.height;
    copyRegion.imageExtent.depth = region.depth;

    vkCmdCopyBufferToImage(cmd->handle, src->handle, dst->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

void CmdUploadTexture(CommandBuffer* cmd, Texture* texture, const TextureRegion& region, void* data)
{
    uint32_t pixelSize = 4;
    switch (texture->format)
    {
        case VK_FORMAT_R8_UNORM:
            pixelSize = 1;
            break;
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_R8G8_UNORM:
            pixelSize = 2;
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            pixelSize = 4;
            break;
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            pixelSize = 8;
            break;
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            pixelSize = 16;
            break;
        default:
            assert(false && "Unsupported texture format for CmdUploadTexture");
            pixelSize = 4;
            break;
    }

    uint32_t dataSize = pixelSize * region.width * region.height * region.depth;

    StageAllocation allocation = RequestStageBuffer(dataSize);

    void* memoryPtr = MapMemory(allocation.buffer);
    memcpy((uint8_t*)memoryPtr + allocation.offset, data, dataSize);
    UnmapMemory(allocation.buffer);

    CmdCopyBufferToTexture(cmd, allocation.buffer, texture, allocation.offset, region);
}

void CmdCopyTexture(CommandBuffer* cmd, Texture* src, Texture* dst)
{
    std::vector<VkImageCopy> regions;
    regions.reserve(src->levels);

    for (uint32_t mip = 0; mip < src->levels; ++mip)
    {
        VkImageCopy region = {};
        region.srcSubresource.aspectMask = GetAspectMask(src->format);
        region.srcSubresource.mipLevel = mip;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = src->layers;
        region.srcOffset = {0, 0, 0};
        region.dstSubresource.aspectMask = GetAspectMask(dst->format);
        region.dstSubresource.mipLevel = mip;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = dst->layers;
        region.dstOffset = {0, 0, 0};
        region.extent.width = src->width >> mip;
        region.extent.height = src->height >> mip;
        region.extent.depth = src->depth;

        if (region.extent.width == 0) region.extent.width = 1;
        if (region.extent.height == 0) region.extent.height = 1;
        if (region.extent.depth == 0) region.extent.depth = 1;

        regions.push_back(region);
    }

    vkCmdCopyImage(cmd->handle, src->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dst->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   static_cast<uint32_t>(regions.size()), regions.data());
}

void CmdCopyTextureToSwapchain(CommandBuffer* cmd, Texture* src, Swapchain* dst)
{
    VkImageCopy region = {};
    region.srcSubresource.aspectMask = GetAspectMask(src->format);
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;
    region.srcOffset = {0, 0, 0};
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;
    region.dstOffset = {0, 0, 0};
    region.extent.width = src->width;
    region.extent.height = src->height;
    region.extent.depth = 1;

    VkImage dstImage = dst->images[dst->imageIndex];
    vkCmdCopyImage(cmd->handle, src->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void CmdBeginRenderPass(CommandBuffer* cmd, RenderPass* renderPass)
{
    auto toVkAttachment = [](const RenderPassAttachmentDesc& att, VkImageLayout layout, VkImageLayout resolveLayout) -> VkRenderingAttachmentInfo {
        VkRenderingAttachmentInfo vkAtt = {};
        vkAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        vkAtt.imageView = att.texture->views[att.textureView].handle;
        vkAtt.imageLayout = layout;
        vkAtt.loadOp = att.loadOp;
        vkAtt.storeOp = att.storeOp;
        vkAtt.clearValue = att.clearValue;

        if (att.resolveTexture)
        {
            vkAtt.resolveImageView = att.resolveTexture->views[att.resolveTextureView].handle;
            vkAtt.resolveImageLayout = resolveLayout;
            vkAtt.resolveMode = att.resolveMode;
        }
        return vkAtt;
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (uint32_t i = 0; i < RHI_MAX_ATTACHMENT_COUNT; ++i)
    {
        if (!renderPass->desc.colors[i].texture) continue;
        colorAttachments.push_back(toVkAttachment(renderPass->desc.colors[i],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }

    std::optional<VkRenderingAttachmentInfo> depthAttachment;
    if (renderPass->desc.depth.texture)
    {
        depthAttachment = toVkAttachment(renderPass->desc.depth,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    }

    std::optional<VkRenderingAttachmentInfo> stencilAttachment;
    if (renderPass->desc.stencil.texture)
    {
        stencilAttachment = toVkAttachment(renderPass->desc.stencil,
            VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL);
    }

    VkRenderingInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.extent.width = renderPass->width;
    info.renderArea.extent.height = renderPass->height;
    info.layerCount = 1;
    info.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    info.pColorAttachments = colorAttachments.data();
    info.pDepthAttachment = depthAttachment ? &depthAttachment.value() : nullptr;
    info.pStencilAttachment = stencilAttachment ? &stencilAttachment.value() : nullptr;

    vkCmdBeginRendering(cmd->handle, &info);
}

void CmdEndRenderPass(CommandBuffer* cmd)
{
    vkCmdEndRendering(cmd->handle);
}

void CmdSetScissor(CommandBuffer* cmd, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    VkRect2D scissor;
    scissor.extent.width = abs(right - left);
    scissor.extent.height = abs(top - bottom);
    scissor.offset.x = left;
    scissor.offset.y = top;
    vkCmdSetScissor(cmd->handle, 0, 1, &scissor);
}

void CmdSetViewport(CommandBuffer* cmd, float x, float y, float w, float h, float minDepth, float maxDepth)
{
    VkViewport viewport;
    viewport.x = x;
    viewport.y = y;
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    vkCmdSetViewport(cmd->handle, 0, 1, &viewport);
}

void CmdBindPipeline(CommandBuffer* cmd, Pipeline* pipeline)
{
    cmd->currentPipeline = pipeline;
    cmd->table.dirty = false;
    cmd->table.bindings.clear();

    vkCmdBindPipeline(cmd->handle, pipeline->bindPoint, pipeline->handle);
}

void CmdBindTexture(CommandBuffer* cmd, uint32_t binding, Texture* texture, int view)
{
    ResourceTable& table = cmd->table;
    table.dirty = true;

    for (auto i = table.bindings.size(); i < binding + 1; ++i)
    {
        table.bindings.emplace_back();
    }

    for (auto i = table.bindings[binding].images.size(); i < 1; ++i)
    {
        table.bindings[binding].images.emplace_back();
    }

    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    auto it = cmd->currentPipeline->bindingToIndexMap.find(binding);
    if (it != cmd->currentPipeline->bindingToIndexMap.end())
    {
        if (cmd->currentPipeline->layoutBindings[it->second].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            layout = VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    table.bindings[binding].images[0].imageView = texture->views[view].handle;
    table.bindings[binding].images[0].imageLayout = layout;
}

void CmdBindBuffer(CommandBuffer* cmd, uint32_t binding, Buffer* buffer, uint64_t size, uint64_t offset)
{
    ResourceTable& table = cmd->table;
    table.dirty = true;

    for (auto i = table.bindings.size(); i < binding + 1; ++i)
    {
        table.bindings.emplace_back();
    }

    for (auto i = table.bindings[binding].buffers.size(); i < 1; ++i)
    {
        table.bindings[binding].buffers.emplace_back();
    }

    uint64_t dynamicBufferOffset = buffer->dynamicBuffer ? buffer->currentWriteIndex * buffer->size : 0;
    table.bindings[binding].buffers[0].buffer = buffer->handle;
    table.bindings[binding].buffers[0].offset = offset + dynamicBufferOffset;
    table.bindings[binding].buffers[0].range = size > 0 ? size : buffer->size;
}

void CmdBindSampler(CommandBuffer* cmd, uint32_t binding, Sampler* sampler)
{
    ResourceTable& table = cmd->table;
    table.dirty = true;

    for (auto i = table.bindings.size(); i < binding + 1; ++i)
    {
        table.bindings.emplace_back();
    }

    for (auto i = table.bindings[binding].images.size(); i < 1; ++i)
    {
        table.bindings[binding].images.emplace_back();
    }

    table.bindings[binding].images[0].sampler = sampler->handle;
}

void CmdBindVertexBuffers(CommandBuffer* cmd, uint32_t firstBinding, uint32_t bindingCount, Buffer** buffers, uint64_t* offsets)
{
    std::vector<VkBuffer> vkBuffers;
    std::vector<VkDeviceSize> vkOffsets;
    vkBuffers.reserve(bindingCount);
    vkOffsets.reserve(bindingCount);

    for (uint32_t i = 0; i < bindingCount; ++i)
    {
        uint64_t dynamicBufferOffset = buffers[i]->dynamicBuffer ? buffers[i]->currentWriteIndex * buffers[i]->size : 0;
        vkBuffers.push_back(buffers[i]->handle);
        vkOffsets.push_back((offsets ? offsets[i] : 0) + dynamicBufferOffset);
    }

    vkCmdBindVertexBuffers(cmd->handle, firstBinding, bindingCount, vkBuffers.data(), vkOffsets.data());
}

void CmdBindIndexBuffer(CommandBuffer* cmd, Buffer* buffer, uint64_t offset, VkIndexType indexType)
{
    uint64_t dynamicBufferOffset = buffer->dynamicBuffer ? buffer->currentWriteIndex * buffer->size : 0;
    vkCmdBindIndexBuffer(cmd->handle, buffer->handle, offset + dynamicBufferOffset, indexType);
}

void CmdPushConstants(CommandBuffer* cmd, const void* data, uint32_t size)
{
    vkCmdPushConstants(cmd->handle,
                       cmd->currentPipeline->pipelineLayout,
                       cmd->currentPipeline->pushConstants.stageFlags,
                       0,
                       size,
                       data);
}

void FlushResourceBinding(CommandBuffer* cmd)
{
    Pipeline* pipeline = cmd->currentPipeline;
    ResourceTable& table = cmd->table;

    if (!table.dirty)
    {
        return;
    }
    table.dirty = false;

    if (cmd->descriptorPool == VK_NULL_HANDLE)
    {
        CreateDynamicDescriptorPool(cmd);
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = cmd->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &pipeline->descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(s_ctx.device, &allocInfo, &descriptorSet);
    while (res == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        cmd->maxSets *= 2;
        DestroyDynamicDescriptorPool(cmd);
        CreateDynamicDescriptorPool(cmd);
        allocInfo.descriptorPool = cmd->descriptorPool;
        res = vkAllocateDescriptorSets(s_ctx.device, &allocInfo, &descriptorSet);
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    for (int i = 0; i < pipeline->layoutBindings.size(); i++)
    {
        auto& layoutBinding = pipeline->layoutBindings[i];
        uint32_t binding = layoutBinding.binding;

        descriptorWrites.emplace_back();
        auto& write = descriptorWrites.back();
        write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstArrayElement = 0;
        write.descriptorType = layoutBinding.descriptorType;
        write.dstBinding = layoutBinding.binding;
        write.descriptorCount = layoutBinding.descriptorCount;

        switch (layoutBinding.descriptorType)
        {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                write.pImageInfo = table.bindings[binding].images.data();
            }
            break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                write.pBufferInfo = table.bindings[binding].buffers.data();
            }
            break;
            default:
                break;
        }
    }

    vkUpdateDescriptorSets(s_ctx.device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    vkCmdBindDescriptorSets(cmd->handle, pipeline->bindPoint, pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
}

void CmdDraw(CommandBuffer* cmd, uint32_t vertexCount, uint32_t vertexOffset)
{
    FlushResourceBinding(cmd);
    vkCmdDraw(cmd->handle, vertexCount, 1, vertexOffset, 0);
}

void CmdDraw(CommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t vertexOffset, uint32_t instanceOffset)
{
    FlushResourceBinding(cmd);
    vkCmdDraw(cmd->handle, vertexCount, instanceCount, vertexOffset, instanceOffset);
}

void CmdDrawIndexed(CommandBuffer* cmd, uint32_t indexCount, uint32_t indexOffset, int32_t vertexOffset)
{
    FlushResourceBinding(cmd);
    vkCmdDrawIndexed(cmd->handle, indexCount, 1, indexOffset, vertexOffset, 0);
}

void CmdDrawIndexed(CommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset, int32_t vertexOffset, uint32_t instanceOffset)
{
    FlushResourceBinding(cmd);
    vkCmdDrawIndexed(cmd->handle, indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
}

void CmdDrawIndirect(CommandBuffer* cmd, Buffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
    FlushResourceBinding(cmd);
    vkCmdDrawIndirect(cmd->handle, buffer->handle, offset, drawCount, stride);
}

void CmdDrawIndexedIndirect(CommandBuffer* cmd, Buffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
    FlushResourceBinding(cmd);
    vkCmdDrawIndexedIndirect(cmd->handle, buffer->handle, offset, drawCount, stride);
}

void CmdDispatch(CommandBuffer* cmd, uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
{
    FlushResourceBinding(cmd);
    vkCmdDispatch(cmd->handle, threadGroupX, threadGroupY, threadGroupZ);
}

void CmdDispatchIndirect(CommandBuffer* cmd, Buffer* buffer, uint64_t offset)
{
    FlushResourceBinding(cmd);
    vkCmdDispatchIndirect(cmd->handle, buffer->handle, offset);
}

void SubmitBarriers(CommandBuffer* cmd, std::span<GenericBarrier> genericBarriers, std::span<BufferBarrier> bufferBarriers, std::span<ImageBarrier> imageBarriers)
{
    if (genericBarriers.empty() && bufferBarriers.empty() && imageBarriers.empty())
    {
        return;
    }

    std::vector<VkMemoryBarrier2> vkGenericBarriers;
    vkGenericBarriers.reserve(genericBarriers.size());
    for (const auto& barrier : genericBarriers)
    {
        VkMemoryBarrier2 vkBarrier = {};
        vkBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        vkBarrier.pNext = nullptr;
        vkBarrier.srcStageMask = barrier.srcStage;
        vkBarrier.srcAccessMask = barrier.srcAccess;
        vkBarrier.dstStageMask = barrier.dstStage;
        vkBarrier.dstAccessMask = barrier.dstAccess;
        vkGenericBarriers.push_back(vkBarrier);
    }

    std::vector<VkBufferMemoryBarrier2> vkBufferBarriers;
    vkBufferBarriers.reserve(bufferBarriers.size());
    for (const auto& barrier : bufferBarriers)
    {
        VkBufferMemoryBarrier2 vkBarrier = {};
        vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        vkBarrier.pNext = nullptr;
        vkBarrier.srcStageMask = barrier.srcStage;
        vkBarrier.srcAccessMask = barrier.srcAccess;
        vkBarrier.dstStageMask = barrier.dstStage;
        vkBarrier.dstAccessMask = barrier.dstAccess;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.buffer = barrier.buffer ? barrier.buffer->handle : VK_NULL_HANDLE;
        vkBarrier.offset = 0;
        vkBarrier.size = VK_WHOLE_SIZE;
        vkBufferBarriers.push_back(vkBarrier);
    }

    std::vector<VkImageMemoryBarrier2> vkImageBarriers;
    vkImageBarriers.reserve(imageBarriers.size());
    for (const auto& barrier : imageBarriers)
    {
        VkImageMemoryBarrier2 vkBarrier = {};
        vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        vkBarrier.pNext = nullptr;
        vkBarrier.srcStageMask = barrier.srcStage;
        vkBarrier.srcAccessMask = barrier.srcAccess;
        vkBarrier.dstStageMask = barrier.dstStage;
        vkBarrier.dstAccessMask = barrier.dstAccess;
        vkBarrier.oldLayout = barrier.oldLayout;
        vkBarrier.newLayout = barrier.newLayout;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        
        if (barrier.swapchain)
        {
            vkBarrier.image = barrier.swapchain->images[barrier.swapchain->imageIndex];
            vkBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        else if (barrier.texture)
        {
            vkBarrier.image = barrier.texture->handle;
            vkBarrier.subresourceRange.aspectMask = GetAspectMask(barrier.texture->format);
        }
        
        vkBarrier.subresourceRange.baseMipLevel = 0;
        vkBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        vkBarrier.subresourceRange.baseArrayLayer = 0;
        vkBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        vkImageBarriers.push_back(vkBarrier);
    }

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.memoryBarrierCount = static_cast<uint32_t>(vkGenericBarriers.size());
    dependencyInfo.pMemoryBarriers = vkGenericBarriers.data();
    dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(vkBufferBarriers.size());
    dependencyInfo.pBufferMemoryBarriers = vkBufferBarriers.data();
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(vkImageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = vkImageBarriers.data();

    vkCmdPipelineBarrier2(cmd->handle, &dependencyInfo);
}

void CmdBeginPipelineBarrier(CommandBuffer* cmd)
{
    cmd->inBarrierScope = true;
    cmd->scopedGenericBarriers.clear();
    cmd->scopedBufferBarriers.clear();
    cmd->scopedImageBarriers.clear();
}

void CmdPipelineBarrier(CommandBuffer* cmd, GenericBarrier genericBarrier)
{
    if (cmd->inBarrierScope)
    {
        cmd->scopedGenericBarriers.push_back(genericBarrier);
    }
    else
    {
        SubmitBarriers(cmd, std::span<GenericBarrier>(&genericBarrier, 1), {}, {});
    }
}

void CmdPipelineBarrier(CommandBuffer* cmd, BufferBarrier bufferBarrier)
{
    if (cmd->inBarrierScope)
    {
        cmd->scopedBufferBarriers.push_back(bufferBarrier);
    }
    else
    {
        SubmitBarriers(cmd, {}, std::span<BufferBarrier>(&bufferBarrier, 1), {});
    }
}

void CmdPipelineBarrier(CommandBuffer* cmd, ImageBarrier imageBarrier)
{
    if (cmd->inBarrierScope)
    {
        cmd->scopedImageBarriers.push_back(imageBarrier);
    }
    else
    {
        SubmitBarriers(cmd, {}, {}, std::span<ImageBarrier>(&imageBarrier, 1));
    }
}

void CmdEndPipelineBarrier(CommandBuffer* cmd)
{
    SubmitBarriers(
        cmd,
        cmd->scopedGenericBarriers,
        cmd->scopedBufferBarriers,
        cmd->scopedImageBarriers
    );

    cmd->inBarrierScope = false;
    cmd->scopedGenericBarriers.clear();
    cmd->scopedBufferBarriers.clear();
    cmd->scopedImageBarriers.clear();
}

void Submit(CommandBuffer* cmd)
{
    vkEndCommandBuffer(cmd->handle);

    Frame& frame = GetFrame();
    frame.submissions.push_back(cmd);
}

void AcquireNextImage(Swapchain* swapchain)
{
    uint32_t frameIndex = GetFrameIndex();
    VK_ASSERT(vkAcquireNextImageKHR(s_ctx.device, swapchain->handle, UINT64_MAX, swapchain->acquireSemaphores[frameIndex], VK_NULL_HANDLE, &swapchain->imageIndex));
}

void Present(Swapchain* swapchain)
{
    s_ctx.presentSwapchains.push_back(swapchain);
}

void WaitIdle()
{
    vkDeviceWaitIdle(s_ctx.device);
}

void NextFrame()
{
    {
        Frame& frame = GetFrame();
        uint32_t frameIndex = GetFrameIndex();

        std::vector<VkCommandBuffer> commandBuffers;
        for (uint32_t i = 0; i < frame.submissions.size(); ++i)
        {
            CommandBuffer* cmd = frame.submissions[i];
            commandBuffers.push_back(cmd->handle);
        }

        std::vector<VkSemaphore> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitStages;
        std::vector<VkSemaphore> signalSemaphores;

        for (auto* swapchain : s_ctx.presentSwapchains)
        {
            if (swapchain->acquireSemaphores[frameIndex] != VK_NULL_HANDLE)
            {
                waitSemaphores.push_back(swapchain->acquireSemaphores[frameIndex]);
                waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
            if (swapchain->releaseSemaphores[frameIndex] != VK_NULL_HANDLE)
            {
                signalSemaphores.push_back(swapchain->releaseSemaphores[frameIndex]);
            }
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
        submitInfo.commandBufferCount = commandBuffers.size();
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.empty() ? nullptr : signalSemaphores.data();

        VK_ASSERT(vkQueueSubmit(s_ctx.queue, 1, &submitInfo, frame.fence));
        frame.submissions.clear();

        if (!s_ctx.presentSwapchains.empty())
        {
            std::vector<VkSwapchainKHR> swapchains;
            std::vector<uint32_t> imageIndices;
            std::vector<VkSemaphore> presentWaitSemaphores;

            for (auto* swapchain : s_ctx.presentSwapchains)
            {
                swapchains.push_back(swapchain->handle);
                imageIndices.push_back(swapchain->imageIndex);
                if (swapchain->releaseSemaphores[frameIndex] != VK_NULL_HANDLE)
                {
                    presentWaitSemaphores.push_back(swapchain->releaseSemaphores[frameIndex]);
                }
            }

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = static_cast<uint32_t>(presentWaitSemaphores.size());
            presentInfo.pWaitSemaphores = presentWaitSemaphores.empty() ? nullptr : presentWaitSemaphores.data();
            presentInfo.swapchainCount = static_cast<uint32_t>(swapchains.size());
            presentInfo.pSwapchains = swapchains.data();
            presentInfo.pImageIndices = imageIndices.data();

            VK_ASSERT(vkQueuePresentKHR(s_ctx.queue, &presentInfo));
            s_ctx.presentSwapchains.clear();
        }
    }

    s_ctx.frameCount++;
    s_ctx.frameIndex = s_ctx.frameCount % RHI_MAX_FRAMES_IN_FLIGHT;

    {
        Frame& frame = GetFrame();

        if (s_ctx.frameCount >= RHI_MAX_FRAMES_IN_FLIGHT)
        {
            VK_ASSERT(vkWaitForFences(s_ctx.device, 1, &frame.fence, true, UINT64_MAX));
            VK_ASSERT(vkResetFences(s_ctx.device, 1, &frame.fence));
            VK_ASSERT(vkResetCommandPool(s_ctx.device, frame.cmdPool, 0));
        }

        for (auto* cmd : frame.cmdBuffers)
        {
            if (cmd->descriptorPool != VK_NULL_HANDLE)
            {
                DestroyDynamicDescriptorPool(cmd);
            }
            delete cmd;
        }
        frame.cmdBuffers.clear();
        ClearStageBufferPool(s_ctx.frameIndex);
        UpdateResourceMgr(s_ctx.frameCount, RHI_MAX_FRAMES_IN_FLIGHT);
    }
}
} // namespace RHI