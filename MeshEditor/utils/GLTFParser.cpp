#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "GLTFParser.h"
#include "Geometry.h"
#include "Node.h"
#include "Mesh.h"
#include "Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <array>
#include <cstring>

#include <unordered_set>

namespace
{
std::string generateUniqueFilename(const std::string &baseDir,
                                   const std::string &mimeType)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    std::string ext = ".png";
    if (mimeType == "image/jpeg" || mimeType == "image/jpg")
    {
        ext = ".jpg";
    }
    std::string name = "gltf_tex_" + std::to_string(dis(gen)) + ext;
    return (std::filesystem::path(baseDir) / name).string();
}

std::string handleEmbeddedImage(cgltf_image *image, const std::string &baseDir)
{
    if (!image)
        return "";

    // 1. If there's an external URI that is not data-URI, resolve and return
    if (image->uri && std::string(image->uri).rfind("data:", 0) != 0)
    {
        std::filesystem::path bDir(baseDir);
        return (bDir / image->uri).lexically_normal().string();
    }

    // 2. If it's a buffer view (embedded image in glTF/GLB)
    if (image->buffer_view)
    {
        cgltf_buffer_view *view = image->buffer_view;
        if (view->buffer && view->buffer->data)
        {
            const uint8_t *src =
                static_cast<const uint8_t *>(view->buffer->data) + view->offset;
            cgltf_size len = view->size;
            std::string mime = image->mime_type ? image->mime_type : "";
            std::string outPath = generateUniqueFilename(baseDir, mime);
            std::ofstream out(outPath, std::ios::binary);
            if (out)
            {
                out.write(reinterpret_cast<const char *>(src), len);
                out.close();
                return outPath;
            }
        }
    }

    return "";
}

