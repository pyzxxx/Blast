#include "RHI/RHI.h"
#include "Foundation/Variant.h"
#include "Foundation/FileSystem.h"

int main()
{
    RHI::Startup();
    RHI::Shutdown();
    return 0;
}