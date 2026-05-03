#include "Importer.h"
#include "Foundation/JsonIO.h"
#include "Foundation/UUID.h"
#include "Foundation/VFS.h"
#include "Acceleration/GpuBVHBuilder.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

cgltf_attribute* GetGltfAttribute(cgltf_primitive* primitive, cgltf_attribute_type type);

static uint32_t PackSignedVector3x10_1x2(const glm::vec3& v)
{
    int32_t x = static_cast<int32_t>(glm::round(glm::clamp(v.x, -1.0f, 1.0f) * 511.0f));
    int32_t y = static_cast<int32_t>(glm::round(glm::clamp(v.y, -1.0f, 1.0f) * 511.0f));
    int32_t z = static_cast<int32_t>(glm::round(glm::clamp(v.z, -1.0f, 1.0f) * 511.0f));
    return (static_cast<uint32_t>(x & 0x3FF)) |
           (static_cast<uint32_t>(y & 0x3FF) << 10) |
           (static_cast<uint32_t>(z & 0x3FF) << 20);
}

namespace {

struct LoadedTexture
{
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
};

LoadedTexture LoadTextureFile(const std::string& path)
{
    LoadedTexture tex;
    int w, h, c;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (data)
    {
        tex.width = w;
        tex.height = h;
        tex.channels = 4;
        tex.pixels.resize(w * h * 4);
        memcpy(tex.pixels.data(), data, w * h * 4);
        stbi_image_free(data);
    }
    return tex;
}

struct PrimitiveMeshData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
};

void SaveBakedFile(const std::string& path, const std::vector<GpuBVHNode>& nodes, const std::vector<GPUTriangle>& triangles)
{
    std::shared_ptr<FS::File> file = std::shared_ptr<FS::File>(VFS::Open(path, FS::FileMode::Write));
    if (!file || !file->IsOpen())
    {
        LOGE("Failed to write baked file: %s", path.c_str());
        return;
    }

    uint32_t bvhNodeOffset = 32;
    uint32_t bvhNodeCount = static_cast<uint32_t>(nodes.size());
    uint32_t triOffset = bvhNodeOffset + bvhNodeCount * static_cast<uint32_t>(sizeof(GpuBVHNode));
    uint32_t triCount = static_cast<uint32_t>(triangles.size());
    uint32_t bvhNodeSize = static_cast<uint32_t>(sizeof(GpuBVHNode));
    uint32_t triSize = static_cast<uint32_t>(sizeof(GPUTriangle));

    char magic[4] = {'B', 'A', 'K', 'D'};
    uint32_t version = 3;

    file->Write((uint8_t*)magic, 4);
    file->Write((uint8_t*)&version, sizeof(uint32_t));
    file->Write((uint8_t*)&bvhNodeOffset, sizeof(uint32_t));
    file->Write((uint8_t*)&bvhNodeCount, sizeof(uint32_t));
    file->Write((uint8_t*)&triOffset, sizeof(uint32_t));
    file->Write((uint8_t*)&triCount, sizeof(uint32_t));
    file->Write((uint8_t*)&bvhNodeSize, sizeof(uint32_t));
    file->Write((uint8_t*)&triSize, sizeof(uint32_t));

    if (!nodes.empty())
    {
        file->Write((uint8_t*)nodes.data(), bvhNodeCount * bvhNodeSize);
    }
    if (!triangles.empty())
    {
        file->Write((uint8_t*)triangles.data(), triCount * triSize);
    }

    LOGI("Baked file saved: %s (nodes=%u, triangles=%u)", path.c_str(), bvhNodeCount, triCount);
}

