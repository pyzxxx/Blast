#pragma once

#include "camera_controller.h"
#include "game.h"
#include "loadset.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class AssetManager;
class LoadSets;
class PakManager;
class Renderer;
class UiSystem;
class World;
struct InputState;

class Viewer : public Game {
public:
    bool init(const Services& services) override;
    void update(float dt) override;
    void shutdown() override;

private:
    struct SceneItem {
        uint64_t id;
        std::string label;
    };

    void load_scene(int index);
    void scene_load_done(LoadSet set, int index);
    void draw_light_panel();
    void add_flashlight();
    void add_random_lights(uint32_t count);
    void clear_demo_lights();
    void spawn_dynamic_copy();
    void mark_dynamic_recursive(ObjectHandle h);

    AssetManager* assets_ = nullptr;
    LoadSets* sets_ = nullptr;
    PakManager* paks_ = nullptr;
    Renderer* renderer_ = nullptr;
    UiSystem* ui_ = nullptr;
    World* world_ = nullptr;
    const InputState* input_ = nullptr;
    std::vector<SceneItem> scenes_;
    CameraController camera_controller_;
    LoadSet scene_set_ = k_loadset_invalid;
    LoadSet loading_set_ = k_loadset_invalid;
    LoadSet light_set_ = k_loadset_invalid;
    ObjectHandle dyn_root_ = k_object_invalid;
    LoadSet dyn_set_ = k_loadset_invalid;
    float dyn_angle_ = 0.0f;
    bool dyn_animate_ = true;
    bool loading_ = false;
    int selected_ = -1;
    std::chrono::steady_clock::time_point scene_t0_{};
};
