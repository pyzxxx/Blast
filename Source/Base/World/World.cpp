#include "World.h"
#include "Model.h"
#include "Camera.h"
#include "Asset/AssetManager.h"
#include "Asset/MeshAsset.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"
#include "Foundation/Log.h"

#include <filesystem>

void World::Initialize()
{
}

void World::Terminate()
{
    Unload();
}

void World::Load(const std::string& scenePath)
{
    if (!VFS::IsDirectory(scenePath))
    {
        LOGE("World: Path is not a directory: %s", scenePath.c_str());
        return;
    }
    LoadSceneDirectory(scenePath);
}

void World::LoadSceneDirectory(const std::string& dirPath)
{
    if (!VFS::IsDirectory(dirPath))
    {
        LOGE("World: Scene directory does not exist: %s", dirPath.c_str());
        return;
    }

    std::vector<std::string> sceneFiles;
    std::vector<std::string> entries = VFS::ListDirectory(dirPath);
    for (const auto& entry : entries)
    {
        std::string fullPath = VFS::Join(dirPath, entry);
        if (VFS::IsFile(fullPath) && VFS::Extension(entry) == ".scene")
        {
            sceneFiles.push_back(fullPath);
        }
    }

    std::sort(sceneFiles.begin(), sceneFiles.end());

    for (const auto& filePath : sceneFiles)
    {
        LOGI("World: Loading scene file: %s", filePath.c_str());
        LoadSceneFile(filePath);
    }
    
    if (!m_cameras.empty())
    {
        bool hasPrimary = false;
        for (auto camera : m_cameras)
        {
            if (camera->IsPrimary())
            {
                hasPrimary = true;
                break;
            }
        }
        
        if (!hasPrimary)
        {
            CameraManager::Get()->SetPrimaryCamera(m_cameras[0]);
            LOGI("World: Auto-set first camera as primary");
        }
    }
    else
    {
        LOGW("World: No cameras loaded from scene directory: %s", dirPath.c_str());
    }
}

void World::LoadSceneFile(const std::string& filePath)
{
    JsonReader reader(filePath);

    std::unordered_map<std::string, Node*> localUuidToNode;
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
            model->meshAsset = AssetManager::Get()->Load<MeshAsset>(meshAssetPath);

            m_models.push_back(model);
            localUuidToNode[uuid] = model;
            if (reader.Field("parent", parent))
            {
                hierarchy[uuid] = parent;
            }
        }
        else if (type == "Camera")
        {
            Camera* camera = CameraManager::Get()->CreateCamera();
            
            camera->SetLocalTranslation(translation);
            camera->SetLocalScale(scale);
            camera->SetLocalEuler(euler);
            
            float fov = 60.0f;
            float nearPlane = 0.1f;
            float farPlane = 1000.0f;
            
            reader.Field("fov", fov);
            reader.Field("nearPlane", nearPlane);
            reader.Field("farPlane", farPlane);
            
            camera->SetPerspective(fov, nearPlane, farPlane);
            
            m_cameras.push_back(camera);
            
            bool isPrimary = false;
            if (reader.Field("isPrimary", isPrimary) && isPrimary)
            {
                CameraManager::Get()->SetPrimaryCamera(camera);
            }
            localUuidToNode[uuid] = camera;
            if (reader.Field("parent", parent))
            {
                hierarchy[uuid] = parent;
            }
        }
    });

    for (auto& [uuid, node] : localUuidToNode)
    {
        if (m_uuidToNode.find(uuid) != m_uuidToNode.end())
        {
            LOGW("World: UUID collision detected for node %s. Using later version.", uuid.c_str());
        }
        m_uuidToNode[uuid] = node;
    }

    for (auto& [uuid, parentUuid] : hierarchy)
    {
        auto childIter = m_uuidToNode.find(uuid);
        auto parentIter = m_uuidToNode.find(parentUuid);
        
        if (childIter != m_uuidToNode.end() && parentIter != m_uuidToNode.end())
        {
            childIter->second->SetParent(parentIter->second);
        }
    }
}

void World::Unload()
{
    for (auto model : m_models)
    {
        ModelManager::Get()->DestroyModel(model);
    }
    m_models.clear();
    
    for (auto camera : m_cameras)
    {
        CameraManager::Get()->DestroyCamera(camera);
    }
    m_cameras.clear();
    
    m_uuidToNode.clear();
}
