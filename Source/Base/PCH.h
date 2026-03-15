#pragma once

// Windows
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

// Standard Library
#include <cstdint>
#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <map>
#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stack>
#include <deque>
#include <mutex>
#include <random>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

// GLM
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>