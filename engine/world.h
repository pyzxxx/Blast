#pragma once

#include "asset.h"
#include "handle_pool.h"
#include "loadset.h"
#include "log.h"
#include "mem.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <new>

using ObjectHandle = uint64_t;
constexpr ObjectHandle k_object_invalid = k_handle_invalid;

class World;
struct Services;

class Component {
public:
    World* world() const { return world_; }
    ObjectHandle owner() const { return owner_; }

private:
    friend class World;

    void bind(World* world, ObjectHandle owner) {
        world_ = world;
        owner_ = owner;
    }

    World* world_ = nullptr;
    ObjectHandle owner_ = k_object_invalid;
};

class Name : public Component {
public:
    static constexpr uint32_t k_capacity = 32;

    const char* str() const { return str_; }

    void set(const char* s);

private:
    char str_[k_capacity] = {};
};

class Hierarchy : public Component {
public:
    ObjectHandle parent() const { return parent_; }
    ObjectHandle first_child() const { return first_child_; }
    ObjectHandle next_sibling() const { return next_sibling_; }

    bool set_parent(ObjectHandle parent);

    void detach();

private:
    ObjectHandle parent_ = k_object_invalid;
    ObjectHandle first_child_ = k_object_invalid;
    ObjectHandle next_sibling_ = k_object_invalid;
    ObjectHandle prev_sibling_ = k_object_invalid;
};

class Transform : public Component {
public:
    const glm::vec3& position() const { return position_; }
    const glm::quat& rotation() const { return rotation_; }
    const glm::vec3& scale() const { return scale_; }

    void set_position(const glm::vec3& p) {
        position_ = p;
        mark_dirty();
    }

    void set_rotation(const glm::quat& r) {
        rotation_ = r;
        mark_dirty();
    }

    void set_scale(const glm::vec3& s) {
        scale_ = s;
        mark_dirty();
    }

    const glm::mat4& world_matrix();

private:
    friend class Hierarchy;

    void mark_dirty();
    glm::mat4 local_matrix() const;

    glm::vec3 position_ = glm::vec3(0.0f);
    glm::quat rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale_ = glm::vec3(1.0f);
    glm::mat4 world_mat_ = glm::mat4(1.0f);
    bool dirty_ = true;
};

class MeshComponent : public Component {
public:
    MeshHandle mesh() const { return mesh_; }
    MaterialHandle material() const { return material_; }

    void set_mesh(MeshHandle mesh) { mesh_ = mesh; }
    void set_material(MaterialHandle material) { material_ = material; }

private:
    MeshHandle mesh_ = k_handle_invalid;
    MaterialHandle material_ = k_handle_invalid;
};

class CameraComponent : public Component {
public:
    float fov_y() const { return fov_y_; }
    float near_z() const { return near_z_; }
    float far_z() const { return far_z_; }

    void set_fov_y(float fov_y) { fov_y_ = fov_y; }
    void set_near_z(float near_z) { near_z_ = near_z; }
    void set_far_z(float far_z) { far_z_ = far_z; }

private:
    float fov_y_ = 1.0471976f;
    float near_z_ = 0.1f;
    float far_z_ = 1000.0f;
};

class LightComponent : public Component {
public:
    static constexpr uint32_t k_type_point = 0;
    static constexpr uint32_t k_type_directional = 1;

    uint32_t light_type() const { return light_type_; }
    const float* color() const { return color_; }
    float range() const { return range_; }

    void set_light_type(uint32_t light_type) { light_type_ = light_type; }
    void set_color(float r, float g, float b) {
        color_[0] = r;
        color_[1] = g;
        color_[2] = b;
    }

    void set_range(float range) { range_ = range; }

private:
    uint32_t light_type_ = k_type_point;
    float color_[3] = {1.0f, 1.0f, 1.0f};
    float range_ = 10.0f;
};

class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void shutdown() = 0;
    virtual void erase(uint32_t idx) = 0;
};

