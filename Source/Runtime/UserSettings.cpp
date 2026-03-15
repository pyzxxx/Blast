#include "UserSettings.h"
#include "Foundation/JsonIO.h"
#include "Foundation/FileSystem.h"
#include "Foundation/Log.h"

static std::string GetSettingsFilePath()
{
    static std::string path = FS::Path::Join(FS::Path::ExecutableDir(), "UserSetting.json");
    return path;
}

UserSettings& UserSettings::Get()
{
    static UserSettings instance;
    if (!instance.m_loaded)
    {
        instance.Load();
        instance.m_loaded = true;
    }
    return instance;
}

void UserSettings::Load()
{
    std::shared_ptr<FS::File> file(FS::File::Open(GetSettingsFilePath(), FS::FileMode::Read));
    if (!file)
    {
        LOGW("UserSettings: File not found, using defaults");
        Save();
        return;
    }

    size_t size = file->GetSize();
    if (size == 0)
    {
        LOGW("UserSettings: File is empty, using defaults");
        Save();
        return;
    }

    std::vector<uint8_t> buffer(size);
    if (!file->Read(buffer.data(), size))
    {
        LOGW("UserSettings: Failed to read file");
        Save();
        return;
    }

    JsonReader reader(reinterpret_cast<const char*>(buffer.data()), static_cast<uint32_t>(size));
    std::string selectedScene;
    if (reader.Field("selectedScene", selectedScene))
    {
        m_selectedScene = selectedScene;
    }
    else
    {
        LOGW("UserSettings: selectedScene field not found");
        m_selectedScene.clear();
    }
}

void UserSettings::Save()
{
    JsonWriter writer;
    writer.Object([&]() {
        writer.Field("selectedScene", m_selectedScene);
    });

    std::shared_ptr<FS::File> file(FS::File::Open(GetSettingsFilePath(), FS::FileMode::Write));
    if (!file)
    {
        LOGW("UserSettings: Failed to open file for writing");
        return;
    }

    const char* data = writer.GetString();
    size_t size = writer.GetSize();
    file->Write((uint8_t*)data, size);
}

const std::string& UserSettings::GetSelectedScene() const
{
    return m_selectedScene;
}

void UserSettings::SetSelectedScene(const std::string& scenePath)
{
    m_selectedScene = scenePath;
    Save();
}
