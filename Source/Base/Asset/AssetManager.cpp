#include "AssetManager.h"
#include "Foundation/FileSystem.h"

void AssetManager::Initialize()
{
}

void AssetManager::Terminate()
{
    for (auto& iter : m_assetDict)
    {
        delete iter.second;
    }
    m_assetDict.clear();
}

bool AssetManager::HasAsset(const std::string& assetPath)
{
    auto iter = m_assetDict.find(assetPath);
    if (iter != m_assetDict.end())
    {
        return true;
    }
    return false;
}

Asset* AssetManager::GetAsset(const std::string& assetPath)
{
    auto iter = m_assetDict.find(assetPath);
    if (iter != m_assetDict.end())
    {
        return iter->second;
    }
    return nullptr;
}


