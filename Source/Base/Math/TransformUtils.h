#pragma once

#include "MathCommon.h"

namespace TransformUtils {
inline glm::vec3 TransformPoint(const glm::vec3& point, const glm::mat4& mat)
{
    glm::vec4 p0 = glm::vec4(point.x, point.y, point.z, 1.0f);
    glm::vec4 p1 = mat * p0;
    return glm::vec3(p1.x, p1.y, p1.z) / p1.w;
}

inline glm::vec3 GetX(const glm::mat4& mat) { return glm::vec3(mat[0][0], mat[0][1], mat[0][2]); }

inline glm::vec3 GetY(const glm::mat4& mat) { return glm::vec3(mat[1][0], mat[1][1], mat[1][2]); }

inline glm::vec3 GetZ(const glm::mat4& mat) { return glm::vec3(mat[2][0], mat[2][1], mat[2][2]); }

inline glm::vec3 GetTranslation(const glm::mat4& mat) { return glm::vec3(mat[3][0], mat[3][1], mat[3][2]); }

inline glm::mat4 RemoveScale(const glm::mat4& mat)
{
    glm::vec3 scale;
    glm::quat rot;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(mat, scale, rot, translation, skew, perspective);
    return glm::translate(glm::mat4(1.0), translation) * glm::toMat4(rot);
}
}// namespace TransformUtils