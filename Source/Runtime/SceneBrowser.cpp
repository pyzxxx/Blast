#include "SceneBrowser.h"
#include "Foundation/FileSystem.h"

SceneBrowser& SceneBrowser::Get()
{
    static SceneBrowser s_instance;
    return s_instance;
}

std::vector<std::string> SceneBrowser::GetAvailableScenes()
{
    std::vector<std::string> scenes;

    std::string realPath = FS::Path::FixPath(s_sceneDirectory);
    if (!FS::IsDirectory(realPath))
    {
        return scenes;
    }

    for (const auto& entry : std::filesystem::directory_iterator(realPath))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        std::string dirName = entry.path().filename().string();
        std::string sceneFile = FS::Path::Join(s_sceneDirectory, dirName, dirName + s_sceneExtension);
        
        if (FS::IsFile(FS::Path::FixPath(sceneFile)))
        {
            scenes.push_back(dirName);
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
