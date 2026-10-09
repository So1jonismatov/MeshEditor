#include "Mesh.h"
#include "Application.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stb_image.h>

static void addWireframeBox(const bbox &box, const glm::vec3 &color,
                            std::vector<Vertex> &lines)
{
    glm::vec3 c[8] = {
        {box.min.x, box.min.y, box.min.z}, // 0
        {box.max.x, box.min.y, box.min.z}, // 1
        {box.max.x, box.max.y, box.min.z}, // 2
        {box.min.x, box.max.y, box.min.z}, // 3
        {box.min.x, box.min.y, box.max.z}, // 4
        {box.max.x, box.min.y, box.max.z}, // 5
        {box.max.x, box.max.y, box.max.z}, // 6
        {box.min.x, box.max.y, box.max.z}  // 7
    };

    auto addLine = [&](int i, int j)
    {
        lines.push_back(Vertex{c[i], glm::vec3(0.0f), color});
        lines.push_back(Vertex{c[j], glm::vec3(0.0f), color});
    };

    addLine(0, 1);
    addLine(1, 2);
    addLine(2, 3);
    addLine(3, 0);
    addLine(4, 5);
    addLine(5, 6);
    addLine(6, 7);
    addLine(7, 4);
    addLine(0, 4);
    addLine(1, 5);
    addLine(2, 6);
    addLine(3, 7);
}

static glm::vec3 getDepthColor(int depth)
{
    static const glm::vec3 palette[] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // Red
        glm::vec3(0.0f, 1.0f, 0.0f), // Green
        glm::vec3(0.0f, 0.0f, 1.0f), // Blue
        glm::vec3(1.0f, 1.0f, 0.0f), // Yellow
        glm::vec3(0.0f, 1.0f, 1.0f), // Cyan
        glm::vec3(1.0f, 0.0f, 1.0f), // Magenta
        glm::vec3(1.0f, 0.5f, 0.0f), // Orange
        glm::vec3(0.5f, 0.0f, 1.0f), // Purple
        glm::vec3(0.0f, 0.5f, 0.5f)  // Teal
    };
    constexpr int numColors = sizeof(palette) / sizeof(palette[0]);
    if (depth < 0)
        depth = 0;
    return palette[depth % numColors];
}

static void collectOctreeLines(const OctreeNode *node, int depth,
                               std::vector<Vertex> &lines)
{
    if (!node)
        return;

    glm::vec3 color = getDepthColor(depth);
    addWireframeBox(node->box, color, lines);

    for (const auto &child : node->children)
    {
        if (child)
        {
            collectOctreeLines(child.get(), depth + 1, lines);
        }
    }
}

Geometry *Mesh::getGeometry() const
{
    return m_geometry.get();
}

const bbox &Mesh::getBoundingBox() const
{
    return m_geometry->getBoundingBox();
}

void Mesh::applyTransformation(FaceHandle fh, const glm::mat4 &trf)
{
    m_geometry->applyTransformation(fh, trf);
}

const HalfEdgeTable &Mesh::getHalfEdgeTable() const
{
    return m_geometry->getHalfEdgeTable();
}
HalfEdgeTable &Mesh::getHalfEdgeTable()
{
    return m_geometry->getHalfEdgeTable();
}
const std::vector<Vertex> &Mesh::getTriangleSoup() const
{
    return m_geometry->getTriangleSoup();
}

void Mesh::setPolylineSegments(const std::vector<glm::vec3> &segments)
{
    m_geometry->setPolylineSegments(segments);
}

FaceHandle Mesh::faceForTriangle(size_t triangleIndex) const
{
    return m_geometry->faceForTriangle(triangleIndex);
}
size_t Mesh::getIndexCount() const
{
    return m_geometry->getIndexCount();
}
std::vector<glm::vec3> Mesh::collectFacePolygon(FaceHandle fh) const
{
    return m_geometry->collectFacePolygon(fh);
}
void Mesh::collectFacePolygon(FaceHandle fh, std::vector<glm::vec3> &out) const
{
    m_geometry->collectFacePolygon(fh, out);
}
const OctreeNode *Mesh::getFaceOctree() const
{
    return m_geometry->getFaceOctree();
}
const std::unordered_set<size_t> &Mesh::getLooseFaces() const
{
    return m_geometry->getLooseFaces();
}

void Mesh::paintBoundaryFaces()
{
    m_geometry->paintBoundaryFaces();
}
void Mesh::moveEvenFaces(glm::vec3 offset)
{
    m_geometry->moveEvenFaces(offset);
}
void Mesh::moveOrthogonalFaces(glm::vec3 normal, glm::vec3 offset)
{
    m_geometry->moveOrthogonalFaces(normal, offset);
}

