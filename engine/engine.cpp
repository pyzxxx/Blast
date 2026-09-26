#include "engine.h"
#include "services.h"

#include <cstring>

bool Engine::init(OS& os, Game& game) {
    os_ = &os;
    game_ = &game;

    Services services;
    services.os = os_;
    services.vfs = &vfs_;
    services.paks = &paks_;
    services.gpu = &gpu_;
    services.rcore = &rcore_;
    services.loadsets = &loadsets_;
    services.jobs = &jobs_;
    services.assets = &assets_;
    services.world = &world_;
    services.ui = &ui_;
    services.renderer = &renderer_;
    services.input = &input_;

    if (!vfs_.mount("assets", os_->asset_root())) {
        return false;
    }

    const std::string shader_dir = std::string(os_->asset_root()) + "/shaders";
    if (!vfs_.mount("shaders", shader_dir.c_str())) {
        return false;
    }

    if (!paks_.init(services)) {
        return false;
    }

    if (!gpu_.init()) {
        return false;
    }

    if (!rcore_.init(services)) {
        return false;
    }

    if (!loadsets_.init()) {
        return false;
    }

    if (!jobs_.init()) {
        return false;
    }

    if (!assets_.init(services)) {
        return false;
    }

    if (!world_.init(services, 65536)) {
        return false;
    }

    if (!ui_.init(services)) {
        return false;
    }

    if (!renderer_.init(services)) {
        return false;
    }

    if (!game_->init(services)) {
        return false;
    }

    last_time_ = os_->time_seconds();
    return true;
}

void Engine::run() {
    while (!os_->should_quit()) {
        os_->poll_events();
        pump_input();

        double now = os_->time_seconds();
        float dt = static_cast<float>(now - last_time_);
        last_time_ = now;
        if (dt > 0.1f) {
            dt = 0.1f;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        os_->window_size(width, height);

        ui_.new_frame(dt, width, height);

        game_->update(dt);

        ui_.end_frame();

        assets_.update();

        rcore_.resize(width, height);

        if (rcore_.begin_frame()) {
            renderer_.render();
            rcore_.end_frame();
        }
    }
}

void Engine::shutdown() {
    game_->shutdown();
    world_.shutdown();
    renderer_.shutdown();
    ui_.shutdown();
    jobs_.shutdown();
    assets_.shutdown();
    loadsets_.shutdown();
    rcore_.shutdown();
    gpu_.shutdown();
    paks_.shutdown();
}

void Engine::pump_input() {
    memset(input_.pressed, 0, sizeof(input_.pressed));
    memset(input_.released, 0, sizeof(input_.released));
    memset(input_.mouse_pressed, 0, sizeof(input_.mouse_pressed));
    memset(input_.mouse_released, 0, sizeof(input_.mouse_released));
    input_.mouse_dx = 0.0f;
    input_.mouse_dy = 0.0f;
    input_.scroll_x = 0.0f;
    input_.scroll_y = 0.0f;

    InputEvent event;
    while (os_->poll_input_event(event)) {
        ui_.on_event(event);
        switch (event.type) {
            case InputEvent::Type::Key: {
                const auto& k = std::get<InputEvent::KeyData>(event.data);
                uint8_t idx = static_cast<uint8_t>(k.key);
                if (k.pressed) {
                    if (!input_.keys[idx]) {
                        input_.pressed[idx] = true;
                    }

                    input_.keys[idx] = true;
                } else {
                    input_.keys[idx] = false;
                    input_.released[idx] = true;
                }

                break;
            }
            case InputEvent::Type::MouseButton: {
                const auto& b = std::get<InputEvent::MouseButtonData>(event.data);
                uint8_t idx = static_cast<uint8_t>(b.button);
                if (idx >= 8) {
                    break;
                }

                if (b.pressed) {
                    if (!input_.mouse_buttons[idx]) {
                        input_.mouse_pressed[idx] = true;
                    }

                    input_.mouse_buttons[idx] = true;
                } else {
                    input_.mouse_buttons[idx] = false;
                    input_.mouse_released[idx] = true;
                }

                break;
            }
            case InputEvent::Type::MouseMove: {
                const auto& m = std::get<InputEvent::MoveData>(event.data);
                if (have_cursor_) {
                    input_.mouse_dx += m.x - input_.mouse_x;
                    input_.mouse_dy += m.y - input_.mouse_y;
                }

                input_.mouse_x = m.x;
                input_.mouse_y = m.y;
                have_cursor_ = true;
                break;
            }
            case InputEvent::Type::Scroll: {
                const auto& s = std::get<InputEvent::ScrollData>(event.data);
                input_.scroll_x += s.x;
                input_.scroll_y += s.y;
                break;
            }
            case InputEvent::Type::Char:
                break;
        }
    }
}
