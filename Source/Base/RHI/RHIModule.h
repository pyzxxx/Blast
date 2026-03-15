#pragma once

#include "Foundation/Module.h"

class RHIModule : public Module<RHIModule>
{
public:
    void Initialize() override;
    void Terminate() override;
};
