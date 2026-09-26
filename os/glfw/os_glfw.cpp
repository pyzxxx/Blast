#include "os_glfw.h"
#include "log.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

bool OS_Glfw::init(const char* title, uint32_t w, uint32_t h) {
    asset_root_ = BLAST_PROJECT_DIR "/assets";

    if (!glfwInit()) {
        LOGE("glfwInit failed");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(static_cast<int>(w), static_cast<int>(h), title, nullptr, nullptr);
    if (!window_) {
        LOGE("glfwCreateWindow failed");
        glfwTerminate();
        return false;
    }

    width_ = w;
    height_ = h;

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, key_callback);
    glfwSetCharCallback(window_, char_callback);
    glfwSetMouseButtonCallback(window_, mouse_button_callback);
    glfwSetCursorPosCallback(window_, cursor_pos_callback);
    glfwSetScrollCallback(window_, scroll_callback);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

    return true;
}

void OS_Glfw::terminate() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void OS_Glfw::poll_events() {
    glfwPollEvents();
}

bool OS_Glfw::should_quit() const {
    return glfwWindowShouldClose(window_);
}

void OS_Glfw::window_size(uint32_t& w, uint32_t& h) const {
    w = width_;
    h = height_;
}

double OS_Glfw::time_seconds() const {
    return glfwGetTime();
}

bool OS_Glfw::poll_input_event(InputEvent& out) {
    if (events_.empty()) {
        return false;
    }

    out = events_.front();
    events_.pop();
    return true;
}

void OS_Glfw::set_cursor_visible(bool visible) {
    cursor_visible_ = visible;
    glfwSetInputMode(window_, GLFW_CURSOR, (visible && !cursor_locked_) ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

void OS_Glfw::set_cursor_locked(bool locked) {
    cursor_locked_ = locked;
    glfwSetInputMode(window_, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : (cursor_visible_ ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN));
}

void* OS_Glfw::native_handle() const {
    return glfwGetWin32Window(window_);
}

const char* OS_Glfw::asset_root() const {
    return asset_root_.c_str();
}

void OS_Glfw::push_event(const InputEvent& event) {
    events_.push(event);
}

void OS_Glfw::key_callback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    if (action == GLFW_REPEAT) {
        return;
    }

    InputEvent event = {};
    event.type = InputEvent::Type::Key;
    event.data = InputEvent::KeyData{map_key(key), action == GLFW_PRESS};
    self->push_event(event);
}

void OS_Glfw::char_callback(GLFWwindow* w, unsigned int codepoint) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    InputEvent event = {};
    event.type = InputEvent::Type::Char;
    event.data = InputEvent::CharData{codepoint};
    self->push_event(event);
}

void OS_Glfw::mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    InputEvent event = {};
    event.type = InputEvent::Type::MouseButton;
    event.data = InputEvent::MouseButtonData{map_mouse_button(button), action == GLFW_PRESS};
    self->push_event(event);
}

void OS_Glfw::cursor_pos_callback(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    InputEvent event = {};
    event.type = InputEvent::Type::MouseMove;
    event.data = InputEvent::MoveData{static_cast<float>(x), static_cast<float>(y)};
    self->push_event(event);
}

void OS_Glfw::scroll_callback(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    InputEvent event = {};
    event.type = InputEvent::Type::Scroll;
    event.data = InputEvent::ScrollData{static_cast<float>(x), static_cast<float>(y)};
    self->push_event(event);
}

void OS_Glfw::framebuffer_size_callback(GLFWwindow* w, int new_width, int new_height) {
    auto* self = static_cast<OS_Glfw*>(glfwGetWindowUserPointer(w));
    self->width_ = static_cast<uint32_t>(new_width);
    self->height_ = static_cast<uint32_t>(new_height);
}