bool Mesh::getRenderBlackEdges() const
{
    return m_renderBlackEdges;
}
void Mesh::setRenderBlackEdges(bool enable)
{
    m_renderBlackEdges = enable;
}

bool Mesh::getNodeHighlighted() const
{
    return m_nodeHighlighted;
}
void Mesh::setNodeHighlighted(bool enable)
{
    m_nodeHighlighted = enable;
}

bool Mesh::getRenderMeshAABB() const
{
    return m_renderMeshAABB;
}
void Mesh::setRenderMeshAABB(bool enable)
{
    if (m_renderMeshAABB != enable)
        m_debugLinesDirty = true;
    m_renderMeshAABB = enable;
}

bool Mesh::getRenderOctreeBB() const
{
    return m_renderOctreeBB;
}
void Mesh::setRenderOctreeBB(bool enable)
{
    if (m_renderOctreeBB != enable)
        m_debugLinesDirty = true;
    m_renderOctreeBB = enable;
}

void Mesh::markDirty()
{
    m_geometry->markGeometryDirty();
}
void Mesh::markVertexDirty(VertexHandle vh)
{
    m_geometry->markVertexDirty(vh);
}
void Mesh::markOctreeDirty()
{
    m_geometry->markOctreeDirty();
    m_debugLinesDirty = true;
}
void Mesh::flushOctreeLooseFaces()
{
    m_geometry->flushOctreeLooseFaces();
}

void Mesh::markPositionsDirty()
{
    m_geometry->markPositionsDirty();
}

Mesh::Mesh() : m_geometry(std::make_shared<Geometry>()) {}

Mesh::Mesh(std::shared_ptr<Geometry> geometry) : m_geometry(std::move(geometry))
{
    if (!m_geometry)
        m_geometry = std::make_shared<Geometry>();
}

Mesh::Mesh(const HalfEdgeTable &halfEdgeTable)
    : m_geometry(std::make_shared<Geometry>(halfEdgeTable))
{
}

Mesh::Mesh(HalfEdgeTable &&halfEdgeTable)
    : m_geometry(std::make_shared<Geometry>(std::move(halfEdgeTable)))
{
}

Mesh::~Mesh()
{
    if (Application::getInstance() &&
        Application::getInstance()->getRenderSystem())
    {
        IRenderSystem *rs = Application::getInstance()->getRenderSystem();
        rs->releaseBuffer(&m_selectionLinesCache);
        rs->releaseBuffer(&m_selectionOverlayCache);
        rs->releaseBuffer(&m_debugLinesCache);
        for (size_t i = 0; i < static_cast<size_t>(TextureSlot::Count); ++i)
        {
            rs->releaseTexture(reinterpret_cast<const char *>(this) + i);
        }
    }
}

void Mesh::syncWithGeometryVersions()
{
    if (m_seenTopologyVersion != m_geometry->topologyVersion())
    {
        m_selectedFaces.clear();
        m_selectionDirty = true;
        m_debugLinesDirty = true;
        m_seenTopologyVersion = m_geometry->topologyVersion();
        m_seenGeometryVersion = m_geometry->geometryVersion();
        return;
    }
    if (m_seenGeometryVersion != m_geometry->geometryVersion())
    {
        m_selectionDirty = true;
        m_debugLinesDirty = true;
        m_seenGeometryVersion = m_geometry->geometryVersion();
    }
}

void Mesh::deleteFace(FaceHandle fh)
{
    syncWithGeometryVersions();
    if (fh.index < 0)
        return;
    m_geometry->deleteFaces({fh.index});
    m_selectedFaces.clear();
    m_selectionDirty = true;
    syncWithGeometryVersions();
}

