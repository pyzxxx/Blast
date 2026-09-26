#include "gpu_driver.h"
#include "handle_pool.h"
#include "log.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#define NOMINMAX
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace {

constexpr uint32_t k_max_swapchain_images = 8;
constexpr uint32_t k_max_push_data_size = 32;
constexpr uint32_t k_max_layout_bindings = 16;
constexpr uint32_t k_pool_sampled_images = 65536;
constexpr uint32_t k_pool_storage_images = 4096;
constexpr uint32_t k_pool_storage_buffers = 1024;
constexpr uint32_t k_pool_samplers = 64;
constexpr uint32_t k_pool_max_sets = 16;

struct VulkanSwapchain {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkImage images[k_max_swapchain_images] = {};
    GpuImageView views[k_max_swapchain_images] = {};
    uint32_t image_count = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {};
    GpuImage gpu_images[k_max_swapchain_images] = {};
    VkFence acquire_fence = VK_NULL_HANDLE;
    VkSemaphore present_sems[k_max_swapchain_images] = {};
};

struct VulkanQueue {
    VkQueue handle = VK_NULL_HANDLE;
};

struct VulkanCmdPool {
    VkCommandPool handle = VK_NULL_HANDLE;
    std::vector<GpuCmd> cmds;
};

struct VulkanSemaphore {
    VkSemaphore handle = VK_NULL_HANDLE;
};

struct VulkanImage {
    VkImage handle = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t array_layers = 1;
    uint32_t mip_levels = 1;
    GpuImageType image_type = GpuImageType::D2;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    bool owned = true;
};

struct VulkanImageView {
    VkImageView handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct VulkanBuffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    uint64_t address = 0;
    uint64_t size = 0;
    void* mapped = nullptr;
};

struct VulkanPipeline {
    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct VulkanDescriptorSetLayout {
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;
    GpuLayoutBinding bindings[k_max_layout_bindings] = {};
    uint32_t binding_count = 0;
};

struct VulkanDescriptorSet {
    VkDescriptorSet handle = VK_NULL_HANDLE;
    GpuDescriptorSetLayout layout = k_gpu_invalid;
};

struct VulkanSampler {
    VkSampler handle = VK_NULL_HANDLE;
};

struct VulkanRenderPass {
    VkFormat color_formats[k_max_color_attachments] = {};
    VkImageView color_views[k_max_color_attachments] = {};
    float color_clears[k_max_color_attachments][4] = {};
    bool color_loads[k_max_color_attachments] = {};
    uint32_t color_count = 0;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkImageView depth_view = VK_NULL_HANDLE;
    float depth_clear = 1.0f;
    bool depth_load = false;
    bool has_depth = false;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct VulkanShader {
    VkShaderModule handle = VK_NULL_HANDLE;
};

struct SemaphoreValue {
    VkSemaphore sem;
    uint64_t value;
};

struct VulkanCommandBuffer {
    VkCommandBuffer handle = VK_NULL_HANDLE;
    std::vector<SemaphoreValue> waits;
    std::vector<SemaphoreValue> signals;
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    VkPhysicalDeviceMemoryProperties mem_props = {};
};

Context g_ctx;
HandlePool<VulkanQueue> g_queues;
HandlePool<VulkanCmdPool> g_cmd_pools;
HandlePool<VulkanSemaphore> g_semaphores;
HandlePool<VulkanSwapchain> g_swapchains;
HandlePool<VulkanImage> g_images;
HandlePool<VulkanBuffer> g_buffers;
HandlePool<VulkanCommandBuffer> g_cmds;
HandlePool<VulkanPipeline> g_pipelines;
HandlePool<VulkanShader> g_shaders;
HandlePool<VulkanImageView> g_image_views;
HandlePool<VulkanRenderPass> g_render_passes;
HandlePool<VulkanDescriptorSetLayout> g_set_layouts;
HandlePool<VulkanDescriptorSet> g_sets;
HandlePool<VulkanSampler> g_samplers;

constexpr uint32_t k_queue_cap = 8;
constexpr uint32_t k_cmd_pool_cap = 8;
constexpr uint32_t k_semaphore_cap = 256;
constexpr uint32_t k_swapchain_cap = 8;
constexpr uint32_t k_image_cap = 1024;
constexpr uint32_t k_buffer_cap = 1024;
constexpr uint32_t k_cmd_cap = 256;
constexpr uint32_t k_pipeline_cap = 64;
constexpr uint32_t k_shader_cap = 64;
constexpr uint32_t k_image_view_cap = 256;
constexpr uint32_t k_render_pass_cap = 64;
constexpr uint32_t k_set_layout_cap = 16;
constexpr uint32_t k_set_cap = 16;
constexpr uint32_t k_sampler_cap = 64;

#define VK_CHECK(x)                                                                   \
    do {                                                                              \
        VkResult check_result = (x);                                                  \
        if (check_result != VK_SUCCESS) {                                             \
            LOGE("%s failed: %d (%s:%d)", #x, static_cast<int32_t>(check_result), __FILE__, __LINE__); \
            return false;                                                             \
        }                                                                             \
    } while (false)

VkQueue universal_queue() {
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(g_ctx.device, g_ctx.queue_family, 0, &queue);
    return queue;
}

struct StateInfo {
    VkImageLayout layout;
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
};

StateInfo state_info(ResourceState state) {
    switch (state) {
        case ResourceState::Undefined:
            return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT};
        case ResourceState::PresentSrc:
            return {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT};
        case ResourceState::ColorAttachment:
            return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        case ResourceState::ShaderRead:
            return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        case ResourceState::StorageWrite:
            return {VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        case ResourceState::TransferSrc:
            return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
        case ResourceState::TransferDst:
            return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
        case ResourceState::DepthWrite:
            return {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT};
        case ResourceState::DepthRead:
            return {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT};
    }

    return {};
}

VkFormat to_vk_format(GpuFormat format) {
    switch (format) {
        case GpuFormat::R8Unorm:
            return VK_FORMAT_R8_UNORM;
        case GpuFormat::R8G8B8A8Unorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case GpuFormat::B8G8R8A8Unorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case GpuFormat::D32Float:
            return VK_FORMAT_D32_SFLOAT;
        case GpuFormat::R32G32Float:
            return VK_FORMAT_R32G32_SFLOAT;
        case GpuFormat::R32G32B32Float:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case GpuFormat::R32G32B32A32Float:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case GpuFormat::R16G16B16A16Float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case GpuFormat::R32G32Uint:
            return VK_FORMAT_R32G32_UINT;
        case GpuFormat::BC1RgbaUnorm:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case GpuFormat::BC2Unorm:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case GpuFormat::BC3Unorm:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case GpuFormat::BC4Unorm:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case GpuFormat::BC5Unorm:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case GpuFormat::BC6HUfloat:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case GpuFormat::BC6HSfloat:
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case GpuFormat::BC7Unorm:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case GpuFormat::BC7Srgb:
            return VK_FORMAT_BC7_SRGB_BLOCK;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

VkImageAspectFlags to_vk_aspect(GpuFormat format) {
    return format == GpuFormat::D32Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageAspectFlags to_vk_aspect_flags(GpuImageAspect aspect) {
    VkImageAspectFlags flags = 0;
    if (aspect & GpuImageAspect::Color) {
        flags |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (aspect & GpuImageAspect::Depth) {
        flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (aspect & GpuImageAspect::Stencil) {
        flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return flags;
}

VkComponentSwizzle to_vk_swizzle(GpuSwizzle swizzle) {
    switch (swizzle) {
        case GpuSwizzle::Identity:
            return VK_COMPONENT_SWIZZLE_IDENTITY;
        case GpuSwizzle::Zero:
            return VK_COMPONENT_SWIZZLE_ZERO;
        case GpuSwizzle::One:
            return VK_COMPONENT_SWIZZLE_ONE;
        case GpuSwizzle::R:
            return VK_COMPONENT_SWIZZLE_R;
        case GpuSwizzle::G:
            return VK_COMPONENT_SWIZZLE_G;
        case GpuSwizzle::B:
            return VK_COMPONENT_SWIZZLE_B;
        case GpuSwizzle::A:
            return VK_COMPONENT_SWIZZLE_A;
    }

    return VK_COMPONENT_SWIZZLE_IDENTITY;
}

VkDescriptorType to_vk_desc_type(GpuDescriptorType type) {
    switch (type) {
        case GpuDescriptorType::SampledImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case GpuDescriptorType::StorageImage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case GpuDescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case GpuDescriptorType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
    }

    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
}

VkFilter to_vk_filter(GpuFilter filter) {
    switch (filter) {
        case GpuFilter::Nearest:
            return VK_FILTER_NEAREST;
        case GpuFilter::Linear:
            return VK_FILTER_LINEAR;
    }

    return VK_FILTER_NEAREST;
}

VkSamplerMipmapMode to_vk_mipmap_mode(GpuMipmapMode mode) {
    switch (mode) {
        case GpuMipmapMode::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case GpuMipmapMode::Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode to_vk_address_mode(GpuAddressMode mode) {
    switch (mode) {
        case GpuAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case GpuAddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case GpuAddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case GpuAddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case GpuAddressMode::MirrorClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }

    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkCompareOp to_vk_compare_op(GpuCompareOp op) {
    switch (op) {
        case GpuCompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case GpuCompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case GpuCompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case GpuCompareOp::LessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GpuCompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case GpuCompareOp::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case GpuCompareOp::GreaterOrEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case GpuCompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
    }

    return VK_COMPARE_OP_NEVER;
}

VkBorderColor to_vk_border_color(GpuBorderColor color) {
    switch (color) {
        case GpuBorderColor::FloatTransparentBlack:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case GpuBorderColor::IntTransparentBlack:
            return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        case GpuBorderColor::FloatOpaqueBlack:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case GpuBorderColor::IntOpaqueBlack:
            return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        case GpuBorderColor::FloatOpaqueWhite:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case GpuBorderColor::IntOpaqueWhite:
            return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    }

    return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
}

GpuDescriptorType binding_type_of(GpuDescriptorSetLayout layout, uint32_t binding) {
    VulkanDescriptorSetLayout& l = g_set_layouts.resolve(layout);
    for (uint32_t i = 0; i < l.binding_count; ++i) {
        if (l.bindings[i].binding == binding) {
            return l.bindings[i].type;
        }
    }

    assert(false && "write_descriptor: binding not in layout");
    return GpuDescriptorType::SampledImage;
}

VkImageViewType view_type_for(GpuImageType type, uint32_t layer_count) {
    switch (type) {
    case GpuImageType::D2:
        return layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    case GpuImageType::D3:
        return VK_IMAGE_VIEW_TYPE_3D;
    case GpuImageType::Cube:
        return layer_count > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
    }

    return VK_IMAGE_VIEW_TYPE_2D;
}

GpuImageView alloc_image_view(const VulkanImage& img, const GpuImageViewDesc& desc) {
    bool inherit = desc.type == GpuImageType::D2 && desc.base_mip == 0 && desc.mip_count == 0
        && desc.base_layer == 0 && desc.layer_count == 0;
    GpuImageType type = inherit ? img.image_type : desc.type;
    uint32_t mip_count = desc.mip_count == 0 ? img.mip_levels - desc.base_mip : desc.mip_count;
    uint32_t layer_count = desc.layer_count == 0 ? img.array_layers - desc.base_layer : desc.layer_count;
    VkFormat format = desc.format == GpuFormat::Undefined ? img.format : to_vk_format(desc.format);
    VkImageAspectFlags aspect = desc.aspect == GpuImageAspect::None ? img.aspect : to_vk_aspect_flags(desc.aspect);

    VkImageViewCreateInfo vci = {};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = img.handle;
    vci.viewType = view_type_for(type, layer_count);
    vci.format = format;
    vci.components = {to_vk_swizzle(desc.swizzle_r), to_vk_swizzle(desc.swizzle_g),
                      to_vk_swizzle(desc.swizzle_b), to_vk_swizzle(desc.swizzle_a)};
    vci.subresourceRange = {aspect, desc.base_mip, mip_count, desc.base_layer, layer_count};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(g_ctx.device, &vci, nullptr, &view) != VK_SUCCESS) {
        LOGE("vkCreateImageView failed");
        return k_gpu_invalid;
    }

    return g_image_views.alloc(VulkanImageView{view, format, img.width, img.height});
}

bool create_swapchain_internal(VulkanSwapchain& sc, uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR caps = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.physical, sc.surface, &caps));

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.physical, sc.surface, &format_count, nullptr);
    VkSurfaceFormatKHR formats[16] = {};
    if (format_count > 16) {
        format_count = 16;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.physical, sc.surface, &format_count, formats);

    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = formats[i];
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFF) {
        extent.width = width;
        extent.height = height;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    if (image_count > k_max_swapchain_images) {
        LOGE("swapchain image count %u exceeds storage", image_count);
        return false;
    }

    VkSwapchainCreateInfoKHR ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = sc.surface;
    ci.minImageCount = image_count;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(g_ctx.device, &ci, nullptr, &sc.handle));

    vkGetSwapchainImagesKHR(g_ctx.device, sc.handle, &sc.image_count, nullptr);
    vkGetSwapchainImagesKHR(g_ctx.device, sc.handle, &sc.image_count, sc.images);

    for (uint32_t i = 0; i < sc.image_count; ++i) {
        VulkanImage img = {};
        img.handle = sc.images[i];
        img.width = extent.width;
        img.height = extent.height;
        img.format = chosen.format;
        img.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        img.owned = false;
        sc.gpu_images[i] = g_images.alloc(img);
        sc.views[i] = alloc_image_view(img, {});
    }

    for (uint32_t i = 0; i < sc.image_count; ++i) {
        VkSemaphoreCreateInfo sem_ci = {};
        sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(g_ctx.device, &sem_ci, nullptr, &sc.present_sems[i]));
    }

    sc.format = chosen.format;
    sc.extent = extent;
    return true;
}

void destroy_swapchain_internal(VulkanSwapchain& sc) {
    for (uint32_t i = 0; i < sc.image_count; ++i) {
        if (sc.present_sems[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_ctx.device, sc.present_sems[i], nullptr);
        }

        sc.present_sems[i] = VK_NULL_HANDLE;
        if (sc.views[i] != k_gpu_invalid) {
            vkDestroyImageView(g_ctx.device, g_image_views.resolve(sc.views[i]).handle, nullptr);
            g_image_views.free(sc.views[i]);
        }

        sc.views[i] = k_gpu_invalid;
        if (sc.gpu_images[i] != k_gpu_invalid) {
            g_images.free(sc.gpu_images[i]);
        }

        sc.gpu_images[i] = k_gpu_invalid;
    }

    if (sc.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_ctx.device, sc.handle, nullptr);
    }

    sc.handle = VK_NULL_HANDLE;
    sc.image_count = 0;
}

} // namespace

bool GpuDriver::init() {
    owner_thread_ = std::this_thread::get_id();
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("volkInitialize failed");
        return false;
    }

    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layer_count = 0;
    bool validation = false;
#ifndef NDEBUG
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> props(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, props.data());
    for (const auto& p : props) {
        if (strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            validation = true;
        }
    }

    if (!validation) {
        LOGW("VK_LAYER_KHRONOS_validation not available");
    }
#endif

    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = extensions;
#ifndef NDEBUG
    if (validation) {
        ici.enabledLayerCount = 1;
        ici.ppEnabledLayerNames = layers;
    }
#endif
    VK_CHECK(vkCreateInstance(&ici, nullptr, &g_ctx.instance));
    volkLoadInstance(g_ctx.instance);

    uint32_t pd_count = 0;
    vkEnumeratePhysicalDevices(g_ctx.instance, &pd_count, nullptr);
    std::vector<VkPhysicalDevice> pds(pd_count);
    vkEnumeratePhysicalDevices(g_ctx.instance, &pd_count, pds.data());

    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    uint32_t fallback_family = 0;
    for (VkPhysicalDevice pd : pds) {
        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> q_props(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &q_count, q_props.data());
        for (uint32_t i = 0; i < q_count; ++i) {
            VkQueueFlags need = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
            if ((q_props[i].queueFlags & need) != need) {
                continue;
            }

            VkPhysicalDeviceProperties props = {};
            vkGetPhysicalDeviceProperties(pd, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                g_ctx.physical = pd;
                g_ctx.queue_family = i;
                break;
            }

            if (fallback == VK_NULL_HANDLE) {
                fallback = pd;
                fallback_family = i;
            }
        }

        if (g_ctx.physical != VK_NULL_HANDLE) {
            break;
        }
    }

    if (g_ctx.physical == VK_NULL_HANDLE) {
        g_ctx.physical = fallback;
        g_ctx.queue_family = fallback_family;
    }

    if (g_ctx.physical == VK_NULL_HANDLE) {
        LOGE("no graphics|compute|transfer queue family found");
        return false;
    }

    vkGetPhysicalDeviceMemoryProperties(g_ctx.physical, &g_ctx.mem_props);

    VkPhysicalDeviceVulkan12Features f12_support = {};
    f12_support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceFeatures2 f2 = {};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.pNext = &f12_support;
    vkGetPhysicalDeviceFeatures2(g_ctx.physical, &f2);
    if (!f2.features.shaderInt64 || !f12_support.bufferDeviceAddress || !f12_support.timelineSemaphore || !f12_support.runtimeDescriptorArray ||
        !f12_support.descriptorBindingPartiallyBound || !f12_support.descriptorBindingVariableDescriptorCount ||
        !f12_support.descriptorBindingSampledImageUpdateAfterBind ||
        !f12_support.descriptorBindingStorageImageUpdateAfterBind ||
        !f12_support.descriptorBindingStorageBufferUpdateAfterBind ||
        !f12_support.shaderSampledImageArrayNonUniformIndexing ||
        !f12_support.shaderStorageBufferArrayNonUniformIndexing) {
        LOGE("Vulkan 1.2 descriptor indexing features insufficient");
        return false;
    }

    VkPhysicalDeviceVulkan12Properties p12 = {};
    p12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    VkPhysicalDeviceProperties2 p2 = {};
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &p12;
    vkGetPhysicalDeviceProperties2(g_ctx.physical, &p2);
    if (p12.maxPerStageDescriptorUpdateAfterBindSampledImages < k_pool_sampled_images ||
        p12.maxPerStageUpdateAfterBindResources < k_pool_sampled_images) {
        LOGE("update-after-bind limits insufficient (per-stage sampled %u, resources %u)",
             p12.maxPerStageDescriptorUpdateAfterBindSampledImages, p12.maxPerStageUpdateAfterBindResources);
        return false;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci = {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_ctx.queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkPhysicalDeviceVulkan12Features f12 = {};
    f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    f12.bufferDeviceAddress = VK_TRUE;
    f12.timelineSemaphore = VK_TRUE;
    f12.runtimeDescriptorArray = VK_TRUE;
    f12.descriptorBindingPartiallyBound = VK_TRUE;
    f12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    f12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    f12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceVulkan13Features f13 = {};
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    f13.shaderDemoteToHelperInvocation = VK_TRUE;
    f13.pNext = &f12;

    VkPhysicalDeviceFeatures features = {};
    features.shaderInt64 = VK_TRUE;

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci = {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &f13;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = device_extensions;
    VK_CHECK(vkCreateDevice(g_ctx.physical, &dci, nullptr, &g_ctx.device));
    volkLoadDevice(g_ctx.device);

    VmaVulkanFunctions vma_funcs = {};
    vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo aci = {};
    aci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    aci.physicalDevice = g_ctx.physical;
    aci.device = g_ctx.device;
    aci.instance = g_ctx.instance;
    aci.pVulkanFunctions = &vma_funcs;
    VK_CHECK(vmaCreateAllocator(&aci, &g_ctx.allocator));

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, k_pool_sampled_images},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_pool_storage_images},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, k_pool_storage_buffers},
        {VK_DESCRIPTOR_TYPE_SAMPLER, k_pool_samplers},
    };
    VkDescriptorPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pci.maxSets = k_pool_max_sets;
    pci.poolSizeCount = 4;
    pci.pPoolSizes = pool_sizes;
    VK_CHECK(vkCreateDescriptorPool(g_ctx.device, &pci, nullptr, &g_ctx.desc_pool));

    if (!g_queues.init(k_queue_cap) || !g_cmd_pools.init(k_cmd_pool_cap) || !g_semaphores.init(k_semaphore_cap) ||
        !g_swapchains.init(k_swapchain_cap) || !g_images.init(k_image_cap) || !g_buffers.init(k_buffer_cap) ||
        !g_cmds.init(k_cmd_cap) || !g_pipelines.init(k_pipeline_cap) || !g_shaders.init(k_shader_cap) ||
        !g_image_views.init(k_image_view_cap) || !g_render_passes.init(k_render_pass_cap) ||
        !g_set_layouts.init(k_set_layout_cap) || !g_sets.init(k_set_cap) || !g_samplers.init(k_sampler_cap)) {
        return false;
    }

    return true;
}

void GpuDriver::shutdown() {
    g_queues.shutdown();
    g_cmd_pools.shutdown();
    g_semaphores.shutdown();
    g_swapchains.shutdown();
    g_images.shutdown();
    g_buffers.shutdown();
    g_cmds.shutdown();
    g_pipelines.shutdown();
    g_shaders.shutdown();
    g_image_views.shutdown();
    g_render_passes.shutdown();
    g_set_layouts.shutdown();
    g_sets.shutdown();
    g_samplers.shutdown();
    vkDestroyDescriptorPool(g_ctx.device, g_ctx.desc_pool, nullptr);
    vmaDestroyAllocator(g_ctx.allocator);
    vkDestroyDevice(g_ctx.device, nullptr);
    vkDestroyInstance(g_ctx.instance, nullptr);
}

void GpuDriver::wait_idle() {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDeviceWaitIdle(g_ctx.device);
}

GpuQueue GpuDriver::create_queue() {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_queues.alloc(VulkanQueue{universal_queue()});
}

void GpuDriver::destroy_queue(GpuQueue handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    g_queues.free(handle);
}

GpuCmdPool GpuDriver::create_cmd_pool() {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkCommandPoolCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = g_ctx.queue_family;

    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(g_ctx.device, &ci, nullptr, &pool) != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed");
        return k_gpu_invalid;
    }

    return g_cmd_pools.alloc(VulkanCmdPool{pool});
}

void GpuDriver::destroy_cmd_pool(GpuCmdPool handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanCmdPool& p = g_cmd_pools.resolve(handle);
    vkDestroyCommandPool(g_ctx.device, p.handle, nullptr);
    for (uint32_t idx : p.cmds) {
        g_cmds.free_index(idx);
    }

    g_cmd_pools.free(handle);
}

void GpuDriver::reset_cmd_pool(GpuCmdPool handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanCmdPool& p = g_cmd_pools.resolve(handle);
    vkResetCommandPool(g_ctx.device, p.handle, 0);
    for (uint32_t idx : p.cmds) {
        g_cmds.free_index(idx);
    }

    p.cmds.clear();
}

GpuCmd GpuDriver::start_command_recording(GpuCmdPool handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanCmdPool& p = g_cmd_pools.resolve(handle);

    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = p.handle;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer buf = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(g_ctx.device, &ai, &buf) != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed");
        return k_gpu_invalid;
    }

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(buf, &bi);

    GpuCmd cmd = g_cmds.alloc(VulkanCommandBuffer{buf, {}, {}});
    if (cmd == k_gpu_invalid) {
        return k_gpu_invalid;
    }

    p.cmds.push_back(g_cmds.index_of(cmd));
    return cmd;
}

void GpuDriver::submit(GpuQueue queue, std::span<GpuCmd> cmds) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    std::vector<VkSubmitInfo2> infos(cmds.size());
    std::vector<VkCommandBufferSubmitInfo> cbi_storage(cmds.size());
    std::vector<std::vector<VkSemaphoreSubmitInfo>> wait_storage(cmds.size());
    std::vector<std::vector<VkSemaphoreSubmitInfo>> signal_storage(cmds.size());

    for (uint32_t i = 0; i < cmds.size(); ++i) {
        VulkanCommandBuffer& rec = g_cmds.resolve(cmds[i]);
        vkEndCommandBuffer(rec.handle);

        for (auto& w : rec.waits) {
            VkSemaphoreSubmitInfo si = {};
            si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            si.semaphore = w.sem;
            si.value = w.value;
            si.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            wait_storage[i].push_back(si);
        }

        for (auto& s : rec.signals) {
            VkSemaphoreSubmitInfo si = {};
            si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            si.semaphore = s.sem;
            si.value = s.value;
            si.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            signal_storage[i].push_back(si);
        }

        cbi_storage[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cbi_storage[i].commandBuffer = rec.handle;

        infos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        infos[i].waitSemaphoreInfoCount = static_cast<uint32_t>(wait_storage[i].size());
        infos[i].pWaitSemaphoreInfos = wait_storage[i].data();
        infos[i].commandBufferInfoCount = 1;
        infos[i].pCommandBufferInfos = &cbi_storage[i];
        infos[i].signalSemaphoreInfoCount = static_cast<uint32_t>(signal_storage[i].size());
        infos[i].pSignalSemaphoreInfos = signal_storage[i].data();

        rec.waits.clear();
        rec.signals.clear();
    }

    vkQueueSubmit2(g_queues.resolve(queue).handle, static_cast<uint32_t>(infos.size()), infos.data(), VK_NULL_HANDLE);
}

void GpuDriver::cmd_wait_semaphore(GpuCmd handle, GpuSemaphore sem, uint64_t value) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    g_cmds.resolve(handle).waits.push_back({g_semaphores.resolve(sem).handle, value});
}

