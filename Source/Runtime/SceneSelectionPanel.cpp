#include "SceneSelectionPanel.h"
#include "Foundation/Log.h"
#include "Foundation/VFS.h"
#include "SceneBrowser.h"
#include "UserSettings.h"
#include "World/World.h"
#include <imgui.h>

SceneSelectionPanel::SceneSelectionPanel() : m_selectedSceneIndex(-1), m_needsRefresh(true) { RefreshSceneList(); }

void SceneSelectionPanel::DrawContent()
{
    if (m_needsRefresh)
    {
        RefreshSceneList();
        m_needsRefresh = false;
    }

    const std::string& currentScene = SceneBrowser::Get().GetCurrentScene();
    ImGui::Text("Current Scene:");
    if (currentScene.empty())
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "None");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", currentScene.c_str());
    }

    ImGui::Separator();
    ImGui::Text("Available Scenes:");

    if (m_availableScenes.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No scenes found in Assets/Scenes/");
    }
    else
    {
        ImGui::BeginChild("SceneList", ImVec2(0, 200), true);
        for (size_t i = 0; i < m_availableScenes.size(); ++i)
        {
            bool isSelected = (static_cast<int>(i) == m_selectedSceneIndex);
            if (ImGui::Selectable(m_availableScenes[i].c_str(), isSelected))
            {
                m_selectedSceneIndex = static_cast<int>(i);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        bool canLoad = m_selectedSceneIndex >= 0 && m_selectedSceneIndex < static_cast<int>(m_availableScenes.size());

        if (canLoad)
        {
            if (ImGui::Button("Load Selected Scene", ImVec2(200, 30)))
            {
                LoadSelectedScene();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Load Selected Scene", ImVec2(200, 30));
            ImGui::EndDisabled();
        }

        const std::string& currentScene = SceneBrowser::Get().GetCurrentScene();
        if (!currentScene.empty())
        {
            ImGui::Spacing();
            if (ImGui::Button("Reload Current Scene", ImVec2(200, 30)))
            {
                ReloadCurrentScene();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Refresh List", ImVec2(200, 30)))
        {
            m_needsRefresh = true;
        }
    }
}

void SceneSelectionPanel::RefreshSceneList()
{
    m_availableScenes = SceneBrowser::GetAvailableScenes();

    std::string selectedScene = UserSettings::Get().GetSelectedScene();
    if (!selectedScene.empty())
    {
        for (size_t i = 0; i < m_availableScenes.size(); ++i)
        {
            if (m_availableScenes[i] == selectedScene)
            {
                m_selectedSceneIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

void SceneSelectionPanel::LoadSelectedScene()
{
    if (m_selectedSceneIndex < 0 || m_selectedSceneIndex >= static_cast<int>(m_availableScenes.size()))
    {
        return;
    }

    std::string sceneName = m_availableScenes[m_selectedSceneIndex];
    std::string sceneDir = VFS::Join("Assets/Scenes", sceneName);

    World::Get()->Load(sceneDir);
    SceneBrowser::Get().SetCurrentScene(sceneName);
    UserSettings::Get().SetSelectedScene(sceneName);
    LOGI("SceneSelectionPanel: Loaded scene directory '%s'", sceneName.c_str());
}

void SceneSelectionPanel::ReloadCurrentScene()
{
    const std::string& currentScene = SceneBrowser::Get().GetCurrentScene();
    if (currentScene.empty())
    {
        return;
    }

    std::string sceneDir = VFS::Join("Assets/Scenes", currentScene);
    World::Get()->Unload();
    World::Get()->Load(sceneDir);
    LOGI("SceneSelectionPanel: Reloaded scene directory '%s'", currentScene.c_str());
}