bool Mesh::ensureTextureBound(IRenderSystem &rs)
{
    bool hasAnyTextureBound = false;

    // Slot name table for readable diagnostics
    static const char* kSlotName[] = {
        "Diffuse", "Normal", "Occlusion", "MetallicRoughness", "Emissive",
        "Clearcoat", "ClearcoatRoughness", "ClearcoatNormal",
        "SheenColor", "SheenRoughness", "Transmission", "Thickness",
        "Specular", "SpecularColor", "Iridescence", "IridescenceThickness",
        "Anisotropy", "DiffuseTransmission", "DiffuseTransmissionColor"
    };
    static const size_t kSlotCount = sizeof(kSlotName) / sizeof(kSlotName[0]);

    for (size_t i = 0; i < static_cast<size_t>(TextureSlot::Count); ++i)
    {
        TextureSlot slot = static_cast<TextureSlot>(i);
        const std::string &path = material.getTexturePath(slot);

        if (!path.empty() && !m_textureLoadFailed[i])
        {
            if (!m_textureUploaded[i])
            {
                int w = 0, h = 0, channels = 0;
                stbi_set_flip_vertically_on_load_thread(0); // Natural top-to-bottom image order
                unsigned char *pixels =
                    stbi_load(path.c_str(), &w, &h, &channels, 4);
                if (!pixels)
                {
                    std::cerr << "[Mesh] [Tex FAIL] slot=" << (i < kSlotCount ? kSlotName[i] : "?")
                              << " path=" << path
                              << " reason=" << stbi_failure_reason() << std::endl;
                    m_textureLoadFailed[i] = true;
                }
                else
                {
                    std::cerr << "[Mesh] [Tex OK  ] slot=" << (i < kSlotCount ? kSlotName[i] : "?")
                              << " " << w << "x" << h << "x" << channels
                              << " path=" << path.substr(path.find_last_of("/\\") + 1) << std::endl;
                    // The pointer offset gives a unique key per texture slot on this mesh
                    rs.uploadTexture(reinterpret_cast<const char *>(this) + i,
                                     w, h, 4, pixels, slot);
                    stbi_image_free(pixels);
                    m_textureUploaded[i] = true;
                }
            }

            if (m_textureUploaded[i])
            {
                hasAnyTextureBound = true;
            }
        }
        else if (!path.empty() && m_textureLoadFailed[i])
        {
            // Already failed — only log once (first frame)
        }
        else if (path.empty() && !m_textureUploaded[i])
        {
            // No path set for this slot — silent, but log at debug verbosity
        }
    }

    // Also check the legacy bump path separately if it's set
    const std::string &bumpPath = material.getBumpTexturePath();
    if (!bumpPath.empty() && !m_textureLoadFailed[static_cast<size_t>(TextureSlot::Occlusion)])
    {
        size_t occIdx = static_cast<size_t>(TextureSlot::Occlusion);
        if (!m_textureUploaded[occIdx])
        {
            int w = 0, h = 0, channels = 0;
            stbi_set_flip_vertically_on_load_thread(0); // Natural top-to-bottom image order
            unsigned char *pixels =
                stbi_load(bumpPath.c_str(), &w, &h, &channels, 4);
            if (!pixels)
            {
                std::cerr << "[Mesh] [Tex FAIL] slot=Occlusion(bump)"
                          << " path=" << bumpPath
                          << " reason=" << stbi_failure_reason() << std::endl;
                m_textureLoadFailed[occIdx] = true;
            }
            else
            {
                std::cerr << "[Mesh] [Tex OK  ] slot=Occlusion(bump) "
                          << w << "x" << h << "x" << channels << std::endl;
                rs.uploadTexture(reinterpret_cast<const char *>(this) + occIdx,
                                 w, h, 4, pixels, TextureSlot::Occlusion);
                stbi_image_free(pixels);
                m_textureUploaded[occIdx] = true;
            }
        }
        if (m_textureUploaded[occIdx]) {
            hasAnyTextureBound = true;
        }
    }

    if (hasAnyTextureBound)
    {
        rs.setActiveTexture(this);
        return true;
    }

    // Log once per mesh when no texture is bound (helps diagnose black models)
    if (!m_loggedNoTexture)
    {
        m_loggedNoTexture = true;
        const std::string &diffPath = material.getTexturePath(TextureSlot::Diffuse);
        if (!diffPath.empty())
            std::cerr << "[Mesh] [Tex MISS] No textures bound despite diffuse path: " << diffPath << std::endl;
    }
    return false;
}


void Mesh::setSelectedFace(FaceHandle fh)
{
    syncWithGeometryVersions();
    m_selectedFaces.clear();
    if (fh.index >= 0)
        m_selectedFaces.insert(fh.index);
    m_selectionDirty = true;
}

void Mesh::addSelectedFace(FaceHandle fh)
{
    syncWithGeometryVersions();
    if (fh.index >= 0)
    {
        m_selectedFaces.insert(fh.index);
        m_selectionDirty = true;
    }
}

void Mesh::clearSelectedFace()
{
    if (!m_selectedFaces.empty())
    {
        m_selectedFaces.clear();
        m_selectionDirty = true;
    }
}

bool Mesh::hasSelectedFace() const
{
    return !m_selectedFaces.empty();
}

FaceHandle Mesh::getSelectedFace() const
{
    if (m_selectedFaces.empty())
        return FaceHandle{-1};
    return FaceHandle{*m_selectedFaces.begin()};
}

