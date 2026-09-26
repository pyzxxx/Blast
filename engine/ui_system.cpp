#include "ui_system.h"

#include "input_event.h"
#include "mem.h"
#include "services.h"

#include <imgui.h>

#include <cstring>

namespace {

void* ui_alloc(size_t size, void*) {
    return memalloc(size);
}

void ui_free(void* ptr, void*) {
    memfree(ptr);
}

ImGuiKey map_imgui_key(InputKey key) {
    switch (key) {
        case InputKey::Tab: return ImGuiKey_Tab;
        case InputKey::Left: return ImGuiKey_LeftArrow;
        case InputKey::Right: return ImGuiKey_RightArrow;
        case InputKey::Up: return ImGuiKey_UpArrow;
        case InputKey::Down: return ImGuiKey_DownArrow;
        case InputKey::PageUp: return ImGuiKey_PageUp;
        case InputKey::PageDown: return ImGuiKey_PageDown;
        case InputKey::Home: return ImGuiKey_Home;
        case InputKey::End: return ImGuiKey_End;
        case InputKey::Insert: return ImGuiKey_Insert;
        case InputKey::Delete: return ImGuiKey_Delete;
        case InputKey::Backspace: return ImGuiKey_Backspace;
        case InputKey::Space: return ImGuiKey_Space;
        case InputKey::Enter: return ImGuiKey_Enter;
        case InputKey::Escape: return ImGuiKey_Escape;
        case InputKey::LeftCtrl: return ImGuiKey_LeftCtrl;
        case InputKey::LeftShift: return ImGuiKey_LeftShift;
        case InputKey::LeftAlt: return ImGuiKey_LeftAlt;
        case InputKey::RightCtrl: return ImGuiKey_RightCtrl;
        case InputKey::RightShift: return ImGuiKey_RightShift;
        case InputKey::RightAlt: return ImGuiKey_RightAlt;
        case InputKey::Minus: return ImGuiKey_Minus;
        case InputKey::Equal: return ImGuiKey_Equal;
        case InputKey::Comma: return ImGuiKey_Comma;
        case InputKey::Period: return ImGuiKey_Period;
        case InputKey::Slash: return ImGuiKey_Slash;
        case InputKey::Backslash: return ImGuiKey_Backslash;
        case InputKey::Semicolon: return ImGuiKey_Semicolon;
        case InputKey::Apostrophe: return ImGuiKey_Apostrophe;
        case InputKey::LeftBracket: return ImGuiKey_LeftBracket;
        case InputKey::RightBracket: return ImGuiKey_RightBracket;
        case InputKey::GraveAccent: return ImGuiKey_GraveAccent;
        default: break;
    }

    if (key >= InputKey::A && key <= InputKey::Z) {
        return static_cast<ImGuiKey>(ImGuiKey_A + (static_cast<int>(key) - static_cast<int>(InputKey::A)));
    }

    if (key >= InputKey::Num0 && key <= InputKey::Num9) {
        return static_cast<ImGuiKey>(ImGuiKey_0 + (static_cast<int>(key) - static_cast<int>(InputKey::Num0)));
    }

    if (key >= InputKey::F1 && key <= InputKey::F12) {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (static_cast<int>(key) - static_cast<int>(InputKey::F1)));
    }

    return ImGuiKey_None;
}

} // namespace

bool UiSystem::init(const Services& services) {
    gpu_ = services.gpu;
    rcore_ = services.rcore;

    ImGui::SetAllocatorFunctions(ui_alloc, ui_free, nullptr);
    ctx_ = ImGui::CreateContext();
    if (ctx_ == nullptr) {
        return false;
    }

    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->SetTexID(0);
    return create_font();
}

