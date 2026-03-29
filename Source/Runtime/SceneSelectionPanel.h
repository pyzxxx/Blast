#pragma once

#include "Panel.h"

class SceneSelectionPanel : public Panel
{
public:
    SceneSelectionPanel();
    ~SceneSelectionPanel() = default;

    const char* GetName() const override { return "Scene Selection"; }
    void DrawContent() override;

private:
    void RefreshSceneList();
    void LoadSelectedScene();
    void ReloadCurrentScene();

    std::vector<std::string> m_availableScenes;
    int m_selectedSceneIndex;
    bool m_needsRefresh;
};