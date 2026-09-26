#pragma once

#include "gpu_driver.h"
#include "render_core.h"

#include <cstdint>

struct ImGuiContext;
struct ImDrawData;
struct InputEvent;
struct Services;

class UiSystem {
public:
    bool init(const Services& services);
    void shutdown();

    void on_event(const InputEvent& event);
    bool wants_mouse() const;
    bool wants_keyboard() const;

    void new_frame(float dt, uint32_t width, uint32_t height);
    void end_frame();

    const ImDrawData* draw_data() const;
    uint32_t font_slot() const { return font_slot_; }

private:
    bool create_font();

    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
    ImGuiContext* ctx_ = nullptr;
    GpuImage font_image_ = k_gpu_invalid;
    GpuImageView font_view_ = k_gpu_invalid;
    uint32_t font_slot_ = k_invalid_slot;
};
