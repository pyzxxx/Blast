#pragma once

#include "Foundation/Log.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>

#include <deque>
#include <vector>

#define VK_DEBUG

#define VK_ASSERT(x)                                              \
    do                                                            \
    {                                                             \
        if (x != VK_SUCCESS)                                      \
        {                                                         \
            LOGE("Vulkan error at %s:%d.\n", __FILE__, __LINE__); \
            abort();                                              \
        }                                                         \
    } while (0)

namespace rhi
{
enum QueueType
{
    QUEUE_GRAPHICS = 0,
    QUEUE_COPY = 1
};

struct BufferDesc
{
    size_t size;
    VmaMemoryUsage memoryUsage;
    VkBufferUsageFlags bufferUsage;
};

struct Buffer
{
    VkBuffer handle;

    VmaAllocation allocation;
    VkDeviceAddress deviceAddress;

    size_t size;
    VmaMemoryUsage memoryUsage;
};

void Startup();
void Shutdown();
} // namespace rhi