void applyMaterial(const cgltf_material *srcMat, Material &destMat,
                   const std::string &baseDir)
{
    if (!srcMat)
        return;

    auto& pbr = destMat.getExtendedParams();

    // Core Properties
    if (srcMat->alpha_mode == cgltf_alpha_mode_opaque) pbr.alphaMode = AlphaMode::Opaque;
    else if (srcMat->alpha_mode == cgltf_alpha_mode_mask) pbr.alphaMode = AlphaMode::Mask;
    else if (srcMat->alpha_mode == cgltf_alpha_mode_blend) pbr.alphaMode = AlphaMode::Blend;

    pbr.alphaCutoff = srcMat->alpha_cutoff;
    pbr.doubleSided = srcMat->double_sided;
    pbr.unlit = srcMat->unlit;

    auto loadTex = [&](const cgltf_texture_view& view, TextureSlot slot) {
        if (view.texture && view.texture->image) {
            std::string path = handleEmbeddedImage(view.texture->image, baseDir);
            if (!path.empty()) destMat.setTexturePath(slot, path);
        }
    };

    // PBR Metallic-Roughness
    if (srcMat->has_pbr_metallic_roughness)
    {
        const auto& mr = srcMat->pbr_metallic_roughness;
        glm::vec3 diffuse(mr.base_color_factor[0], mr.base_color_factor[1], mr.base_color_factor[2]);
        destMat.setDiffuse(diffuse);
        pbr.metallic = mr.metallic_factor;
        pbr.roughness = mr.roughness_factor;
        loadTex(mr.base_color_texture, TextureSlot::Diffuse);
        loadTex(mr.metallic_roughness_texture, TextureSlot::MetallicRoughness);
    }

    // Normal, Occlusion, Emissive
    loadTex(srcMat->normal_texture, TextureSlot::Normal);
    loadTex(srcMat->occlusion_texture, TextureSlot::Occlusion);
    pbr.emissive = glm::vec3(srcMat->emissive_factor[0], srcMat->emissive_factor[1], srcMat->emissive_factor[2]);
    loadTex(srcMat->emissive_texture, TextureSlot::Emissive);

    // IOR & Transmission
    if (srcMat->has_ior) pbr.ior = srcMat->ior.ior;
    if (srcMat->has_transmission) {
        pbr.transmission = srcMat->transmission.transmission_factor;
        loadTex(srcMat->transmission.transmission_texture, TextureSlot::Transmission);
    }

    // Clearcoat
    if (srcMat->has_clearcoat) {
        pbr.clearcoatFactor = srcMat->clearcoat.clearcoat_factor;
        pbr.clearcoatRoughness = srcMat->clearcoat.clearcoat_roughness_factor;
        loadTex(srcMat->clearcoat.clearcoat_texture, TextureSlot::Clearcoat);
        loadTex(srcMat->clearcoat.clearcoat_roughness_texture, TextureSlot::ClearcoatRoughness);
        loadTex(srcMat->clearcoat.clearcoat_normal_texture, TextureSlot::ClearcoatNormal);
    }

    // Sheen
    if (srcMat->has_sheen) {
        pbr.sheenColor = glm::vec3(srcMat->sheen.sheen_color_factor[0], srcMat->sheen.sheen_color_factor[1], srcMat->sheen.sheen_color_factor[2]);
        pbr.sheenRoughness = srcMat->sheen.sheen_roughness_factor;
        loadTex(srcMat->sheen.sheen_color_texture, TextureSlot::SheenColor);
        loadTex(srcMat->sheen.sheen_roughness_texture, TextureSlot::SheenRoughness);
    }

    // Volume
    if (srcMat->has_volume) {
        pbr.thickness = srcMat->volume.thickness_factor;
        pbr.attenuationDistance = srcMat->volume.attenuation_distance;
        pbr.attenuationColor = glm::vec3(srcMat->volume.attenuation_color[0], srcMat->volume.attenuation_color[1], srcMat->volume.attenuation_color[2]);
        loadTex(srcMat->volume.thickness_texture, TextureSlot::Thickness);
    }

    // Specular
    if (srcMat->has_specular) {
        pbr.specularFactor = srcMat->specular.specular_factor;
        pbr.specularColor = glm::vec3(srcMat->specular.specular_color_factor[0], srcMat->specular.specular_color_factor[1], srcMat->specular.specular_color_factor[2]);
        loadTex(srcMat->specular.specular_texture, TextureSlot::Specular);
        loadTex(srcMat->specular.specular_color_texture, TextureSlot::SpecularColor);
    }

    // Emissive Strength
    if (srcMat->has_emissive_strength) {
        pbr.emissiveStrength = srcMat->emissive_strength.emissive_strength;
    }

    // Iridescence
    if (srcMat->has_iridescence) {
        pbr.iridescenceFactor = srcMat->iridescence.iridescence_factor;
        pbr.iridescenceIor = srcMat->iridescence.iridescence_ior;
        pbr.iridescenceThicknessMin = srcMat->iridescence.iridescence_thickness_min;
        pbr.iridescenceThicknessMax = srcMat->iridescence.iridescence_thickness_max;
        loadTex(srcMat->iridescence.iridescence_texture, TextureSlot::Iridescence);
        loadTex(srcMat->iridescence.iridescence_thickness_texture, TextureSlot::IridescenceThickness);
    }

    // Anisotropy
    if (srcMat->has_anisotropy) {
        pbr.anisotropyStrength = srcMat->anisotropy.anisotropy_strength;
        pbr.anisotropyRotation = srcMat->anisotropy.anisotropy_rotation;
        loadTex(srcMat->anisotropy.anisotropy_texture, TextureSlot::Anisotropy);
    }

    // Dispersion
    if (srcMat->has_dispersion) {
        pbr.dispersion = srcMat->dispersion.dispersion;
    }

    // Diffuse Transmission
    if (srcMat->has_diffuse_transmission) {
        pbr.diffuseTransmissionFactor = srcMat->diffuse_transmission.diffuse_transmission_factor;
        pbr.diffuseTransmissionColor = glm::vec3(srcMat->diffuse_transmission.diffuse_transmission_color_factor[0], srcMat->diffuse_transmission.diffuse_transmission_color_factor[1], srcMat->diffuse_transmission.diffuse_transmission_color_factor[2]);
        loadTex(srcMat->diffuse_transmission.diffuse_transmission_texture, TextureSlot::DiffuseTransmission);
        loadTex(srcMat->diffuse_transmission.diffuse_transmission_color_texture, TextureSlot::DiffuseTransmissionColor);
    }
}

