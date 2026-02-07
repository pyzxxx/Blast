#include "Importer.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"
#include "Foundation/UUID.h"
#include "Math/BoundingBox.h"
#include "RHI/RHI.h"

#include <set>
#include <map>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

glm::mat4 GetLocalMatrixFromGltf(cgltf_node* node)
{
    glm::vec3 translation = glm::vec3(0.0f);
    if (node->has_translation)
    {
        translation.x = node->translation[0];
        translation.y = node->translation[1];
        translation.z = node->translation[2];
    }

    glm::quat rotation = glm::quat(1, 0, 0, 0);
    if (node->has_rotation)
    {
        rotation.x = node->rotation[0];
        rotation.y = node->rotation[1];
        rotation.z = node->rotation[2];
        rotation.w = node->rotation[3];
    }

    glm::vec3 scale = glm::vec3(1.0f);
    if (node->has_scale)
    {
        scale.x = node->scale[0];
        scale.y = node->scale[1];
        scale.z = node->scale[2];
    }

    glm::mat4 r, t, s;
    r = glm::toMat4(rotation);
    t = glm::translate(glm::mat4(1.0), translation);
    s = glm::scale(glm::mat4(1.0), scale);
    return t * r * s;
}

glm::mat4 GetWorldMatrixFromGltf(cgltf_node* node)
{
    cgltf_node* curNode = node;
    glm::mat4 out = GetLocalMatrixFromGltf(curNode);

    while (curNode->parent != nullptr)
    {
        curNode = node->parent;
        out = GetLocalMatrixFromGltf(curNode) * out;
    }
    return out;
}

cgltf_attribute* GetGltfAttribute(cgltf_primitive* primitive, cgltf_attribute_type type)
{
    for (int i = 0; i < primitive->attributes_count; i++)
    {
        cgltf_attribute* att = &primitive->attributes[i];
        if (att->type == type)
            return att;
    }
    return nullptr;
}

