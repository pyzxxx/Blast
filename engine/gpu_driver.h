#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

#ifndef NDEBUG
#define BLAST_GPU_ASSERT_OWNER(owner) assert((owner) == std::this_thread::get_id())
#else
#define BLAST_GPU_ASSERT_OWNER(owner) ((void)0)
#endif

using GpuQueue = uint64_t;
using GpuCmd = uint64_t;
using GpuCmdPool = uint64_t;
using GpuSemaphore = uint64_t;
using GpuSwapchain = uint64_t;
using GpuBuffer = uint64_t;
using GpuImage = uint64_t;
using GpuImageView = uint64_t;
using GpuShader = uint64_t;
using GpuPipeline = uint64_t;
using GpuRenderPass = uint64_t;
using GpuDescriptorSetLayout = uint64_t;
using GpuDescriptorSet = uint64_t;
using GpuSampler = uint64_t;

constexpr uint64_t k_gpu_invalid = 0xFFFFFFFFFFFFFFFF;
constexpr uint32_t k_max_pipeline_sets = 4;
constexpr uint32_t k_max_vertex_attrs = 16;
constexpr uint32_t k_max_color_attachments = 8;

#define GPU_ENUM_FLAGS(T, U)                                                                          \
    constexpr T operator|(T a, T b) { return static_cast<T>(static_cast<U>(a) | static_cast<U>(b)); } \
    constexpr U operator&(T a, T b) { return static_cast<U>(a) & static_cast<U>(b); }                 \
    constexpr T& operator|=(T& a, T b) { return a = a | b; }

enum class ResourceState : uint8_t {
    Undefined,
    PresentSrc,
    ColorAttachment,
    ShaderRead,
    StorageWrite,
    TransferSrc,
    TransferDst,
    DepthWrite,
    DepthRead,
};

enum class GpuFormat : uint32_t {
    Undefined,
    R8Unorm,
    R8G8B8A8Unorm,
    B8G8R8A8Unorm,
    D32Float,
    R32G32Float,
    R32G32B32Float,
    R32G32B32A32Float,
    R16G16B16A16Float,
    R32G32Uint,
    BC1RgbaUnorm,
    BC2Unorm,
    BC3Unorm,
    BC4Unorm,
    BC5Unorm,
    BC6HUfloat,
    BC6HSfloat,
    BC7Unorm,
    BC7Srgb,
};

enum class GpuBufferUsage : uint32_t {
    None = 0,
    Storage = 1,
    Index = 2,
    DeviceAddress = 4,
    TransferSrc = 8,
    TransferDst = 16,
    Vertex = 32,
};
GPU_ENUM_FLAGS(GpuBufferUsage, uint32_t)

enum class GpuImageUsage : uint32_t {
    None = 0,
    Sampled = 1,
    Storage = 2,
    ColorAttachment = 4,
    DepthAttachment = 8,
    TransferSrc = 16,
    TransferDst = 32,
};
GPU_ENUM_FLAGS(GpuImageUsage, uint32_t)

enum class GpuImageType : uint8_t {
    D2,
    D3,
    Cube,
};

enum class GpuImageAspect : uint32_t {
    None = 0,
    Color = 1,
    Depth = 2,
    Stencil = 4,
};
GPU_ENUM_FLAGS(GpuImageAspect, uint32_t)

enum class GpuSwizzle : uint8_t {
    Identity,
    Zero,
    One,
    R,
    G,
    B,
    A,
};

enum class GpuMemoryDomain : uint8_t {
    GpuOnly,
    Upload,
};

enum class GpuFilter : uint8_t {
    Nearest,
    Linear,
};

enum class GpuMipmapMode : uint8_t {
    Nearest,
    Linear,
};

enum class GpuAddressMode : uint8_t {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
    MirrorClampToEdge,
};