bool buildGeometryFromPrimitive(const cgltf_primitive *prim, HalfEdgeTable &het)
{
    if (prim->type != cgltf_primitive_type_triangles)
        return false;

    const cgltf_attribute *posAttr = nullptr;
    const cgltf_attribute *uvAttr = nullptr;
    const cgltf_attribute *normAttr = nullptr;
    const cgltf_attribute *tanAttr = nullptr;
    const cgltf_attribute *colAttr = nullptr;
    for (cgltf_size i = 0; i < prim->attributes_count; ++i)
    {
        if (prim->attributes[i].type == cgltf_attribute_type_position)
            posAttr = &prim->attributes[i];
        else if (prim->attributes[i].type == cgltf_attribute_type_texcoord)
        {
            // Prioritize primary texture coordinates (TEXCOORD_0)
            if (!uvAttr || prim->attributes[i].index == 0)
                uvAttr = &prim->attributes[i];
        }
        else if (prim->attributes[i].type == cgltf_attribute_type_normal)
            normAttr = &prim->attributes[i];
        else if (prim->attributes[i].type == cgltf_attribute_type_tangent)
            tanAttr = &prim->attributes[i];
        else if (prim->attributes[i].type == cgltf_attribute_type_color)
            colAttr = &prim->attributes[i];
    }

    if (!posAttr)
        return false;

    const cgltf_accessor *posAcc = posAttr->data;
    cgltf_size vertexCount = posAcc->count;

    het.reserve(vertexCount, prim->indices ? prim->indices->count : vertexCount,
                vertexCount / 3);

    std::vector<VertexHandle> vertexHandles;
    vertexHandles.reserve(vertexCount);
    for (cgltf_size i = 0; i < vertexCount; ++i)
    {
        float pos[3];
        cgltf_accessor_read_float(posAcc, i, pos, 3);
        vertexHandles.push_back(
            het.addVertex(glm::vec3(pos[0], pos[1], pos[2])));
    }

    // Decode all per-vertex attributes
    std::vector<glm::vec2> decodedUVs(vertexCount, glm::vec2(0.0f));
    std::vector<glm::vec3> decodedNorms(vertexCount, glm::vec3(0.0f));
    std::vector<glm::vec4> decodedTans(vertexCount, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    std::vector<glm::vec3> decodedCols(vertexCount, glm::vec3(1.0f));

    // Check if any material texture view defines KHR_texture_transform
    const cgltf_texture_transform *texTransform = nullptr;
    if (prim->material) {
        if (prim->material->has_pbr_metallic_roughness &&
            prim->material->pbr_metallic_roughness.base_color_texture.has_transform) {
            texTransform = &prim->material->pbr_metallic_roughness.base_color_texture.transform;
        } else if (prim->material->normal_texture.has_transform) {
            texTransform = &prim->material->normal_texture.transform;
        } else if (prim->material->has_sheen &&
                   prim->material->sheen.sheen_color_texture.has_transform) {
            texTransform = &prim->material->sheen.sheen_color_texture.transform;
        }
    }

    if (uvAttr && uvAttr->data) {
        float cosR = 1.0f, sinR = 0.0f;
        float sx = 1.0f, sy = 1.0f;
        float ox = 0.0f, oy = 0.0f;
        if (texTransform) {
            sx = texTransform->scale[0];
            sy = texTransform->scale[1];
            ox = texTransform->offset[0];
            oy = texTransform->offset[1];
            if (texTransform->rotation != 0.0f) {
                cosR = std::cos(texTransform->rotation);
                sinR = std::sin(texTransform->rotation);
            }
        }

        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float uv[2];
            cgltf_accessor_read_float(uvAttr->data, i, uv, 2);
            float u = uv[0];
            float v = uv[1];
            if (texTransform) {
                // KHR_texture_transform spec: T = Scale * Rotate * Translate
                // Matrix: [sx*cos(r)  -sy*sin(r)  ox]
                //         [sx*sin(r)   sy*cos(r)  oy]
                float u_t = sx * cosR * u - sy * sinR * v + ox;
                float v_t = sx * sinR * u + sy * cosR * v + oy;
                u = u_t;
                v = v_t;
            }
            // Note: Mesh::ensureTextureBound already flips images vertically on load
            // via stbi_set_flip_vertically_on_load_thread(1), aligning glTF (u,v) with OpenGL.
            decodedUVs[i] = glm::vec2(u, v);
        }
    }
    if (normAttr && normAttr->data) {
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float norm[3];
            cgltf_accessor_read_float(normAttr->data, i, norm, 3);
            decodedNorms[i] = glm::vec3(norm[0], norm[1], norm[2]);
        }
    }
    if (tanAttr && tanAttr->data) {
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float tan[4];
            cgltf_accessor_read_float(tanAttr->data, i, tan, 4);
            decodedTans[i] = glm::vec4(tan[0], tan[1], tan[2], tan[3]);
        }
    }
    if (colAttr && colAttr->data) {
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float col[4];
            cgltf_accessor_read_float(colAttr->data, i, col, 4);
            decodedCols[i] = glm::vec3(col[0], col[1], col[2]);
        }
    }

    // Assigns per-vertex attributes to the three half-edges of a triangular face
    // in winding order (i0 → i1 → i2), matching the order passed to addFace().
    // This is safer than matching by destination vertex index, which can
    // mis-assign UVs when two glTF vertices share the same position (UV seams).
    auto applyAttributesToFace = [&](FaceHandle fh, cgltf_size i0, cgltf_size i1, cgltf_size i2) {
        if (fh.index < 0 || fh.index >= static_cast<int64_t>(het.getFaces().size()))
            return;
        HalfEdgeHandle start = het.getFaces()[fh.index].heh;
        if (start.index < 0)
            return;

        // addFace(vh0, vh1, vh2) creates he0→he1→he2 with:
        //   he0.dst = vh1 (=glTF vertex i1)
        //   he1.dst = vh2 (=glTF vertex i2)
        //   he2.dst = vh0 (=glTF vertex i0)
        // buildBuffersFromHET reads positions via he.dst, so each slot must carry
        // the attributes of the DESTINATION vertex, not the source vertex.
        const cgltf_size windings[3] = {i1, i2, i0};

        HalfEdgeHandle curr = start;
        int guard = 0;
        for (int slot = 0; slot < 3 && curr.index != -1 && guard < 16; ++slot, ++guard)
        {
            cgltf_size vIdx = windings[slot];
            if (uvAttr)   het.setUV(curr, decodedUVs[vIdx]);
            if (normAttr) het.setNormal(curr, decodedNorms[vIdx]);
            if (tanAttr)  het.setTangent(curr, decodedTans[vIdx]);
            if (colAttr)  het.setColor(curr, decodedCols[vIdx]);
            curr = het.next(curr);
        }
    };

    if (prim->indices)
    {
        const cgltf_accessor *idxAcc = prim->indices;
        cgltf_size indexCount = idxAcc->count;
        for (cgltf_size i = 0; i + 2 < indexCount; i += 3)
        {
            cgltf_size i0 = cgltf_accessor_read_index(idxAcc, i + 0);
            cgltf_size i1 = cgltf_accessor_read_index(idxAcc, i + 1);
            cgltf_size i2 = cgltf_accessor_read_index(idxAcc, i + 2);

            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                continue;
            if (i0 == i1 || i1 == i2 || i0 == i2)
                continue;

            FaceHandle fh = het.addFace(vertexHandles[i0], vertexHandles[i1],
                                        vertexHandles[i2]);
            if (fh.index >= 0)
                applyAttributesToFace(fh, i0, i1, i2);
        }
    }
    else
    {
        for (cgltf_size i = 0; i + 2 < vertexCount; i += 3)
        {
            FaceHandle fh = het.addFace(vertexHandles[i], vertexHandles[i + 1],
                                        vertexHandles[i + 2]);
            if (fh.index >= 0)
                applyAttributesToFace(fh, i, i + 1, i + 2);
        }
    }

    het.connectTwins();

    // ── UV + geometry diagnostic ─────────────────────────────────────────────
    {
        const auto &uvs   = het.getUVs();
        const auto &faces = het.getFaces();
        std::cerr << "[GLTF] Primitive parsed: verts=" << vertexCount
                  << " faces=" << faces.size()
                  << " hasUV=" << (het.hasCustomUVs() ? "yes" : "NO")
                  << " uvEntries=" << uvs.size() << "\n";

        if (het.hasCustomUVs() && !decodedUVs.empty())
        {
            // Compute UV bounding box from decoded UVs (after v-flip)
            glm::vec2 uvMin(decodedUVs[0]), uvMax(decodedUVs[0]);
            for (const auto &uv : decodedUVs)
            {
                uvMin = glm::min(uvMin, uv);
                uvMax = glm::max(uvMax, uv);
            }
            std::cerr << "[GLTF]   UV range: u=[" << uvMin.x << "," << uvMax.x
                      << "] v=[" << uvMin.y << "," << uvMax.y << "]\n";
            // First 3 vertex UV samples for sanity check
            for (size_t k = 0; k < std::min<size_t>(3, decodedUVs.size()); ++k)
                std::cerr << "[GLTF]   uv[" << k << "]=(" << decodedUVs[k].x << ","
                          << decodedUVs[k].y << ")\n";
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    return true;
}

std::unique_ptr<Node> buildNode(const cgltf_node *srcNode,
                                const std::string &baseDir,
                                std::unordered_set<const cgltf_node *> &visited)
{
    if (!srcNode || visited.count(srcNode))
        return nullptr;
    visited.insert(srcNode);

    auto node = std::make_unique<Node>();
    node->setName(srcNode->name ? srcNode->name : "Node");

    // Transform matrix
    glm::mat4 m(1.0f);
    if (srcNode->has_matrix)
    {
        for (int i = 0; i < 16; ++i)
        {
            m[i % 4][i / 4] = srcNode->matrix[i];
        }
        node->setRelativeTransform(m);
    }
    else
    {
        glm::vec3 t(0.0f);
        glm::quat r(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 s(1.0f);

        if (srcNode->has_translation)
        {
            t = glm::vec3(srcNode->translation[0], srcNode->translation[1],
                          srcNode->translation[2]);
        }
        if (srcNode->has_rotation)
        {
            r = glm::quat(srcNode->rotation[3], srcNode->rotation[0],
                          srcNode->rotation[1], srcNode->rotation[2]);
        }
        if (srcNode->has_scale)
        {
            s = glm::vec3(srcNode->scale[0], srcNode->scale[1],
                          srcNode->scale[2]);
        }
        m = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) *
            glm::scale(glm::mat4(1.0f), s);
        node->setRelativeTransform(m);
    }

    // Prim/Mesh building
    if (srcNode->mesh)
    {
        const cgltf_mesh *srcMesh = srcNode->mesh;
        for (cgltf_size p = 0; p < srcMesh->primitives_count; ++p)
        {
            const cgltf_primitive *prim = &srcMesh->primitives[p];
            HalfEdgeTable het;
            if (buildGeometryFromPrimitive(prim, het))
            {
                auto geom = std::make_shared<Geometry>(std::move(het));
                auto meshInstance = std::make_unique<Mesh>(geom);
                applyMaterial(prim->material, meshInstance->material, baseDir);

                if (p == 0)
                {
                    node->attachMesh(std::move(meshInstance));
                }
                else
                {
                    auto primNode = std::make_unique<Node>();
                    primNode->setName(std::string(srcNode->name ? srcNode->name
                                                                : "Primitive") +
                                      "_" + std::to_string(p));
                    primNode->setRelativeTransform(glm::mat4(1.0f));
                    primNode->attachMesh(std::move(meshInstance));
                    node->attachNode(std::move(primNode));
                }
            }
        }
    }

    // Children
    for (cgltf_size i = 0; i < srcNode->children_count; ++i)
    {
        auto child = buildNode(srcNode->children[i], baseDir, visited);
        if (child)
        {
            node->attachNode(std::move(child));
        }
    }

    return node;
}

} // namespace

