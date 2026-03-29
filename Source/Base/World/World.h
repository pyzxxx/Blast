#pragma once

#include "PCH.h"
#include "Node.h"
#include "Foundation/Module.h"

class Model;
class Camera;

class World : public Module<World>
{
public:
    void Initialize() override;
    void Terminate() override;

    void Load(const std::string& scenePath);
    void Unload();

private:
    void LoadSceneDirectory(const std::string& dirPath);
    void LoadSceneFile(const std::string& filePath);

private:
    std::vector<Model*> m_models;
    std::vector<Camera*> m_cameras;
    std::unordered_map<std::string, Node*> m_uuidToNode;
};
