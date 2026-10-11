#pragma once

#include "math_utils.h"

#include <cmath>

constexpr float lm_to_cd_point(float lumens) { return lumens / (4.0f * k_pi); }

inline float lm_to_cd_spot(float lumens, float outer_rad) {
    return lumens / (2.0f * k_pi * (1.0f - std::cos(outer_rad * 0.5f)));
}

inline float cd_spot_to_lm(float candela, float outer_rad) {
    return candela * 2.0f * k_pi * (1.0f - std::cos(outer_rad * 0.5f));
}

inline float ev100_to_exposure(float ev100) { return 1.0f / (1.2f * std::exp2(ev100)); }
