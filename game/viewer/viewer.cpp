#include "viewer.h"
#include "input_state.h"
#include "log.h"
#include "math_utils.h"
#include "pak_manager.h"
#include "renderer.h"
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
    renderer_ = services.renderer;
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
            if (CameraComponent* cc = world_->get<CameraComponent>(camera)) {
                float ev = cc->ev100();
                if (ImGui::SliderFloat("EV100", &ev, -5.0f, 20.0f, "%.1f")) {
                    cc->set_ev100(ev);
                }
            }
        } else {
            ImGui::TextUnformatted("no camera");
        }
    }

    draw_light_panel();

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

    if (dyn_root_ != k_object_invalid && dyn_animate_ && world_->valid(dyn_root_)) {
        if (Transform* t = world_->get<Transform>(dyn_root_)) {
            dyn_angle_ += dt * 0.8f;
            const float h = dyn_angle_ * 0.5f;
            t->set_rotation(glm::quat(glm::cos(h), 0.0f, glm::sin(h), 0.0f));
        }
    }

    camera_controller_.update(dt, *world_, *input_, *ui_);
}

void Viewer::shutdown() {
    clear_demo_lights();
    if (dyn_set_ != k_loadset_invalid) {
        sets_->release(dyn_set_);
        dyn_set_ = k_loadset_invalid;
        dyn_root_ = k_object_invalid;
    }
}