enum class GpuCompareOp : uint8_t {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

enum class GpuBorderColor : uint8_t {
    FloatTransparentBlack,
    IntTransparentBlack,
    FloatOpaqueBlack,
    IntOpaqueBlack,
    FloatOpaqueWhite,
    IntOpaqueWhite,
};

enum class GpuDescriptorType : uint8_t {
    SampledImage,
    StorageImage,
    StorageBuffer,
    Sampler,
};

enum class GpuCull : uint8_t {
    None,
    Back,
    Front,
};

enum class GpuTopology : uint8_t {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

enum class GpuFrontFace : uint8_t {
    CounterClockwise,
    Clockwise,
};

enum class GpuPolygonMode : uint8_t {
    Fill,
    Line,
    Point,
};

enum class GpuBlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcAlphaSaturate,
};

enum class GpuBlendOp : uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class GpuStencilOp : uint8_t {
    Keep,
    Zero,
    Replace,
    IncrementAndClamp,
    DecrementAndClamp,
    Invert,
    IncrementAndWrap,
    DecrementAndWrap,
};

enum class GpuColorComponent : uint32_t {
    None = 0,
    R = 1,
    G = 2,
    B = 4,
    A = 8,
    RGBA = R | G | B | A,
};
GPU_ENUM_FLAGS(GpuColorComponent, uint32_t)

struct GpuBufferDesc {
    uint64_t size = 0;
    GpuBufferUsage usage = GpuBufferUsage::None;
    GpuMemoryDomain memory = GpuMemoryDomain::GpuOnly;
};

struct GpuImageDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t array_layers = 1;
    GpuImageType type = GpuImageType::D2;
    GpuFormat format = GpuFormat::Undefined;
    uint32_t mip_levels = 1;
    uint32_t samples = 1;
    GpuImageUsage usage = GpuImageUsage::None;
};

struct GpuImageViewDesc {
    GpuImageType type = GpuImageType::D2;
    GpuFormat format = GpuFormat::Undefined;
    GpuImageAspect aspect = GpuImageAspect::None;
    GpuSwizzle swizzle_r = GpuSwizzle::Identity;
    GpuSwizzle swizzle_g = GpuSwizzle::Identity;
    GpuSwizzle swizzle_b = GpuSwizzle::Identity;
    GpuSwizzle swizzle_a = GpuSwizzle::Identity;
    uint32_t base_mip = 0;
    uint32_t mip_count = 0;
    uint32_t base_layer = 0;
    uint32_t layer_count = 0;
};

struct GpuSamplerDesc {
    GpuFilter mag_filter = GpuFilter::Linear;
    GpuFilter min_filter = GpuFilter::Linear;
    GpuMipmapMode mipmap_mode = GpuMipmapMode::Linear;
    GpuAddressMode address_u = GpuAddressMode::Repeat;
    GpuAddressMode address_v = GpuAddressMode::Repeat;
    GpuAddressMode address_w = GpuAddressMode::Repeat;
    float mip_lod_bias = 0.0f;
    bool anisotropy_enable = false;
    float max_anisotropy = 1.0f;
    bool compare_enable = false;
    GpuCompareOp compare_op = GpuCompareOp::LessOrEqual;
    float min_lod = 0.0f;
    float max_lod = 1000.0f;
    GpuBorderColor border_color = GpuBorderColor::FloatOpaqueBlack;
    bool unnormalized_coordinates = false;
};

struct GpuLayoutBinding {
    uint32_t binding = 0;
    GpuDescriptorType type = GpuDescriptorType::SampledImage;
    uint32_t count = 0;
};

struct GpuVertexAttr {
    uint32_t location = 0;
    uint32_t stream = 0;
    uint32_t offset = 0;
    GpuFormat format = GpuFormat::Undefined;
};

struct GpuColorTarget {
    GpuImageView view = k_gpu_invalid;
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    bool load = false;
};

struct GpuDepthTarget {
    GpuImageView view = k_gpu_invalid;
    float clear = 1.0f;
    bool load = false;
};

struct GpuRenderPassDesc {
    GpuColorTarget colors[k_max_color_attachments] = {};
    uint32_t color_count = 0;
    GpuDepthTarget depth = {};
};

