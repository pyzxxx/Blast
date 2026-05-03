#pragma once

#include "Panel.h"

class DebugPanel : public Panel
{
public:
    DebugPanel();

    const char* GetName() const override { return "Debug"; }
    void DrawContent() override;
};
