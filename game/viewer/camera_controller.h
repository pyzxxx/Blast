#pragma once

#include "world.h"

class UiSystem;
struct InputState;

class CameraController {
public:
    void update(float dt, World& world, const InputState& input, const UiSystem& ui);

    float move_speed = 10.0f;
    float look_sensitivity = 0.003f;
    float boost_multiplier = 4.0f;

private:
    ObjectHandle camera_ = k_object_invalid;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
};