void GpuDriver::cmd_signal_semaphore(GpuCmd handle, GpuSemaphore sem, uint64_t value) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    g_cmds.resolve(handle).signals.push_back({g_semaphores.resolve(sem).handle, value});
}

GpuSemaphore GpuDriver::create_semaphore(uint64_t init_value) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkSemaphoreTypeCreateInfo ti = {};
    ti.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    ti.initialValue = init_value;
    VkSemaphoreCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    ci.pNext = &ti;

    VkSemaphore sem = VK_NULL_HANDLE;
    if (vkCreateSemaphore(g_ctx.device, &ci, nullptr, &sem) != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed");
        return k_gpu_invalid;
    }

    return g_semaphores.alloc(VulkanSemaphore{sem});
}

void GpuDriver::wait_semaphore(GpuSemaphore handle, uint64_t value) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkSemaphore sem = g_semaphores.resolve(handle).handle;
    VkSemaphoreWaitInfo wi = {};
    wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wi.semaphoreCount = 1;
    wi.pSemaphores = &sem;
    wi.pValues = &value;
    vkWaitSemaphores(g_ctx.device, &wi, UINT64_MAX);
}

uint64_t GpuDriver::semaphore_value(GpuSemaphore handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    uint64_t value = 0;
    vkGetSemaphoreCounterValue(g_ctx.device, g_semaphores.resolve(handle).handle, &value);
    return value;
}

