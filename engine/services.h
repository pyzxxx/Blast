#pragma once

class OS;
class GpuDriver;
class JobSystem;
class RenderCore;
class Vfs;
class PakManager;
class LoadSets;
class AssetManager;
class World;
class Renderer;
class UiSystem;
struct InputState;

struct Services {
    OS* os = nullptr;
    GpuDriver* gpu = nullptr;
    RenderCore* rcore = nullptr;
    Vfs* vfs = nullptr;
    PakManager* paks = nullptr;
    LoadSets* loadsets = nullptr;
    AssetManager* assets = nullptr;
    World* world = nullptr;
    Renderer* renderer = nullptr;
    UiSystem* ui = nullptr;
    JobSystem* jobs = nullptr;
    const InputState* input = nullptr;
};
