#pragma once

#include "asset.h"
#include "game.h"
#include "gpu_driver.h"
#include "input_state.h"
#include "job_system.h"
#include "loadset.h"
#include "os.h"
#include "pak_manager.h"
#include "render_core.h"
#include "renderer.h"
#include "ui_system.h"
#include "vfs.h"
#include "world.h"

class Engine {
public:
    bool init(OS& os, Game& game);
    void run();
    void shutdown();

private:
    void pump_input();

    OS* os_ = nullptr;
    Game* game_ = nullptr;
    Vfs vfs_;
    PakManager paks_;
    GpuDriver gpu_;
    RenderCore rcore_;
    LoadSets loadsets_;
    JobSystem jobs_;
    AssetManager assets_;
    World world_;
    Renderer renderer_;
    UiSystem ui_;
    InputState input_;
    double last_time_ = 0.0;
    bool have_cursor_ = false;
};