void GpuDriver::destroy_semaphore(GpuSemaphore handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDestroySemaphore(g_ctx.device, g_semaphores.resolve(handle).handle, nullptr);
    g_semaphores.free(handle);
}

GpuBuffer GpuDriver::create_buffer(const GpuBufferDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkBufferCreateInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = desc.size;
    bi.usage = 0;
    if (desc.usage & GpuBufferUsage::Storage) {
        bi.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if (desc.usage & GpuBufferUsage::Index) {
        bi.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (desc.usage & GpuBufferUsage::Vertex) {
        bi.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if (desc.usage & GpuBufferUsage::DeviceAddress) {
        bi.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    if (desc.usage & GpuBufferUsage::TransferSrc) {
        bi.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    if (desc.usage & GpuBufferUsage::TransferDst) {
        bi.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    if (bi.size == 0 || bi.usage == 0) {
        LOGE("create_buffer: invalid desc");
        return k_gpu_invalid;
    }

    bool host_visible = desc.memory == GpuMemoryDomain::Upload;
    VmaAllocationCreateInfo mai = {};
    if (host_visible) {
        mai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        mai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    } else {
        mai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    }

    VulkanBuffer buf = {};
    buf.size = desc.size;
    if (vmaCreateBuffer(g_ctx.allocator, &bi, &mai, &buf.handle, &buf.allocation, nullptr) != VK_SUCCESS) {
        LOGE("vmaCreateBuffer failed");
        return k_gpu_invalid;
    }

    if (host_visible) {
        VmaAllocationInfo alloc_info = {};
        vmaGetAllocationInfo(g_ctx.allocator, buf.allocation, &alloc_info);
        assert((g_ctx.mem_props.memoryTypes[alloc_info.memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);
        if (vmaMapMemory(g_ctx.allocator, buf.allocation, &buf.mapped) != VK_SUCCESS) {
            LOGE("vmaMapMemory failed");
            vmaDestroyBuffer(g_ctx.allocator, buf.handle, buf.allocation);
            return k_gpu_invalid;
        }
    }

    if (desc.usage & GpuBufferUsage::DeviceAddress) {
        VkBufferDeviceAddressInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        ai.buffer = buf.handle;
        buf.address = vkGetBufferDeviceAddress(g_ctx.device, &ai);
    }

    return g_buffers.alloc(buf);
}

void GpuDriver::destroy_buffer(GpuBuffer handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanBuffer& buf = g_buffers.resolve(handle);
    if (buf.allocation != nullptr) {
        if (buf.mapped != nullptr) {
            vmaUnmapMemory(g_ctx.allocator, buf.allocation);
        }

        vmaDestroyBuffer(g_ctx.allocator, buf.handle, buf.allocation);
    } else if (buf.handle != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_ctx.device, buf.handle, nullptr);
    }

    g_buffers.free(handle);
}

uint64_t GpuDriver::gpu_address(GpuBuffer handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_buffers.resolve(handle).address;
}

void* GpuDriver::buffer_mapped(GpuBuffer handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_buffers.resolve(handle).mapped;
}

void GpuDriver::buffer_unmap(GpuBuffer handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanBuffer& buf = g_buffers.resolve(handle);
    if (buf.mapped == nullptr) {
        return;
    }

    vmaFlushAllocation(g_ctx.allocator, buf.allocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(g_ctx.allocator, buf.allocation);
    buf.mapped = nullptr;
}

GpuImage GpuDriver::create_image(const GpuImageDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkFormat format = to_vk_format(desc.format);
    uint32_t array_layers = desc.type == GpuImageType::Cube ? 6 : desc.array_layers;
    bool bad_samples = desc.samples == 0 || (desc.samples & (desc.samples - 1)) != 0 || desc.samples > 64;
    if (format == VK_FORMAT_UNDEFINED || desc.width == 0 || desc.height == 0 || desc.mip_levels == 0
        || array_layers == 0 || bad_samples
        || (desc.type == GpuImageType::Cube && (desc.width != desc.height || desc.samples != 1))
        || (desc.type == GpuImageType::D3 && (desc.depth == 0 || desc.array_layers != 1 || desc.samples != 1))
        || (desc.samples != 1 && desc.type != GpuImageType::D2)) {
        LOGE("create_image: invalid desc");
        return k_gpu_invalid;
    }

    GpuImageUsage usage = desc.usage;
    if (usage == GpuImageUsage::None) {
        usage = GpuImageUsage::Sampled | GpuImageUsage::TransferDst;
    }

    VkImageCreateInfo ii = {};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = desc.type == GpuImageType::D3 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ii.flags = desc.type == GpuImageType::Cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    ii.format = format;
    ii.extent = {desc.width, desc.height, desc.type == GpuImageType::D3 ? desc.depth : 1};
    ii.mipLevels = desc.mip_levels;
    ii.arrayLayers = array_layers;
    ii.samples = (VkSampleCountFlagBits)desc.samples;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = 0;
    if (usage & GpuImageUsage::Sampled) {
        ii.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (usage & GpuImageUsage::Storage) {
        ii.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (usage & GpuImageUsage::ColorAttachment) {
        ii.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (usage & GpuImageUsage::DepthAttachment) {
        ii.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (usage & GpuImageUsage::TransferSrc) {
        ii.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (usage & GpuImageUsage::TransferDst) {
        ii.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo mai = {};
    mai.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VulkanImage img = {};
    if (vmaCreateImage(g_ctx.allocator, &ii, &mai, &img.handle, &img.allocation, nullptr) != VK_SUCCESS) {
        LOGE("vmaCreateImage failed");
        return k_gpu_invalid;
    }

    img.aspect = to_vk_aspect(desc.format);
    img.width = desc.width;
    img.height = desc.height;
    img.depth = desc.type == GpuImageType::D3 ? desc.depth : 1;
    img.array_layers = array_layers;
    img.mip_levels = desc.mip_levels;
    img.image_type = desc.type;
    img.format = format;
    return g_images.alloc(img);
}

void GpuDriver::image_size(GpuImage handle, uint32_t& width, uint32_t& height) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    const VulkanImage& img = g_images.resolve(handle);
    width = img.width;
    height = img.height;
}

void GpuDriver::destroy_image(GpuImage handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanImage& img = g_images.resolve(handle);
    if (img.owned) {
        if (img.allocation != nullptr) {
            vmaDestroyImage(g_ctx.allocator, img.handle, img.allocation);
        } else if (img.handle != VK_NULL_HANDLE) {
            vkDestroyImage(g_ctx.device, img.handle, nullptr);
        }
    }    g_images.free(handle);
}

GpuImageView GpuDriver::create_image_view(GpuImage image, const GpuImageViewDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return alloc_image_view(g_images.resolve(image), desc);
}

void GpuDriver::destroy_image_view(GpuImageView handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDestroyImageView(g_ctx.device, g_image_views.resolve(handle).handle, nullptr);
    g_image_views.free(handle);
}

void GpuDriver::cmd_barrier(GpuCmd handle, ResourceState from, ResourceState to) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    StateInfo src = state_info(from);
    StateInfo dst = state_info(to);

    VkMemoryBarrier2 b = {};
    b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    b.srcStageMask = src.stage;
    b.srcAccessMask = src.access;
    b.dstStageMask = dst.stage;
    b.dstAccessMask = dst.access;

    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &b;
    vkCmdPipelineBarrier2(g_cmds.resolve(handle).handle, &dep);
}

void GpuDriver::cmd_buffer_barrier(GpuCmd handle, GpuBuffer buffer, ResourceState from, ResourceState to) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    StateInfo src = state_info(from);
    StateInfo dst = state_info(to);

    VkBufferMemoryBarrier2 b = {};
    b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    b.srcStageMask = src.stage;
    b.srcAccessMask = src.access;
    b.dstStageMask = dst.stage;
    b.dstAccessMask = dst.access;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.buffer = g_buffers.resolve(buffer).handle;
    b.offset = 0;
    b.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &b;
    vkCmdPipelineBarrier2(g_cmds.resolve(handle).handle, &dep);
}

void GpuDriver::cmd_image_barrier(GpuCmd handle, GpuImage image, ResourceState from, ResourceState to, uint32_t base_mip, uint32_t mip_count) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    StateInfo src = state_info(from);
    StateInfo dst = state_info(to);
    VulkanImage& img = g_images.resolve(image);

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = src.stage;
    barrier.srcAccessMask = src.access;
    barrier.dstStageMask = dst.stage;
    barrier.dstAccessMask = dst.access;
    barrier.oldLayout = src.layout;
    barrier.newLayout = dst.layout;
    barrier.image = img.handle;
    uint32_t mips = mip_count == 0 ? img.mip_levels - base_mip : mip_count;
    barrier.subresourceRange = {img.aspect, base_mip, mips, 0, img.array_layers};

    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(g_cmds.resolve(handle).handle, &dep);
}

static VkCullModeFlags to_vk_cull(GpuCull cull) {
    switch (cull) {
        case GpuCull::None:
            return VK_CULL_MODE_NONE;
        case GpuCull::Front:
            return VK_CULL_MODE_FRONT_BIT;
        default:
            return VK_CULL_MODE_BACK_BIT;
    }
}

static VkPrimitiveTopology to_vk_topology(GpuTopology topology) {
    switch (topology) {
        case GpuTopology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case GpuTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case GpuTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case GpuTopology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case GpuTopology::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static VkFrontFace to_vk_front_face(GpuFrontFace face) {
    switch (face) {
        case GpuFrontFace::CounterClockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case GpuFrontFace::Clockwise:
            return VK_FRONT_FACE_CLOCKWISE;
    }

    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

static VkPolygonMode to_vk_polygon_mode(GpuPolygonMode mode) {
    switch (mode) {
        case GpuPolygonMode::Fill:
            return VK_POLYGON_MODE_FILL;
        case GpuPolygonMode::Line:
            return VK_POLYGON_MODE_LINE;
        case GpuPolygonMode::Point:
            return VK_POLYGON_MODE_POINT;
    }

    return VK_POLYGON_MODE_FILL;
}

static VkBlendFactor to_vk_blend_factor(GpuBlendFactor factor) {
    switch (factor) {
        case GpuBlendFactor::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case GpuBlendFactor::One:
            return VK_BLEND_FACTOR_ONE;
        case GpuBlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case GpuBlendFactor::OneMinusSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case GpuBlendFactor::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case GpuBlendFactor::OneMinusDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case GpuBlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case GpuBlendFactor::OneMinusSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case GpuBlendFactor::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case GpuBlendFactor::OneMinusDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case GpuBlendFactor::SrcAlphaSaturate:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }

    return VK_BLEND_FACTOR_ONE;
}

static VkBlendOp to_vk_blend_op(GpuBlendOp op) {
    switch (op) {
        case GpuBlendOp::Add:
            return VK_BLEND_OP_ADD;
        case GpuBlendOp::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case GpuBlendOp::ReverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case GpuBlendOp::Min:
            return VK_BLEND_OP_MIN;
        case GpuBlendOp::Max:
            return VK_BLEND_OP_MAX;
    }

    return VK_BLEND_OP_ADD;
}

static VkStencilOp to_vk_stencil_op(GpuStencilOp op) {
    switch (op) {
        case GpuStencilOp::Keep:
            return VK_STENCIL_OP_KEEP;
        case GpuStencilOp::Zero:
            return VK_STENCIL_OP_ZERO;
        case GpuStencilOp::Replace:
            return VK_STENCIL_OP_REPLACE;
        case GpuStencilOp::IncrementAndClamp:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case GpuStencilOp::DecrementAndClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case GpuStencilOp::Invert:
            return VK_STENCIL_OP_INVERT;
        case GpuStencilOp::IncrementAndWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case GpuStencilOp::DecrementAndWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }

    return VK_STENCIL_OP_KEEP;
}

static VkColorComponentFlags to_vk_color_mask(GpuColorComponent mask) {
    return static_cast<VkColorComponentFlags>(mask & GpuColorComponent::RGBA);
}

static VkSampleCountFlagBits to_vk_sample_count(uint32_t samples) {
    switch (samples) {
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
        case 64:
            return VK_SAMPLE_COUNT_64_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

static VkStencilOpState to_vk_stencil_face(const GpuStencilFace& face, uint8_t read_mask, uint8_t write_mask) {
    VkStencilOpState s = {};
    s.failOp = to_vk_stencil_op(face.fail_op);
    s.passOp = to_vk_stencil_op(face.pass_op);
    s.depthFailOp = to_vk_stencil_op(face.depth_fail_op);
    s.compareOp = to_vk_compare_op(face.compare);
    s.compareMask = read_mask;
    s.writeMask = write_mask;
    s.reference = 0;
    return s;
}

GpuShader GpuDriver::create_shader(std::span<const uint32_t> spv) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkShaderModuleCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spv.size() * sizeof(uint32_t);
    ci.pCode = spv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(g_ctx.device, &ci, nullptr, &module) != VK_SUCCESS) {
        LOGE("vkCreateShaderModule failed");
        return k_gpu_invalid;
    }

    return g_shaders.alloc(VulkanShader{module});
}

void GpuDriver::destroy_shader(GpuShader handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDestroyShaderModule(g_ctx.device, g_shaders.resolve(handle).handle, nullptr);
    g_shaders.free(handle);
}

GpuRenderPass GpuDriver::create_render_pass(const GpuRenderPassDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    assert(desc.color_count <= k_max_color_attachments);
    assert(desc.color_count > 0 || desc.depth.view != k_gpu_invalid);
    VulkanRenderPass rp = {};
    rp.color_count = desc.color_count;
    for (uint32_t i = 0; i < rp.color_count; ++i) {
        const VulkanImageView& v = g_image_views.resolve(desc.colors[i].view);
        rp.color_views[i] = v.handle;
        rp.color_formats[i] = v.format;
        memcpy(rp.color_clears[i], desc.colors[i].clear, sizeof(float) * 4);
        rp.color_loads[i] = desc.colors[i].load;
        if (rp.width == 0) {
            rp.width = v.width;
            rp.height = v.height;
        }

        assert(v.width == rp.width && v.height == rp.height);
    }

    if (desc.depth.view != k_gpu_invalid) {
        const VulkanImageView& v = g_image_views.resolve(desc.depth.view);
        rp.has_depth = true;
        rp.depth_view = v.handle;
        rp.depth_format = v.format;
        rp.depth_clear = desc.depth.clear;
        rp.depth_load = desc.depth.load;
        if (rp.width == 0) {
            rp.width = v.width;
            rp.height = v.height;
        }

        assert(v.width == rp.width && v.height == rp.height);
    }

    return g_render_passes.alloc(rp);
}

void GpuDriver::destroy_render_pass(GpuRenderPass handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    g_render_passes.free(handle);
}

GpuPipeline GpuDriver::create_pipeline(const GpuPipelineDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    bool is_compute = desc.cs != k_gpu_invalid;
    if (is_compute && (desc.vs != k_gpu_invalid || desc.fs != k_gpu_invalid)) {
        LOGE("create_pipeline: cs mixed with graphics stages");
        return k_gpu_invalid;
    }

    if (!is_compute && desc.vs == k_gpu_invalid) {
        LOGE("create_pipeline: graphics pipeline requires vs");
        return k_gpu_invalid;
    }

    uint32_t layout_count = desc.set_layout_count;
    assert(layout_count <= k_max_pipeline_sets);

    VkDescriptorSetLayout set_layouts[k_max_pipeline_sets] = {};
    for (uint32_t i = 0; i < layout_count; ++i) {
        set_layouts[i] = g_set_layouts.resolve(desc.set_layouts[i]).handle;
    }

    VulkanPipeline pipe = {};
    VkPushConstantRange push_range = {};
    push_range.stageFlags = VK_SHADER_STAGE_ALL;
    push_range.offset = 0;
    push_range.size = k_max_push_data_size;

    VkPipelineLayoutCreateInfo lci = {};
    lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.setLayoutCount = layout_count;
    lci.pSetLayouts = set_layouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(g_ctx.device, &lci, nullptr, &pipe.layout) != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed");
        return k_gpu_invalid;
    }

    VkResult result = VK_ERROR_UNKNOWN;

    if (is_compute) {
        VkPipelineShaderStageCreateInfo stage = {};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = g_shaders.resolve(desc.cs).handle;
        stage.pName = "main";

        VkComputePipelineCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage = stage;
        ci.layout = pipe.layout;

        result = vkCreateComputePipelines(g_ctx.device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe.handle);
        pipe.bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    } else {
        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = g_shaders.resolve(desc.vs).handle;
        stages[0].pName = "main";
        uint32_t stage_count = 1;
        if (desc.fs != k_gpu_invalid) {
            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = g_shaders.resolve(desc.fs).handle;
            stages[1].pName = "main";
            stage_count = 2;
        }

        VkVertexInputBindingDescription bindings[2] = {};
        VkVertexInputAttributeDescription attributes[k_max_vertex_attrs] = {};
        assert(desc.vertex_input.attr_count <= k_max_vertex_attrs);
        bool stream_used[2] = {};
        for (uint32_t i = 0; i < desc.vertex_input.attr_count; ++i) {
            const GpuVertexAttr& a = desc.vertex_input.attrs[i];
            assert(a.stream < 2);
            stream_used[a.stream] = true;
            attributes[i] = {a.location, a.stream, to_vk_format(a.format), a.offset};
        }

        uint32_t binding_count = 0;
        for (uint32_t s = 0; s < 2; ++s) {
            if (!stream_used[s]) {
                continue;
            }

            bindings[binding_count++] = {s, desc.vertex_input.stream_strides[s], VK_VERTEX_INPUT_RATE_VERTEX};
        }

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount = binding_count;
        vi.pVertexBindingDescriptions = bindings;
        vi.vertexAttributeDescriptionCount = desc.vertex_input.attr_count;
        vi.pVertexAttributeDescriptions = attributes;

        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = to_vk_topology(desc.topology);

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs = {};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.depthClampEnable = desc.raster.depth_clamp ? VK_TRUE : VK_FALSE;
        rs.polygonMode = to_vk_polygon_mode(desc.raster.polygon_mode);
        rs.cullMode = to_vk_cull(desc.raster.cull);
        rs.frontFace = to_vk_front_face(desc.raster.front_face);
        rs.depthBiasEnable = desc.raster.depth_bias_enable ? VK_TRUE : VK_FALSE;
        rs.depthBiasConstantFactor = desc.raster.depth_bias_constant;
        rs.depthBiasClamp = desc.raster.depth_bias_clamp;
        rs.depthBiasSlopeFactor = desc.raster.depth_bias_slope;
        rs.lineWidth = desc.raster.line_width;

        VkPipelineMultisampleStateCreateInfo ms = {};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = to_vk_sample_count(desc.multisample.samples);
        ms.sampleShadingEnable = desc.multisample.sample_shading ? VK_TRUE : VK_FALSE;
        ms.minSampleShading = desc.multisample.min_sample_shading;
        ms.alphaToCoverageEnable = desc.multisample.alpha_to_coverage ? VK_TRUE : VK_FALSE;
        ms.alphaToOneEnable = desc.multisample.alpha_to_one ? VK_TRUE : VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo ds = {};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = desc.depth.depth_test ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable = desc.depth.depth_write ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp = to_vk_compare_op(desc.depth.depth_compare);
        ds.stencilTestEnable = desc.stencil.stencil_test ? VK_TRUE : VK_FALSE;
        ds.front = to_vk_stencil_face(desc.stencil.front, desc.stencil.read_mask, desc.stencil.write_mask);
        ds.back = to_vk_stencil_face(desc.stencil.back, desc.stencil.read_mask, desc.stencil.write_mask);

        const VulkanRenderPass& rp = g_render_passes.resolve(desc.pass);
        VkPipelineColorBlendAttachmentState blend_atts[k_max_color_attachments] = {};
        for (uint32_t i = 0; i < rp.color_count; ++i) {
            blend_atts[i].colorWriteMask = to_vk_color_mask(desc.blend.color_write_mask);
            if (desc.blend.blend_enable) {
                blend_atts[i].blendEnable = VK_TRUE;
                blend_atts[i].srcColorBlendFactor = to_vk_blend_factor(desc.blend.src_color);
                blend_atts[i].dstColorBlendFactor = to_vk_blend_factor(desc.blend.dst_color);
                blend_atts[i].colorBlendOp = to_vk_blend_op(desc.blend.color_op);
                blend_atts[i].srcAlphaBlendFactor = to_vk_blend_factor(desc.blend.src_alpha);
                blend_atts[i].dstAlphaBlendFactor = to_vk_blend_factor(desc.blend.dst_alpha);
                blend_atts[i].alphaBlendOp = to_vk_blend_op(desc.blend.alpha_op);
            }
        }

        VkPipelineColorBlendStateCreateInfo cb = {};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = rp.color_count;
        cb.pAttachments = blend_atts;

        VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dyn = {};
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates = dyn_states;

        VkPipelineRenderingCreateInfo rendering = {};
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = rp.color_count;
        rendering.pColorAttachmentFormats = rp.color_formats;
        rendering.depthAttachmentFormat = rp.depth_format;

        VkGraphicsPipelineCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        ci.pNext = &rendering;
        ci.stageCount = stage_count;
        ci.pStages = stages;
        ci.pVertexInputState = &vi;
        ci.pInputAssemblyState = &ia;
        ci.pViewportState = &vp;
        ci.pRasterizationState = &rs;
        ci.pMultisampleState = &ms;
        ci.pDepthStencilState = &ds;
        ci.pColorBlendState = &cb;
        ci.pDynamicState = &dyn;
        ci.layout = pipe.layout;

        result = vkCreateGraphicsPipelines(g_ctx.device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe.handle);
        pipe.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    if (result != VK_SUCCESS) {
        LOGE("create_pipeline failed: %d", static_cast<int32_t>(result));
        vkDestroyPipelineLayout(g_ctx.device, pipe.layout, nullptr);
        return k_gpu_invalid;
    }

    return g_pipelines.alloc(pipe);
}

void GpuDriver::destroy_pipeline(GpuPipeline handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanPipeline& pipe = g_pipelines.resolve(handle);
    vkDestroyPipeline(g_ctx.device, pipe.handle, nullptr);
    vkDestroyPipelineLayout(g_ctx.device, pipe.layout, nullptr);
    g_pipelines.free(handle);
}

void GpuDriver::cmd_bind_pipeline(GpuCmd handle, GpuPipeline pipeline) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanPipeline& pipe = g_pipelines.resolve(pipeline);
    VulkanCommandBuffer& rec = g_cmds.resolve(handle);
    rec.bind_point = pipe.bind_point;
    vkCmdBindPipeline(rec.handle, pipe.bind_point, pipe.handle);
}

void GpuDriver::cmd_bind_descriptors(GpuCmd handle, GpuPipeline pipeline, GpuDescriptorSet set) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanCommandBuffer& rec = g_cmds.resolve(handle);
    VulkanPipeline& pipe = g_pipelines.resolve(pipeline);
    VkDescriptorSet vk_set = g_sets.resolve(set).handle;
    vkCmdBindDescriptorSets(rec.handle, rec.bind_point, pipe.layout, 0, 1, &vk_set, 0, nullptr);
}

void GpuDriver::cmd_push_data(GpuCmd handle, GpuPipeline pipeline, const void* data, uint32_t size) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    assert(size <= k_max_push_data_size);
    assert(size % 4 == 0);
    VulkanPipeline& pipe = g_pipelines.resolve(pipeline);
    vkCmdPushConstants(g_cmds.resolve(handle).handle, pipe.layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void GpuDriver::cmd_bind_vertex_buffer(GpuCmd handle, uint32_t stream, GpuBuffer buffer, uint64_t offset) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkBuffer vk_buffer = g_buffers.resolve(buffer).handle;
    VkDeviceSize vk_offset = offset;
    vkCmdBindVertexBuffers(g_cmds.resolve(handle).handle, stream, 1, &vk_buffer, &vk_offset);
}

void GpuDriver::cmd_bind_index_buffer(GpuCmd handle, GpuBuffer buffer, uint64_t offset) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkCmdBindIndexBuffer(g_cmds.resolve(handle).handle, g_buffers.resolve(buffer).handle, offset, VK_INDEX_TYPE_UINT32);
}

void GpuDriver::cmd_draw(GpuCmd handle, uint32_t vertex_count, uint32_t first_vertex) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkCmdDraw(g_cmds.resolve(handle).handle, vertex_count, 1, first_vertex, 0);
}

void GpuDriver::cmd_draw_indexed(GpuCmd handle, uint32_t index_count, uint32_t instance_count, uint32_t first_index,
                                 int32_t base_vertex, uint32_t first_instance) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkCmdDrawIndexed(g_cmds.resolve(handle).handle, index_count, instance_count, first_index, base_vertex,
                     first_instance);
}

void GpuDriver::cmd_set_scissor(GpuCmd handle, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkRect2D scissor = {{x, y}, {width, height}};
    vkCmdSetScissor(g_cmds.resolve(handle).handle, 0, 1, &scissor);
}

void GpuDriver::cmd_dispatch(GpuCmd handle, uint32_t group_x, uint32_t group_y, uint32_t group_z) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkCmdDispatch(g_cmds.resolve(handle).handle, group_x, group_y, group_z);
}

void GpuDriver::cmd_begin_render_pass(GpuCmd handle, GpuRenderPass pass) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    const VulkanRenderPass& rp = g_render_passes.resolve(pass);

    VkRenderingAttachmentInfo color_atts[k_max_color_attachments] = {};
    for (uint32_t i = 0; i < rp.color_count; ++i) {
        VkRenderingAttachmentInfo& att = color_atts[i];
        att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        att.imageView = rp.color_views[i];
        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp = rp.color_loads[i] ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        memcpy(att.clearValue.color.float32, rp.color_clears[i], sizeof(float) * 4);
    }

    VkRenderingAttachmentInfo depth_att = {};
    if (rp.has_depth) {
        depth_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView = rp.depth_view;
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_att.loadOp = rp.depth_load ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_att.clearValue.depthStencil = {rp.depth_clear, 0};
    }

    VkRenderingInfo ri = {};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = {{0, 0}, {rp.width, rp.height}};
    ri.layerCount = 1;
    ri.colorAttachmentCount = rp.color_count;
    ri.pColorAttachments = color_atts;
    ri.pDepthAttachment = rp.has_depth ? &depth_att : nullptr;

    VkCommandBuffer c = g_cmds.resolve(handle).handle;
    vkCmdBeginRendering(c, &ri);

    VkViewport viewport = {0.0f, 0.0f, static_cast<float>(rp.width), static_cast<float>(rp.height), 0.0f, 1.0f};
    vkCmdSetViewport(c, 0, 1, &viewport);
    vkCmdSetScissor(c, 0, 1, &ri.renderArea);
}

void GpuDriver::cmd_end_render_pass(GpuCmd handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkCmdEndRendering(g_cmds.resolve(handle).handle);
}

GpuDescriptorSetLayout GpuDriver::create_set_layout(std::span<const GpuLayoutBinding> bindings) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    assert(bindings.size() <= k_max_layout_bindings);

    VkDescriptorSetLayoutBinding vk_bindings[k_max_layout_bindings] = {};
    VkDescriptorBindingFlags vk_flags[k_max_layout_bindings] = {};
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        vk_bindings[i].binding = bindings[i].binding;
        vk_bindings[i].descriptorType = to_vk_desc_type(bindings[i].type);
        vk_bindings[i].descriptorCount = bindings[i].count;
        vk_bindings[i].stageFlags = VK_SHADER_STAGE_ALL;
        vk_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci = {};
    flags_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_ci.bindingCount = static_cast<uint32_t>(bindings.size());
    flags_ci.pBindingFlags = vk_flags;

    VkDescriptorSetLayoutCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.pNext = &flags_ci;
    ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    ci.bindingCount = static_cast<uint32_t>(bindings.size());
    ci.pBindings = vk_bindings;

    VulkanDescriptorSetLayout layout = {};
    if (vkCreateDescriptorSetLayout(g_ctx.device, &ci, nullptr, &layout.handle) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed");
        return k_gpu_invalid;
    }

    layout.binding_count = static_cast<uint32_t>(bindings.size());
    for (uint32_t i = 0; i < layout.binding_count; ++i) {
        layout.bindings[i] = bindings[i];
    }

    return g_set_layouts.alloc(layout);
}

void GpuDriver::destroy_set_layout(GpuDescriptorSetLayout handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDestroyDescriptorSetLayout(g_ctx.device, g_set_layouts.resolve(handle).handle, nullptr);
    g_set_layouts.free(handle);
}

GpuDescriptorSet GpuDriver::create_descriptor_set(GpuDescriptorSetLayout layout) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkDescriptorSetLayout vk_layout = g_set_layouts.resolve(layout).handle;
    VkDescriptorSetAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_ctx.desc_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &vk_layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(g_ctx.device, &ai, &set) != VK_SUCCESS) {
        LOGE("vkAllocateDescriptorSets failed");
        return k_gpu_invalid;
    }

    return g_sets.alloc(VulkanDescriptorSet{set, layout});
}

void GpuDriver::destroy_descriptor_set(GpuDescriptorSet handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkDescriptorSet set = g_sets.resolve(handle).handle;
    vkFreeDescriptorSets(g_ctx.device, g_ctx.desc_pool, 1, &set);
    g_sets.free(handle);
}

GpuSampler GpuDriver::create_sampler(const GpuSamplerDesc& desc) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkSamplerCreateInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = to_vk_filter(desc.mag_filter);
    si.minFilter = to_vk_filter(desc.min_filter);
    si.mipmapMode = to_vk_mipmap_mode(desc.mipmap_mode);
    si.addressModeU = to_vk_address_mode(desc.address_u);
    si.addressModeV = to_vk_address_mode(desc.address_v);
    si.addressModeW = to_vk_address_mode(desc.address_w);
    si.mipLodBias = desc.mip_lod_bias;
    si.anisotropyEnable = desc.anisotropy_enable ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy = desc.max_anisotropy;
    si.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
    si.compareOp = to_vk_compare_op(desc.compare_op);
    si.minLod = desc.min_lod;
    si.maxLod = desc.max_lod;
    si.borderColor = to_vk_border_color(desc.border_color);
    si.unnormalizedCoordinates = desc.unnormalized_coordinates ? VK_TRUE : VK_FALSE;

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(g_ctx.device, &si, nullptr, &sampler) != VK_SUCCESS) {
        LOGE("vkCreateSampler failed");
        return k_gpu_invalid;
    }

    return g_samplers.alloc(VulkanSampler{sampler});
}

void GpuDriver::destroy_sampler(GpuSampler handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    vkDestroySampler(g_ctx.device, g_samplers.resolve(handle).handle, nullptr);
    g_samplers.free(handle);
}

void GpuDriver::write_descriptor_image(GpuDescriptorSet set, uint32_t binding, uint32_t slot, GpuImageView view) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanDescriptorSet& s = g_sets.resolve(set);
    GpuDescriptorType type = binding_type_of(s.layout, binding);
    assert(type == GpuDescriptorType::SampledImage || type == GpuDescriptorType::StorageImage);

    VkDescriptorImageInfo ii = {};
    ii.imageView = g_image_views.resolve(view).handle;
    ii.imageLayout = type == GpuDescriptorType::StorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = s.handle;
    w.dstBinding = binding;
    w.dstArrayElement = slot;
    w.descriptorCount = 1;
    w.descriptorType = to_vk_desc_type(type);
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(g_ctx.device, 1, &w, 0, nullptr);
}

