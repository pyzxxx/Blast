#pragma once

#include "Asset.h"
#include "Foundation/Module.h"

#include <unordered_map>

class AssetManager : public Module<AssetManager>
{
public:
    AssetManager() = default;
    ~AssetManager() = default;

    void Initialize() override;
    void Terminate() override;

    template<class T>
    T* Create(const std::string& assetPath)
    {
        if (HasAsset(assetPath))
        {
            return nullptr;
        }

        T* asset = new T(assetPath);
        m_assetDict[assetPath] = asset;
        return (T*)asset;
    }

    template<class T>
    T* Load(const std::string& assetPath)
    {
        if (HasAsset(assetPath))
        {
            return (T*)GetAsset(assetPath);
        }

        T* asset = Create<T>(assetPath);
        return asset;
    }

    bool HasAsset(const std::string& assetPath);

    Asset* GetAsset(const std::string& assetPath);

private:
    std::unordered_map<std::string, Asset*> m_assetDict;
};