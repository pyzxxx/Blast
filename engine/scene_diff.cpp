#include "scene_diff.h"

static bool object_key_equal(const ObjectKey& a, const ObjectKey& b) {
    return a.mesh == b.mesh && a.mat == b.mat && a.albedo_slot == b.albedo_slot &&
           a.depth_flags == b.depth_flags && a.wbmin == b.wbmin && a.wbmax == b.wbmax &&
           a.casts_shadow == b.casts_shadow && a.is_static == b.is_static;
}

void SceneDiff::begin_frame() {
    changes_.clear();
    cur_.clear();
}

void SceneDiff::track(ObjectHandle owner, const ObjectKey& key) {
    cur_.emplace(owner, key);
}

void SceneDiff::end_frame() {
    for (const auto& [owner, k] : cur_) {
        auto it = prev_.find(owner);
        if (it == prev_.end()) {
            changes_.push_back({owner, ChangeKind::Added, {}, k});
        } else if (!object_key_equal(it->second, k)) {
            changes_.push_back({owner, ChangeKind::Changed, it->second, k});
        }
    }
    for (const auto& [owner, k] : prev_) {
        if (cur_.find(owner) == cur_.end()) {
            changes_.push_back({owner, ChangeKind::Removed, k, {}});
        }
    }
    prev_.swap(cur_);
}