void GpuDriver::write_descriptor_buffer(GpuDescriptorSet set, uint32_t binding, uint32_t slot, GpuBuffer buffer) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanDescriptorSet& s = g_sets.resolve(set);
    GpuDescriptorType type = binding_type_of(s.layout, binding);
    assert(type == GpuDescriptorType::StorageBuffer);

    VkDescriptorBufferInfo bi = {};
    bi.buffer = g_buffers.resolve(buffer).handle;
    bi.offset = 0;
    bi.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = s.handle;
    w.dstBinding = binding;
    w.dstArrayElement = slot;
    w.descriptorCount = 1;
    w.descriptorType = to_vk_desc_type(type);
    w.pBufferInfo = &bi;
    vkUpdateDescriptorSets(g_ctx.device, 1, &w, 0, nullptr);
}

void GpuDriver::write_descriptor_sampler(GpuDescriptorSet set, uint32_t binding, uint32_t slot, GpuSampler sampler) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanDescriptorSet& s = g_sets.resolve(set);
    GpuDescriptorType type = binding_type_of(s.layout, binding);
    assert(type == GpuDescriptorType::Sampler);

    VkDescriptorImageInfo ii = {};
    ii.sampler = g_samplers.resolve(sampler).handle;

    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = s.handle;
    w.dstBinding = binding;
    w.dstArrayElement = slot;
    w.descriptorCount = 1;
    w.descriptorType = to_vk_desc_type(type);
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(g_ctx.device, 1, &w, 0, nullptr);
}

