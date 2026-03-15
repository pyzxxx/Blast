#pragma once

class UserSettings
{
public:
    static UserSettings& Get();

    void Save();

    const std::string& GetSelectedScene() const;
    void SetSelectedScene(const std::string& scenePath);

private:
    void Load();

private:
    std::string m_selectedScene;
    bool m_loaded = false;
};
