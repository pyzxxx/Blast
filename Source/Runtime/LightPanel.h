#pragma once

#include "Panel.h"

class LightPanel : public Panel
{
public:
    LightPanel();

    const char* GetName() const override { return "Light"; }
    void DrawContent() override;
};
