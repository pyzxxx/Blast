#pragma once

#include "os.h"

#include <queue>
#include <string>

struct GLFWwindow;

class OS_Glfw : public OS {
public:
    bool init(const char* title, uint32_t width, uint32_t height);
    void terminate();

    void poll_events() override;
    bool should_quit() const override;
    void window_size(uint32_t& w, uint32_t& h) const override;
    double time_seconds() const override;

    bool poll_input_event(InputEvent& out) override;

    void* native_handle() const override;
    const char* asset_root() const override;

    void set_cursor_visible(bool visible);
    void set_cursor_locked(bool locked);

private:
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void char_callback(GLFWwindow* window, unsigned int codepoint);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double x, double y);
    static void scroll_callback(GLFWwindow* window, double x, double y);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

    static InputKey map_key(int key);
    static InputMouseButton map_mouse_button(int button);

    void push_event(const InputEvent& event);

    GLFWwindow* window_ = nullptr;
    std::queue<InputEvent> events_;
    std::string asset_root_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool cursor_visible_ = true;
    bool cursor_locked_ = false;
};
