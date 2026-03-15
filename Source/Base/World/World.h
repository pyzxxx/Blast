#pragma once

#include "PCH.h"
#include "Foundation/Module.h"

class Model;

class World : public Module<World>
{
public:
    void Initialize() override;
    void Terminate() override;

    void LoadScene(const std::string& scenePath);
    void UnloadScene();

private:
    std::vector<Model*> m_models;
};