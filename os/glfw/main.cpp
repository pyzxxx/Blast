#include "engine.h"
#include "os_glfw.h"
#include "viewer.h"

int main() {
    OS_Glfw os;
    if (!os.init("Blast", 1280, 720)) {
        return 1;
    }

    Viewer game;
    Engine engine;
    if (!engine.init(os, game)) {
        return 1;
    }

    engine.run();
    engine.shutdown();
    os.terminate();
    return 0;
}