const std::unordered_set<int64_t> &Mesh::getSelectedFaces() const
{
    return m_selectedFaces;
}

void Mesh::deleteSelectedFaces()
{

    syncWithGeometryVersions();
    if (m_selectedFaces.empty())
        return;

    std::vector<int64_t> sorted(m_selectedFaces.begin(), m_selectedFaces.end());
    std::sort(sorted.begin(), sorted.end(), std::greater<int64_t>());

    m_geometry->deleteFaces(sorted);

    m_selectedFaces.clear();
    m_selectionDirty = true;
    m_debugLinesDirty = true;
    syncWithGeometryVersions();
}

void Mesh::colorHoles()
{
    m_holesHighlighted = !m_holesHighlighted;
}


void Mesh::renderIncremental(IRenderSystem &rs, bool lineMode)
{
    if (rs.getOpaquePassOnly() && material.getTransmission() > 0.001f)
        return;

    Geometry &geo = *m_geometry;

    syncWithGeometryVersions();
    geo.syncGpuBuffers(rs);

    const bool textured = !lineMode && ensureTextureBound(rs);
    glm::vec3 diffuse = material.getDiffuse();
    // Scene-tree selection: tint the whole mesh with the same slate blue the
    // face-selection overlay uses. On textured meshes the tint multiplies the texel.
    if (m_nodeHighlighted)
        diffuse = glm::vec3(0.0f, 0.8f, 1.0f);
    rs.setMaterial(diffuse * 0.30f, diffuse, material.getSpecular(),
                   material.getShininess());
    rs.setPbrMaterial(material.getMetallic(), material.getRoughness(),
                      material.getEmissive(), material.getIOR(),
                      material.getTransmission());
    rs.setPbrExtendedParams(material.getExtendedParams());

    if (lineMode)
    {
        rs.setLineWidth(1.0f);
        rs.renderPolyline(geo.getTriangleSoup());
        rs.setLineWidth(1.0f);
    }
    else
    {
        geo.drawIndexed(rs, m_renderBlackEdges);
        if (textured)
            rs.setActiveTexture(nullptr);
    }

    // The flat white material is only needed by the overlay draws below.
    // Setting it unconditionally re-dirtied the render system's material on
    // every mesh (the next mesh's own material always differed from white),
    // forcing a redundant uniform upload per draw even on overlay-free
    // meshes — the common case.
    bool overlayMaterialSet = false;
    auto ensureOverlayMaterial = [&rs, &overlayMaterialSet]()
    {
        if (overlayMaterialSet)
            return;
        rs.setMaterial(glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(0.0f), 1.0f);
        overlayMaterialSet = true;
    };

    if (geo.hasPolyline())
    {
        ensureOverlayMaterial();
        geo.drawPolyline(rs);
    }

    if (m_holesHighlighted)
    {
        ensureOverlayMaterial();
        rs.setOverlayMode(true);
        geo.drawHoleBoundaries(rs);
        rs.setOverlayMode(false);
    }

    if (m_selectionDirty)
    {
        rebuildSelectionCaches();
        m_selectionDirty = false;
        if (!m_selectionLinesCache.empty())
            rs.uploadTriangleSoup(&m_selectionLinesCache,
                                  m_selectionLinesCache);
        if (!m_selectionOverlayCache.empty())
            rs.uploadTriangleSoup(&m_selectionOverlayCache,
                                  m_selectionOverlayCache);
    }

    if (!m_selectionLinesCache.empty())
    {
        ensureOverlayMaterial();
        rs.setOverlayMode(true);
        rs.setLineWidth(4.0f);
        rs.drawLineBuffer(&m_selectionLinesCache, m_selectionLinesCache.size());
        rs.setLineWidth(1.0f);
        rs.setOverlayMode(false);
    }

    if (!m_selectionOverlayCache.empty())
    {
        ensureOverlayMaterial();
        rs.setOverlayMode(true);
        rs.drawTriangleSoup(&m_selectionOverlayCache,
                            m_selectionOverlayCache.size());
        rs.setOverlayMode(false);
    }

    if (m_renderMeshAABB || m_renderOctreeBB)
    {
        const OctreeNode *octreeRoot =
            m_renderOctreeBB ? geo.getFaceOctree() : nullptr;
        if (m_debugLinesDirty || octreeRoot != m_debugLinesOctreeRoot)
        {
            m_debugLinesCache.clear();
            if (m_renderMeshAABB)
            {
                addWireframeBox(geo.getBoundingBox(),
                                glm::vec3(1.0f, 0.0f, 0.0f), m_debugLinesCache);
            }
            if (octreeRoot)
            {
                collectOctreeLines(octreeRoot, 0, m_debugLinesCache);
            }
            m_debugLinesOctreeRoot = octreeRoot;
            m_debugLinesDirty = false;
            if (!m_debugLinesCache.empty())
                rs.uploadTriangleSoup(&m_debugLinesCache, m_debugLinesCache);
        }
        if (!m_debugLinesCache.empty())
        {
            ensureOverlayMaterial();
            rs.setLineWidth(2.0f);
            rs.drawLineBuffer(&m_debugLinesCache, m_debugLinesCache.size());
            rs.setLineWidth(1.0f);
        }
    }
}