void GpuDriver::cmd_copy_buffer(GpuCmd handle, GpuBuffer dst, uint64_t dst_offset, GpuBuffer src, uint64_t src_offset, uint64_t size) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VkBufferCopy region = {src_offset, dst_offset, size};
    vkCmdCopyBuffer(g_cmds.resolve(handle).handle, g_buffers.resolve(src).handle, g_buffers.resolve(dst).handle, 1, &region);
}

void GpuDriver::cmd_copy_to_image(GpuCmd handle, GpuImage dst, GpuBuffer src, uint64_t src_offset, uint32_t mip) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanImage& img = g_images.resolve(dst);
    assert(mip < img.mip_levels);
    VkBufferImageCopy region = {};
    region.bufferOffset = src_offset;
    region.imageSubresource = {img.aspect, mip, 0, img.array_layers};
    region.imageExtent = {std::max(1u, img.width >> mip), std::max(1u, img.height >> mip), std::max(1u, img.depth >> mip)};
    vkCmdCopyBufferToImage(g_cmds.resolve(handle).handle, g_buffers.resolve(src).handle, img.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

GpuSwapchain GpuDriver::create_swapchain(void* native_window, uint32_t width, uint32_t height) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    GpuSwapchain handle = g_swapchains.alloc(VulkanSwapchain{});
    if (handle == k_gpu_invalid) {
        return k_gpu_invalid;
    }

    VulkanSwapchain& sc = g_swapchains.resolve(handle);
    for (auto& image : sc.gpu_images) {
        image = k_gpu_invalid;
    }

    VkWin32SurfaceCreateInfoKHR sci = {};
    sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = GetModuleHandle(nullptr);
    sci.hwnd = static_cast<HWND>(native_window);
    if (vkCreateWin32SurfaceKHR(g_ctx.instance, &sci, nullptr, &sc.surface) != VK_SUCCESS) {
        LOGE("vkCreateWin32SurfaceKHR failed");
        g_swapchains.free(handle);
        return k_gpu_invalid;
    }

    VkBool32 present_ok = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_ctx.physical, g_ctx.queue_family, sc.surface, &present_ok);
    if (!present_ok) {
        LOGE("queue family %u does not support present", g_ctx.queue_family);
        vkDestroySurfaceKHR(g_ctx.instance, sc.surface, nullptr);
        g_swapchains.free(handle);
        return k_gpu_invalid;
    }

    if (!create_swapchain_internal(sc, width, height)) {
        destroy_swapchain_internal(sc);
        vkDestroySurfaceKHR(g_ctx.instance, sc.surface, nullptr);
        g_swapchains.free(handle);
        return k_gpu_invalid;
    }

    VkFenceCreateInfo fci = {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(g_ctx.device, &fci, nullptr, &sc.acquire_fence) != VK_SUCCESS) {
        LOGE("vkCreateFence failed");
        destroy_swapchain_internal(sc);
        vkDestroySurfaceKHR(g_ctx.instance, sc.surface, nullptr);
        g_swapchains.free(handle);
        return k_gpu_invalid;
    }

    return handle;
}

