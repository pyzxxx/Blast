#pragma once

#include <string>
#include <vector>

class SceneBrowser
{
public:
    static SceneBrowser& Get();

    static std::vector<std::string> GetAvailableScenes();

    void SetCurrentScene(const std::string& sceneName);
    const std::string& GetCurrentScene() const;

private:
    static constexpr const char* s_sceneDirectory = "Assets/Scenes";
    static constexpr const char* s_sceneExtension = ".scene";

    std::string m_currentScene;
};
