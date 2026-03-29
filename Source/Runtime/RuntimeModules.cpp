#include "RuntimeModules.h"
#include "Foundation/Module.h"
#include "CameraController.h"
#include "World/Camera.h"
#include "World/World.h"

void RegisterRuntimeModules()
{
    MODULE(CameraController)
        DEPENDS(CameraManager)
        DEPENDS(World)
        DEPENDS(Input);
}
