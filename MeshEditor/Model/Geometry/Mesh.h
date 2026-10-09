#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_set>

#include <HalfEdge.h>
#include "Material.h"
#include "../Interfaces/IRenderSystem.h"
#include "Geometry.h"

class TaskHandle;

class Mesh
{
public:
    Mesh();
    explicit Mesh(std::shared_ptr<Geometry> geometry);

    Mesh(const HalfEdgeTable &halfEdgeTable);
    Mesh(HalfEdgeTable &&halfEdgeTable);
    ~Mesh();

    // The shared geometry — also the render system's vertex/index buffer key
    // (used by the FBO pick pass). Raw pointer on purpose: nothing outside
    // the scene graph may retain shared ownership.
    Geometry *getGeometry() const;

    void renderIncremental(IRenderSystem &rs, bool lineMode = false);
    const bbox &getBoundingBox() const;

    void applyTransformation(FaceHandle fh, const glm::mat4 &trf);
    void deleteFace(FaceHandle fh);
    void setSelectedFace(FaceHandle fh);
    void addSelectedFace(FaceHandle fh);
    void clearSelectedFace();
    bool hasSelectedFace() const;
    FaceHandle getSelectedFace() const;
    const std::unordered_set<int64_t> &getSelectedFaces() const;
    void deleteSelectedFaces();

    const HalfEdgeTable &getHalfEdgeTable() const;
    HalfEdgeTable &getHalfEdgeTable();
    const std::vector<Vertex> &getTriangleSoup() const;

    // Standalone line geometry (COLLADA <lines>/<linestrips>).
    void setPolylineSegments(const std::vector<glm::vec3> &segments);

    FaceHandle faceForTriangle(size_t triangleIndex) const;
    size_t getIndexCount() const;
    std::vector<glm::vec3> collectFacePolygon(FaceHandle fh) const;
    void collectFacePolygon(FaceHandle fh, std::vector<glm::vec3> &out) const;
    const OctreeNode *getFaceOctree() const;
    const std::unordered_set<size_t> &getLooseFaces() const;

    void colorHoles();
    void paintBoundaryFaces();
    void moveEvenFaces(glm::vec3 offset);
    void moveOrthogonalFaces(glm::vec3 normal, glm::vec3 offset);

    bool getRenderBlackEdges() const;
    void setRenderBlackEdges(bool enable);

    // Whole-node highlight driven by the scene-tree selection: every triangle
    // is tinted with the same slate blue the face-selection overlay uses.
    bool getNodeHighlighted() const;
    void setNodeHighlighted(bool enable);

    bool getRenderMeshAABB() const;
    void setRenderMeshAABB(bool enable);

    bool getRenderOctreeBB() const;
    void setRenderOctreeBB(bool enable);

    void markDirty();
    void markVertexDirty(VertexHandle vh);
    void markOctreeDirty();
    void flushOctreeLooseFaces();
    // Position-only re-upload — skips full soup rebuild. Use after smooth/weld.
    void markPositionsDirty();

    Material material;

private:
    void syncWithGeometryVersions();

    bool ensureTextureBound(IRenderSystem &rs);
    void rebuildSelectionCaches();

    std::shared_ptr<Geometry> m_geometry; // never null

    mutable std::array<bool, static_cast<size_t>(TextureSlot::Count)> m_textureUploaded{};
    mutable std::array<bool, static_cast<size_t>(TextureSlot::Count)> m_textureLoadFailed{};
    mutable bool m_loggedNoTexture = false; // one-shot "no texture bound" diagnostic

    std::unordered_set<int64_t> m_selectedFaces;

    bool m_renderBlackEdges = false;
    bool m_holesHighlighted = false;
    bool m_nodeHighlighted = false;
    bool m_renderMeshAABB = false;
    bool m_renderOctreeBB = false;

    uint64_t m_seenGeometryVersion = 0;
    uint64_t m_seenTopologyVersion = 0;

    std::vector<Vertex> m_selectionLinesCache;
    std::vector<Vertex> m_selectionOverlayCache;
    bool m_selectionDirty = true;
    std::vector<Vertex> m_debugLinesCache;
    bool m_debugLinesDirty = true;

    const OctreeNode *m_debugLinesOctreeRoot = nullptr;
};
