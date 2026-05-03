#include "LightPanel.h"
#include "Foundation/Var.h"
#include <imgui.h>

LightPanel::LightPanel() {}

void LightPanel::DrawContent()
{
    ImGui::Text("Cluster Debug:");

    Var<bool>* clusterDebugVar = VarRegistry::Get().FindVarTyped<bool>("cv_clusterDebug");
    bool enabled = clusterDebugVar ? clusterDebugVar->Get() : false;
    if (ImGui::Checkbox("Enable Cluster Debug", &enabled))
    {
        if (clusterDebugVar)
        {
            clusterDebugVar->Set(enabled);
        }
    }

    ImGui::Separator();

    if (enabled)
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Cluster Debug: ON");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Cluster Debug: OFF");
    }
}
