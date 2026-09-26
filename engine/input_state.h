#pragma once

#include <cstdint>

struct InputState {
    bool keys[256] = {};
    bool pressed[256] = {};
    bool released[256] = {};
    bool mouse_buttons[8] = {};
    bool mouse_pressed[8] = {};
    bool mouse_released[8] = {};
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
};