void GpuDriver::destroy_swapchain(GpuSwapchain handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanSwapchain& sc = g_swapchains.resolve(handle);
    destroy_swapchain_internal(sc);
    if (sc.acquire_fence != VK_NULL_HANDLE) {
        vkDestroyFence(g_ctx.device, sc.acquire_fence, nullptr);
    }

    vkDestroySurfaceKHR(g_ctx.instance, sc.surface, nullptr);
    g_swapchains.free(handle);
}

void GpuDriver::resize_swapchain(GpuSwapchain handle, uint32_t width, uint32_t height) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    if (width == 0 || height == 0) {
        return;
    }

    VulkanSwapchain& sc = g_swapchains.resolve(handle);
    destroy_swapchain_internal(sc);
    if (!create_swapchain_internal(sc, width, height)) {
        LOGE("resize_swapchain failed");
        return;
    }
}

void GpuDriver::swapchain_extent(GpuSwapchain handle, uint32_t& width, uint32_t& height) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanSwapchain& sc = g_swapchains.resolve(handle);
    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.physical, sc.surface, &caps);
    width = caps.currentExtent.width;
    height = caps.currentExtent.height;
}

GpuFormat GpuDriver::swapchain_format(GpuSwapchain handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    switch (g_swapchains.resolve(handle).format) {
        case VK_FORMAT_B8G8R8A8_UNORM:
            return GpuFormat::B8G8R8A8Unorm;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return GpuFormat::R8G8B8A8Unorm;
        default:
            return GpuFormat::Undefined;
    }
}

