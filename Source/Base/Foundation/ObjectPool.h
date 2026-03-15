#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>

#define INVALID_HANDLE 0

template<typename T>
class ObjectPool
{
public:
    explicit ObjectPool(uint32_t reservedCount = 8)
    {
        m_nextHandle = 1;
        m_objects.reserve(reservedCount);
        m_handles.reserve(reservedCount);
        m_lookup.reserve(reservedCount);
    }

    void Clear()
    {
        m_objects.clear();
        m_handles.clear();
        m_lookup.clear();
    }

    size_t Size() const { return m_objects.size(); }

    size_t Capacity() const { return m_objects.capacity(); }

    T* Data() { return m_objects.data(); }

    template<typename... Args>
    uint32_t Add(Args&&... args)
    {
        uint32_t handle = m_nextHandle++;

        m_objects.emplace_back(std::forward<Args>(args)...);
        m_handles.push_back(handle);
        m_lookup[handle] = static_cast<uint32_t>(m_objects.size() - 1);
        return handle;
    }

    void Remove(uint32_t handle)
    {
        auto it = m_lookup.find(handle);
        if (it == m_lookup.end()) return;

        uint32_t index = it->second;
        uint32_t lastIndex = static_cast<uint32_t>(m_objects.size()) - 1;

        if (index != lastIndex)
        {
            m_objects[index] = std::move(m_objects[lastIndex]);
            m_handles[index] = m_handles[lastIndex];
            uint32_t movedHandle = m_handles[index];
            m_lookup[movedHandle] = index;
        }

        m_objects.pop_back();
        m_handles.pop_back();
        m_lookup.erase(handle);
    }

    T* Get(uint32_t handle)
    {
        auto it = m_lookup.find(handle);
        return (it != m_lookup.end()) ? &m_objects[it->second] : nullptr;
    }

    const T* Get(uint32_t handle) const
    {
        auto it = m_lookup.find(handle);
        return (it != m_lookup.end()) ? &m_objects[it->second] : nullptr;
    }

    uint32_t GetIndex(uint32_t handle)
    {
        auto it = m_lookup.find(handle);
        return (it != m_lookup.end()) ? it->second : UINT32_MAX;
    }

    T& operator[](size_t index) { return m_objects[index]; }
    const T& operator[](size_t index) const { return m_objects[index]; }

private:
    uint32_t m_nextHandle;
    std::vector<T> m_objects;
    std::vector<uint32_t> m_handles;
    std::unordered_map<uint32_t, uint32_t> m_lookup;
};