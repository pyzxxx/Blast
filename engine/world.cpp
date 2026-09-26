#include "world.h"
#include "pak_format.h"
#include "services.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

void Name::set(const char* s) {
    if (strlen(s) >= k_capacity) {
        LOGW("Name: '%s' truncated to %u chars", s, k_capacity - 1);
    }

    snprintf(str_, sizeof(str_), "%s", s);
}

bool Hierarchy::set_parent(ObjectHandle parent) {
    if (parent == owner()) {
        LOGE("Hierarchy: set_parent rejected, self");
        return false;
    }

    if (parent == k_object_invalid) {
        detach();
        if (Transform* t = world()->get<Transform>(owner())) {
            t->mark_dirty();
        }

        return true;
    }

    if (!world()->valid(parent)) {
        LOGE("Hierarchy: set_parent rejected, invalid parent");
        return false;
    }

    for (ObjectHandle cur = parent; cur != k_object_invalid;) {
        if (cur == owner()) {
            LOGE("Hierarchy: set_parent rejected, cycle");
            return false;
        }

        const Hierarchy* hc = world()->get<Hierarchy>(cur);
        cur = hc ? hc->parent_ : k_object_invalid;
    }

    if (parent_ == parent) {
        return true;
    }

    detach();
    Hierarchy& pc = world()->add<Hierarchy>(parent);
    parent_ = parent;
    prev_sibling_ = k_object_invalid;
    next_sibling_ = pc.first_child_;
    if (pc.first_child_ != k_object_invalid) {
        if (Hierarchy* fc = world()->get<Hierarchy>(pc.first_child_)) {
            fc->prev_sibling_ = owner();
        }
    }

    pc.first_child_ = owner();

    if (Transform* t = world()->get<Transform>(owner())) {
        t->mark_dirty();
    }

    return true;
}

void Hierarchy::detach() {
    if (prev_sibling_ != k_object_invalid) {
        if (Hierarchy* p = world()->get<Hierarchy>(prev_sibling_)) {
            p->next_sibling_ = next_sibling_;
        }
    } else if (parent_ != k_object_invalid) {
        if (Hierarchy* pp = world()->get<Hierarchy>(parent_)) {
            pp->first_child_ = next_sibling_;
        }
    }

    if (next_sibling_ != k_object_invalid) {
        if (Hierarchy* n = world()->get<Hierarchy>(next_sibling_)) {
            n->prev_sibling_ = prev_sibling_;
        }
    }

    parent_ = k_object_invalid;
    next_sibling_ = k_object_invalid;
    prev_sibling_ = k_object_invalid;
}

void Transform::mark_dirty() {
    dirty_ = true;
    world()->for_each_child(owner(), [this](ObjectHandle child) {
        if (Transform* ct = world()->get<Transform>(child)) {
            ct->mark_dirty();
        }
    });
}

glm::mat4 Transform::local_matrix() const {
    return glm::translate(glm::mat4(1.0f), position_) * glm::mat4_cast(rotation_) * glm::scale(glm::mat4(1.0f), scale_);
}

const glm::mat4& Transform::world_matrix() {
    if (!dirty_) {
        return world_mat_;
    }

    glm::mat4 parent_mat(1.0f);
    const Hierarchy* h = world()->get<Hierarchy>(owner());
    if (h && h->parent() != k_object_invalid) {
        if (Transform* pt = world()->get<Transform>(h->parent())) {
            parent_mat = pt->world_matrix();
        }
    }

    world_mat_ = parent_mat * local_matrix();
    dirty_ = false;
    return world_mat_;
}

bool World::init(const Services& services, uint32_t object_cap) {
    assets_ = services.assets;
    sets_ = services.loadsets;
    object_cap_ = object_cap;
    if (!objects_.init(object_cap, static_cast<uint8_t>(HandleTag::Object))) {
        return false;
    }

    sets_->register_hooks(HandleTag::Object,
                          {this, [](void* ctx, uint64_t h) { static_cast<World*>(ctx)->add_object_ref(h); },
                           [](void* ctx, uint64_t h) { static_cast<World*>(ctx)->destroy_object(h); }});
    return true;
}

