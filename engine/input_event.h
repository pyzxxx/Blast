#pragma once

#include "input_key.h"
#include "input_mouse_button.h"

#include <variant>

struct InputEvent {
    enum class Type : uint8_t {
        Key,
        MouseButton,
        MouseMove,
        Scroll,
        Char,
    };

    struct KeyData {
        InputKey key;
        bool pressed;
    };

    struct MouseButtonData {
        InputMouseButton button;
        bool pressed;
    };

    struct MoveData {
        float x;
        float y;
    };

    struct ScrollData {
        float x;
        float y;
    };

    struct CharData {
        uint32_t codepoint;
    };

    Type type;
    std::variant<KeyData, MouseButtonData, MoveData, ScrollData, CharData> data;
};
