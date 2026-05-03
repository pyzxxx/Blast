#include "RuntimeModules.h"
#include "CameraController.h"
#include "Foundation/Module.h"
#include "World/Camera.h"
#include "World/World.h"

void RegisterRuntimeModules()
{
    MODULE(CameraController)
    DEPENDS(CameraManager)
    DEPENDS(World)
    DEPENDS(Input);
}