void Mesh::rebuildSelectionCaches()
{
    m_selectionLinesCache.clear();
    m_selectionOverlayCache.clear();

    const HalfEdgeTable &het = m_geometry->getHalfEdgeTable();
    std::vector<Vertex> &batchSelectedLines = m_selectionLinesCache;
    std::vector<Vertex> &batchSelectedOverlay = m_selectionOverlayCache;
    const glm::vec3 selectedColor(0.0f, 0.8f, 1.0f); // Vibrant cyan/blue for visibility

    for (int64_t selectedIdx : m_selectedFaces)
    {
        if (selectedIdx < 0 ||
            selectedIdx >= static_cast<int64_t>(het.getFaces().size()))
            continue;

        const Face &selectedFace = het.getFaces()[selectedIdx];
        const auto &halfEdges = het.getHalfEdges();
        const auto &vertices = het.getVertices();

        if (selectedFace.heh.index < 0 ||
            selectedFace.heh.index >= static_cast<int64_t>(halfEdges.size()))
            continue;

        std::vector<glm::vec3> selectedFacePolygon;
        HalfEdgeHandle start = selectedFace.heh;
        HalfEdgeHandle current = start;
        std::size_t guard = 0;
        const std::size_t maxSteps = halfEdges.size() + 1;

        while (guard < maxSteps)
        {
            ++guard;

            if (current.index < 0 ||
                current.index >= static_cast<int64_t>(halfEdges.size()))
                break;

            const HalfEdge &edge = het.deref(current);
            if (edge.dst.index < 0 ||
                edge.dst.index >= static_cast<int64_t>(vertices.size()) ||
                edge.next.index < 0 ||
                edge.next.index >= static_cast<int64_t>(halfEdges.size()))
            {
                selectedFacePolygon.clear();
                break;
            }

            const glm::vec3 endPoint = het.getPoint(edge.dst);
            selectedFacePolygon.push_back(endPoint);

            HalfEdgeHandle nextHeh = het.next(current);
            if (nextHeh == start)
                break;

            current = nextHeh;
        }

        if (selectedFacePolygon.size() >= 3)
        {
            glm::vec3 faceNormal(0.0f);
            for (size_t i = 0; i < selectedFacePolygon.size(); ++i)
            {
                const glm::vec3 &cur = selectedFacePolygon[i];
                const glm::vec3 &next =
                    selectedFacePolygon[(i + 1) % selectedFacePolygon.size()];
                faceNormal.x += (cur.y - next.y) * (cur.z + next.z);
                faceNormal.y += (cur.z - next.z) * (cur.x + next.x);
                faceNormal.z += (cur.x - next.x) * (cur.y + next.y);
            }
            if (glm::dot(faceNormal, faceNormal) > 1e-12f)
            {
                faceNormal = glm::normalize(faceNormal);
                // Subtle offset to avoid z-fighting without noticeably floating above the face
                const glm::vec3 offset = faceNormal * 0.0002f;
                for (size_t i = 0; i < selectedFacePolygon.size(); ++i)
                {
                    const glm::vec3 startPoint =
                        selectedFacePolygon[i] + offset;
                    const glm::vec3 endPoint =
                        selectedFacePolygon[(i + 1) %
                                            selectedFacePolygon.size()] +
                        offset;
                    batchSelectedLines.push_back(
                        Vertex{startPoint, glm::vec3(0.0f), selectedColor});
                    batchSelectedLines.push_back(
                        Vertex{endPoint, glm::vec3(0.0f), selectedColor});
                }

                for (size_t i = 1; i + 1 < selectedFacePolygon.size(); ++i)
                {
                    batchSelectedOverlay.push_back(
                        Vertex{selectedFacePolygon[0] + offset, faceNormal,
                               selectedColor});
                    batchSelectedOverlay.push_back(
                        Vertex{selectedFacePolygon[i] + offset, faceNormal,
                               selectedColor});
                    batchSelectedOverlay.push_back(
                        Vertex{selectedFacePolygon[i + 1] + offset, faceNormal,
                               selectedColor});
                }
            }
        }
    }
}


