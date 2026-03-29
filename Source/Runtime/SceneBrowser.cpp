#include "SceneBrowser.h"
#include "Foundation/VFS.h"

SceneBrowser& SceneBrowser::Get()
{
    static SceneBrowser s_instance;
    return s_instance;
}

std::vector<std::string> SceneBrowser::GetAvailableScenes()
{
    std::vector<std::string> scenes;
    const std::string sceneDir = "Assets/Scene";

    if (!VFS::IsDirectory(sceneDir))
    {
        return scenes;
    }

    std::vector<std::string> entries = VFS::ListDirectory(sceneDir);
    for (const auto& entry : entries)
    {
        std::string fullPath = VFS::Join(sceneDir, entry);
        if (!VFS::IsDirectory(fullPath))
        {
            continue;
        }

        std::vector<std::string> sceneFiles = VFS::ListDirectory(fullPath);
        bool hasSceneFile = false;
        for (const auto& file : sceneFiles)
        {
            if (VFS::Extension(file) == ".scene")
            {
                hasSceneFile = true;
                break;
            }
        }

        if (hasSceneFile)
        {
            scenes.push_back(entry);
        }
    }

    return scenes;
}

void SceneBrowser::SetCurrentScene(const std::string& sceneName)
{
    m_currentScene = sceneName;
}

const std::string& SceneBrowser::GetCurrentScene() const
{
    return m_currentScene;
}