struct GpuBlendState {
    bool blend_enable = false;
    GpuBlendFactor src_color = GpuBlendFactor::SrcAlpha;
    GpuBlendFactor dst_color = GpuBlendFactor::OneMinusSrcAlpha;
    GpuBlendOp color_op = GpuBlendOp::Add;
    GpuBlendFactor src_alpha = GpuBlendFactor::One;
    GpuBlendFactor dst_alpha = GpuBlendFactor::One;
    GpuBlendOp alpha_op = GpuBlendOp::Add;
    GpuColorComponent color_write_mask = GpuColorComponent::RGBA;
};

struct GpuStencilFace {
    GpuStencilOp fail_op = GpuStencilOp::Keep;
    GpuStencilOp depth_fail_op = GpuStencilOp::Keep;
    GpuStencilOp pass_op = GpuStencilOp::Keep;
    GpuCompareOp compare = GpuCompareOp::Always;
};

struct GpuStencilState {
    bool stencil_test = false;
    uint8_t read_mask = 0xFF;
    uint8_t write_mask = 0xFF;
    GpuStencilFace front = {};
    GpuStencilFace back = {};
};

struct GpuMultisampleState {
    uint32_t samples = 1;
    bool sample_shading = false;
    float min_sample_shading = 0.0f;
    bool alpha_to_coverage = false;
    bool alpha_to_one = false;
};

struct GpuRasterState {
    GpuCull cull = GpuCull::Back;
    GpuFrontFace front_face = GpuFrontFace::CounterClockwise;
    GpuPolygonMode polygon_mode = GpuPolygonMode::Fill;
    bool depth_clamp = false;
    bool depth_bias_enable = false;
    float depth_bias_constant = 0.0f;
    float depth_bias_clamp = 0.0f;
    float depth_bias_slope = 0.0f;
    float line_width = 1.0f;
};

struct GpuDepthState {
    bool depth_test = false;
    bool depth_write = false;
    GpuCompareOp depth_compare = GpuCompareOp::Less;
};

struct GpuVertexInput {
    GpuVertexAttr attrs[k_max_vertex_attrs] = {};
    uint32_t attr_count = 0;
    uint32_t stream_strides[2] = {0, 0};
};

struct GpuPipelineDesc {
    GpuShader vs = k_gpu_invalid;
    GpuShader fs = k_gpu_invalid;
    GpuShader cs = k_gpu_invalid;
    GpuRenderPass pass = k_gpu_invalid;
    GpuDescriptorSetLayout set_layouts[k_max_pipeline_sets] = {};
    uint32_t set_layout_count = 0;
    GpuVertexInput vertex_input = {};
    GpuTopology topology = GpuTopology::TriangleList;
    GpuRasterState raster = {};
    GpuDepthState depth = {};
    GpuBlendState blend = {};
    GpuStencilState stencil = {};
    GpuMultisampleState multisample = {};
};

class GpuDriver {
public:
    bool init();
    void shutdown();
    void wait_idle();

    GpuQueue create_queue();
    void destroy_queue(GpuQueue);

    GpuCmdPool create_cmd_pool();
    void destroy_cmd_pool(GpuCmdPool);
    void reset_cmd_pool(GpuCmdPool);

    GpuCmd start_command_recording(GpuCmdPool);
    void submit(GpuQueue, std::span<GpuCmd>);
    void cmd_wait_semaphore(GpuCmd, GpuSemaphore, uint64_t value);
    void cmd_signal_semaphore(GpuCmd, GpuSemaphore, uint64_t value);

    GpuSemaphore create_semaphore(uint64_t init_value);
    void wait_semaphore(GpuSemaphore, uint64_t value);
    uint64_t semaphore_value(GpuSemaphore);
    void destroy_semaphore(GpuSemaphore);

    GpuBuffer create_buffer(const GpuBufferDesc& desc);
    void destroy_buffer(GpuBuffer);
    uint64_t gpu_address(GpuBuffer);
    void* buffer_mapped(GpuBuffer);
    void buffer_unmap(GpuBuffer);

