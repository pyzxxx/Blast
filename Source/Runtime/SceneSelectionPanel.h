#pragma once

class SceneSelectionPanel
{
public:
    SceneSelectionPanel();
    ~SceneSelectionPanel() = default;

    void Draw();

private:
    void RefreshSceneList();
    void LoadSelectedScene();
    void ReloadCurrentScene();

    std::vector<std::string> m_availableScenes;
    int m_selectedSceneIndex;
    bool m_needsRefresh;
};
