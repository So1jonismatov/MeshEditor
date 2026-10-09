#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <glm/glm.hpp>
#include "HalfEdge.h"
#include "../Interfaces/IRenderSystem.h"

class Geometry;

struct FaceRegion
{
    size_t triangleStart = 0;
    size_t triangleVertexCount = 0;
    size_t indexedStart = 0;
    size_t indexedVertexCount = 0;
    size_t indexStart = 0;
    size_t indexCount = 0;
};

struct PrebuiltBuffers
{
    std::vector<Vertex> indexedSoup;
    std::vector<unsigned int> triangleIndices;
    std::vector<int64_t> triangleToFace;
    std::unordered_map<int64_t, FaceRegion> faceOffsetMap;
};

class GeometryBuffers
{
public:
    explicit GeometryBuffers(const Geometry* geometry);
    ~GeometryBuffers() = default;

    void syncGpuBuffers(IRenderSystem &rs);
    void drawIndexed(IRenderSystem &rs, bool blackEdges);
    size_t getIndexCount() const;
    FaceHandle faceForTriangle(size_t triangleIndex) const;
    void paintBoundaryFaces();

    void markSoupDirty();
    void markBBoxDirty();
    void markFaceDirty(int64_t faceIndex);
    void requestFullUpload();
    void clearDirtyFaces();
    bool isSoupDirty() const;
    void clearRedFaces();
    void clearState();
    // Re-upload only vertex positions/normals without rebuilding the soup index structure.
    // Call this after position-only changes (e.g. Laplacian Smooth).
    void markPositionsDirty();

    // Asynchronous Pre-built Buffer Pipeline
    static std::vector<glm::vec3> computeSmoothNormals(const HalfEdgeTable &het);
    static PrebuiltBuffers buildBuffersFromHET(const HalfEdgeTable &het, const std::unordered_set<int64_t> &redFaces = {});
    static std::vector<Vertex> rebuildIndexedPositions(const HalfEdgeTable &het,
                                                       const std::unordered_map<int64_t, FaceRegion> &faceOffsetMap,
                                                       const std::vector<Vertex> &existingIndexedSoup);

    void adoptPrebuiltBuffers(PrebuiltBuffers &&prebuilt);
    void adoptPrebuiltBuffers(PrebuiltBuffers &&prebuilt, IRenderSystem &rs);
    void adoptPrebuiltPositions(std::vector<Vertex> &&indexedSoup, IRenderSystem &rs);

    const std::unordered_map<int64_t, FaceRegion>& getFaceOffsetMap() const { return m_faceOffsetMap; }
    std::unordered_map<int64_t, FaceRegion>& getFaceOffsetMap() { return m_faceOffsetMap; }
    
    std::vector<Vertex>& getTriangleSoup() { return m_triangleSoup; }
    const std::vector<Vertex>& getTriangleSoup() const { return m_triangleSoup; }

    std::vector<Vertex>& getIndexedSoup() { return m_indexedSoup; }
    const std::vector<Vertex>& getIndexedSoup() const { return m_indexedSoup; }

    std::vector<unsigned int>& getTriangleIndices() { return m_triangleIndices; }
    const std::vector<unsigned int>& getTriangleIndices() const { return m_triangleIndices; }

    std::vector<int64_t>& getTriangleToFace() { return m_triangleToFace; }
    const std::vector<int64_t>& getTriangleToFace() const { return m_triangleToFace; }

    void removeFaceOffsetMapEntry(int64_t index);
    const std::unordered_set<int64_t>& getRedFaces() const { return m_redFaces; }
    bool getBoundaryFacesHighlighted() const { return m_boundaryFacesHighlighted; }
    void setBoundaryFacesHighlighted(bool highlighted) { m_boundaryFacesHighlighted = highlighted; }

    void updateTriangleSoup() const;

private:
    struct FaceRenderData
    {
        std::vector<Vertex> triangleSoup;
        std::vector<Vertex> indexedSoup;
        std::vector<unsigned int> indices;
    };

    FaceRenderData buildFaceRenderData(FaceHandle fh, glm::vec3 color,
                                       const std::vector<glm::vec3> &smoothNormals = {}) const;
    std::vector<glm::vec2> collectFaceUVs(FaceHandle fh) const;

    const Geometry* m_geometry;

    mutable bool m_triangleSoupDirty = true;
    mutable std::vector<Vertex> m_triangleSoup;
    mutable std::vector<Vertex> m_indexedSoup;
    mutable std::vector<unsigned int> m_triangleIndices;
    mutable std::unordered_map<int64_t, FaceRegion> m_faceOffsetMap;
    mutable std::vector<int64_t> m_triangleToFace;
    mutable std::unordered_set<int64_t> m_dirtyFaces;
    mutable bool m_fullUploadNeeded = true;
    std::unordered_set<int64_t> m_redFaces;
    bool m_boundaryFacesHighlighted = false;
    mutable bool m_positionsDirty = false;
};