void World::shutdown() {
    for (uint32_t s = 0; s < k_max_components; ++s) {
        if (ComponentPoolBase* p = pools_[s]) {
            p->shutdown();
            memfree(p);
            pools_[s] = nullptr;
        }
    }

    objects_.shutdown();
}

ObjectHandle World::create_object(LoadSet set) {
    if (set == k_loadset_invalid) {
        set = sets_->global();
    }

    ObjectHandle h = objects_.alloc();
    if (h == k_handle_invalid) {
        return k_object_invalid;
    }

    sets_->track(set, h);
    return h;
}

void World::add_object_ref(ObjectHandle h) {
    objects_.add_ref(h);
}

void World::destroy_object(ObjectHandle h) {
    if (!objects_.valid(h)) {
        return;
    }

    if (!objects_.release_ref(h)) {
        return;
    }

    std::vector<ObjectHandle> subtree;
    subtree.push_back(h);
    for (uint32_t i = 0; i < subtree.size(); ++i) {
        for_each_child(subtree[i], [&](ObjectHandle child) { subtree.push_back(child); });
    }

    if (Hierarchy* hc = get<Hierarchy>(h)) {
        hc->detach();
    }

    for (ObjectHandle e : subtree) {
        uint32_t idx = HandlePool<ObjectRec>::index_of(e);
        for (uint32_t s = 0; s < k_max_components; ++s) {
            if (pools_[s]) {
                pools_[s]->erase(idx);
            }
        }

        objects_.free(e);
    }
}

ObjectHandle World::spawn_prefab(const char* path, LoadSet set, AssetPriority priority) {
    return spawn_prefab(pak_format::hash_path(path), set, priority);
}

static bool parse_prefab_blob(uint64_t id, const std::vector<uint8_t>& bytes, pak_format::PrefabHeader& hdr,
                              const pak_format::PrefabNode*& nodes, const char*& strings,
                              const pak_format::PrefabComponent*& components) {
    if (bytes.size() < sizeof(pak_format::PrefabHeader)) {
        LOGE("World: prefab %llu truncated header", static_cast<uint64_t>(id));
        return false;
    }

    memcpy(&hdr, bytes.data(), sizeof(hdr));
    if (hdr.magic != pak_format::k_prefab_magic) {
        LOGE("World: prefab %llu bad magic", static_cast<uint64_t>(id));
        return false;
    }

    uint64_t strings_offset = sizeof(pak_format::PrefabHeader) + static_cast<uint64_t>(hdr.node_count) * sizeof(pak_format::PrefabNode);
    if (bytes.size() < strings_offset + hdr.string_table_size) {
        LOGE("World: prefab %llu truncated node table", static_cast<uint64_t>(id));
        return false;
    }

    nodes = reinterpret_cast<const pak_format::PrefabNode*>(bytes.data() + sizeof(pak_format::PrefabHeader));
    strings = reinterpret_cast<const char*>(bytes.data() + strings_offset);
    components = nullptr;
    if (hdr.component_count > 0) {
        uint64_t components_offset = strings_offset + hdr.string_table_size;
        if (bytes.size() < components_offset + static_cast<uint64_t>(hdr.component_count) * sizeof(pak_format::PrefabComponent)) {
            LOGW("World: prefab %llu truncated component section, %u components ignored",
                 static_cast<uint64_t>(id), hdr.component_count);
            hdr.component_count = 0;
        } else {
            components = reinterpret_cast<const pak_format::PrefabComponent*>(bytes.data() + components_offset);
        }
    }

    return true;
}

