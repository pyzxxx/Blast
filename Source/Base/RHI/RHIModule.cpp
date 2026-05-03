#include "RHIModule.h"
#include "RHI.h"

void RHIModule::Initialize() { RHI::Startup(); }

void RHIModule::Terminate() { RHI::Shutdown(); }