bool UiSystem::create_font() {
    unsigned char* pixels = nullptr;
    int w = 0;
    int h = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    if (pixels == nullptr) {
        return false;
    }

    GpuImageDesc d = {};
    d.width = static_cast<uint32_t>(w);
    d.height = static_cast<uint32_t>(h);
    d.format = GpuFormat::R8G8B8A8Unorm;
    d.usage = GpuImageUsage::Sampled | GpuImageUsage::TransferDst;
    font_image_ = gpu_->create_image(d);
    uint32_t size = d.width * d.height * 4;
    uint32_t off = rcore_->upload_alloc(size);
    if (font_image_ == k_gpu_invalid || off == UINT32_MAX) {
        return false;
    }

    font_view_ = gpu_->create_image_view(font_image_);
    font_slot_ = rcore_->heap_add_view(font_view_);
    memcpy(rcore_->upload_cpu_addr() + off, pixels, size);
    GpuCmd cmd = rcore_->new_upload_cmd();
    gpu_->cmd_image_barrier(cmd, font_image_, ResourceState::Undefined, ResourceState::TransferDst);
    gpu_->cmd_copy_to_image(cmd, font_image_, rcore_->upload_buffer(), off);
    gpu_->cmd_image_barrier(cmd, font_image_, ResourceState::TransferDst, ResourceState::ShaderRead);
    return true;
}

void UiSystem::shutdown() {
    if (font_view_ != k_gpu_invalid) {
        rcore_->heap_remove_view(font_view_);
        font_view_ = k_gpu_invalid;
        font_slot_ = k_invalid_slot;
    }

    if (font_image_ != k_gpu_invalid) {
        gpu_->destroy_image(font_image_);
        font_image_ = k_gpu_invalid;
    }

    if (ctx_ != nullptr) {
        ImGui::DestroyContext(ctx_);
        ctx_ = nullptr;
    }
}

void UiSystem::on_event(const InputEvent& event) {
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
        case InputEvent::Type::Key: {
            const auto& k = std::get<InputEvent::KeyData>(event.data);
            ImGuiKey key = map_imgui_key(k.key);
            if (key != ImGuiKey_None) {
                io.AddKeyEvent(key, k.pressed);
            }

            io.AddKeyEvent(ImGuiMod_Ctrl, io.KeysData[ImGuiKey_LeftCtrl - ImGuiKey_NamedKey_BEGIN].Down ||
                                              io.KeysData[ImGuiKey_RightCtrl - ImGuiKey_NamedKey_BEGIN].Down);
            io.AddKeyEvent(ImGuiMod_Shift, io.KeysData[ImGuiKey_LeftShift - ImGuiKey_NamedKey_BEGIN].Down ||
                                               io.KeysData[ImGuiKey_RightShift - ImGuiKey_NamedKey_BEGIN].Down);
            io.AddKeyEvent(ImGuiMod_Alt, io.KeysData[ImGuiKey_LeftAlt - ImGuiKey_NamedKey_BEGIN].Down ||
                                             io.KeysData[ImGuiKey_RightAlt - ImGuiKey_NamedKey_BEGIN].Down);
            break;
        }
        case InputEvent::Type::MouseButton: {
            const auto& b = std::get<InputEvent::MouseButtonData>(event.data);
            if (b.button != InputMouseButton::Max) {
                io.AddMouseButtonEvent(static_cast<int>(b.button), b.pressed);
            }

            break;
        }
        case InputEvent::Type::MouseMove: {
            const auto& m = std::get<InputEvent::MoveData>(event.data);
            io.AddMousePosEvent(m.x, m.y);
            break;
        }
        case InputEvent::Type::Scroll: {
            const auto& s = std::get<InputEvent::ScrollData>(event.data);
            io.AddMouseWheelEvent(s.x, s.y);
            break;
        }
        case InputEvent::Type::Char: {
            const auto& c = std::get<InputEvent::CharData>(event.data);
            io.AddInputCharacter(c.codepoint);
            break;
        }
    }
}

bool UiSystem::wants_mouse() const {
    ImGui::SetCurrentContext(ctx_);
    return ImGui::GetIO().WantCaptureMouse;
}

bool UiSystem::wants_keyboard() const {
    ImGui::SetCurrentContext(ctx_);
    return ImGui::GetIO().WantCaptureKeyboard;
}

void UiSystem::new_frame(float dt, uint32_t width, uint32_t height) {
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = dt > 1e-6f ? dt : 1e-6f;
    ImGui::NewFrame();
}

void UiSystem::end_frame() {
    ImGui::SetCurrentContext(ctx_);
    ImGui::Render();
}

const ImDrawData* UiSystem::draw_data() const {
    ImGui::SetCurrentContext(ctx_);
    return ImGui::GetDrawData();
}
