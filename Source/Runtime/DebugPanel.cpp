#include "DebugPanel.h"
#include "Foundation/Var.h"
#include <imgui.h>

DebugPanel::DebugPanel() {}

void DebugPanel::DrawContent()
{
    Var<bool>* var = VarRegistry::Get().FindVarTyped<bool>("cv_bvhDebug");
    if (!var)
    {
        ImGui::Text("BVH debug var not available");
        return;
    }

    bool enabled = var->Get();
    if (ImGui::Checkbox("BVH Debug", &enabled))
    {
        var->Set(enabled);
    }
}
