#include "ShaderCache.h"

#include "Foundation/Hash.h"
#include "Foundation/FileSystem.h"

std::unordered_map<std::size_t, RHI::Shader*> ShaderCache::m_shaderDict;

void ShaderCache::Initialize()
{
}

void ShaderCache::Terminate()
{
    for (auto& pair : m_shaderDict)
    {
        RHI::DestroyShader(pair.second);
    }
    m_shaderDict.clear();
}

RHI::Shader* ShaderCache::Get(const std::string& path)
{
    std::vector<std::string> macros;
    return Get(path, macros);
}

RHI::Shader* ShaderCache::Get(const std::string& path, const std::vector<std::string>& macros)
{
    std::size_t hash = 0;
    HashCombine(hash, path);
    for (const auto& macro : macros)
    {
        HashCombine(hash, macro);
    }

    auto iter = m_shaderDict.find(hash);
    if (iter != m_shaderDict.end())
    {
        return iter->second;
    }

    void* data = nullptr;
    uint32_t size = 0;
    RHI::Shader* shader = nullptr;
    Compile(path, macros, &data, size);

    if (data && size > 0)
    {
        RHI::CreateShader(data, size, shader);
        m_shaderDict[hash] = shader;
        free(data);
    }

    return shader;
}

void ShaderCache::Compile(const std::string& path, const std::vector<std::string>& macros, void** data, uint32_t& size)
{
    std::string fileName = FS::Path::FileName(path);
    std::string parentPath = FS::Path::ParentPath(path);
    std::string binDir = parentPath + "/bin";

    if (!FS::IsDirectory(binDir))
    {
        FS::MakeDirectory(path);
    }

    std::string logFilePath = binDir + "/" + fileName + "Compile.log";
    std::string outFilePath = binDir + "/" + fileName + ".spv";

    std::string glslangValidator = getenv("VULKAN_SDK");
    glslangValidator += "/Bin/glslangValidator";

    std::string commandLine;
    commandLine += glslangValidator;

    char buff[256];
    sprintf(buff, R"( -V "%s" --target-env vulkan1.3 -o "%s")", path.c_str(), outFilePath.c_str());
    commandLine += buff;

    for (const auto& macro : macros)
    {
        commandLine += " \"-D" + macro + "\"";
    }

    sprintf(buff, " >\"%s\"", logFilePath.c_str());
    commandLine += buff;

    if (std::system(commandLine.c_str()) == 0)
    {
        std::shared_ptr<FS::File> file = std::shared_ptr<FS::File>(FS::File::Open(outFilePath, FS::FileMode::Read));
        if (file)
        {
            size = file->GetSize();
            *data = malloc(size);
            file->Read(static_cast<uint8_t*>(*data), size);
            return;
        }
    }

    *data = nullptr;
    size = 0;
}