#include "PCH.h"

#include "BaseModules.h"

#include "Foundation/Module.h"
#include "Input/Input.h"
#include "RHI/RHIModule.h"
#include "Asset/AssetManager.h"
#include "Rendering/ShaderCache.h"
#include "Rendering/Renderer.h"
#include "World/World.h"
#include "World/Model.h"
#include "World/Camera.h"

void RegisterBaseModules()
{
    MODULE(RHIModule);

    MODULE(Input)
        ORDER(1000);

    MODULE(AssetManager)
        DEPENDS(RHIModule);

    MODULE(ShaderCache)
        DEPENDS(RHIModule);

    MODULE(Renderer)
        DEPENDS(ShaderCache)
        DEPENDS(RHIModule);

    MODULE(ModelManager);

    MODULE(CameraManager)
        ORDER(-100);

    MODULE(World)
        DEPENDS(Renderer)
        DEPENDS(CameraManager);
}