InputKey OS_Glfw::map_key(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return static_cast<InputKey>(static_cast<int>(InputKey::A) + (key - GLFW_KEY_A));
    }

    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        return static_cast<InputKey>(static_cast<int>(InputKey::Num0) + (key - GLFW_KEY_0));
    }

    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
        return static_cast<InputKey>(static_cast<int>(InputKey::F1) + (key - GLFW_KEY_F1));
    }

    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
        return static_cast<InputKey>(static_cast<int>(InputKey::Numpad0) + (key - GLFW_KEY_KP_0));
    }

    switch (key) {
        case GLFW_KEY_ESCAPE: return InputKey::Escape;
        case GLFW_KEY_SPACE: return InputKey::Space;
        case GLFW_KEY_ENTER: return InputKey::Enter;
        case GLFW_KEY_TAB: return InputKey::Tab;
        case GLFW_KEY_BACKSPACE: return InputKey::Backspace;
        case GLFW_KEY_DELETE: return InputKey::Delete;
        case GLFW_KEY_INSERT: return InputKey::Insert;
        case GLFW_KEY_HOME: return InputKey::Home;
        case GLFW_KEY_END: return InputKey::End;
        case GLFW_KEY_PAGE_UP: return InputKey::PageUp;
        case GLFW_KEY_PAGE_DOWN: return InputKey::PageDown;
        case GLFW_KEY_LEFT: return InputKey::Left;
        case GLFW_KEY_RIGHT: return InputKey::Right;
        case GLFW_KEY_UP: return InputKey::Up;
        case GLFW_KEY_DOWN: return InputKey::Down;
        case GLFW_KEY_LEFT_SHIFT: return InputKey::LeftShift;
        case GLFW_KEY_RIGHT_SHIFT: return InputKey::RightShift;
        case GLFW_KEY_LEFT_CONTROL: return InputKey::LeftCtrl;
        case GLFW_KEY_RIGHT_CONTROL: return InputKey::RightCtrl;
        case GLFW_KEY_LEFT_ALT: return InputKey::LeftAlt;
        case GLFW_KEY_RIGHT_ALT: return InputKey::RightAlt;
        case GLFW_KEY_CAPS_LOCK: return InputKey::CapsLock;
        case GLFW_KEY_NUM_LOCK: return InputKey::NumLock;
        case GLFW_KEY_PRINT_SCREEN: return InputKey::PrintScreen;
        case GLFW_KEY_PAUSE: return InputKey::Pause;
        case GLFW_KEY_KP_ADD: return InputKey::NumpadAdd;
        case GLFW_KEY_KP_SUBTRACT: return InputKey::NumpadSubtract;
        case GLFW_KEY_KP_MULTIPLY: return InputKey::NumpadMultiply;
        case GLFW_KEY_KP_DIVIDE: return InputKey::NumpadDivide;
        case GLFW_KEY_KP_DECIMAL: return InputKey::NumpadDecimal;
        case GLFW_KEY_KP_ENTER: return InputKey::NumpadEnter;
        case GLFW_KEY_MINUS: return InputKey::Minus;
        case GLFW_KEY_EQUAL: return InputKey::Equal;
        case GLFW_KEY_COMMA: return InputKey::Comma;
        case GLFW_KEY_PERIOD: return InputKey::Period;
        case GLFW_KEY_SLASH: return InputKey::Slash;
        case GLFW_KEY_BACKSLASH: return InputKey::Backslash;
        case GLFW_KEY_SEMICOLON: return InputKey::Semicolon;
        case GLFW_KEY_APOSTROPHE: return InputKey::Apostrophe;
        case GLFW_KEY_LEFT_BRACKET: return InputKey::LeftBracket;
        case GLFW_KEY_RIGHT_BRACKET: return InputKey::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return InputKey::GraveAccent;
        default: return InputKey::Unknown;
    }
}

InputMouseButton OS_Glfw::map_mouse_button(int button) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT: return InputMouseButton::Left;
        case GLFW_MOUSE_BUTTON_RIGHT: return InputMouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE: return InputMouseButton::Middle;
        case GLFW_MOUSE_BUTTON_4: return InputMouseButton::X1;
        case GLFW_MOUSE_BUTTON_5: return InputMouseButton::X2;
        default: return InputMouseButton::Max;
    }
}
