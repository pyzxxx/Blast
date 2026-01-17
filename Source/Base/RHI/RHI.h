#pragma once

#include "Foundation/Log.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <spirv_reflect.h>

#include <array>
#include <algorithm>
#include <deque>
#include <set>
#include <span>
#include <vector>
#include <unordered_map>

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

#define RHI_MAX_FRAMES_IN_FLIGHT 3
#define RHI_MAX_ATTACHMENT_COUNT 4
#define RHI_MAX_VERTEX_BUFFER_COUNT 4
#define RHI_MAX_VERTEX_ATTRIB_COUNT 16

namespace RHI
{
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

struct TextureDesc
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t levels = 1;
    uint32_t layers = 1;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};

struct TextureView
{
    VkImageView handle;
    VkImageSubresourceRange subresourceRange;
};

struct Texture
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t levels;
    uint32_t layers;
    VkFormat format;
    VkImage handle;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkAccessFlags2 accessMask = 0;
    VkPipelineStageFlags2 stageMask = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::vector<TextureView> views;
};

struct EzSamplerDesc
{
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    bool anisotropyEnable = false;
    bool compareEnable = false;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
};

struct Sampler
{
    VkSampler handle = VK_NULL_HANDLE;
};

struct Shader
{
    VkShaderModule handle = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stageInfo = {};
    VkPushConstantRange pushConstants = {};
    SpvReflectShaderModule reflect = {};
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
};

struct RenderingFormat
{
    VkFormat depth = VK_FORMAT_UNDEFINED;
    VkFormat stencil = VK_FORMAT_UNDEFINED;
    std::array<VkFormat, RHI_MAX_ATTACHMENT_COUNT> colors = {};
};

struct RenderingAttachmentInfo
{
    Texture* texture = nullptr;
    int textureView = 0;
    Texture* resolveTexture = nullptr;
    int resolveTextureView = 0;
    VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkClearValue clearValue = {};
};

struct RenderingInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    RenderingAttachmentInfo depth = {};
    RenderingAttachmentInfo stencil = {};
    std::array<RenderingAttachmentInfo, RHI_MAX_ATTACHMENT_COUNT> colors = {};
};

struct VertexAttrib
{
    uint32_t binding = 0;
    uint32_t offset = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

struct VertexBinding
{
    uint32_t stride = 0;
    VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
};

struct VertexLayout
{
    std::array<VertexBinding, RHI_MAX_VERTEX_ATTRIB_COUNT> bindings = {};
    std::array<VertexAttrib, RHI_MAX_VERTEX_BUFFER_COUNT> attributes = {};
};

struct BlendState
{
    bool blendEnable = false;
    VkBlendFactor srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp colorOp = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlpha = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlpha = VK_BLEND_FACTOR_ONE;
    VkBlendOp alphaOp = VK_BLEND_OP_ADD;
};

struct DepthState
{
    bool depthTest = true;
    bool depthWrite = true;
    VkCompareOp depthFunc = VK_COMPARE_OP_LESS_OR_EQUAL;
};

struct StencilState
{
    bool stencilTest = false;
    uint8_t stencilReadMask = 0xff;
    uint8_t stencilWriteMask = 0xff;
    VkStencilOp frontStencilFailOp = VK_STENCIL_OP_KEEP;
    VkStencilOp frontStencilDepthFailOp = VK_STENCIL_OP_KEEP;
    VkStencilOp frontStencilPassOp = VK_STENCIL_OP_KEEP;
    VkCompareOp frontStencilFunc = VK_COMPARE_OP_ALWAYS;
    VkStencilOp backStencilFailOp = VK_STENCIL_OP_KEEP;
    VkStencilOp backStencilDepthFailOp = VK_STENCIL_OP_KEEP;
    VkStencilOp backStencilPassOp = VK_STENCIL_OP_KEEP;
    VkCompareOp backStencilFunc = VK_COMPARE_OP_ALWAYS;
};

struct MultisampleState
{
    bool sampleShading = false;
    bool alphaToCoverage = false;
    bool alphaToOne = false;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};

struct ComputePipelineDesc
{
    Shader* computeShader = nullptr;
};

struct GraphicsPipelineDesc
{
    Shader* vertexShader = nullptr;
    Shader* fragmentShader = nullptr;
    VertexLayout vertexLayout = {};
    BlendState blendState = {};
    DepthState depthState = {};
    StencilState stencilState = {};
    MultisampleState multisampleState = {};
    RenderingFormat renderingFormat = {};
    VkPolygonMode fillMode = VK_POLYGON_MODE_FILL;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCullModeFlagBits cullMode = VK_CULL_MODE_NONE;
};

struct Pipeline
{
    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPushConstantRange pushConstants = {};
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
};

enum class SwapchainStatus
{
    Ready,
    Resized,
    NotReady,
};

struct Swapchain
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t imageIndex = 0;
    uint32_t imageCount = 0;
    VkAccessFlags2 accessMask = 0;
    VkPipelineStageFlags2 stageMask = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore releaseSemaphore = VK_NULL_HANDLE;
    std::vector<VkImage> images;
};

struct GenericBarrier
{
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
};

struct BufferBarrier
{
    Buffer* buffer = nullptr;
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
};

struct ImageBarrier
{
    Texture* texture = nullptr;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
};

struct CommandBuffer
{
    VkCommandBuffer handle;

    bool inBarrierScope = false;
    std::vector<GenericBarrier> scopedGenericBarriers;
    std::vector<BufferBarrier> scopedBufferBarriers;
    std::vector<ImageBarrier> scopedImageBarriers;
};

void Startup();
void Shutdown();

uint8_t GetFrameIndex();
uint8_t GetMaxFramesInFlight();

void CreateBuffer(const BufferDesc& desc, Buffer*& buffer);
void DestroyBuffer(Buffer* buffer);

void CreateTexture(const TextureDesc& desc, Texture*& texture);
void DestroyTexture(Texture* texture);
int CreateTextureView(Texture* texture, VkImageViewType view_type, VkImageAspectFlags aspect_mask,
                      uint32_t base_level, uint32_t level_count,
                      uint32_t base_layer, uint32_t layer_count);

void CreateSampler(const EzSamplerDesc& desc, Sampler*& sampler);
void DestroySampler(Sampler* sampler);

void CreateShader(void* data, size_t size, Shader*& shader);
void DestroyShader(Shader* shader);

void CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, Pipeline*& pipeline);
void CreateComputePipeline(const ComputePipelineDesc& desc, Pipeline*& pipeline);
void DestroyPipeline(Pipeline* pipeline);

void CreateSwapchain(void* window, Swapchain*& swapchain);
SwapchainStatus UpdateSwapchain(Swapchain* swapchain);
void DestroySwapchain(Swapchain* swapchain);

CommandBuffer* RequestCommandBuffer();
void CmdBeginPipelineBarrier(CommandBuffer* cmd);
void CmdPipelineBarrier(CommandBuffer* cmd, GenericBarrier genericBarrier);
void CmdPipelineBarrier(CommandBuffer* cmd, BufferBarrier bufferBarrier);
void CmdPipelineBarrier(CommandBuffer* cmd, ImageBarrier imageBarrier);
void CmdEndPipelineBarrier(CommandBuffer* cmd);
void Submit(CommandBuffer* cmd);

void WaitIdle();
void NextFrame();
} // namespace RHI