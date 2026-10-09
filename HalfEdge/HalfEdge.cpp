#include "HalfEdge.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <iostream>
#include <algorithm>

std::size_t HalfEdgeTable::EdgeKeyHash::operator()(
    const std::pair<int64_t, int64_t> &key) const noexcept
{
    const std::size_t h1 = std::hash<int64_t>{}(key.first);
    const std::size_t h2 = std::hash<int64_t>{}(key.second);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

bool operator==(const HalfEdgeHandle &a, const HalfEdgeHandle &b)
{
    return a.index == b.index;
}
bool operator!=(const HalfEdgeHandle &a, const HalfEdgeHandle &b)
{
    return a.index != b.index;
}
bool operator==(const VertexHandle &a, const VertexHandle &b)
{
    return a.index == b.index;
}
bool operator!=(const VertexHandle &a, const VertexHandle &b)
{
    return a.index != b.index;
}

HalfEdge &HalfEdgeTable::deref(HalfEdgeHandle heh)
{
    return m_halfEdges[heh.index];
}
const HalfEdge &HalfEdgeTable::deref(HalfEdgeHandle heh) const
{
    return m_halfEdges[heh.index];
}

HalfEdgeVertex &HalfEdgeTable::deref(VertexHandle vh)
{
    return m_vertices[vh.index];
}
const HalfEdgeVertex &HalfEdgeTable::deref(VertexHandle vh) const
{
    return m_vertices[vh.index];
}

Face &HalfEdgeTable::deref(FaceHandle fh)
{
    return m_faces[fh.index];
}
const Face &HalfEdgeTable::deref(FaceHandle fh) const
{
    return m_faces[fh.index];
}

HalfEdgeHandle HalfEdgeTable::handle(const HalfEdge &he) const
{
    return {static_cast<int64_t>(&he - &m_halfEdges[0])};
}
VertexHandle HalfEdgeTable::handle(const HalfEdgeVertex &v) const
{
    return {static_cast<int64_t>(&v - &m_vertices[0])};
}
FaceHandle HalfEdgeTable::handle(const Face &f) const
{
    return {static_cast<int64_t>(&f - &m_faces[0])};
}

HalfEdgeHandle HalfEdgeTable::next(HalfEdgeHandle heh) const
{
    return deref(heh).next;
}
HalfEdgeHandle HalfEdgeTable::prev(HalfEdgeHandle heh) const
{
    return deref(heh).prev;
}
HalfEdgeHandle HalfEdgeTable::twin(HalfEdgeHandle heh) const
{
    return deref(heh).twin;
}

VertexHandle HalfEdgeTable::destVertex(HalfEdgeHandle heh) const
{
    return deref(heh).dst;
}
VertexHandle HalfEdgeTable::sourceVertex(HalfEdgeHandle heh) const
{
    return destVertex(twin(heh));
}

const glm::vec3 &HalfEdgeTable::getPoint(VertexHandle handle) const
{
    return m_positions[handle.index];
}
void HalfEdgeTable::setPoint(VertexHandle handle, glm::vec3 data)
{
    m_positions[handle.index] = data;
}

const glm::vec2 &HalfEdgeTable::getUV(HalfEdgeHandle heh) const
{
    static const glm::vec2 zero(0.0f);
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_uvs.size()))
        return m_uvs[heh.index];
    return zero;
}
void HalfEdgeTable::setUV(HalfEdgeHandle heh, glm::vec2 uv)
{
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_uvs.size()))
    {
        m_uvs[heh.index] = uv;
        m_hasCustomUVs = true;
    }
}
const std::vector<glm::vec2> &HalfEdgeTable::getUVs() const
{
    return m_uvs;
}

const glm::vec3 &HalfEdgeTable::getNormal(HalfEdgeHandle heh) const
{
    static const glm::vec3 zero(0.0f);
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_normals.size()))
        return m_normals[heh.index];
    return zero;
}
void HalfEdgeTable::setNormal(HalfEdgeHandle heh, glm::vec3 normal)
{
    if (m_normals.size() < m_halfEdges.size())
        m_normals.resize(m_halfEdges.size(), glm::vec3(0.0f));
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_normals.size()))
        m_normals[heh.index] = normal;
}

const glm::vec3 &HalfEdgeTable::getColor(HalfEdgeHandle heh) const
{
    static const glm::vec3 white(1.0f);
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_colors.size()))
        return m_colors[heh.index];
    return white;
}
void HalfEdgeTable::setColor(HalfEdgeHandle heh, glm::vec3 color)
{
    if (m_colors.size() < m_halfEdges.size())
        m_colors.resize(m_halfEdges.size(), glm::vec3(1.0f));
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_colors.size()))
        m_colors[heh.index] = color;
}

