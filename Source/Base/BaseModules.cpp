#include "PCH.h"

#include "BaseModules.h"

#include "Asset/AssetManager.h"
#include "Foundation/Module.h"
#include "Input/Input.h"
#include "RHI/RHIModule.h"
#include "Rendering/RenderScene.h"
#include "Rendering/Renderer.h"
#include "Rendering/ShaderCache.h"
#include "World/Camera.h"
#include "World/Light.h"
#include "World/Model.h"
#include "World/World.h"

void RegisterBaseModules()
{
    MODULE(RHIModule);

    MODULE(Input)
    ORDER(1000);

    MODULE(ShaderCache)
    DEPENDS(RHIModule);

    MODULE(Renderer)
    DEPENDS(ShaderCache)
    DEPENDS(RHIModule);

    MODULE(AssetManager)
    DEPENDS(Renderer);

    MODULE(ModelManager)
    DEPENDS(AssetManager)
    DEPENDS(Renderer);

    MODULE(CameraManager);

    MODULE(LightManager)
    DEPENDS(Renderer);

    MODULE(World)
    DEPENDS(Renderer)
    DEPENDS(CameraManager)
    DEPENDS(LightManager);
}