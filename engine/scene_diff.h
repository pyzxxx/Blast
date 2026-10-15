#pragma once

#include "mesh_library.h"
#include "material_library.h"
#include "render_core.h"
#include "world.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

struct ObjectKey {
    MeshHandle mesh = k_handle_invalid;
    MaterialHandle mat = k_handle_invalid;
    uint32_t albedo_slot = k_invalid_slot;
    uint32_t depth_flags = 0;
    glm::vec3 wbmin = glm::vec3(0.0f);
    glm::vec3 wbmax = glm::vec3(0.0f);
    bool casts_shadow = false;
    bool is_static = false;
};

enum class ChangeKind : uint8_t { Added, Removed, Changed };

struct ObjectChange {
    ObjectHandle owner = k_object_invalid;
    ChangeKind kind = ChangeKind::Added;
    ObjectKey prev;
    ObjectKey cur;
};

class SceneDiff {
public:
    void begin_frame();
    void track(ObjectHandle owner, const ObjectKey& key);
    void end_frame();

    const std::vector<ObjectChange>& changes() const { return changes_; }
    bool empty() const { return changes_.empty(); }

private:
    std::unordered_map<uint64_t, ObjectKey> prev_;
    std::unordered_map<uint64_t, ObjectKey> cur_;
    std::vector<ObjectChange> changes_;
};
