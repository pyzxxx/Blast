#pragma once

#include <string>

class SceneBrowser
{
public:
    static SceneBrowser& Get();

    static std::vector<std::string> GetAvailableScenes();

    void SetCurrentScene(const std::string& sceneName);
    const std::string& GetCurrentScene() const;

private:
    static constexpr const char* s_sceneDirectory = "asset://Scene";
    static constexpr const char* s_sceneExtension = ".scene";

    std::string m_currentScene;
};