std::string GetSamplerName(cgltf_sampler* sampler)
{
    if (!sampler)
    {
        return "linearClamp";
    }

    static constexpr cgltf_int NEAREST = 9728;
    static constexpr cgltf_int LINEAR = 9729;
    static constexpr cgltf_int NEAREST_MIPMAP_NEAREST = 9984;
    static constexpr cgltf_int LINEAR_MIPMAP_NEAREST = 9985;
    static constexpr cgltf_int NEAREST_MIPMAP_LINEAR = 9986;
    static constexpr cgltf_int LINEAR_MIPMAP_LINEAR = 9987;
    static constexpr cgltf_int CLAMP_TO_EDGE = 33071;
    static constexpr cgltf_int MIRRORED_REPEAT = 33648;
    static constexpr cgltf_int REPEAT = 10497;

    std::string filterName;
    switch (sampler->min_filter)
    {
        case NEAREST: filterName = "nearest"; break;
        case LINEAR: filterName = "linear"; break;
        case NEAREST_MIPMAP_NEAREST: filterName = "nearestMipmapNearest"; break;
        case LINEAR_MIPMAP_NEAREST: filterName = "linearMipmapNearest"; break;
        case NEAREST_MIPMAP_LINEAR: filterName = "nearestMipmapLinear"; break;
        case LINEAR_MIPMAP_LINEAR: filterName = "linearMipmapLinear"; break;
        default: filterName = "linear"; break;
    }

    std::string wrapName;
    switch (sampler->wrap_s)
    {
        case CLAMP_TO_EDGE: wrapName = "Clamp"; break;
        case MIRRORED_REPEAT: wrapName = "Mirror"; break;
        case REPEAT: wrapName = "Repeat"; break;
        default: wrapName = "Clamp"; break;
    }

    return "linearClamp";
}

}// namespace

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
        curNode = curNode->parent;
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
        {
            return att;
        }
    }
    return nullptr;
}

