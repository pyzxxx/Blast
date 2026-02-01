#include "Importer.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"

void ImportGltf(const std::string& rawPath, const std::string& outPath)
{

}

void ImportAsset(const std::string& rawPath, const std::string& outPath)
{
    std::string ext = FS::Path::Extension(rawPath);

    if (ext == ".gltf")
    {
        ImportGltf(rawPath, outPath);
    }
}