    GpuImage create_image(const GpuImageDesc& desc);
    void destroy_image(GpuImage);
    void image_size(GpuImage, uint32_t& width, uint32_t& height);

    GpuImageView create_image_view(GpuImage, const GpuImageViewDesc& desc = {});
    void destroy_image_view(GpuImageView);

    void cmd_barrier(GpuCmd, ResourceState from, ResourceState to);
    void cmd_buffer_barrier(GpuCmd, GpuBuffer, ResourceState from, ResourceState to);
    void cmd_image_barrier(GpuCmd, GpuImage, ResourceState from, ResourceState to, uint32_t base_mip = 0, uint32_t mip_count = 0);

    GpuShader create_shader(std::span<const uint32_t> spv);
    void destroy_shader(GpuShader);

    GpuRenderPass create_render_pass(const GpuRenderPassDesc& desc);
    void destroy_render_pass(GpuRenderPass);

    GpuPipeline create_pipeline(const GpuPipelineDesc& desc);
    void destroy_pipeline(GpuPipeline);

    GpuDescriptorSetLayout create_set_layout(std::span<const GpuLayoutBinding> bindings);
    void destroy_set_layout(GpuDescriptorSetLayout);
    GpuDescriptorSet create_descriptor_set(GpuDescriptorSetLayout);
    void destroy_descriptor_set(GpuDescriptorSet);
    GpuSampler create_sampler(const GpuSamplerDesc& desc);
    void destroy_sampler(GpuSampler);
    void write_descriptor_image(GpuDescriptorSet, uint32_t binding, uint32_t slot, GpuImageView);
    void write_descriptor_buffer(GpuDescriptorSet, uint32_t binding, uint32_t slot, GpuBuffer);
    void write_descriptor_sampler(GpuDescriptorSet, uint32_t binding, uint32_t slot, GpuSampler);

    void cmd_bind_pipeline(GpuCmd, GpuPipeline);
    void cmd_bind_descriptors(GpuCmd, GpuPipeline, GpuDescriptorSet);
    void cmd_push_data(GpuCmd, GpuPipeline, const void* data, uint32_t size);
    void cmd_bind_vertex_buffer(GpuCmd, uint32_t stream, GpuBuffer, uint64_t offset);
    void cmd_bind_index_buffer(GpuCmd, GpuBuffer, uint64_t offset = 0);
    void cmd_draw(GpuCmd, uint32_t vertex_count, uint32_t first_vertex);
    void cmd_draw_indexed(GpuCmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance);
    void cmd_set_scissor(GpuCmd, int32_t x, int32_t y, uint32_t width, uint32_t height);
    void cmd_dispatch(GpuCmd, uint32_t group_x, uint32_t group_y, uint32_t group_z);
    void cmd_begin_render_pass(GpuCmd, GpuRenderPass);
    void cmd_end_render_pass(GpuCmd);

    void cmd_copy_buffer(GpuCmd, GpuBuffer dst, uint64_t dst_offset, GpuBuffer src, uint64_t src_offset, uint64_t size);
    void cmd_copy_to_image(GpuCmd, GpuImage dst, GpuBuffer src, uint64_t src_offset, uint32_t mip = 0);

    GpuSwapchain create_swapchain(void* native_window, uint32_t width, uint32_t height);
    void destroy_swapchain(GpuSwapchain);
    void resize_swapchain(GpuSwapchain, uint32_t width, uint32_t height);
    void swapchain_extent(GpuSwapchain, uint32_t& width, uint32_t& height);
    GpuFormat swapchain_format(GpuSwapchain);
    uint32_t swapchain_image_count(GpuSwapchain);
    GpuImage swapchain_image(GpuSwapchain, uint32_t slot);
    GpuImageView swapchain_image_view(GpuSwapchain, uint32_t slot);
    bool swapchain_acquire(GpuSwapchain, uint32_t& slot_out);
    void swapchain_present(GpuQueue, GpuSwapchain, uint32_t slot, GpuSemaphore wait, uint64_t value);

private:
    std::thread::id owner_thread_;
};
