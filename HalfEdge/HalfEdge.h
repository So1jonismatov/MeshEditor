#pragma once
#include <cstddef>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <utility>
#include <unordered_map>

struct HalfEdgeHandle
{
    int64_t index = -1;
};
struct VertexHandle
{
    int64_t index = -1;
};
struct FaceHandle
{
    int64_t index = -1;
};

bool operator==(const HalfEdgeHandle &a, const HalfEdgeHandle &b);
bool operator!=(const HalfEdgeHandle &a, const HalfEdgeHandle &b);
bool operator==(const VertexHandle &a, const VertexHandle &b);
bool operator!=(const VertexHandle &a, const VertexHandle &b);

struct HalfEdge
{
    FaceHandle fh;
    VertexHandle dst;
    HalfEdgeHandle twin;
    HalfEdgeHandle next;
    HalfEdgeHandle prev;
};

struct Face
{
    HalfEdgeHandle heh;
};

struct HalfEdgeVertex
{
    HalfEdgeHandle heh;
};

class HalfEdgeTable
{
public:
    HalfEdgeTable() = default;
    ~HalfEdgeTable() = default;

    void reserve(size_t vertexCount, size_t halfEdgeCount, size_t faceCount);

    VertexHandle addVertex(const glm::vec3 &position);
    FaceHandle addFace(VertexHandle vh0, VertexHandle vh1, VertexHandle vh2);
    FaceHandle addFace(VertexHandle vh0, VertexHandle vh1, VertexHandle vh2,
                       VertexHandle vh3);

    void connectTwins();
    void deleteFaceLocally(FaceHandle fh);
    bool canCollapse(HalfEdgeHandle heh) const;
    bool collapseEdge(HalfEdgeHandle heh);
    void validateTopology() const;

    HalfEdge &deref(HalfEdgeHandle heh);
    const HalfEdge &deref(HalfEdgeHandle heh) const;

    HalfEdgeVertex &deref(VertexHandle vh);
    const HalfEdgeVertex &deref(VertexHandle vh) const;

    Face &deref(FaceHandle fh);
    const Face &deref(FaceHandle fh) const;

    HalfEdgeHandle handle(const HalfEdge &he) const;
    VertexHandle handle(const HalfEdgeVertex &v) const;
    FaceHandle handle(const Face &f) const;

    HalfEdgeHandle next(HalfEdgeHandle heh) const;
    HalfEdgeHandle prev(HalfEdgeHandle heh) const;
    HalfEdgeHandle twin(HalfEdgeHandle heh) const;

    VertexHandle destVertex(HalfEdgeHandle heh) const;
    VertexHandle sourceVertex(HalfEdgeHandle heh) const;

    const glm::vec3 &getPoint(VertexHandle handle) const;
    void setPoint(VertexHandle handle, glm::vec3 data);

    const glm::vec2 &getUV(HalfEdgeHandle heh) const;
    void setUV(HalfEdgeHandle heh, glm::vec2 uv);
    const std::vector<glm::vec2> &getUVs() const;
    bool hasCustomUVs() const { return m_hasCustomUVs; }

    const glm::vec3 &getStartPoint(HalfEdgeHandle heh) const;
    void setStartPoint(HalfEdgeHandle heh, glm::vec3 data);

    const glm::vec3 &getEndPoint(HalfEdgeHandle heh) const;
    void setEndPoint(HalfEdgeHandle heh, glm::vec3 data);
    
    // Per-halfedge attributes
    const glm::vec3 &getNormal(HalfEdgeHandle heh) const;
    void setNormal(HalfEdgeHandle heh, glm::vec3 normal);
    bool hasCustomNormals() const { return !m_normals.empty(); }

    const glm::vec3 &getColor(HalfEdgeHandle heh) const;
    void setColor(HalfEdgeHandle heh, glm::vec3 color);
    bool hasCustomColors() const { return !m_colors.empty(); }

    const glm::vec4 &getTangent(HalfEdgeHandle heh) const;
    void setTangent(HalfEdgeHandle heh, glm::vec4 tangent);
    bool hasCustomTangents() const { return !m_tangents.empty(); }

    const std::vector<HalfEdgeVertex> &getVertices() const;
    const std::vector<glm::vec3> &getPositions() const;
    void setPositions(const std::vector<glm::vec3> &positions) { m_positions = positions; }
    void setPositions(std::vector<glm::vec3> &&positions) { m_positions = std::move(positions); }
    const std::vector<Face> &getFaces() const;
    const std::vector<HalfEdge> &getHalfEdges() const;

    void refreshEdgeMap();

private:
    struct EdgeKeyHash
    {
        std::size_t operator()(
            const std::pair<int64_t, int64_t> &key) const noexcept;
    };

    std::vector<HalfEdgeVertex> m_vertices;
    std::vector<glm::vec3> m_positions; // index-aligned with m_vertices
    std::vector<HalfEdge> m_halfEdges;
    std::vector<glm::vec2> m_uvs;       // index-aligned with m_halfEdges
    bool m_hasCustomUVs = false;        // set to true by setUV(); used by GeometryBuffers
    std::vector<glm::vec3> m_normals;   // index-aligned with m_halfEdges
    std::vector<glm::vec3> m_colors;    // index-aligned with m_halfEdges
    std::vector<glm::vec4> m_tangents;  // index-aligned with m_halfEdges
    std::vector<Face> m_faces;
    std::unordered_map<std::pair<int64_t, int64_t>, HalfEdgeHandle, EdgeKeyHash>
        m_edgeMap;

    void connectTwinsFindAndCreate();  // Find twins or create boundary edges
    void connectTwinsLinkBoundaries(); // Link boundary edges into loops
    int64_t sourceVertexIndex(HalfEdgeHandle heh) const;
    HalfEdgeHandle findAlternativeOutgoingHalfEdge(HalfEdgeHandle heh) const;
    HalfEdgeHandle findNextBoundaryEdge(HalfEdgeHandle heh) const;

    void removeHalfEdge(int64_t heIndex);
    void removeFace(int64_t fIndex);
};