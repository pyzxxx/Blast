#include "World.h"
#include "Model.h"
#include "Asset/AssetManager.h"
#include "Asset/MeshAsset.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"

void World::Initialize()
{
}

void World::Terminate()
{
    UnloadScene();
}

void World::LoadScene(const std::string& scenePath)
{
    JsonReader reader(FS::Path::FixPath(scenePath));

    std::unordered_map<std::string, Node*> uuidToNode;
    std::unordered_map<std::string, std::string> hierarchy;

    reader.Array("nodes", [&]()
    {
        std::string type;
        std::string uuid;
        std::string parent;
        glm::vec3 translation;
        glm::vec3 scale;
        glm::vec3 euler;

        reader.Field("type", type);
        reader.Field("uuid", uuid);
        reader.Field("translation", translation);
        reader.Field("scale", scale);
        reader.Field("euler", euler);

        if (type == "Model")
        {
            std::string meshAssetPath;
            reader.Field("mesh", meshAssetPath);

            Model* model = ModelManager::Get()->CreateModel();
            model->SetLocalTranslation(translation);
            model->SetLocalScale(scale);
            model->SetLocalEuler(euler);
            model->meshAsset = (MeshAsset*)AssetManager::Get()->GetAsset(meshAssetPath);

            m_models.push_back(model);
            uuidToNode[uuid] = model;
            if (reader.Field("parent", parent))
            {
                hierarchy[uuid] = parent;
            }
        }
    });

    for (auto& [uuid, node] : uuidToNode)
    {
        auto iter = hierarchy.find(uuid);
        if (iter != hierarchy.end())
        {
            Node* parent = uuidToNode[iter->second];
            node->SetParent(parent);
        }
    }
}

void World::UnloadScene()
{
    for (auto model : m_models)
    {
        ModelManager::Get()->DestroyModel(model);
    }
    m_models.clear();
}


