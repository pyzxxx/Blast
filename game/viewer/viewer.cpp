#include "viewer.h"
#include "input_state.h"
#include "log.h"
#include "pak_manager.h"
#include "services.h"
#include "ui_system.h"
#include "world.h"

#include <imgui.h>

#include <chrono>
#include <cstdio>

bool Viewer::init(const Services& services) {
    assets_ = services.assets;
    sets_ = services.loadsets;
    paks_ = services.paks;
    ui_ = services.ui;
    world_ = services.world;
    input_ = services.input;

    std::vector<PakEntryInfo> entries;
    paks_->enumerate(pak_format::AssetType::Prefab, entries);
    for (const PakEntryInfo& entry : entries) {
        SceneItem item;
        item.id = entry.id;
        if (entry.path != nullptr && entry.path[0] != '\0') {
            item.label = entry.path;
        } else {
            char hex[20];
            snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(entry.id));
            item.label = hex;
        }

        scenes_.push_back(std::move(item));
    }

    if (!scenes_.empty()) {
        load_scene(0);
    }

    return true;
}

void Viewer::update(float dt) {
    ImGui::Begin("Viewer");

    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        const double ms = static_cast<double>(dt) * 1000.0;
        ImGui::Text("dt: %.3f ms (%.1f fps)", ms, dt > 0.0f ? 1.0 / static_cast<double>(dt) : 0.0);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("speed", &camera_controller_.move_speed, 0.1f, 200.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("sensitivity", &camera_controller_.look_sensitivity, 0.0005f, 0.01f, "%.4f");

        ObjectHandle camera = k_object_invalid;
        world_->each<CameraComponent>([&](ObjectHandle h, CameraComponent&) {
            if (camera == k_object_invalid) {
                camera = h;
            }
        });

        if (camera != k_object_invalid) {
            if (const Transform* t = world_->get<Transform>(camera)) {
                const glm::vec3& p = t->position();
                ImGui::Text("position: %.2f  %.2f  %.2f", static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z));
            }
        } else {
            ImGui::TextUnformatted("no camera");
        }
    }

    if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (loading_ && loading_set_ != k_loadset_invalid) {
            uint32_t done = 0;
            uint32_t total = 0;
            sets_->progress(loading_set_, done, total);
            const float fraction = total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f;
            ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));
        }

        if (scenes_.empty()) {
            ImGui::TextUnformatted("no scenes in mounted paks");
        } else if (ImGui::BeginListBox("##scenes", ImVec2(-1.0f, 240.0f))) {
            for (int i = 0; i < static_cast<int>(scenes_.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::Selectable(scenes_[i].label.c_str(), i == selected_) && i != selected_) {
                    load_scene(i);
                }

                ImGui::PopID();
            }

            ImGui::EndListBox();
        }
    }

    ImGui::End();

    camera_controller_.update(dt, *world_, *input_, *ui_);
}

void Viewer::shutdown() {
}

void Viewer::load_scene(int index) {
    scene_t0_ = std::chrono::steady_clock::now();

    const LoadSet prev_loading = loading_set_;
    LoadSet next = sets_->create("scene");
    loading_set_ = next;
    loading_ = true;
    sets_->set_on_idle(next, [this, next, index] { scene_load_done(next, index); });

    int cameras_before = 0;
    int lights_before = 0;
    world_->each<CameraComponent>([&](ObjectHandle, CameraComponent&) { ++cameras_before; });
    world_->each<LightComponent>([&](ObjectHandle, LightComponent&) { ++lights_before; });

    world_->spawn_prefab(scenes_[index].id, next, AssetPriority::Normal);

    int cameras_after = 0;
    int lights_after = 0;
    world_->each<CameraComponent>([&](ObjectHandle, CameraComponent&) { ++cameras_after; });
    world_->each<LightComponent>([&](ObjectHandle, LightComponent&) { ++lights_after; });

    if (prev_loading != k_loadset_invalid) {
        sets_->release(prev_loading);
    }

    if (cameras_after == cameras_before) {
        ObjectHandle handle = world_->create_object(next);
        Transform& t = world_->add<Transform>(handle);
        const glm::vec3 eye(15.0f, 8.0f, 0.0f);
        const glm::vec3 target(0.0f, 4.0f, 0.0f);
        const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
        t.set_position(eye);
        t.set_rotation(glm::quat_cast(glm::inverse(view)));
        world_->add<CameraComponent>(handle);
    }

    if (lights_after == lights_before) {
        ObjectHandle handle = world_->create_object(next);
        Transform& t = world_->add<Transform>(handle);
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(-0.5f, -1.0f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f));
        t.set_rotation(glm::quat_cast(glm::inverse(view)));
        LightComponent& light = world_->add<LightComponent>(handle);
        light.set_light_type(LightComponent::k_type_directional);
        light.set_color(3.0f, 3.0f, 3.0f);
    }

    uint32_t done = 0;
    uint32_t total = 0;
    sets_->progress(next, done, total);
    if (total == 0) {
        sets_->set_on_idle(next, nullptr);
        scene_load_done(next, index);
        return;
    }

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scene_t0_).count();
    LOGI("Viewer: scene %d loaded in %.1f ms", index, ms);
}

void Viewer::scene_load_done(LoadSet set, int index) {
    sets_->release(scene_set_);
    scene_set_ = set;
    if (loading_set_ == set) {
        loading_set_ = k_loadset_invalid;
    }

    loading_ = false;
    selected_ = index;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scene_t0_).count();
    LOGI("Viewer: scene %d fully resident in %.1f ms", index, ms);
}
