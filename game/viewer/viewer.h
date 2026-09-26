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

    AssetManager* assets_ = nullptr;
    LoadSets* sets_ = nullptr;
    PakManager* paks_ = nullptr;
    UiSystem* ui_ = nullptr;
    World* world_ = nullptr;
    const InputState* input_ = nullptr;
    std::vector<SceneItem> scenes_;
    CameraController camera_controller_;
    LoadSet scene_set_ = k_loadset_invalid;
    LoadSet loading_set_ = k_loadset_invalid;
    bool loading_ = false;
    int selected_ = -1;
    std::chrono::steady_clock::time_point scene_t0_{};
};