std::unique_ptr<Model> GLTFParser::read(const std::string &filename)
{
    cgltf_options options = {};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filename.c_str(), &data);
    if (result != cgltf_result_success)
    {
        std::cerr << "[GLTF] Failed to parse: " << filename << std::endl;
        return nullptr;
    }

    result = cgltf_load_buffers(&options, data, filename.c_str());
    if (result != cgltf_result_success)
    {
        std::cerr << "[GLTF] Failed to load buffers: " << filename << std::endl;
        cgltf_free(data);
        return nullptr;
    }

    std::filesystem::path filePath(filename);
    std::string baseDir = filePath.parent_path().string();
    if (!baseDir.empty() && baseDir.back() != '/' && baseDir.back() != '\\')
    {
        baseDir += "/";
    }

    auto model = std::make_unique<Model>();
    std::unordered_set<const cgltf_node *> visited;

    const cgltf_scene *activeScene = data->scene;
    if (!activeScene && data->scenes_count > 0)
    {
        activeScene = &data->scenes[0];
    }

    if (activeScene)
    {
        for (cgltf_size i = 0; i < activeScene->nodes_count; ++i)
        {
            auto rootNode = buildNode(activeScene->nodes[i], baseDir, visited);
            if (rootNode)
            {
                model->attachNode(std::move(rootNode));
            }
        }
    }
    else
    {
        // Fallback: load all nodes that don't have parents
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
        {
            if (!data->nodes[i].parent)
            {
                auto rootNode = buildNode(&data->nodes[i], baseDir, visited);
                if (rootNode)
                {
                    model->attachNode(std::move(rootNode));
                }
            }
        }
    }

    cgltf_free(data);

    if (model->getNodes().empty())
    {
        std::cerr << "[GLTF] Loaded empty model" << std::endl;
        return nullptr;
    }

    return model;
}
