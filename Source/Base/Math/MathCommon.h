#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define INV_PI 0.31830988618f
#define INV_TWO_PI 0.15915494309f
#define INF std::numeric_limits<float>::infinity()
#define NEG_INF -INF

enum class ClockDirection : uint8_t
{
    CW, // Clockwise
    CCW // CounterClockwise
};