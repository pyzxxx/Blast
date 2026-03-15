#include "BaseModules.h"

#include "Foundation/Module.h"
#include "RHI/RHIModule.h"
#include "Asset/AssetManager.h"
#include "Rendering/ShaderCache.h"
#include "Rendering/Renderer.h"
#include "World/World.h"
#include "World/Model.h"

void RegisterBaseModules()
{
    MODULE(RHIModule);

    MODULE(AssetManager);
    
    MODULE(ShaderCache)
        DEPENDS(RHIModule);
    
    MODULE(Renderer)
        DEPENDS(ShaderCache)
        DEPENDS(RHIModule);
    
    MODULE(World);
    
    MODULE(ModelManager);
}