const glm::vec4 &HalfEdgeTable::getTangent(HalfEdgeHandle heh) const
{
    static const glm::vec4 zero(0.0f, 0.0f, 0.0f, 1.0f);
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_tangents.size()))
        return m_tangents[heh.index];
    return zero;
}
void HalfEdgeTable::setTangent(HalfEdgeHandle heh, glm::vec4 tangent)
{
    if (m_tangents.size() < m_halfEdges.size())
        m_tangents.resize(m_halfEdges.size(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    if (heh.index >= 0 && heh.index < static_cast<int64_t>(m_tangents.size()))
        m_tangents[heh.index] = tangent;
}

const std::vector<HalfEdgeVertex> &HalfEdgeTable::getVertices() const
{
    return m_vertices;
}
const std::vector<glm::vec3> &HalfEdgeTable::getPositions() const
{
    return m_positions;
}
const std::vector<Face> &HalfEdgeTable::getFaces() const
{
    return m_faces;
}
const std::vector<HalfEdge> &HalfEdgeTable::getHalfEdges() const
{
    return m_halfEdges;
}

const glm::vec3 &HalfEdgeTable::getStartPoint(HalfEdgeHandle handle) const
{
    return getPoint(sourceVertex(handle));
}

void HalfEdgeTable::reserve(size_t vertexCount, size_t halfEdgeCount,
                            size_t faceCount)
{
    m_vertices.reserve(vertexCount);
    m_positions.reserve(vertexCount);
    m_halfEdges.reserve(halfEdgeCount);
    m_uvs.reserve(halfEdgeCount);
    // Don't reserve normals/colors/tangents by default to save memory,
    // they will lazily resize if used.
    m_faces.reserve(faceCount);
}

void HalfEdgeTable::setStartPoint(HalfEdgeHandle handle, glm::vec3 data)
{
    setPoint(sourceVertex(handle), data);
}

const glm::vec3 &HalfEdgeTable::getEndPoint(HalfEdgeHandle handle) const
{
    return getPoint(destVertex(handle));
}

void HalfEdgeTable::setEndPoint(HalfEdgeHandle handle, glm::vec3 data)
{
    setPoint(destVertex(handle), data);
}

int64_t HalfEdgeTable::sourceVertexIndex(HalfEdgeHandle heh) const
{
    if (heh.index < 0 || heh.index >= static_cast<int64_t>(m_halfEdges.size()))
        return -1;

    const HalfEdge &he = m_halfEdges[heh.index];
    if (he.prev.index >= 0 &&
        he.prev.index < static_cast<int64_t>(m_halfEdges.size()))
    {
        return m_halfEdges[he.prev.index].dst.index;
    }

    if (he.twin.index >= 0 &&
        he.twin.index < static_cast<int64_t>(m_halfEdges.size()))
    {
        return m_halfEdges[he.twin.index].dst.index;
    }

    return -1;
}

HalfEdgeHandle HalfEdgeTable::findAlternativeOutgoingHalfEdge(
    HalfEdgeHandle heh) const
{
    const int64_t sourceIndex = sourceVertexIndex(heh);
    if (sourceIndex < 0 ||
        sourceIndex >= static_cast<int64_t>(m_vertices.size()))
        return HalfEdgeHandle{-1};

    if (heh.index < 0 || heh.index >= static_cast<int64_t>(m_halfEdges.size()))
        return HalfEdgeHandle{-1};

    HalfEdgeHandle current = heh;
    const std::size_t maxSteps = m_halfEdges.size() + 1;
    std::size_t guard = 0;
    HalfEdgeHandle fallbackBoundary{-1};

    do
    {
        const HalfEdge &edge = m_halfEdges[current.index];
        const HalfEdgeHandle incoming = edge.twin;
        if (incoming.index < 0 ||
            incoming.index >= static_cast<int64_t>(m_halfEdges.size()))
        {
            break;
        }

        current = m_halfEdges[incoming.index].next;
        if (current.index < 0 ||
            current.index >= static_cast<int64_t>(m_halfEdges.size()))
        {
            break;
        }

        if (current != heh && sourceVertexIndex(current) == sourceIndex)
        {
            if (m_halfEdges[current.index].fh.index != -1)
            {
                return current; // Found a face edge
            }
            else if (fallbackBoundary.index == -1)
            {
                fallbackBoundary = current; // Fall back to boundary edge
            }
        }
    } while (current != heh && ++guard < maxSteps);

    return fallbackBoundary;
}

HalfEdgeHandle HalfEdgeTable::findNextBoundaryEdge(HalfEdgeHandle heh) const
{
    if (heh.index < 0 || heh.index >= static_cast<int64_t>(m_halfEdges.size()))
        return HalfEdgeHandle{-1};

    HalfEdgeHandle curr = m_halfEdges[heh.index].twin;
    if (curr.index == -1)
        return HalfEdgeHandle{-1};

    const std::size_t maxSteps = m_halfEdges.size() + 1;
    std::size_t guard = 0;

    while (curr.index >= 0 &&
           curr.index < static_cast<int64_t>(m_halfEdges.size()) &&
           m_halfEdges[curr.index].fh.index != -1 && ++guard < maxSteps)
    {
        HalfEdgeHandle nextHe = m_halfEdges[curr.index].next;
        if (nextHe.index < 0 ||
            nextHe.index >= static_cast<int64_t>(m_halfEdges.size()))
            break;
        curr = m_halfEdges[nextHe.index].twin;
    }

    if (curr.index < 0 ||
        curr.index >= static_cast<int64_t>(m_halfEdges.size()))
        return HalfEdgeHandle{-1};

    return curr;
}

VertexHandle HalfEdgeTable::addVertex(const glm::vec3 &position)
{
    VertexHandle vh{(int64_t)m_vertices.size()};
    m_vertices.push_back({HalfEdgeHandle{-1}});
    m_positions.push_back(position); // kept index-aligned with m_vertices
    return vh;
}

FaceHandle HalfEdgeTable::addFace(VertexHandle vh0, VertexHandle vh1,
                                  VertexHandle vh2)
{
    const int64_t vSize = static_cast<int64_t>(m_vertices.size());
    if (vh0.index < 0 || vh1.index < 0 || vh2.index < 0 ||
        vh0.index >= vSize || vh1.index >= vSize || vh2.index >= vSize)
    {
        return FaceHandle{-1};
    }

    FaceHandle fh{(int64_t)m_faces.size()};
    m_faces.push_back({HalfEdgeHandle{-1}});

    int64_t heIndex = m_halfEdges.size();
    HalfEdgeHandle he0{heIndex};
    HalfEdgeHandle he1{heIndex + 1};
    HalfEdgeHandle he2{heIndex + 2};

    m_halfEdges.push_back({fh, vh1, {-1}, he1, he2});
    m_halfEdges.push_back({fh, vh2, {-1}, he2, he0});
    m_halfEdges.push_back({fh, vh0, {-1}, he0, he1});
    m_uvs.resize(m_halfEdges.size(), glm::vec2(0.0f)); // keep aligned
    if (!m_normals.empty()) m_normals.resize(m_halfEdges.size(), glm::vec3(0.0f));
    if (!m_colors.empty()) m_colors.resize(m_halfEdges.size(), glm::vec3(1.0f));
    if (!m_tangents.empty()) m_tangents.resize(m_halfEdges.size(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    m_faces[fh.index].heh = he0;

    m_vertices[vh0.index].heh = he0;
    m_vertices[vh1.index].heh = he1;
    m_vertices[vh2.index].heh = he2;

    return fh;
}

FaceHandle HalfEdgeTable::addFace(VertexHandle vh0, VertexHandle vh1,
                                  VertexHandle vh2, VertexHandle vh3)
{
    const int64_t vSize = static_cast<int64_t>(m_vertices.size());
    if (vh0.index < 0 || vh1.index < 0 || vh2.index < 0 || vh3.index < 0 ||
        vh0.index >= vSize || vh1.index >= vSize || vh2.index >= vSize ||
        vh3.index >= vSize)
    {
        return FaceHandle{-1};
    }

    FaceHandle fh{(int64_t)m_faces.size()};
    m_faces.push_back({HalfEdgeHandle{-1}});

    int64_t heIndex = m_halfEdges.size();
    HalfEdgeHandle he0{heIndex};
    HalfEdgeHandle he1{heIndex + 1};
    HalfEdgeHandle he2{heIndex + 2};
    HalfEdgeHandle he3{heIndex + 3};

    m_halfEdges.push_back({fh, vh1, {-1}, he1, he3});
    m_halfEdges.push_back({fh, vh2, {-1}, he2, he0});
    m_halfEdges.push_back({fh, vh3, {-1}, he3, he1});
    m_halfEdges.push_back({fh, vh0, {-1}, he0, he2});
    m_uvs.resize(m_halfEdges.size(), glm::vec2(0.0f)); // keep aligned
    if (!m_normals.empty()) m_normals.resize(m_halfEdges.size(), glm::vec3(0.0f));
    if (!m_colors.empty()) m_colors.resize(m_halfEdges.size(), glm::vec3(1.0f));
    if (!m_tangents.empty()) m_tangents.resize(m_halfEdges.size(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    m_faces[fh.index].heh = he0;
    m_vertices[vh0.index].heh = he0;
    m_vertices[vh1.index].heh = he1;
    m_vertices[vh2.index].heh = he2;
    m_vertices[vh3.index].heh = he3;

    return fh;
}

void HalfEdgeTable::refreshEdgeMap()
{
    m_edgeMap.clear();
    m_edgeMap.reserve(m_halfEdges.size() * 2);
    for (size_t i = 0; i < m_halfEdges.size(); ++i)
    {
        const HalfEdge &he = m_halfEdges[i];
        if (he.dst.index < 0 ||
            he.dst.index >= static_cast<int64_t>(m_vertices.size()))
        {
            continue;
        }

        int64_t dst = he.dst.index;
        int64_t src = -1;
        if (he.prev.index >= 0 &&
            he.prev.index < static_cast<int64_t>(m_halfEdges.size()))
        {
            src = m_halfEdges[he.prev.index].dst.index;
        }
        else if (he.twin.index >= 0 &&
                 he.twin.index < static_cast<int64_t>(m_halfEdges.size()))
        {
            src = m_halfEdges[he.twin.index].dst.index;
        }

        if (src < 0 || src >= static_cast<int64_t>(m_vertices.size()))
            continue;

        m_edgeMap[{src, dst}] = HalfEdgeHandle{(int64_t)i};
    }
}

void HalfEdgeTable::connectTwins()
{
    connectTwinsFindAndCreate();
    connectTwinsLinkBoundaries();
}

void HalfEdgeTable::connectTwinsFindAndCreate()
{
    m_edgeMap.clear();
    m_edgeMap.reserve(m_halfEdges.size());
    std::vector<HalfEdge> newBoundaries;
    newBoundaries.reserve(m_halfEdges.size() / 4);

    const size_t numEdges = m_halfEdges.size();
    for (size_t i = 0; i < numEdges; ++i)
    {
        const HalfEdge &he = m_halfEdges[i];
        if (he.twin.index != -1)
            continue;

        if (he.dst.index < 0 ||
            he.dst.index >= static_cast<int64_t>(m_vertices.size()))
        {
            continue;
        }

        int64_t dst = he.dst.index;
        int64_t src = -1;
        if (he.prev.index >= 0 &&
            he.prev.index < static_cast<int64_t>(numEdges))
        {
            src = m_halfEdges[he.prev.index].dst.index;
        }

        if (src < 0 || src >= static_cast<int64_t>(m_vertices.size()))
            continue;

        // Check if the opposite directed edge (dst -> src) was already registered
        auto it = m_edgeMap.find({dst, src});
        if (it != m_edgeMap.end())
        {
            HalfEdgeHandle twin_heh = it->second;
            m_halfEdges[i].twin = twin_heh;
            m_halfEdges[twin_heh.index].twin = HalfEdgeHandle{(int64_t)i};
            m_edgeMap.erase(it); // Matched: remove from search map
        }
        else
        {
            m_edgeMap[{src, dst}] = HalfEdgeHandle{(int64_t)i};
        }
    }

    // Any remaining entries in m_edgeMap are unmatched boundary edges
    for (const auto &[edge, heh] : m_edgeMap)
    {
        const int64_t src = edge.first;
        HalfEdgeHandle boundary_heh{
            (int64_t)(m_halfEdges.size() + newBoundaries.size())};
        HalfEdge boundaryEdge;
        boundaryEdge.fh = FaceHandle{-1};
        boundaryEdge.dst = VertexHandle{src};
        boundaryEdge.twin = heh;
        boundaryEdge.next = HalfEdgeHandle{-1};
        boundaryEdge.prev = HalfEdgeHandle{-1};
        newBoundaries.push_back(boundaryEdge);
        m_halfEdges[heh.index].twin = boundary_heh;
    }

    m_halfEdges.insert(m_halfEdges.end(), newBoundaries.begin(),
                       newBoundaries.end());
    m_uvs.resize(m_halfEdges.size(), glm::vec2(0.0f));
    if (!m_normals.empty()) m_normals.resize(m_halfEdges.size(), glm::vec3(0.0f));
    if (!m_colors.empty()) m_colors.resize(m_halfEdges.size(), glm::vec3(1.0f));
    if (!m_tangents.empty()) m_tangents.resize(m_halfEdges.size(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    m_edgeMap.clear();
}

void HalfEdgeTable::connectTwinsLinkBoundaries()
{
    std::unordered_multimap<int64_t, int64_t> boundaryBySrc;
    std::vector<HalfEdgeHandle> boundaryEdges;
    for (size_t i = 0; i < m_halfEdges.size(); ++i)
    {
        if (m_halfEdges[i].fh.index == -1)
        {
            if (m_halfEdges[i].twin.index < 0 ||
                m_halfEdges[i].twin.index >=
                    static_cast<int64_t>(m_halfEdges.size()))
            {
                continue;
            }

            int64_t src = m_halfEdges[m_halfEdges[i].twin.index].dst.index;
            boundaryBySrc.emplace(src,
                                  static_cast<int64_t>(boundaryEdges.size()));
            boundaryEdges.push_back(HalfEdgeHandle{(int64_t)i});
        }
    }
    std::vector<int64_t> nextByBoundary(boundaryEdges.size(), -1);
    std::vector<int64_t> prevByBoundary(boundaryEdges.size(), -1);
    for (size_t i = 0; i < boundaryEdges.size(); ++i)
    {
        const HalfEdgeHandle current = boundaryEdges[i];
        const int64_t dst = m_halfEdges[current.index].dst.index;
        const auto range = boundaryBySrc.equal_range(dst);
        int64_t chosen = -1;
        for (auto it = range.first; it != range.second; ++it)
        {
            int64_t candidatePos = it->second;
            if (boundaryEdges[candidatePos].index == current.index)
                continue;
            if (prevByBoundary[candidatePos] == -1)
            {
                chosen = candidatePos;
                break;
            }
            if (chosen == -1)
                chosen = candidatePos;
        }
        if (chosen != -1)
        {
            nextByBoundary[i] = chosen;
            if (prevByBoundary[chosen] == -1)
                prevByBoundary[chosen] = (int64_t)i;
        }
    }
    for (size_t i = 0; i < boundaryEdges.size(); ++i)
    {
        if (nextByBoundary[i] != -1)
            m_halfEdges[boundaryEdges[i].index].next =
                boundaryEdges[nextByBoundary[i]];
        else
            m_halfEdges[boundaryEdges[i].index].next = HalfEdgeHandle{-1};
        if (prevByBoundary[i] != -1)
            m_halfEdges[boundaryEdges[i].index].prev =
                boundaryEdges[prevByBoundary[i]];
        else
            m_halfEdges[boundaryEdges[i].index].prev = HalfEdgeHandle{-1};
    }
}

void HalfEdgeTable::removeHalfEdge(int64_t heIndex)
{
    if (heIndex < 0 || heIndex >= static_cast<int64_t>(m_halfEdges.size()))
        return;

    int64_t lastIndex = static_cast<int64_t>(m_halfEdges.size()) - 1;

    // Safety fallback: disconnect isolated vertex handle pointers before removing
    int64_t src = sourceVertexIndex(HalfEdgeHandle{heIndex});
    if (src >= 0 && src < static_cast<int64_t>(m_vertices.size()) &&
        m_vertices[src].heh.index == heIndex)
    {
        HalfEdgeHandle alt =
            findAlternativeOutgoingHalfEdge(HalfEdgeHandle{heIndex});
        if (alt.index == heIndex)
            m_vertices[src].heh.index = -1;
        else
            m_vertices[src].heh = alt;
    }

    if (heIndex == lastIndex)
    {
        m_halfEdges.pop_back();
        if (!m_uvs.empty())
            m_uvs.pop_back();
        return;
    }

    // Vector swap-delete (m_uvs stays index-aligned with m_halfEdges).
    m_halfEdges[heIndex] = m_halfEdges[lastIndex];
    m_halfEdges.pop_back();
    if (static_cast<int64_t>(m_uvs.size()) > lastIndex)
    {
        m_uvs[heIndex] = m_uvs[lastIndex];
        m_uvs.pop_back();
    }
    if (static_cast<int64_t>(m_normals.size()) > lastIndex)
    {
        m_normals[heIndex] = m_normals[lastIndex];
        m_normals.pop_back();
    }
    if (static_cast<int64_t>(m_colors.size()) > lastIndex)
    {
        m_colors[heIndex] = m_colors[lastIndex];
        m_colors.pop_back();
    }
    if (static_cast<int64_t>(m_tangents.size()) > lastIndex)
    {
        m_tangents[heIndex] = m_tangents[lastIndex];
        m_tangents.pop_back();
    }

    HalfEdge &moved = m_halfEdges[heIndex];

    // CRITICAL: If the moved element contained pointers resolving to its old
    // spot (`lastIndex`), update them first.
    if (moved.twin.index == lastIndex)
        moved.twin.index = heIndex;
    if (moved.next.index == lastIndex)
        moved.next.index = heIndex;
    if (moved.prev.index == lastIndex)
        moved.prev.index = heIndex;

    // Re-link adjacent segments pointing to the newly moved element
    if (moved.twin.index >= 0 && moved.twin.index < static_cast<int64_t>(m_halfEdges.size()))
        m_halfEdges[moved.twin.index].twin.index = heIndex;
    if (moved.next.index >= 0 && moved.next.index < static_cast<int64_t>(m_halfEdges.size()))
        m_halfEdges[moved.next.index].prev.index = heIndex;
    if (moved.prev.index >= 0 && moved.prev.index < static_cast<int64_t>(m_halfEdges.size()))
        m_halfEdges[moved.prev.index].next.index = heIndex;

    // Fix up its native face's handle tracking if required
    if (moved.fh.index != -1 &&
        moved.fh.index < static_cast<int64_t>(m_faces.size()))
    {
        if (m_faces[moved.fh.index].heh.index == lastIndex)
            m_faces[moved.fh.index].heh.index = heIndex;
    }

    // Fix up its source vertex handle tracking in O(1)
    int64_t movedSrc = sourceVertexIndex(HalfEdgeHandle{heIndex});
    if (movedSrc >= 0 && movedSrc < static_cast<int64_t>(m_vertices.size()))
    {
        if (m_vertices[movedSrc].heh.index == lastIndex)
            m_vertices[movedSrc].heh.index = heIndex;
    }
}

void HalfEdgeTable::removeFace(int64_t fIndex)
{
    if (fIndex < 0 || fIndex >= static_cast<int64_t>(m_faces.size()))
        return;

    int64_t lastIndex = static_cast<int64_t>(m_faces.size()) - 1;
    if (fIndex == lastIndex)
    {
        m_faces.pop_back();
        return;
    }

    // Vector swap-delete
    m_faces[fIndex] = m_faces[lastIndex];
    m_faces.pop_back();

    // Re-link affected half-edges tracking this newly shifted face index
    HalfEdgeHandle start = m_faces[fIndex].heh;
    if (start.index != -1 &&
        start.index < static_cast<int64_t>(m_halfEdges.size()))
    {
        HalfEdgeHandle curr = start;
        std::size_t guard = 0;
        const std::size_t maxSteps = m_halfEdges.size() + 1;
        do
        {
            if (curr.index < 0 ||
                curr.index >= static_cast<int64_t>(m_halfEdges.size()))
                break;
            m_halfEdges[curr.index].fh.index = fIndex;
            curr = m_halfEdges[curr.index].next;
            ++guard;
        } while (curr != start && curr.index != -1 && guard < maxSteps);
    }
}

void HalfEdgeTable::deleteFaceLocally(FaceHandle fh)
{
    if (fh.index < 0 || fh.index >= static_cast<int64_t>(m_faces.size()))
        return;

    Face &face = m_faces[fh.index];
    if (face.heh.index < 0 ||
        face.heh.index >= static_cast<int64_t>(m_halfEdges.size()))
    {
        removeFace(fh.index);
        return;
    }

    const HalfEdgeHandle start = face.heh;
    std::vector<HalfEdgeHandle> faceLoop;
    HalfEdgeHandle current = start;
    std::size_t guard = 0;
    const std::size_t maxSteps = m_halfEdges.size() + 1;

    do
    {
        if (current.index < 0 ||
            current.index >= static_cast<int64_t>(m_halfEdges.size()))
        {
            break;
        }

        faceLoop.push_back(current);
        current = m_halfEdges[current.index].next;
        ++guard;
    } while (current != start && current.index != -1 && guard < maxSteps);

    // 1. Mark face's loop edges as boundary edges
    for (const HalfEdgeHandle heh : faceLoop)
    {
        m_halfEdges[heh.index].fh = FaceHandle{-1};
    }
    face.heh = HalfEdgeHandle{-1}; // Detach face record immediately

    // 2. Identify and collect orphaned boundary edge pairs slated for deletion
    std::vector<int64_t> orphaned;
    for (const HalfEdgeHandle heh : faceLoop)
    {
        const HalfEdgeHandle twin = m_halfEdges[heh.index].twin;
        if (twin.index >= 0 && m_halfEdges[twin.index].fh.index == -1)
        {
            orphaned.push_back(heh.index);
            orphaned.push_back(twin.index);
        }
    }

    // Sort descending and drop uniqueness duplicates to prepare for orderly
    // deletions
    std::sort(orphaned.begin(), orphaned.end(), std::greater<int64_t>());
    orphaned.erase(std::unique(orphaned.begin(), orphaned.end()),
                   orphaned.end());

    // 3. Safely repoint vertices away from orphaned edges (while vertex fan
    // links are intact)
    for (int64_t candidate : orphaned)
    {
        int64_t src = sourceVertexIndex(HalfEdgeHandle{candidate});
        if (src >= 0 && src < static_cast<int64_t>(m_vertices.size()) &&
            m_vertices[src].heh.index == candidate)
        {
            HalfEdgeHandle currentHE = HalfEdgeHandle{candidate};
            HalfEdgeHandle alt{-1};
            const std::size_t maxStepsV = m_halfEdges.size() + 1;
            std::size_t guardV = 0;
            do
            {
                const HalfEdgeHandle incoming =
                    m_halfEdges[currentHE.index].twin;
                if (incoming.index < 0 ||
                    incoming.index >= static_cast<int64_t>(m_halfEdges.size()))
                    break;
                currentHE = m_halfEdges[incoming.index].next;
                if (currentHE.index < 0 ||
                    currentHE.index >= static_cast<int64_t>(m_halfEdges.size()))
                    break;

                // If this outgoing half-edge candidate is NOT in the deletion
                // list, it is safe to use!
                if (!std::binary_search(orphaned.rbegin(), orphaned.rend(),
                                        currentHE.index))
                {
                    alt = currentHE;
                    break;
                }
            } while (currentHE.index != candidate && ++guardV < maxStepsV);

            m_vertices[src].heh = alt;
        }
    }

    // 4. Safely splice out pairs of opposing boundary edges (merging the mesh
    // holes securely)
    for (const HalfEdgeHandle heh : faceLoop)
    {
        const HalfEdgeHandle twin = m_halfEdges[heh.index].twin;
        if (twin.index >= 0 && m_halfEdges[twin.index].fh.index == -1)
        {
            // Both heh and twin are boundary edges. Splice them out perfectly
            // maintaining doubly-linked symmetry!
            HalfEdgeHandle heh_prev = m_halfEdges[heh.index].prev;
            HalfEdgeHandle heh_next = m_halfEdges[heh.index].next;
            HalfEdgeHandle T_prev = m_halfEdges[twin.index].prev;
            HalfEdgeHandle T_next = m_halfEdges[twin.index].next;

            if (heh_prev.index >= 0)
                m_halfEdges[heh_prev.index].next = T_next;
            if (T_next.index >= 0)
                m_halfEdges[T_next.index].prev = heh_prev;

            if (T_prev.index >= 0)
                m_halfEdges[T_prev.index].next = heh_next;
            if (heh_next.index >= 0)
                m_halfEdges[heh_next.index].prev = T_prev;

            // Isolate them completely to prevent any tangled links during
            // swap-delete
            m_halfEdges[heh.index].next = HalfEdgeHandle{-1};
            m_halfEdges[heh.index].prev = HalfEdgeHandle{-1};
            m_halfEdges[twin.index].next = HalfEdgeHandle{-1};
            m_halfEdges[twin.index].prev = HalfEdgeHandle{-1};
        }
    }

    // 5. Safely swap-delete the strictly isolated orphaned half-edges
    // (descending index order)
    for (int64_t candidate : orphaned)
    {
        removeHalfEdge(candidate);
    }

    // 6. Delete the face itself
    removeFace(fh.index);
}

bool HalfEdgeTable::canCollapse(HalfEdgeHandle heh) const
{
    const int64_t heTotal = static_cast<int64_t>(m_halfEdges.size());
    if (heh.index < 0 || heh.index >= heTotal) return false;
    
    int64_t v_src = sourceVertexIndex(heh);
    int64_t v_dst = m_halfEdges[heh.index].dst.index;
    const int64_t vTotal = static_cast<int64_t>(m_vertices.size());
    if (v_src < 0 || v_src >= vTotal || v_dst < 0 || v_dst >= vTotal) return false;
    if (v_src == v_dst) return false;

    // Both faces must be triangles if they exist
    if (m_halfEdges[heh.index].fh.index != -1) {
        int64_t n1 = m_halfEdges[heh.index].next.index;
        if (n1 < 0 || n1 >= heTotal) return false;
        int64_t n2 = m_halfEdges[n1].next.index;
        if (n2 < 0 || n2 >= heTotal) return false;
        if (n2 != m_halfEdges[heh.index].prev.index) return false;
    }
    if (m_halfEdges[heh.index].twin.index != -1) {
        HalfEdgeHandle tw = m_halfEdges[heh.index].twin;
        if (tw.index < 0 || tw.index >= heTotal) return false;
        if (m_halfEdges[tw.index].fh.index != -1) {
            int64_t tn1 = m_halfEdges[tw.index].next.index;
            if (tn1 < 0 || tn1 >= heTotal) return false;
            int64_t tn2 = m_halfEdges[tn1].next.index;
            if (tn2 < 0 || tn2 >= heTotal) return false;
            if (tn2 != m_halfEdges[tw.index].prev.index) return false;
        }
    }

    // Link condition: count shared vertices in 1-ring without heap allocations
    int64_t ring_src[64];
    int ring_src_count = 0;
    {
        HalfEdgeHandle start = m_vertices[v_src].heh;
        if (start.index != -1) {
            HalfEdgeHandle curr = start;
            int guard = 0;
            do {
                if (curr.index < 0 || curr.index >= heTotal) break;
                int64_t d = m_halfEdges[curr.index].dst.index;
                if (d >= 0 && ring_src_count < 64) {
                    ring_src[ring_src_count++] = d;
                }
                HalfEdgeHandle t = m_halfEdges[curr.index].twin;
                if (t.index < 0 || t.index >= heTotal) break;
                curr = m_halfEdges[t.index].next;
            } while (curr != start && curr.index != -1 && ++guard < 64);
        }
    }

    int64_t ring_dst[64];
    int ring_dst_count = 0;
    {
        HalfEdgeHandle start = m_vertices[v_dst].heh;
        if (start.index != -1) {
            HalfEdgeHandle curr = start;
            int guard = 0;
            do {
                if (curr.index < 0 || curr.index >= heTotal) break;
                int64_t d = m_halfEdges[curr.index].dst.index;
                if (d >= 0 && ring_dst_count < 64) {
                    ring_dst[ring_dst_count++] = d;
                }
                HalfEdgeHandle t = m_halfEdges[curr.index].twin;
                if (t.index < 0 || t.index >= heTotal) break;
                curr = m_halfEdges[t.index].next;
            } while (curr != start && curr.index != -1 && ++guard < 64);
        }
    }

    int sharedCount = 0;
    for (int i = 0; i < ring_src_count; ++i) {
        for (int j = 0; j < ring_dst_count; ++j) {
            if (ring_src[i] == ring_dst[j]) {
                ++sharedCount;
                break;
            }
        }
    }

    int facesCount = 0;
    if (m_halfEdges[heh.index].fh.index != -1) facesCount++;
    if (m_halfEdges[heh.index].twin.index != -1 &&
        m_halfEdges[m_halfEdges[heh.index].twin.index].fh.index != -1) facesCount++;

    return sharedCount == facesCount;
}

bool HalfEdgeTable::collapseEdge(HalfEdgeHandle heh)
{
    if (!canCollapse(heh)) return false;

    const int64_t heTotal = static_cast<int64_t>(m_halfEdges.size());
    auto safeHE = [&](int64_t idx) -> bool {
        return idx >= 0 && idx < heTotal;
    };

    HalfEdgeHandle h  = heh;
    HalfEdgeHandle ht = safeHE(m_halfEdges[h.index].twin.index)
                            ? m_halfEdges[h.index].twin
                            : HalfEdgeHandle{-1};

    const int64_t v_src = sourceVertexIndex(h);
    const int64_t v_dst = m_halfEdges[h.index].dst.index;
    if (v_src < 0 || v_dst < 0) return false;

    // Collect faces and edges to delete
    int64_t facesToDelete[2] = {-1, -1};
    int facesToDeleteCount = 0;

    int64_t edgesToDelete[6];
    int edgesToDeleteCount = 0;

    auto addEdgeToDelete = [&](int64_t ei) {
        if (!safeHE(ei)) return;
        for (int i = 0; i < edgesToDeleteCount; ++i) {
            if (edgesToDelete[i] == ei) return;
        }
        if (edgesToDeleteCount < 6) edgesToDelete[edgesToDeleteCount++] = ei;
    };

    addEdgeToDelete(h.index);
    if (ht.index != -1) addEdgeToDelete(ht.index);

    // Process face on h side
    if (m_halfEdges[h.index].fh.index != -1) {
        int64_t f1 = m_halfEdges[h.index].fh.index;
        facesToDelete[facesToDeleteCount++] = f1;

        int64_t h_next = m_halfEdges[h.index].next.index;
        int64_t h_prev = m_halfEdges[h.index].prev.index;
        addEdgeToDelete(h_next);
        addEdgeToDelete(h_prev);

        if (safeHE(h_next) && safeHE(h_prev)) {
            int64_t t_next = m_halfEdges[h_next].twin.index;
            int64_t t_prev = m_halfEdges[h_prev].twin.index;
            if (safeHE(t_next)) m_halfEdges[t_next].twin.index = t_prev;
            if (safeHE(t_prev)) m_halfEdges[t_prev].twin.index = t_next;

            // Opposite vertex on face 1
            int64_t v_opp1 = m_halfEdges[h_next].dst.index;
            if (v_opp1 >= 0 && v_opp1 < static_cast<int64_t>(m_vertices.size())) {
                if (m_vertices[v_opp1].heh.index == h_prev || m_vertices[v_opp1].heh.index == h_next) {
                    m_vertices[v_opp1].heh.index = safeHE(t_next) ? t_next : (safeHE(t_prev) ? t_prev : -1);
                }
            }
        }
    } else {
        // Boundary splicing for h
        int64_t ni = m_halfEdges[h.index].next.index;
        int64_t pi = m_halfEdges[h.index].prev.index;
        if (safeHE(ni)) m_halfEdges[ni].prev.index = pi;
        if (safeHE(pi)) m_halfEdges[pi].next.index = ni;
    }

    // Process face on ht side
    if (ht.index != -1 && m_halfEdges[ht.index].fh.index != -1) {
        int64_t f2 = m_halfEdges[ht.index].fh.index;
        facesToDelete[facesToDeleteCount++] = f2;

        int64_t ht_next = m_halfEdges[ht.index].next.index;
        int64_t ht_prev = m_halfEdges[ht.index].prev.index;
        addEdgeToDelete(ht_next);
        addEdgeToDelete(ht_prev);

        if (safeHE(ht_next) && safeHE(ht_prev)) {
            int64_t tt_next = m_halfEdges[ht_next].twin.index;
            int64_t tt_prev = m_halfEdges[ht_prev].twin.index;
            if (safeHE(tt_next)) m_halfEdges[tt_next].twin.index = tt_prev;
            if (safeHE(tt_prev)) m_halfEdges[tt_prev].twin.index = tt_next;

            // Opposite vertex on face 2
            int64_t v_opp2 = m_halfEdges[ht_next].dst.index;
            if (v_opp2 >= 0 && v_opp2 < static_cast<int64_t>(m_vertices.size())) {
                if (m_vertices[v_opp2].heh.index == ht_prev || m_vertices[v_opp2].heh.index == ht_next) {
                    m_vertices[v_opp2].heh.index = safeHE(tt_next) ? tt_next : (safeHE(tt_prev) ? tt_prev : -1);
                }
            }
        }
    } else if (ht.index != -1) {
        // Boundary splicing for ht
        int64_t ni = m_halfEdges[ht.index].next.index;
        int64_t pi = m_halfEdges[ht.index].prev.index;
        if (safeHE(ni)) m_halfEdges[ni].prev.index = pi;
        if (safeHE(pi)) m_halfEdges[pi].next.index = ni;
    }

    // ── Local re-routing of incoming edges around v_src to point to v_dst in O(valence) ──
    {
        HalfEdgeHandle start = m_vertices[v_src].heh;
        if (start.index != -1) {
            HalfEdgeHandle curr = start;
            int guard = 0;
            do {
                if (curr.index < 0 || curr.index >= heTotal) break;
                HalfEdgeHandle tw = m_halfEdges[curr.index].twin;
                if (tw.index >= 0 && tw.index < heTotal) {
                    if (m_halfEdges[tw.index].dst.index == v_src) {
                        m_halfEdges[tw.index].dst.index = v_dst;
                    }
                    curr = m_halfEdges[tw.index].next;
                } else {
                    break;
                }
            } while (curr != start && curr.index != -1 && ++guard < 256);
        }
    }

    // Midpoint collapse position
    if (v_src < static_cast<int64_t>(m_positions.size()) && v_dst < static_cast<int64_t>(m_positions.size())) {
        m_positions[v_dst] = (m_positions[v_src] + m_positions[v_dst]) * 0.5f;
    }

    // Update v_src and v_dst vertex handles
    m_vertices[v_src].heh.index = -1;

    auto isDeleting = [&](int64_t ei) {
        for (int i = 0; i < edgesToDeleteCount; ++i)
            if (edgesToDelete[i] == ei) return true;
        return false;
    };

    if (isDeleting(m_vertices[v_dst].heh.index)) {
        HalfEdgeHandle alt = findAlternativeOutgoingHalfEdge(m_vertices[v_dst].heh);
        if (alt.index == -1 || isDeleting(alt.index)) {
            // Find any valid outgoing edge around v_dst locally
            m_vertices[v_dst].heh.index = -1;
            HalfEdgeHandle start_dst = h;
            if (ht.index != -1) start_dst = ht;
            HalfEdgeHandle alt_candidate = findAlternativeOutgoingHalfEdge(start_dst);
            if (alt_candidate.index != -1 && !isDeleting(alt_candidate.index)) {
                m_vertices[v_dst].heh = alt_candidate;
            }
        } else {
            m_vertices[v_dst].heh = alt;
        }
    }

    // ── Delete faces in descending order ──
    if (facesToDeleteCount == 2 && facesToDelete[0] < facesToDelete[1]) {
        std::swap(facesToDelete[0], facesToDelete[1]);
    }
    for (int i = 0; i < facesToDeleteCount; ++i) {
        if (facesToDelete[i] >= 0) removeFace(facesToDelete[i]);
    }

    // ── Delete half-edges in descending order ──
    std::sort(edgesToDelete, edgesToDelete + edgesToDeleteCount, std::greater<int64_t>());
    for (int i = 0; i < edgesToDeleteCount; ++i) {
        if (edgesToDelete[i] < static_cast<int64_t>(m_halfEdges.size())) {
            removeHalfEdge(edgesToDelete[i]);
        }
    }

    return true;
}


void HalfEdgeTable::validateTopology() const
{
    const int64_t heCount = static_cast<int64_t>(m_halfEdges.size());
    const int64_t vCount = static_cast<int64_t>(m_vertices.size());
    const int64_t fCount = static_cast<int64_t>(m_faces.size());

    for (int64_t i = 0; i < heCount; ++i)
    {
        const HalfEdge &he = m_halfEdges[i];

        if (he.next.index < 0 || he.next.index >= heCount ||
            he.prev.index < 0 || he.prev.index >= heCount)
        {
            std::cerr << "[HalfEdge] Invalid next/prev at half-edge " << i
                      << " (fh=" << he.fh.index << ", dst=" << he.dst.index
                      << ", twin=" << he.twin.index
                      << ", next=" << he.next.index
                      << ", prev=" << he.prev.index << ")" << std::endl;
            return;
        }

        if (he.twin.index < 0 || he.twin.index >= heCount)
        {
            std::cerr << "[HalfEdge] Invalid twin at half-edge " << i
                      << " (fh=" << he.fh.index << ", dst=" << he.dst.index
                      << ", twin=" << he.twin.index
                      << ", next=" << he.next.index
                      << ", prev=" << he.prev.index << ")" << std::endl;
            return;
        }

        const HalfEdge &twin = m_halfEdges[he.twin.index];
        if (twin.twin.index != i)
        {
            std::cerr << "[HalfEdge] Twin symmetry broken at half-edge " << i
                      << " (twin=" << he.twin.index
                      << ", twin.twin=" << twin.twin.index << ")" << std::endl;
            return;
        }

        if (he.dst.index < 0 || he.dst.index >= vCount)
        {
            std::cerr << "[HalfEdge] Invalid destination vertex at half-edge "
                      << i << " (dst=" << he.dst.index << ")" << std::endl;
            return;
        }

        if (he.fh.index != -1 && (he.fh.index < 0 || he.fh.index >= fCount))
        {
            std::cerr << "[HalfEdge] Invalid face handle at half-edge " << i
                      << " (fh=" << he.fh.index << ")" << std::endl;
            return;
        }
    }

    for (int64_t i = 0; i < vCount; ++i)
    {
        const HalfEdgeHandle heh = m_vertices[i].heh;
        if (heh.index != -1 && (heh.index < 0 || heh.index >= heCount))
        {
            std::cerr << "[HalfEdge] Invalid outgoing half-edge at vertex " << i
                      << std::endl;
            return;
        }
    }

    for (int64_t i = 0; i < fCount; ++i)
    {
        const HalfEdgeHandle heh = m_faces[i].heh;
        if (heh.index != -1 && (heh.index < 0 || heh.index >= heCount))
        {
            std::cerr << "[HalfEdge] Invalid face half-edge for face " << i
                      << std::endl;
            return;
        }
    }

    return;
}