template<class T>
class ComponentPool : public ComponentPoolBase {
public:
    bool init(uint32_t cap) {
        sparse_ = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * cap));
        dense_ = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * cap));
        data_ = static_cast<T*>(memalloc(sizeof(T) * cap));
        if (!sparse_ || !dense_ || !data_) {
            LOGE("ComponentPool: alloc failed (cap %u)", cap);
            return false;
        }

        memset(sparse_, 0xFF, sizeof(uint32_t) * cap);
        cap_ = cap;
        size_ = 0;
        return true;
    }

    void shutdown() override {
        for (uint32_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }

        memfree(sparse_);
        memfree(dense_);
        memfree(data_);
        sparse_ = nullptr;
        dense_ = nullptr;
        data_ = nullptr;
        cap_ = 0;
        size_ = 0;
    }

    T& add(uint32_t idx) {
        if (T* t = get(idx)) {
            return *t;
        }

        if (idx >= cap_ || size_ >= cap_) {
            LOGE("ComponentPool: add out of range (idx %u, size %u, cap %u)", idx, size_, cap_);
            assert(false);
            static T fallback{};
            return fallback;
        }

        uint32_t slot = size_++;
        sparse_[idx] = slot;
        dense_[slot] = idx;
        new (&data_[slot]) T();
        return data_[slot];
    }

    T* get(uint32_t idx) {
        if (idx >= cap_) {
            return nullptr;
        }

        uint32_t slot = sparse_[idx];
        if (slot == k_slot_invalid || dense_[slot] != idx) {
            return nullptr;
        }

        return &data_[slot];
    }

    void erase(uint32_t idx) override {
        T* t = get(idx);
        if (!t) {
            return;
        }

        uint32_t slot = sparse_[idx];
        uint32_t last = size_ - 1;
        if (slot != last) {
            uint32_t moved = dense_[last];
            data_[slot] = static_cast<T&&>(data_[last]);
            dense_[slot] = moved;
            sparse_[moved] = slot;
        }

        data_[last].~T();
        sparse_[idx] = k_slot_invalid;
        size_--;
    }

    template<class F>
    void each(F&& fn) {
        for (uint32_t i = 0; i < size_; ++i) {
            fn(dense_[i], data_[i]);
        }
    }

private:
    static constexpr uint32_t k_slot_invalid = 0xFFFFFFFF;

    uint32_t* sparse_ = nullptr;
    uint32_t* dense_ = nullptr;
    T* data_ = nullptr;
    uint32_t cap_ = 0;
    uint32_t size_ = 0;
};

class World {
public:
    bool init(const Services& services, uint32_t object_cap);

    void shutdown();

    bool valid(ObjectHandle h) const {
        return objects_.valid(h);
    }

    ObjectHandle create_object(LoadSet set = k_loadset_invalid);

    ObjectHandle spawn_prefab(const char* path, LoadSet set = k_loadset_invalid, AssetPriority priority = AssetPriority::Immediate);

    ObjectHandle spawn_prefab(uint64_t id, LoadSet set = k_loadset_invalid, AssetPriority priority = AssetPriority::Immediate);

    template<class T>
    T& add(ObjectHandle h) {
        T& t = pool<T>().add(HandlePool<ObjectRec>::index_of(h));
        t.bind(this, h);
        return t;
    }

    template<class T>
    T* get(ObjectHandle h) {
        ComponentPool<T>* p = find_pool<T>();
        return p ? p->get(HandlePool<ObjectRec>::index_of(h)) : nullptr;
    }

    template<class T>
    void remove(ObjectHandle h) {
        if (ComponentPool<T>* p = find_pool<T>()) {
            p->erase(HandlePool<ObjectRec>::index_of(h));
        }
    }

    template<class T, class F>
    void each(F&& fn) {
        if (ComponentPool<T>* p = find_pool<T>()) {
            p->each([&](uint32_t idx, T& t) { fn(objects_.handle_of(idx), t); });
        }
    }

    template<class F>
    void for_each_child(ObjectHandle parent, F&& fn) {
        const Hierarchy* h = get<Hierarchy>(parent);
        ObjectHandle child = h ? h->first_child() : k_object_invalid;
        while (child != k_object_invalid) {
            fn(child);
            const Hierarchy* hc = get<Hierarchy>(child);
            child = hc ? hc->next_sibling() : k_object_invalid;
        }
    }

private:
    struct ObjectRec {};

    void destroy_object(ObjectHandle h);
    void add_object_ref(ObjectHandle h);

    template<class T>
    ComponentPool<T>& pool() {
        uint32_t id = component_index<T>();
        assert(id < k_max_components);
        if (!pools_[id]) {
            ComponentPool<T>* p = static_cast<ComponentPool<T>*>(memalloc(sizeof(ComponentPool<T>)));
            new (p) ComponentPool<T>();
            if (!p->init(object_cap_)) {
                LOGE("World: component pool init failed");
                assert(false);
                memfree(p);
                p = nullptr;
            }

            pools_[id] = p;
        }

        return *static_cast<ComponentPool<T>*>(pools_[id]);
    }

    template<class T>
    ComponentPool<T>* find_pool() {
        uint32_t id = component_index<T>();
        if (id >= k_max_components) {
            return nullptr;
        }

        return static_cast<ComponentPool<T>*>(pools_[id]);
    }

    template<class T>
    static uint32_t component_index() {
        static const uint32_t id = next_component_id_++;
        return id;
    }

    static constexpr uint32_t k_max_components = 32;
    static inline uint32_t next_component_id_ = 0;

    HandlePool<ObjectRec> objects_;
    AssetManager* assets_ = nullptr;
    LoadSets* sets_ = nullptr;
    ComponentPoolBase* pools_[k_max_components] = {};
    uint32_t object_cap_ = 0;
};