uint32_t GpuDriver::swapchain_image_count(GpuSwapchain handle) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_swapchains.resolve(handle).image_count;
}

GpuImage GpuDriver::swapchain_image(GpuSwapchain handle, uint32_t slot) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_swapchains.resolve(handle).gpu_images[slot];
}

GpuImageView GpuDriver::swapchain_image_view(GpuSwapchain handle, uint32_t slot) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    return g_swapchains.resolve(handle).views[slot];
}

bool GpuDriver::swapchain_acquire(GpuSwapchain handle, uint32_t& slot_out) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanSwapchain& sc = g_swapchains.resolve(handle);

    vkResetFences(g_ctx.device, 1, &sc.acquire_fence);

    VkResult result = vkAcquireNextImageKHR(g_ctx.device, sc.handle, UINT64_MAX, VK_NULL_HANDLE, sc.acquire_fence, &slot_out);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGW("vkAcquireNextImageKHR: out of date (resize_swapchain needed)");
        return false;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("vkAcquireNextImageKHR failed: %d", static_cast<int32_t>(result));
        return false;
    }

    vkWaitForFences(g_ctx.device, 1, &sc.acquire_fence, VK_TRUE, UINT64_MAX);
    return true;
}

void GpuDriver::swapchain_present(GpuQueue queue, GpuSwapchain handle, uint32_t slot, GpuSemaphore wait, uint64_t value) {
    BLAST_GPU_ASSERT_OWNER(owner_thread_);
    VulkanSwapchain& sc = g_swapchains.resolve(handle);
    VkQueue vk_queue = g_queues.resolve(queue).handle;
    VkSemaphore present_sem = sc.present_sems[slot];

    VkSemaphoreSubmitInfo wait_si = {};
    wait_si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_si.semaphore = g_semaphores.resolve(wait).handle;
    wait_si.value = value;
    wait_si.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSemaphoreSubmitInfo signal_si = {};
    signal_si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_si.semaphore = present_sem;
    signal_si.value = 0;
    signal_si.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    si.waitSemaphoreInfoCount = 1;
    si.pWaitSemaphoreInfos = &wait_si;
    si.signalSemaphoreInfoCount = 1;
    si.pSignalSemaphoreInfos = &signal_si;
    vkQueueSubmit2(vk_queue, 1, &si, VK_NULL_HANDLE);

    VkPresentInfoKHR pi = {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &present_sem;
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc.handle;
    pi.pImageIndices = &slot;

    VkResult result = vkQueuePresentKHR(vk_queue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGW("vkQueuePresentKHR: out of date or suboptimal (resize_swapchain needed)");
    } else if (result != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR failed: %d", static_cast<int32_t>(result));
    }
}