ObjectHandle World::spawn_prefab(uint64_t id, LoadSet set, AssetPriority priority) {
    std::vector<uint8_t> bytes;
    if (!assets_->prefab_blob(id, bytes)) {
        LOGE("World: prefab %llu not in any pak", static_cast<uint64_t>(id));
        return k_object_invalid;
    }

    pak_format::PrefabHeader hdr;
    const pak_format::PrefabNode* nodes = nullptr;
    const char* strings = nullptr;
    const pak_format::PrefabComponent* components = nullptr;
    if (!parse_prefab_blob(id, bytes, hdr, nodes, strings, components)) {
        return k_object_invalid;
    }

    ObjectHandle root = create_object(set);
    if (root == k_object_invalid) {
        LOGE("World: prefab %llu spawn failed, object pool exhausted", static_cast<uint64_t>(id));
        return k_object_invalid;
    }

    add<Transform>(root);

    std::vector<ObjectHandle> spawned(hdr.node_count, k_object_invalid);
    for (uint32_t i = 0; i < hdr.node_count; ++i) {
        const pak_format::PrefabNode& node = nodes[i];
        ObjectHandle e = create_object(set);
        if (e == k_object_invalid) {
            LOGE("World: prefab %llu node %u spawn failed, object pool exhausted", static_cast<uint64_t>(id), i);
            return k_object_invalid;
        }

        spawned[i] = e;

        if (node.name_offset < hdr.string_table_size &&
            memchr(strings + node.name_offset, '\0', hdr.string_table_size - node.name_offset) != nullptr) {
            add<Name>(e).set(strings + node.name_offset);
        }

        Transform& t = add<Transform>(e);
        t.set_position(glm::vec3(node.position[0], node.position[1], node.position[2]));
        t.set_rotation(glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));
        t.set_scale(glm::vec3(node.scale[0], node.scale[1], node.scale[2]));

        if (node.mesh_id != 0) {
            MeshHandle mesh = assets_->load(node.mesh_id, set, priority);
            MaterialHandle material = node.material_id != 0 ? assets_->load(node.material_id, set, priority) : k_handle_invalid;
            if (mesh != k_handle_invalid) {
                MeshComponent& mc = add<MeshComponent>(e);
                mc.set_mesh(mesh);
                mc.set_material(material);
            } else {
                LOGE("World: prefab %llu node %u mesh load failed", static_cast<uint64_t>(id), i);
            }
        }
    }

    for (uint32_t i = 0; i < hdr.node_count; ++i) {
        int32_t p = nodes[i].parent_index;
        ObjectHandle parent = root;
        if (p >= 0 && static_cast<uint32_t>(p) < hdr.node_count && static_cast<uint32_t>(p) != i && spawned[p] != k_object_invalid) {
            parent = spawned[p];
        }

        add<Hierarchy>(spawned[i]).set_parent(parent);
    }

    for (uint32_t i = 0; i < hdr.component_count; ++i) {
        const pak_format::PrefabComponent& comp = components[i];
        if (comp.node_index >= hdr.node_count || spawned[comp.node_index] == k_object_invalid) {
            LOGW("World: prefab %llu component %u bad node_index %u, skipped",
                 static_cast<uint64_t>(id), i, comp.node_index);
            continue;
        }

        ObjectHandle e = spawned[comp.node_index];
        if (comp.type == pak_format::k_component_camera) {
            float fov_y = 0.0f;
            float near_z = 0.0f;
            float far_z = 0.0f;
            memcpy(&fov_y, comp.payload + 0, sizeof(float));
            memcpy(&near_z, comp.payload + 4, sizeof(float));
            memcpy(&far_z, comp.payload + 8, sizeof(float));
            CameraComponent& cc = add<CameraComponent>(e);
            cc.set_fov_y(fov_y);
            cc.set_near_z(near_z);
            cc.set_far_z(far_z);
        } else if (comp.type == pak_format::k_component_light) {
            uint32_t light_type = 0;
            float color[3] = {};
            float range = 0.0f;
            memcpy(&light_type, comp.payload + 0, sizeof(uint32_t));
            memcpy(color, comp.payload + 4, sizeof(color));
            memcpy(&range, comp.payload + 20, sizeof(float));
            LightComponent& lc = add<LightComponent>(e);
            lc.set_light_type(light_type);
            lc.set_color(color[0], color[1], color[2]);
            lc.set_range(range);
        } else {
            LOGW("World: prefab %llu component %u unknown type %u, skipped",
                 static_cast<uint64_t>(id), i, comp.type);
        }
    }

    return root;
}
