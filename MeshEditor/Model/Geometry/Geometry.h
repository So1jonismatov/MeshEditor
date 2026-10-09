#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <HalfEdge.h>
#include "../Interfaces/IRenderSystem.h"
#include "Octree.h"
#include "FaceOctree.h"
#include "GeometryBuffers.h"

class Geometry
{
public:
    Geometry() = default;
    explicit Geometry(const HalfEdgeTable &halfEdgeTable);
    explicit Geometry(HalfEdgeTable &&halfEdgeTable);
    ~Geometry();
    Geometry(const Geometry &) = delete;
    Geometry &operator=(const Geometry &) = delete;

    const HalfEdgeTable &getHalfEdgeTable() const;
    HalfEdgeTable &getHalfEdgeTable();
    void setHalfEdgeTable(const HalfEdgeTable &het);
    void setHalfEdgeTable(HalfEdgeTable &&het);
    const bbox &getBoundingBox() const;
    const std::vector<Vertex> &getTriangleSoup() const;
    std::vector<glm::vec3> collectFacePolygon(FaceHandle fh) const;

    void collectFacePolygon(FaceHandle fh, std::vector<glm::vec3> &out) const;
    const OctreeNode *getFaceOctree() const;
    const std::unordered_set<size_t> &getLooseFaces() const;
    
    FaceHandle faceForTriangle(size_t triangleIndex) const;
    size_t getIndexCount() const;

    void syncGpuBuffers(IRenderSystem &rs);
    void drawIndexed(IRenderSystem &rs, bool blackEdges);
    void drawPolyline(IRenderSystem &rs);
    bool hasPolyline() const { return !m_polylineSoup.empty(); }

    void drawHoleBoundaries(IRenderSystem &rs);
    void setPolylineSegments(const std::vector<glm::vec3> &segments);

    void applyTransformation(FaceHandle fh, const glm::mat4 &trf);

    void deleteFaces(const std::vector<int64_t> &sortedDescending);
    void moveEvenFaces(glm::vec3 offset);
    void moveOrthogonalFaces(glm::vec3 normal, glm::vec3 offset);

    void paintBoundaryFaces();

    void markGeometryDirty();
    void markVertexDirty(VertexHandle vh);
    void markFaceDirty(FaceHandle fh);
    // Position-only dirty: re-uploads vertex data without rebuilding index soup.
    // Much cheaper than markGeometryDirty() for operators that only move vertices.
    void markPositionsDirty();

    void markOctreeDirty();
    void flushOctreeLooseFaces();

    uint64_t geometryVersion() const;
    uint64_t topologyVersion() const;

    GeometryBuffers *getBuffers() { return m_buffers.get(); }
    const GeometryBuffers *getBuffers() const { return m_buffers.get(); }

    void adoptPrebuilt(HalfEdgeTable &&het,
                       PrebuiltBuffers &&buffers,
                       PrebuiltOctree &&octree);

    void adoptPrebuilt(HalfEdgeTable &&het,
                       PrebuiltBuffers &&buffers,
                       PrebuiltOctree &&octree,
                       IRenderSystem &rs);

    void adoptPrebuilt(PrebuiltBuffers &&buffers,
                       PrebuiltOctree &&octree);

    void adoptPrebuiltPositions(std::vector<glm::vec3> &&positions,
                                std::vector<Vertex> &&indexedSoup,
                                IRenderSystem &rs);

    void updateBoundingBox() const;

private:
    void rebuildHoleBoundaryCache();

    HalfEdgeTable m_het;
    mutable bbox m_bbox;
    mutable bool m_bboxDirty = true;
    
    std::unique_ptr<FaceOctree> m_octree;
    std::unique_ptr<GeometryBuffers> m_buffers;

    // Standalone line geometry (COLLADA lines/linestrips).
    std::vector<Vertex> m_polylineSoup;
    bool m_polylineUploaded = false;

    // Hole boundaries are derived from topology alone, so the cache is shared
    // by all instances. The vector address doubles as the GPU buffer key.
    std::vector<Vertex> m_holeBoundaryCache;
    bool m_holeBoundaryDirty = true;

    uint64_t m_geometryVersion = 0;
    uint64_t m_topologyVersion = 0;
};
