#include "SceneIO.h"

#include "utils/ColladaParser.h"
#include "utils/STLParser.h"
#include "utils/GLTFParser.h"
#include "Geometry/Geometry.h"
#include "Geometry/GeometryBuffers.h"
#include "Geometry/FaceOctree.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <utility>

namespace
{
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return value;
}

size_t getMaxTriangleCount(const Model &model)
{
    size_t maxTriangleCount = 0;
    model.forEachMeshRecursive(
        [&](Mesh *mesh)
        {
            const OctreeNode *octree = mesh->getFaceOctree();
            if (!octree)
                return;

            std::function<void(const OctreeNode *)> visit =
                [&](const OctreeNode *node)
            {
                if (!node)
                    return;

                maxTriangleCount =
                    std::max(maxTriangleCount, node->faceIndices.size());

                for (const auto &child : node->children)
                {
                    visit(child.get());
                }
            };

            visit(octree);
        });
    return maxTriangleCount;
}

// Light but clearly distinguishable pastels — enough saturation that
// neighbouring nodes read as different colours at a glance, while staying in
// the whiteish/creamy range.
const glm::vec3 kNodePastelPalette[] = {
    {0.93f, 0.93f, 0.93f}, // sand beige
    {0.97f, 0.74f, 0.79f}, // rose pink
    {0.98f, 0.93f, 0.64f}, // butter cream
    {0.68f, 0.83f, 0.97f}, // sky blue
    {0.72f, 0.92f, 0.73f}, // mint green
    {0.85f, 0.75f, 0.96f}, // lilac
    {0.98f, 0.80f, 0.60f}, // apricot
    {0.66f, 0.91f, 0.87f}, // aqua
    {0.90f, 0.90f, 0.78f}, // pale olive
    {0.94f, 0.78f, 0.92f}, // orchid
};

// Collada materials in this codebase only ever carry a texture path, never a
// diffuse colour, so meshes would otherwise render the same default white.
// Tint EVERY mesh with the next pastel so nodes are distinguishable.
void assignFallbackNodeColors(const Model &model)
{
    size_t next = 0;
    model.forEachMeshRecursive(
        [&next](Mesh *mesh)
        {
            constexpr size_t paletteSize =
                sizeof(kNodePastelPalette) / sizeof(kNodePastelPalette[0]);
            mesh->material.setDiffuse(kNodePastelPalette[next % paletteSize]);
            ++next;
        });
}

// Prebuilds all GeometryBuffers and FaceOctrees on the background worker thread.
// This is pure CPU math (normals, triangulation, bounding boxes, spatial tree)
// and performs 0 OpenGL / UI allocations.
void prebuildModelBuffersAndOctrees(Model &model)
{
    std::unordered_set<Geometry *> seen;
    model.forEachMeshRecursive([&seen](Mesh *mesh) {
        if (!mesh) return;
        Geometry *geom = mesh->getGeometry();
        if (!geom || !seen.insert(geom).second) return;
        const HalfEdgeTable &het = geom->getHalfEdgeTable();
        if (het.getFaces().empty()) return;

        // 1. Calculate mesh bounding box
        geom->updateBoundingBox();
        const bbox &bounds = geom->getBoundingBox();

        // 2. Prebuild GPU buffers (pure CPU)
        PrebuiltBuffers buffers = GeometryBuffers::buildBuffersFromHET(het);

        // 3. Prebuild Face Octree (pure CPU)
        PrebuiltOctree octree = FaceOctree::buildOctreeFromHET(het, bounds);

        // 4. Adopt into Geometry on the worker thread
        geom->adoptPrebuilt(std::move(buffers), std::move(octree));
    });
}

void logModelTriangleStats(const Model &model)
{
    std::cout << "Max triangles in any octree node: "
              << getMaxTriangleCount(model) << std::endl;
}
} // namespace

std::unique_ptr<Model> loadSceneModel(const std::string &filename)
{
    if (filename.empty())
        return std::make_unique<Model>();

    std::unique_ptr<Model> model;
    const std::filesystem::path filePath(filename);
    const std::string extension = toLowerCopy(filePath.extension().string());
    if (extension == ".gltf" || extension == ".glb")
    {
        model = GLTFParser::read(filename);
    }
    else if (extension == ".stl")
    {
        const auto [vertices, faces] = STLParser::read(filename);
        if (vertices.empty() || faces.empty())
        {
            std::cerr << "Failed to load STL file: " << filename << std::endl;
            return nullptr;
        }

        HalfEdgeTable het;
        het.reserve(vertices.size(), faces.size() * 3, faces.size());
        std::vector<VertexHandle> vertexHandles;
        vertexHandles.reserve(vertices.size());
        for (const auto &vertex : vertices)
        {
            vertexHandles.push_back(het.addVertex(vertex));
        }

        for (const auto &face : faces)
        {
            if (face[0] >= vertexHandles.size() ||
                face[1] >= vertexHandles.size() ||
                face[2] >= vertexHandles.size())
            {
                continue;
            }

            het.addFace(vertexHandles[face[0]], vertexHandles[face[1]],
                        vertexHandles[face[2]]);
        }

        het.connectTwins();

        model = std::make_unique<Model>();
        auto node = std::make_unique<Node>();
        const std::string nodeName = filePath.stem().string();
        node->setName(nodeName.empty() ? "ImportedMesh" : nodeName);
        node->attachMesh(std::make_unique<Mesh>(std::move(het)));
        model->attachNode(std::move(node));
    }
    else
    {
        model = loadModel(filename);
    }

    if (model)
    {
        if (extension != ".gltf" && extension != ".glb")
        {
            assignFallbackNodeColors(*model);
        }
        prebuildModelBuffersAndOctrees(*model);
        logModelTriangleStats(*model);
    }

    return model;
}

void warmupModel(Model &model)
{
    prebuildModelBuffersAndOctrees(model);
    logModelTriangleStats(model);
}