void Viewer::draw_light_panel() {
    if (!ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (renderer_ != nullptr) {
        float lux = renderer_->ambient()[0];
        if (ImGui::SliderFloat("ambient lux", &lux, 0.01f, 10000.0f, "%.2f lx", ImGuiSliderFlags_Logarithmic)) {
            renderer_->set_ambient(lux, lux, lux);
        }
    }

    if (renderer_ != nullptr && ImGui::TreeNode("Shadows")) {
        bool enabled = renderer_->shadow_enabled();
        if (ImGui::Checkbox("enabled", &enabled)) {
            renderer_->set_shadow_enabled(enabled);
        }

        bool local = renderer_->local_shadows_enabled();
        if (ImGui::Checkbox("local shadows", &local)) {
            renderer_->set_local_shadows_enabled(local);
        }

        float dist = renderer_->shadow_distance();
        if (ImGui::SliderFloat("distance", &dist, 10.0f, 1000.0f, "%.0f m")) {
            renderer_->set_shadow_distance(dist);
        }

        float radius = renderer_->shadow_pcss_radius();
        if (ImGui::SliderFloat("pcss light size", &radius, 0.0f, 0.2f, "%.3f")) {
            renderer_->set_shadow_pcss_radius(radius);
        }

        float bias = renderer_->shadow_depth_bias();
        if (ImGui::SliderFloat("depth bias", &bias, 0.0f, 1.0f, "%.3f m")) {
            renderer_->set_shadow_depth_bias(bias);
        }

        float nbias = renderer_->shadow_normal_bias();
        if (ImGui::SliderFloat("normal bias", &nbias, 0.0f, 8.0f, "%.2f texel")) {
            renderer_->set_shadow_normal_bias(nbias);
        }

        uint32_t full = 0, overlay = 0, cached = 0;
        renderer_->shadow_stats(full, overlay, cached);
        ImGui::Text("layers: %u full redraw, %u overlay, %u cached", full, overlay, cached);

        if (ImGui::Button("spawn dynamic copy") && selected_ >= 0 && dyn_root_ == k_object_invalid) {
            spawn_dynamic_copy();
        }
        if (dyn_root_ != k_object_invalid) {
            ImGui::SameLine();
            ImGui::Checkbox("animate", &dyn_animate_);
        }

        ImGui::TreePop();
    }

    static const char* k_type_names[] = {"point", "directional", "spot"};
    int index = 0;
    world_->each<LightComponent>([&](ObjectHandle, LightComponent& lc) {
        ImGui::PushID(index);
        uint32_t type = lc.light_type();
        if (type > LightComponent::k_type_spot) {
            type = LightComponent::k_type_point;
        }

        if (ImGui::TreeNode("##light", "Light %d (%s)", index, k_type_names[type])) {
            int t = static_cast<int>(type);
            if (ImGui::Combo("type", &t, k_type_names, 3)) {
                lc.set_light_type(static_cast<uint32_t>(t));
            }

            float c[3] = {lc.color()[0], lc.color()[1], lc.color()[2]};
            if (ImGui::ColorEdit3("color", c)) {
                lc.set_color(c[0], c[1], c[2]);
            }

            if (lc.light_type() == LightComponent::k_type_directional) {
                float lux = lc.intensity();
                if (ImGui::SliderFloat("lux", &lux, 0.1f, 100000.0f, "%.1f lx", ImGuiSliderFlags_Logarithmic)) {
                    lc.set_intensity_lux(lux);
                }
            } else {
                float lumens = lc.intensity_lumens();
                if (ImGui::SliderFloat("lumens", &lumens, 1.0f, 100000.0f, "%.1f lm", ImGuiSliderFlags_Logarithmic)) {
                    lc.set_intensity_lumens(lumens);
                }

                float r = lc.range();
                if (ImGui::DragFloat("range", &r, 0.1f, 0.1f, 1000.0f)) {
                    lc.set_range(r);
                }
            }

            if (lc.light_type() == LightComponent::k_type_spot) {
                float inner = lc.spot_inner() * k_rad2deg;
                float outer = lc.spot_outer() * k_rad2deg;
                if (ImGui::SliderFloat("inner deg", &inner, 1.0f, 89.0f) ||
                    ImGui::SliderFloat("outer deg", &outer, 1.0f, 89.0f)) {
                    lc.set_spot_angles(inner * k_deg2rad, outer * k_deg2rad);
                }
            }

            bool cast = lc.cast_shadow();
            if (ImGui::Checkbox("cast shadow", &cast)) {
                lc.set_cast_shadow(cast);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
        ++index;
    });

    if (ImGui::Button("spotlight at camera")) {
        add_flashlight();
    }

    ImGui::SameLine();
    if (ImGui::Button("add 16 point lights")) {
        add_random_lights(16);
    }

    ImGui::SameLine();
    if (ImGui::Button("clear demo lights")) {
        clear_demo_lights();
    }
}

void Viewer::add_flashlight() {
    if (light_set_ == k_loadset_invalid) {
        light_set_ = sets_->create("demo-lights");
    }

    ObjectHandle camera = k_object_invalid;
    world_->each<CameraComponent>([&](ObjectHandle h, CameraComponent&) {
        if (camera == k_object_invalid) {
            camera = h;
        }
    });

    ObjectHandle handle = world_->create_object(light_set_);
    Transform& t = world_->add<Transform>(handle);
    const Transform* ct = camera != k_object_invalid ? world_->get<Transform>(camera) : nullptr;
    if (ct != nullptr) {
        t.set_position(ct->position());
        t.set_rotation(ct->rotation());
    }

    LightComponent& light = world_->add<LightComponent>(handle);
    light.set_light_type(LightComponent::k_type_spot);
    light.set_intensity_lumens(800.0f);
    light.set_range(40.0f);
}

void Viewer::add_random_lights(uint32_t count) {
    if (light_set_ == k_loadset_invalid) {
        light_set_ = sets_->create("demo-lights");
    }

    auto frand = [](float lo, float hi) {
        return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
    };

    for (uint32_t i = 0; i < count; ++i) {
        ObjectHandle handle = world_->create_object(light_set_);
        Transform& t = world_->add<Transform>(handle);
        t.set_position(glm::vec3(frand(-20.0f, 20.0f), frand(2.0f, 12.0f), frand(-10.0f, 10.0f)));
        LightComponent& light = world_->add<LightComponent>(handle);
        light.set_light_type(LightComponent::k_type_point);
        light.set_color(frand(0.5f, 1.0f), frand(0.5f, 1.0f), frand(0.5f, 1.0f));
        light.set_intensity_lumens(frand(50.0f, 500.0f));
        light.set_range(frand(6.0f, 18.0f));
    }
}

void Viewer::clear_demo_lights() {
    if (light_set_ != k_loadset_invalid) {
        sets_->release(light_set_);
        light_set_ = k_loadset_invalid;
    }
}

void Viewer::mark_dynamic_recursive(ObjectHandle h) {
    if (MeshComponent* mc = world_->get<MeshComponent>(h)) {
        mc->set_static(false);
    }
    if (world_->get<CameraComponent>(h)) {
        world_->remove<CameraComponent>(h);
    }
    if (world_->get<LightComponent>(h)) {
        world_->remove<LightComponent>(h);
    }
    world_->for_each_child(h, [this](ObjectHandle child) { mark_dynamic_recursive(child); });
}

void Viewer::spawn_dynamic_copy() {
    if (dyn_set_ == k_loadset_invalid) {
        dyn_set_ = sets_->create("dynamic-copy");
    }

    dyn_root_ = world_->spawn_prefab(scenes_[selected_].id, dyn_set_, AssetPriority::Normal);
    if (dyn_root_ == k_object_invalid) {
        return;
    }

    mark_dynamic_recursive(dyn_root_);
    Transform& t = world_->add<Transform>(dyn_root_);
    t.set_position(glm::vec3(0.0f, 6.0f, 0.0f));
    t.set_scale(glm::vec3(0.05f));
    dyn_angle_ = 0.0f;
}

void Viewer::load_scene(int index) {
    scene_t0_ = std::chrono::steady_clock::now();

    if (dyn_set_ != k_loadset_invalid) {
        sets_->release(dyn_set_);
        dyn_set_ = k_loadset_invalid;
        dyn_root_ = k_object_invalid;
    }

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
        glm::vec3 eye(15.0f, 8.0f, 0.0f);
        glm::vec3 target(0.0f, 4.0f, 0.0f);
        const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
        t.set_position(eye);
        t.set_rotation(glm::quat_cast(glm::inverse(view)));
        world_->add<CameraComponent>(handle);
    }

    if (lights_after == lights_before) {
        ObjectHandle handle = world_->create_object(next);
        Transform& t = world_->add<Transform>(handle);
        glm::vec3 ldir(-0.5f, -1.0f, -0.3f);
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), ldir, glm::vec3(0.0f, 1.0f, 0.0f));
        t.set_rotation(glm::quat_cast(glm::inverse(view)));
        LightComponent& light = world_->add<LightComponent>(handle);
        light.set_light_type(LightComponent::k_type_directional);
        light.set_intensity_lux(1000.0f);
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
