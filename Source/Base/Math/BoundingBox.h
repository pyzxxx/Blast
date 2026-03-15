#pragma once

#include "MathCommon.h"
#include "TransformUtils.h"

class AABB
{
public:
    AABB()
    {
        m_min = glm::vec3(INF, INF, INF);
        m_max = glm::vec3(NEG_INF, NEG_INF, NEG_INF);
    }

    explicit AABB(const glm::vec3& p)
    {
        m_min = p;
        m_max = p;
    }

    AABB(const glm::vec3& p1, const glm::vec3& p2)
    {
        m_min = glm::min(p1, p2);
        m_max = glm::max(p1, p2);
    }

    glm::vec3 GetMin() { return m_min; }

    glm::vec3 GetMax() { return m_max; }

    void SetMin(const glm::vec3& p) { m_min = p; }

    void SetMax(const glm::vec3& p) { m_max = p; }

    glm::vec3 GetCenter() const
    {
        return (m_max + m_min) * 0.5f;
    }

    glm::vec3 GetSize() const
    {
        return (m_max - m_min);
    }

    float GetSurfaceArea() const
    {
        glm::vec3 size = GetSize();
        return (size.x * size.y + size.x * size.z + size.y * size.z) * 2;
    }

    float GetVolume() const
    {
        glm::vec3 size = GetSize();
        return size.x * size.y * size.z;
    }

    float GetMaxExtent() const
    {
        glm::vec3 size = GetSize();
        if (size.x > size.y && size.x > size.z)
        {
            return size.x;
        }
        else if (size.y > size.z)
        {
            return size.y;
        }
        else
        {
            return size.z;
        }
    }

    bool IsEmpty() const
    {
        bool ret = false;
        for (int i = 0; i < 3; ++i)
        {
            ret |= m_min[i] >= m_max[i];
        }
        return ret;
    }

    void GetCorners(glm::vec3 corners[8]) const
    {
        corners[0] = {m_min.x, m_min.y, m_min.z};
        corners[1] = {m_max.x, m_min.y, m_min.z};
        corners[2] = {m_min.x, m_max.y, m_min.z};
        corners[3] = {m_max.x, m_max.y, m_min.z};
        corners[4] = {m_min.x, m_min.y, m_max.z};
        corners[5] = {m_max.x, m_min.y, m_max.z};
        corners[6] = {m_min.x, m_max.y, m_max.z};
        corners[7] = {m_max.x, m_max.y, m_max.z};
    }

    void Merge(const AABB& bbox)
    {
        m_min.x = glm::min(m_min.x, bbox.m_min.x);
        m_min.y = glm::min(m_min.y, bbox.m_min.y);
        m_min.z = glm::min(m_min.z, bbox.m_min.z);
        m_max.x = glm::max(m_max.x, bbox.m_max.x);
        m_max.y = glm::max(m_max.y, bbox.m_max.y);
        m_max.z = glm::max(m_max.z, bbox.m_max.z);
    }

    void Merge(const glm::vec3& point)
    {
        m_min.x = glm::min(m_min.x, point.x);
        m_min.y = glm::min(m_min.y, point.y);
        m_min.z = glm::min(m_min.z, point.z);
        m_max.x = glm::max(m_max.x, point.x);
        m_max.y = glm::max(m_max.y, point.y);
        m_max.z = glm::max(m_max.z, point.z);
    }

    static AABB merge(const AABB& bbox1, const AABB& bbox2)
    {
        AABB ret;
        ret.m_min.x = glm::min(bbox1.m_min.x, bbox2.m_min.x);
        ret.m_min.y = glm::min(bbox1.m_min.y, bbox2.m_min.y);
        ret.m_min.z = glm::min(bbox1.m_min.z, bbox2.m_min.z);
        ret.m_max.x = glm::max(bbox1.m_max.x, bbox2.m_max.x);
        ret.m_max.y = glm::max(bbox1.m_max.y, bbox2.m_max.y);
        ret.m_max.z = glm::max(bbox1.m_max.z, bbox2.m_max.z);
        return ret;
    }

    static AABB merge(const AABB& bbox, const glm::vec3& point)
    {
        AABB ret;
        ret.m_min.x = glm::min(bbox.m_min.x, point.x);
        ret.m_min.y = glm::min(bbox.m_min.y, point.y);
        ret.m_min.z = glm::min(bbox.m_min.z, point.z);
        ret.m_max.x = glm::max(bbox.m_max.x, point.x);
        ret.m_max.y = glm::max(bbox.m_max.y, point.y);
        ret.m_max.z = glm::max(bbox.m_max.z, point.z);
        return ret;
    }

    void Grow(float amount)
    {
        m_min.x -= amount;
        m_min.y -= amount;
        m_min.z -= amount;
        m_max.x += amount;
        m_max.y += amount;
        m_max.z += amount;
    }

    AABB Transform(const glm::mat4 m) const
    {
        glm::vec3 newMin = glm::vec3(INF, INF, INF);
        glm::vec3 newMax = glm::vec3(NEG_INF, NEG_INF, NEG_INF);

        glm::vec3 corners[8];
        GetCorners(corners);
        for (int i = 0; i < 8; i++)
        {
            glm::vec3 p = TransformUtils::TransformPoint(corners[i], m);
            newMin = glm::min(newMin, p);
            newMax = glm::max(newMax, p);
        }
        return AABB(newMin, newMax);
    }

private:
    glm::vec3 m_min{};
    glm::vec3 m_max{};
};