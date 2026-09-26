#include "camera_controller.h"
#include "input_key.h"
#include "input_mouse_button.h"
#include "input_state.h"
#include "ui_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace {
constexpr float k_pitch_limit = 1.5533430f; // 89 degrees
}

void CameraController::update(float dt, World& world, const InputState& input, const UiSystem& ui) {
    ObjectHandle handle = k_object_invalid;
    world.each<CameraComponent>([&](ObjectHandle h, CameraComponent&) {
        if (handle == k_object_invalid) {
            handle = h;
        }
    });

    if (handle == k_object_invalid) {
        camera_ = k_object_invalid;
        return;
    }

    Transform* t = world.get<Transform>(handle);
    if (!t) {
        return;
    }

    if (handle != camera_) {
        camera_ = handle;
        const glm::vec3 f = t->rotation() * glm::vec3(0.0f, 0.0f, -1.0f);
        pitch_ = asinf(glm::clamp(f.y, -1.0f, 1.0f));
        yaw_ = atan2f(-f.x, -f.z);
    }

    if (!ui.wants_mouse()) {
        const glm::vec3 fwd = t->rotation() * glm::vec3(0.0f, 0.0f, -1.0f);

        if (input.mouse_buttons[static_cast<uint8_t>(InputMouseButton::Right)]) {
            yaw_ -= input.mouse_dx * look_sensitivity;
            pitch_ -= input.mouse_dy * look_sensitivity;
            pitch_ = glm::clamp(pitch_, -k_pitch_limit, k_pitch_limit);

            const glm::quat q_yaw = glm::angleAxis(yaw_, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::quat q_pitch = glm::angleAxis(pitch_, glm::vec3(1.0f, 0.0f, 0.0f));
            t->set_rotation(q_yaw * q_pitch);
        }

        if (input.scroll_y != 0.0f) {
            t->set_position(t->position() + fwd * (input.scroll_y * move_speed * 0.1f));
        }
    }

    if (ui.wants_keyboard()) {
        return;
    }

    auto key = [&input](InputKey k) { return input.keys[static_cast<uint8_t>(k)]; };

    const glm::quat rot = t->rotation();
    glm::vec3 dir(0.0f);
    if (key(InputKey::W)) {
        dir += rot * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    if (key(InputKey::S)) {
        dir -= rot * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    if (key(InputKey::D)) {
        dir += rot * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    if (key(InputKey::A)) {
        dir -= rot * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    if (key(InputKey::E)) {
        dir += glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (key(InputKey::Q)) {
        dir -= glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (glm::dot(dir, dir) == 0.0f) {
        return;
    }

    const float boost = (key(InputKey::LeftShift) || key(InputKey::RightShift)) ? boost_multiplier : 1.0f;
    t->set_position(t->position() + glm::normalize(dir) * (move_speed * boost * dt));
}