VkPrimitiveTopology GetPrimitiveTopologyFromGltf(cgltf_primitive_type type)
{
    switch (type)
    {
        case cgltf_primitive_type_points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case cgltf_primitive_type_lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case cgltf_primitive_type_line_strip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case cgltf_primitive_type_triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case cgltf_primitive_type_triangle_strip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case cgltf_primitive_type_triangle_fan:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
}

void ImportGltf(const std::string& rawFilePath)
{
    cgltf_options cgltfOptions = {static_cast<cgltf_file_type>(0)};
    cgltf_data* cgltfData = nullptr;
    if (cgltf_parse_file(&cgltfOptions, rawFilePath.c_str(), &cgltfData) != cgltf_result_success)
    {
        cgltf_free(cgltfData);
        return;
    }

    if (cgltf_load_buffers(&cgltfOptions, cgltfData, rawFilePath.c_str()) != cgltf_result_success)
    {
        cgltf_free(cgltfData);
        return;
    }

    if (cgltf_validate(cgltfData) != cgltf_result_success)
    {
        cgltf_free(cgltfData);
        return;
    }

    std::string parentPath = FS::Path::ParentPath(rawFilePath);
    std::string gltfFileName = FS::Path::FileName(rawFilePath);

    // Load gltf images
    std::map<cgltf_image*, std::string> imageHelper;
    for (int i = 0; i < cgltfData->images_count; ++i)
    {
        cgltf_image* cimage = &cgltfData->images[i];
        std::string imageName = FS::Path::FullFileName(cimage->uri);
        std::string assetName = imageName + ".json";
        std::string assetPath = FS::Path::Join("asset://", gltfFileName, assetName);
        std::string rawImagePath = parentPath + "/" + cimage->uri;
        std::string outImagePath = FS::Path::Join("asset://", gltfFileName, imageName);

        FS::DuplicateFile(rawFilePath, FS::Path::FixPath(outImagePath));

        JsonWriter writer;
        writer.Object([&]() {
            writer.Field("uri", imageName);
        });

        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(FS::File::Open(FS::Path::FixPath(assetPath), FS::FileMode::Write));
        jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());

        imageHelper[cimage] = assetPath;
    }

    // Load gltf materials
    std::map<cgltf_material*, std::string> materialHelper;
    for (int i = 0; i < cgltfData->materials_count; ++i)
    {
        // TODO
        cgltf_material* cmaterial = &cgltfData->materials[i];
    }

    // Load gltf meshs
    std::map<cgltf_mesh*, std::string> meshHelper;
    for (int i = 0; i < cgltfData->meshes_count; ++i)
    {
        cgltf_mesh* cmesh = &cgltfData->meshes[i];
        std::string meshName = cmesh->name;
        std::string binName = meshName + ".bin";
        std::string assetName = meshName + ".json";
        std::string binPath = FS::Path::Join("asset://", gltfFileName, binName);
        std::string assetPath = FS::Path::Join("asset://", gltfFileName, assetName);

        meshHelper[cmesh] = assetPath;

        std::shared_ptr<FS::File> binFile = std::shared_ptr<FS::File>(FS::File::Open(FS::Path::FixPath(binPath), FS::FileMode::Write));
        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(FS::File::Open(FS::Path::FixPath(assetPath), FS::FileMode::Write));

        uint32_t totalSize = 0;
        JsonWriter writer;
        writer.Object([&]() {
            writer.Array("primitives", [&](){
                for (int j = 0; j < cmesh->primitives_count; j++)
                {
                    cgltf_primitive* cprimitive = &cmesh->primitives[j];
                    cgltf_material* cmaterial = cprimitive->material;
                    uint32_t vertexCount = 0;
                    uint32_t indexCount = 0;
                    uint32_t positionSize = 0;
                    uint32_t positionOffset = 0;
                    uint32_t normalSize = 0;
                    uint32_t normalOffset = 0;
                    uint32_t tangentSize = 0;
                    uint32_t tangentOffset = 0;
                    uint32_t uv0Size = 0;
                    uint32_t uv0Offset = 0;
                    uint32_t indexSize = 0;
                    uint32_t indexOffset = 0;
                    bool using16uIndex = false;

                    cgltf_attribute* positionAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_position);
                    cgltf_accessor* positionAccessor = positionAttribute->data;
                    cgltf_buffer_view* positionView = positionAccessor->buffer_view;
                    uint8_t* positionData = (uint8_t*)(positionView->buffer->data) + positionAccessor->offset + positionView->offset;
                    vertexCount = (uint32_t)positionAccessor->count;
                    positionSize = vertexCount * 3 * sizeof(float);
                    positionOffset = totalSize;
                    totalSize += positionSize;
                    binFile->Write(positionData, positionSize);

                    cgltf_attribute* normalAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_normal);
                    cgltf_accessor* normalAccessor = normalAttribute->data;
                    cgltf_buffer_view* normalView = normalAccessor->buffer_view;
                    uint8_t* normalData = (uint8_t*)(normalView->buffer->data) + normalAccessor->offset + normalView->offset;
                    normalSize = vertexCount * 3 * sizeof(float);
                    normalOffset = totalSize;
                    totalSize += normalSize;
                    binFile->Write(normalData, normalSize);

                    cgltf_attribute* tangentAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_tangent);
                    cgltf_accessor* tangentAccessor = tangentAttribute->data;
                    cgltf_buffer_view* tangentView = tangentAccessor->buffer_view;
                    uint8_t* tangentData = (uint8_t*)(tangentView->buffer->data) + tangentAccessor->offset + tangentView->offset;
                    tangentSize = vertexCount * 3 * sizeof(float);
                    tangentOffset = totalSize;
                    totalSize += tangentSize;
                    binFile->Write(tangentData, tangentSize);

                    cgltf_attribute* texcoordAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_texcoord);
                    cgltf_accessor* texcoordAccessor = texcoordAttribute->data;
                    cgltf_buffer_view* texcoordView = texcoordAccessor->buffer_view;
                    uint8_t* uvData = (uint8_t*)(texcoordView->buffer->data) + texcoordAccessor->offset + texcoordView->offset;
                    uv0Size = vertexCount * 2 * sizeof(float);
                    uv0Offset = totalSize;
                    totalSize += uv0Size;
                    binFile->Write(uvData, uv0Size);

                    cgltf_accessor* indexAccessor = cprimitive->indices;
                    cgltf_buffer_view* indexBufferView = indexAccessor->buffer_view;
                    cgltf_buffer* indexBuffer = indexBufferView->buffer;
                    uint8_t* indexData = (uint8_t*)indexBuffer->data + indexAccessor->offset + indexBufferView->offset;
                    indexCount = (uint32_t)indexAccessor->count;
                    using16uIndex = indexAccessor->component_type == cgltf_component_type_r_16u;
                    indexSize = using16uIndex ? indexCount * sizeof(uint16_t) : indexCount * sizeof(uint32_t);
                    indexOffset = totalSize;
                    totalSize += indexSize;
                    binFile->Write(indexData, indexSize);

                    writer.Object([&](){
                        writer.Field("vertexCount", vertexCount);
                        writer.Field("indexCount", indexCount);
                        writer.Field("using16uIndex", using16uIndex);

                        writer.Field("positionOffset", positionOffset);
                        writer.Field("positionSize", positionSize);
                        writer.Field("normalOffset", normalOffset);
                        writer.Field("normalSize", normalSize);
                        writer.Field("tangentOffset", tangentOffset);
                        writer.Field("tangentSize", tangentSize);
                        writer.Field("uv0Offset", uv0Offset);
                        writer.Field("uv0Size", uv0Size);

                        writer.Field("indexOffset", indexOffset);
                        writer.Field("indexSize", indexSize);
                    });
                }
            });
        });
        jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());
    }

    // Load level
    struct LevelNodeInfo
    {
        std::string uuid;
        glm::vec3 translation;
        glm::vec3 scale;
        glm::vec3 euler;
    };
    std::vector<LevelNodeInfo> levelNodeInfos;
    levelNodeInfos.resize(cgltfData->nodes_count);
    std::set<std::string> uuids;

    for (size_t i = 0; i < cgltfData->nodes_count; ++i)
    {
        cgltf_node* cnode = &cgltfData->nodes[i];
        auto& levelNodeInfo = levelNodeInfos[i];

        bool isUnique = false;
        std::string uuid;
        while (!isUnique)
        {
            uuid = UUIDGenerator::Generate();
            if (uuids.find(uuid) == uuids.end())
            {
                isUnique = true;
                uuids.insert(uuid);
            }
        }
        levelNodeInfo.uuid = uuid;

        glm::vec3 translation = glm::vec3(0.0f);
        if (cnode->has_translation)
        {
            translation.x = cnode->translation[0];
            translation.y = cnode->translation[1];
            translation.z = cnode->translation[2];
        }
        levelNodeInfo.translation = translation;

        glm::quat rotation = glm::quat(1, 0, 0, 0);
        if (cnode->has_rotation)
        {
            rotation.x = cnode->rotation[0];
            rotation.y = cnode->rotation[1];
            rotation.z = cnode->rotation[2];
            rotation.w = cnode->rotation[3];
        }
        levelNodeInfo.euler = glm::eulerAngles(rotation) * 3.14159f / 180.f;

        glm::vec3 scale = glm::vec3(1.0f);
        if (cnode->has_scale)
        {
            scale.x = cnode->scale[0];
            scale.y = cnode->scale[1];
            scale.z = cnode->scale[2];
        }
        levelNodeInfo.scale = scale;
    }

    {
        std::string assetName = gltfFileName + ".json";
        std::string assetPath = FS::Path::Join("asset://", gltfFileName, assetName);

        JsonWriter writer;
        writer.Object([&]() {
            writer.Array("nodes", [&]() {
                for (size_t i = 0; i < cgltfData->nodes_count; ++i)
                {
                    cgltf_node* cnode = &cgltfData->nodes[i];
                    auto& levelNodeInfo = levelNodeInfos[i];

                    if (cnode->mesh)
                    {
                        writer.Object([&]() {
                            writer.Field("type", "Model");
                            writer.Field("uuid", levelNodeInfo.uuid);
                            writer.Field("translation", levelNodeInfo.translation);
                            writer.Field("scale", levelNodeInfo.scale);
                            writer.Field("euler", levelNodeInfo.euler);
                            writer.Field("mesh", meshHelper[cnode->mesh]);
                        });
                    }
                }
            });
        });

        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(FS::File::Open(FS::Path::FixPath(assetPath), FS::FileMode::Write));
        jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());
    }
}

void ImportAsset(const std::string& rawFilePath)
{
    std::string ext = FS::Path::Extension(rawFilePath);

    if (ext == ".gltf")
    {
        ImportGltf(rawFilePath);
    }
}