VkPrimitiveTopology GetPrimitiveTopologyFromGltf(cgltf_primitive_type type)
{
    switch (type)
    {
        case cgltf_primitive_type_points: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case cgltf_primitive_type_lines: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case cgltf_primitive_type_line_strip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case cgltf_primitive_type_triangles: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case cgltf_primitive_type_triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case cgltf_primitive_type_triangle_fan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
}

void CombindVertexData(uint8_t* dst, uint8_t* src, uint32_t vertexCount, uint32_t attributeSize, uint32_t offset,
                       uint32_t stride)
{
    uint8_t* dstData = dst + offset;
    uint8_t* srcData = src;
    for (uint32_t i = 0; i < vertexCount; i++)
    {
        memcpy(dstData, srcData, attributeSize);
        dstData += stride;
        srcData += attributeSize;
    }
}

void ImportGltf(const std::string& rawFilePath)
{
    LOGI("Importing: %s", rawFilePath.c_str());

    cgltf_options cgltfOptions = {static_cast<cgltf_file_type>(0)};
    cgltf_data* cgltfData = nullptr;
    if (cgltf_parse_file(&cgltfOptions, rawFilePath.c_str(), &cgltfData) != cgltf_result_success)
    {
        LOGE("Failed to parse gltf file: %s", rawFilePath.c_str());
        cgltf_free(cgltfData);
        return;
    }

    if (cgltf_load_buffers(&cgltfOptions, cgltfData, rawFilePath.c_str()) != cgltf_result_success)
    {
        LOGE("Failed to load gltf buffers: %s", rawFilePath.c_str());
        cgltf_free(cgltfData);
        return;
    }

    if (cgltf_validate(cgltfData) != cgltf_result_success)
    {
        LOGE("Failed to validate gltf: %s", rawFilePath.c_str());
        cgltf_free(cgltfData);
        return;
    }

    LOGI("Gltf loaded successfully: %zu meshes, %zu images", (size_t)cgltfData->meshes_count,
         (size_t)cgltfData->images_count);

    std::string parentPath = FS::Path::ParentPath(rawFilePath);
    std::string gltfFileName = FS::Path::FileName(rawFilePath);
    LOGI("Output directory: Assets/Scenes/%s", gltfFileName.c_str());

    // Load gltf images
    std::map<cgltf_image*, std::string> imageHelper;
    for (int i = 0; i < cgltfData->images_count; ++i)
    {
        cgltf_image* cimage = &cgltfData->images[i];
        std::string imageName = FS::Path::FullFileName(cimage->uri);
        std::string assetName = imageName + ".json";
        std::string assetPath = VFS::Join("Assets", "Scenes", gltfFileName, assetName);
        std::string rawImagePath = parentPath + "/" + cimage->uri;
        std::string outImagePath = VFS::Join("Assets", "Scenes", gltfFileName, imageName);

        FS::DuplicateFile(rawImagePath, VFS::GetRealPath(outImagePath));

        JsonWriter writer;
        writer.Object([&]() { writer.Field("binary", outImagePath); });

        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(VFS::Open(assetPath, FS::FileMode::Write));
        jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());

        imageHelper[cimage] = assetPath;
    }

    // Load gltf materials
    std::map<cgltf_material*, std::string> materialHelper;
    LOGI("Processing %zu materials...", (size_t)cgltfData->materials_count);
    for (int i = 0; i < cgltfData->materials_count; ++i)
    {
        cgltf_material* cmaterial = &cgltfData->materials[i];
        std::string materialName = cmaterial->name ? cmaterial->name : "material_" + std::to_string(i);
        std::string assetName = materialName + ".json";
        std::string assetPath = VFS::Join("Assets", "Scenes", gltfFileName, assetName);

        LOGI("Material %d: %s -> %s", i, materialName.c_str(), assetPath.c_str());

        materialHelper[cmaterial] = assetPath;

        JsonWriter writer;
        writer.Object([&]() {
            if (cmaterial->has_pbr_metallic_roughness)
            {
                cgltf_texture_view* baseColorTex = &cmaterial->pbr_metallic_roughness.base_color_texture;
                if (baseColorTex && baseColorTex->texture && baseColorTex->texture->image)
                {
                    auto it = imageHelper.find(baseColorTex->texture->image);
                    if (it != imageHelper.end())
                    {
                        writer.Field("albedo");
                        writer.Object([&]() {
                            writer.Field("texture", it->second);
                            writer.Field("sampler", GetSamplerName(baseColorTex->texture->sampler));
                        });
                    }
                }
                writer.Field("baseColor",
                             glm::vec4(cmaterial->pbr_metallic_roughness.base_color_factor[0],
                                       cmaterial->pbr_metallic_roughness.base_color_factor[1],
                                       cmaterial->pbr_metallic_roughness.base_color_factor[2],
                                       cmaterial->pbr_metallic_roughness.base_color_factor[3]));
                writer.Field("roughness", cmaterial->pbr_metallic_roughness.roughness_factor);
                writer.Field("metallic", cmaterial->pbr_metallic_roughness.metallic_factor);

                cgltf_texture_view* roughnessMetallicTex =
                    &cmaterial->pbr_metallic_roughness.metallic_roughness_texture;
                if (roughnessMetallicTex && roughnessMetallicTex->texture && roughnessMetallicTex->texture->image)
                {
                    auto it = imageHelper.find(roughnessMetallicTex->texture->image);
                    if (it != imageHelper.end())
                    {
                        writer.Field("roughnessMetallic");
                        writer.Object([&]() {
                            writer.Field("texture", it->second);
                            writer.Field("sampler", GetSamplerName(roughnessMetallicTex->texture->sampler));
                        });
                    }
                }
            }
            if (cmaterial->normal_texture.texture && cmaterial->normal_texture.texture->image)
            {
                auto it = imageHelper.find(cmaterial->normal_texture.texture->image);
                if (it != imageHelper.end())
                {
                    writer.Field("normal");
                    writer.Object([&]() {
                        writer.Field("texture", it->second);
                        writer.Field("sampler", GetSamplerName(cmaterial->normal_texture.texture->sampler));
                    });
                }
            }
            if (cmaterial->emissive_texture.texture && cmaterial->emissive_texture.texture->image)
            {
                auto it = imageHelper.find(cmaterial->emissive_texture.texture->image);
                if (it != imageHelper.end())
                {
                    writer.Field("emissive");
                    writer.Object([&]() {
                        writer.Field("texture", it->second);
                        writer.Field("sampler", GetSamplerName(cmaterial->emissive_texture.texture->sampler));
                    });
                }
            }
            writer.Field("emissiveColor",
                         glm::vec4(cmaterial->emissive_factor[0], cmaterial->emissive_factor[1],
                                   cmaterial->emissive_factor[2], 1.0f));

            switch (cmaterial->alpha_mode)
            {
                case cgltf_alpha_mode_mask: writer.Field("blendMode", "mask"); break;
                case cgltf_alpha_mode_blend: writer.Field("blendMode", "blend"); break;
                default: writer.Field("blendMode", "opaque"); break;
            }
            writer.Field("alphaCutoff", cmaterial->alpha_cutoff);
        });

        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(VFS::Open(assetPath, FS::FileMode::Write));
        if (jsonFile && jsonFile->IsOpen())
        {
            jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());
        }
        else
        {
            LOGE("Failed to write material file: %s", assetPath.c_str());
        }
    }

    // Load gltf meshs
    std::map<cgltf_mesh*, std::string> meshHelper;
    LOGI("Processing %zu meshes...", (size_t)cgltfData->meshes_count);
    for (int i = 0; i < cgltfData->meshes_count; ++i)
    {
        cgltf_mesh* cmesh = &cgltfData->meshes[i];
        std::string meshName = cmesh->name ? cmesh->name : "mesh_" + std::to_string(i);
        std::string binName = meshName + ".bin";
        std::string assetName = meshName + ".json";
        std::string binPath = VFS::Join("Assets", "Scenes", gltfFileName, binName);
        std::string assetPath = VFS::Join("Assets", "Scenes", gltfFileName, assetName);

        LOGI("Mesh %d: %s -> %s", i, meshName.c_str(), assetPath.c_str());

        meshHelper[cmesh] = assetPath;

        std::shared_ptr<FS::File> binFile = std::shared_ptr<FS::File>(VFS::Open(binPath, FS::FileMode::Write));
        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(VFS::Open(assetPath, FS::FileMode::Write));

        if (!binFile || !binFile->IsOpen())
        {
            LOGE("Failed to open bin file: %s", binPath.c_str());
            continue;
        }
        if (!jsonFile || !jsonFile->IsOpen())
        {
            LOGE("Failed to open json file: %s", assetPath.c_str());
            continue;
        }

        uint32_t totalSize = 0;
        JsonWriter writer;
        writer.Object([&]() {
            writer.Field("binary", binPath);

            writer.Array("primitives", [&]() {
                for (int j = 0; j < cmesh->primitives_count; j++)
                {
                    cgltf_primitive* cprimitive = &cmesh->primitives[j];
                    cgltf_material* cmaterial = cprimitive->material;
                    uint32_t vertexCount = 0;
                    uint32_t indexCount = 0;
                    uint32_t positionSize = 0;
                    uint32_t positionOffset = 0;
                    uint32_t attributeSize = 0;
                    uint32_t attributeOffset = 0;
                    uint32_t indexSize = 0;
                    uint32_t indexOffset = 0;
                    bool using16uIndex = false;

                    cgltf_attribute* positionAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_position);
                    if (!positionAttribute)
                    {
                        LOGE("Primitive %d has no position attribute, skipping", j);
                        continue;
                    }
                    cgltf_accessor* positionAccessor = positionAttribute->data;
                    cgltf_buffer_view* positionView = positionAccessor->buffer_view;
                    uint8_t* positionData =
                        (uint8_t*)(positionView->buffer->data) + positionAccessor->offset + positionView->offset;
                    vertexCount = (uint32_t)positionAccessor->count;
                    positionSize = vertexCount * 3 * sizeof(float);
                    positionOffset = totalSize;
                    totalSize += positionSize;
                    binFile->Write(positionData, positionSize);

                    cgltf_attribute* texcoordAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_texcoord);
                    cgltf_attribute* normalAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_normal);
                    cgltf_attribute* tangentAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_tangent);

                    uint32_t attributeStride = (2 + 3 + 3) * sizeof(float);
                    attributeSize = vertexCount * attributeStride;
                    attributeOffset = totalSize;
                    totalSize += attributeSize;
                    std::vector<uint8_t> attributeData(attributeSize, 0);

                    if (texcoordAttribute)
                    {
                        cgltf_accessor* texcoordAccessor = texcoordAttribute->data;
                        cgltf_buffer_view* texcoordView = texcoordAccessor->buffer_view;
                        uint8_t* uvData =
                            (uint8_t*)(texcoordView->buffer->data) + texcoordAccessor->offset + texcoordView->offset;
                        CombindVertexData(attributeData.data(), uvData, vertexCount, 2 * sizeof(float), 0,
                                          attributeStride);
                    }
                    else
                    {
                        std::vector<float> defaultUV(vertexCount * 2, 0.0f);
                        CombindVertexData(attributeData.data(), (uint8_t*)defaultUV.data(), vertexCount,
                                          2 * sizeof(float), 0, attributeStride);
                    }

                    if (normalAttribute)
                    {
                        cgltf_accessor* normalAccessor = normalAttribute->data;
                        cgltf_buffer_view* normalView = normalAccessor->buffer_view;
                        uint8_t* normalData =
                            (uint8_t*)(normalView->buffer->data) + normalAccessor->offset + normalView->offset;
                        CombindVertexData(attributeData.data(), normalData, vertexCount, 3 * sizeof(float),
                                          2 * sizeof(float), attributeStride);
                    }
                    else
                    {
                        std::vector<float> defaultNormal(vertexCount * 3, 0.0f);
                        for (uint32_t n = 0; n < vertexCount; ++n)
                        {
                            defaultNormal[n * 3 + 2] = 1.0f;
                        }
                        CombindVertexData(attributeData.data(), (uint8_t*)defaultNormal.data(), vertexCount,
                                          3 * sizeof(float), 2 * sizeof(float), attributeStride);
                    }

                    if (tangentAttribute)
                    {
                        cgltf_accessor* tangentAccessor = tangentAttribute->data;
                        cgltf_buffer_view* tangentView = tangentAccessor->buffer_view;
                        uint8_t* tangentData =
                            (uint8_t*)(tangentView->buffer->data) + tangentAccessor->offset + tangentView->offset;
                        CombindVertexData(attributeData.data(), tangentData, vertexCount, 3 * sizeof(float),
                                          5 * sizeof(float), attributeStride);
                    }
                    else
                    {
                        std::vector<float> defaultTangent(vertexCount * 3, 0.0f);
                        for (uint32_t n = 0; n < vertexCount; ++n)
                        {
                            defaultTangent[n * 3 + 0] = 1.0f;
                        }
                        CombindVertexData(attributeData.data(), (uint8_t*)defaultTangent.data(), vertexCount,
                                          3 * sizeof(float), 5 * sizeof(float), attributeStride);
                    }

                    binFile->Write(attributeData.data(), attributeData.size());

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

                    // ===== BVH Build =====
                    std::string bakedPath;

                    PrimitiveMeshData meshData;
                    meshData.positions.resize(vertexCount);
                    memcpy(meshData.positions.data(), positionData, vertexCount * sizeof(glm::vec3));

                    meshData.uvs.resize(vertexCount);
                    if (texcoordAttribute)
                    {
                        cgltf_accessor* texcoordAccessor = texcoordAttribute->data;
                        cgltf_buffer_view* texcoordView = texcoordAccessor->buffer_view;
                        uint8_t* uvSrc =
                            (uint8_t*)(texcoordView->buffer->data) + texcoordAccessor->offset + texcoordView->offset;
                        memcpy(meshData.uvs.data(), uvSrc, vertexCount * sizeof(glm::vec2));
                    }

                    meshData.normals.resize(vertexCount);
                    if (normalAttribute)
                    {
                        cgltf_accessor* normalAccessor = normalAttribute->data;
                        cgltf_buffer_view* normalView = normalAccessor->buffer_view;
                        uint8_t* normalSrc =
                            (uint8_t*)(normalView->buffer->data) + normalAccessor->offset + normalView->offset;
                        memcpy(meshData.normals.data(), normalSrc, vertexCount * sizeof(glm::vec3));
                    }

                    meshData.indices.resize(indexCount);
                    if (using16uIndex)
                    {
                        for (uint32_t idx = 0; idx < indexCount; ++idx)
                        {
                            meshData.indices[idx] = ((uint16_t*)indexData)[idx];
                        }
                    }
                    else
                    {
                        memcpy(meshData.indices.data(), indexData, indexCount * sizeof(uint32_t));
                    }

                    if (!meshData.indices.empty())
                    {
                        std::string primName = meshName + "_prim" + std::to_string(j);
                        std::string bakedFileName = primName + ".baked";
                        bakedPath = VFS::Join("Assets", "Scenes", gltfFileName, bakedFileName);

                        uint32_t triCount = indexCount / 3;
                        std::vector<GPUTriangle> triangles;
                        triangles.resize(triCount);
                        for (uint32_t i = 0; i < triCount; ++i)
                        {
                            uint32_t i0 = meshData.indices[i * 3 + 0];
                            uint32_t i1 = meshData.indices[i * 3 + 1];
                            uint32_t i2 = meshData.indices[i * 3 + 2];

                            GPUTriangle& tri = triangles[i];
                            tri.v0 = glm::vec4(meshData.positions[i0], 0.0f);
                            tri.v1 = glm::vec4(meshData.positions[i1], 0.0f);
                            tri.v2 = glm::vec4(meshData.positions[i2], 0.0f);

                            glm::vec3 n0, n1, n2;
                            if (!meshData.normals.empty())
                            {
                                n0 = meshData.normals[i0];
                                n1 = meshData.normals[i1];
                                n2 = meshData.normals[i2];
                            }
                            else
                            {
                                glm::vec3 n = glm::normalize(
                                    glm::cross(glm::vec3(tri.v1) - glm::vec3(tri.v0), glm::vec3(tri.v2) - glm::vec3(tri.v0)));
                                n0 = n1 = n2 = n;
                            }
                            uint32_t pn0 = PackSignedVector3x10_1x2(n0);
                            uint32_t pn1 = PackSignedVector3x10_1x2(n1);
                            uint32_t pn2 = PackSignedVector3x10_1x2(n2);
                            tri.v0.w = reinterpret_cast<float&>(pn0);
                            tri.v1.w = reinterpret_cast<float&>(pn1);
                            tri.v2.w = reinterpret_cast<float&>(pn2);

                            glm::vec2 uv0 = glm::vec2(0.0f);
                            glm::vec2 uv1 = glm::vec2(0.0f);
                            glm::vec2 uv2 = glm::vec2(0.0f);
                            if (!meshData.uvs.empty())
                            {
                                uv0 = meshData.uvs[i0];
                                uv1 = meshData.uvs[i1];
                                uv2 = meshData.uvs[i2];
                            }
                            uint32_t puv0 = glm::packHalf2x16(uv0);
                            uint32_t puv1 = glm::packHalf2x16(uv1);
                            uint32_t puv2 = glm::packHalf2x16(uv2);
                            uint32_t matIdx = 0xFFFFFFFF;
                            tri.d0 = glm::vec4(
                                reinterpret_cast<float&>(puv0),
                                reinterpret_cast<float&>(puv1),
                                reinterpret_cast<float&>(puv2),
                                reinterpret_cast<float&>(matIdx));
                        }

                        GpuBVHBuilder builder;
                        GpuBVHBuilder::Input input = {};
                        input.positions = meshData.positions.data();
                        input.indices = meshData.indices.data();
                        input.triCount = triCount;

                        GpuBVHBuilder::Output bvhOutput;
                        builder.Build(input, bvhOutput);

                        if (!bvhOutput.nodes.empty() && !triangles.empty())
                        {
                            std::vector<GPUTriangle> reorderedTriangles(triangles.size());
                            for (uint32_t i = 0; i < bvhOutput.triangleOrder.size(); ++i)
                            {
                                reorderedTriangles[i] = triangles[bvhOutput.triangleOrder[i]];
                            }
                            SaveBakedFile(bakedPath, bvhOutput.nodes, reorderedTriangles);
                        }
                    }

                    writer.Object([&]() {
                        writer.Field("vertexCount", vertexCount);
                        writer.Field("indexCount", indexCount);
                        writer.Field("using16uIndex", using16uIndex);

                        writer.Field("positionOffset", positionOffset);
                        writer.Field("positionSize", positionSize);
                        writer.Field("attributeOffset", attributeOffset);
                        writer.Field("attributeSize", attributeSize);

                        writer.Field("indexOffset", indexOffset);
                        writer.Field("indexSize", indexSize);

                        if (cmaterial)
                        {
                            auto it = materialHelper.find(cmaterial);
                            if (it != materialHelper.end())
                            {
                                writer.Field("material", it->second);
                            }
                        }

                        if (!bakedPath.empty())
                        {
                            writer.Field("baked", bakedPath);
                        }
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
    std::map<cgltf_node*, std::string> nodeHelper;

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
        nodeHelper[cnode] = uuid;

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
        std::string assetName = gltfFileName + ".scene";
        std::string assetPath = VFS::Join("Assets", "Scenes", gltfFileName, assetName);
        LOGI("Writing scene file: %s", assetPath.c_str());

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

                            if (cnode->parent)
                            {
                                writer.Field("parent", nodeHelper[cnode->parent]);
                            }
                        });
                    }
                }
            });
        });

        std::shared_ptr<FS::File> jsonFile = std::shared_ptr<FS::File>(VFS::Open(assetPath, FS::FileMode::Write));
        if (!jsonFile || !jsonFile->IsOpen())
        {
            LOGE("Failed to open scene file: %s", assetPath.c_str());
        }
        else
        {
            jsonFile->Write((uint8_t*)writer.GetString(), writer.GetSize());
            LOGI("Scene file saved: %s (%zu bytes)", assetPath.c_str(), writer.GetSize());
        }
    }

    LOGI("Import completed: %s", gltfFileName.c_str());
}

void ImportAsset(const std::string& rawFilePath)
{
    std::string ext = VFS::Extension(rawFilePath);

    if (ext == ".gltf")
    {
        ImportGltf(rawFilePath);
    }
}
