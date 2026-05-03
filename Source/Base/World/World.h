#pragma once

#include "Foundation/Module.h"
#include "Node.h"
#include "PCH.h"

class Model;
class Camera;
class Light;

class World : public Module<World>
{
public:
    void Initialize() override;
    void Terminate() override;

    void Load(const std::string& scenePath);
    void Unload();

    const std::vector<Model*>& GetModels() const { return m_models; }

private:
    void LoadSceneDirectory(const std::string& dirPath);
    void LoadSceneFile(const std::string& filePath);

private:
    std::vector<Model*> m_models;
    std::vector<Camera*> m_cameras;
    std::vector<Light*> m_lights;
    std::unordered_map<std::string, Node*> m_uuidToNode;
};
