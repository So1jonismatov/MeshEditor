#include "GeometryBuffers.h"
#include "Geometry.h"
#include <algorithm>

GeometryBuffers::GeometryBuffers(const Geometry* geometry) : m_geometry(geometry)
{
}

void GeometryBuffers::markSoupDirty()
{
    m_triangleSoupDirty = true;
    m_fullUploadNeeded = true;
}

void GeometryBuffers::markBBoxDirty()
{
    // Now handled directly in Geometry, but we can keep it here if needed or just empty it out
}

void GeometryBuffers::markFaceDirty(int64_t faceIndex)
{
    if (faceIndex >= 0)
    {
        m_dirtyFaces.insert(faceIndex);
    }
}

void GeometryBuffers::requestFullUpload()
{
    m_fullUploadNeeded = true;
}

void GeometryBuffers::clearDirtyFaces()
{
    m_dirtyFaces.clear();
}

bool GeometryBuffers::isSoupDirty() const
{
    return m_triangleSoupDirty;
}

void GeometryBuffers::clearRedFaces()
{
    m_redFaces.clear();
}

void GeometryBuffers::clearState()
{
    m_triangleSoupDirty = true;
    m_fullUploadNeeded = true;
    m_dirtyFaces.clear();
    m_redFaces.clear();
    m_boundaryFacesHighlighted = false;
}

void GeometryBuffers::removeFaceOffsetMapEntry(int64_t index)
{
    m_faceOffsetMap.erase(index);
}

size_t GeometryBuffers::getIndexCount() const
{
    if (m_triangleSoupDirty || m_faceOffsetMap.empty())
        updateTriangleSoup();
    return m_triangleIndices.size();
}

FaceHandle GeometryBuffers::faceForTriangle(size_t triangleIndex) const
{
    if (m_triangleSoupDirty || m_faceOffsetMap.empty())
        updateTriangleSoup();
    if (triangleIndex >= m_triangleToFace.size())
        return FaceHandle{-1};
    return FaceHandle{m_triangleToFace[triangleIndex]};
}

void GeometryBuffers::paintBoundaryFaces()
{
    const std::unordered_set<int64_t> previousRedFaces = m_redFaces;
    m_redFaces.clear();

    if (!m_boundaryFacesHighlighted)
    {
        const auto &halfEdges = m_geometry->getHalfEdgeTable().getHalfEdges();
        for (const auto &he : halfEdges)
        {
            if (he.fh.index == -1 && he.twin.index != -1)
            {
                const auto &twinHE = m_geometry->getHalfEdgeTable().deref(he.twin);
                if (twinHE.fh.index != -1)
                {
                    m_redFaces.insert(twinHE.fh.index);
                }
            }
        }
    }

    m_boundaryFacesHighlighted = !m_boundaryFacesHighlighted;

    for (int64_t faceIndex : previousRedFaces)
    {
        if (faceIndex >= 0)
            m_dirtyFaces.insert(faceIndex);
    }
    for (int64_t faceIndex : m_redFaces)
    {
        if (faceIndex >= 0)
            m_dirtyFaces.insert(faceIndex);
    }
}

std::vector<glm::vec3> GeometryBuffers::computeSmoothNormals(const HalfEdgeTable &het)
{
    const auto &positions = het.getPositions();
    const auto &faces = het.getFaces();
    const auto &halfEdges = het.getHalfEdges();
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());

    std::vector<glm::vec3> normals(posSize, glm::vec3(0.0f));

    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (faces[i].heh.index == -1)
            continue;

        HalfEdgeHandle start = faces[i].heh;
        HalfEdgeHandle curr = start;
        std::vector<int64_t> polyVIndices;
        polyVIndices.reserve(4);
        int guard = 0;
        do
        {
            if (curr.index < 0 || curr.index >= heSize)
                break;
            const HalfEdge &he = halfEdges[curr.index];
            if (he.dst.index >= 0 && he.dst.index < posSize)
            {
                polyVIndices.push_back(he.dst.index);
            }
            curr = he.next;
        } while (curr != start && curr.index != -1 && ++guard < 64);

        if (polyVIndices.size() < 3)
            continue;

        for (size_t j = 1; j + 1 < polyVIndices.size(); ++j)
        {
            const glm::vec3 &p0 = positions[polyVIndices[0]];
            const glm::vec3 &p1 = positions[polyVIndices[j]];
            const glm::vec3 &p2 = positions[polyVIndices[j + 1]];
            glm::vec3 crossNorm = glm::cross(p1 - p0, p2 - p0);
            normals[polyVIndices[0]] += crossNorm;
            normals[polyVIndices[j]] += crossNorm;
            normals[polyVIndices[j + 1]] += crossNorm;
        }
    }

    for (size_t i = 0; i < normals.size(); ++i)
    {
        float lenSq = glm::dot(normals[i], normals[i]);
        if (lenSq > 1e-12f)
            normals[i] /= std::sqrt(lenSq);
        else
            normals[i] = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    return normals;
}

GeometryBuffers::FaceRenderData GeometryBuffers::buildFaceRenderData(
    FaceHandle fh, glm::vec3 color,
    const std::vector<glm::vec3> &smoothNormals) const
{
    FaceRenderData data;
    if (fh.index < 0 ||
        fh.index >= static_cast<int64_t>(m_geometry->getHalfEdgeTable().getFaces().size()))
        return data;

    const auto &het = m_geometry->getHalfEdgeTable();
    const auto &faces = het.getFaces();
    const auto &halfEdges = het.getHalfEdges();
    const auto &positions = het.getPositions();
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());

    const Face &face = faces[fh.index];
    if (face.heh.index < 0)
        return data;

    HalfEdgeHandle start = face.heh;
    HalfEdgeHandle curr = start;
    std::vector<glm::vec3> polygon;
    std::vector<int64_t> polyVIndices;
    polygon.reserve(4);
    polyVIndices.reserve(4);
    int guard = 0;
    do
    {
        if (curr.index < 0 || curr.index >= heSize)
            break;
        const HalfEdge &he = halfEdges[curr.index];
        if (he.dst.index >= 0 && he.dst.index < posSize)
        {
            polygon.push_back(positions[he.dst.index]);
            polyVIndices.push_back(he.dst.index);
        }
        curr = he.next;
    } while (curr != start && curr.index != -1 && ++guard < 64);

    if (polygon.size() < 3)
        return data;

    glm::vec3 faceNorm =
        glm::cross(polygon[1] - polygon[0], polygon[2] - polygon[0]);
    if (glm::dot(faceNorm, faceNorm) > 0.0f)
        faceNorm = glm::normalize(faceNorm);
    else
        faceNorm = glm::vec3(0.0f, 0.0f, 1.0f);

    const std::vector<glm::vec2> uvs = collectFaceUVs(fh);
    auto uvAt = [&](std::size_t i) -> glm::vec2
    { return i < uvs.size() ? uvs[i] : glm::vec2(0.0f); };

    std::vector<glm::vec3> polyNorms;
    std::vector<glm::vec3> polyColors;
    std::vector<glm::vec4> polyTangents;
    if (het.hasCustomNormals() || het.hasCustomColors() || het.hasCustomTangents()) {
        HalfEdgeHandle curr2 = start;
        int guard2 = 0;
        do {
            if (curr2.index < 0 || curr2.index >= heSize) break;
            if (het.hasCustomNormals()) polyNorms.push_back(het.getNormal(curr2));
            if (het.hasCustomColors()) polyColors.push_back(het.getColor(curr2));
            if (het.hasCustomTangents()) polyTangents.push_back(het.getTangent(curr2));
            curr2 = halfEdges[curr2.index].next;
        } while (curr2 != start && curr2.index != -1 && ++guard2 < 64);
    }

    data.triangleSoup.reserve((polygon.size() - 2) * 3);
    data.indexedSoup.reserve(polygon.size());
    data.indices.reserve((polygon.size() - 2) * 3);

    auto getVNormal = [&](std::size_t i) -> glm::vec3 {
        if (i < polyNorms.size() && glm::dot(polyNorms[i], polyNorms[i]) > 0.01f) return polyNorms[i];
        if (i < polyVIndices.size() && polyVIndices[i] >= 0 && polyVIndices[i] < static_cast<int64_t>(smoothNormals.size())) return smoothNormals[polyVIndices[i]];
        return faceNorm;
    };
    
    auto getVColor = [&](std::size_t i) -> glm::vec3 {
        return (i < polyColors.size()) ? (color * polyColors[i]) : color;
    };
    
    auto getVTangent = [&](std::size_t i) -> glm::vec4 {
        return (i < polyTangents.size()) ? polyTangents[i] : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    };

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        data.indexedSoup.push_back(Vertex{polygon[i], getVNormal(i), getVColor(i), uvAt(i), getVTangent(i)});
    }

    for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
    {
        data.triangleSoup.push_back(Vertex{polygon[0], getVNormal(0), getVColor(0), uvAt(0), getVTangent(0)});
        data.triangleSoup.push_back(Vertex{polygon[i], getVNormal(i), getVColor(i), uvAt(i), getVTangent(i)});
        data.triangleSoup.push_back(Vertex{polygon[i + 1], getVNormal(i + 1), getVColor(i + 1), uvAt(i + 1), getVTangent(i + 1)});
        data.indices.push_back(0u);
        data.indices.push_back(static_cast<unsigned int>(i));
        data.indices.push_back(static_cast<unsigned int>(i + 1));
    }

    return data;
}

std::vector<glm::vec2> GeometryBuffers::collectFaceUVs(FaceHandle fh) const
{
    std::vector<glm::vec2> uvs;
    if (fh.index < 0 ||
        fh.index >= static_cast<int64_t>(m_geometry->getHalfEdgeTable().getFaces().size()))
        return uvs;

    const Face &face = m_geometry->getHalfEdgeTable().getFaces()[fh.index];
    if (face.heh.index < 0)
        return uvs;

    const auto &halfEdges = m_geometry->getHalfEdgeTable().getHalfEdges();
    HalfEdgeHandle start = face.heh;
    HalfEdgeHandle current = start;
    std::size_t guard = 0;
    const std::size_t maxSteps = halfEdges.size() + 1;

    do
    {
        if (current.index < 0 ||
            current.index >= static_cast<int64_t>(halfEdges.size()) ||
            ++guard > maxSteps)
        {
            uvs.clear();
            break;
        }
        uvs.push_back(m_geometry->getHalfEdgeTable().getUV(current));
        current = m_geometry->getHalfEdgeTable().next(current);
    } while (current != start && current.index != -1);

    return uvs;
}

void GeometryBuffers::updateTriangleSoup() const
{
    m_triangleSoup.clear();
    m_indexedSoup.clear();
    m_triangleIndices.clear();
    m_faceOffsetMap.clear();
    m_triangleToFace.clear();

    const glm::vec3 defaultColor(1.0f);
    const auto &het = m_geometry->getHalfEdgeTable();
    const std::vector<glm::vec3> smoothNormals = computeSmoothNormals(het);

    for (size_t i = 0; i < het.getFaces().size(); ++i)
    {
        FaceHandle fh{(int64_t)i};
        const Face &face = het.deref(fh);
        if (face.heh.index == -1)
            continue;

        glm::vec3 color = defaultColor;
        if (m_redFaces.count(i))
            color = glm::vec3(1.0f, 0.0f, 0.0f);

        FaceRenderData faceData = buildFaceRenderData(fh, color, smoothNormals);
        if (faceData.triangleSoup.empty())
            continue;

        const size_t triangleStart = m_triangleSoup.size();
        const size_t indexedStart = m_indexedSoup.size();
        const size_t indexStart = m_triangleIndices.size();

        m_triangleSoup.insert(m_triangleSoup.end(),
                              faceData.triangleSoup.begin(),
                              faceData.triangleSoup.end());
        m_indexedSoup.insert(m_indexedSoup.end(), faceData.indexedSoup.begin(),
                             faceData.indexedSoup.end());

        for (unsigned int index : faceData.indices)
        {
            m_triangleIndices.push_back(
                index + static_cast<unsigned int>(indexedStart));
        }

        m_triangleToFace.insert(m_triangleToFace.end(),
                                faceData.indices.size() / 3,
                                static_cast<int64_t>(i));

        m_faceOffsetMap[static_cast<int64_t>(i)] = {
            triangleStart, faceData.triangleSoup.size(),
            indexedStart,  faceData.indexedSoup.size(),
            indexStart,    faceData.indices.size()};
    }

    m_triangleSoupDirty = false;
    m_dirtyFaces.clear();
    m_fullUploadNeeded = true;
}

void GeometryBuffers::markPositionsDirty()
{
    // If soup was never built, fall through to a full build instead.
    if (m_triangleSoupDirty || m_faceOffsetMap.empty())
    {
        return;
    }

    const auto& het = m_geometry->getHalfEdgeTable();
    const auto& faces = het.getFaces();
    const auto& halfEdges = het.getHalfEdges();
    const auto& positions = het.getPositions();
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());

    const std::vector<glm::vec3> smoothNormals = computeSmoothNormals(het);

    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (faces[i].heh.index == -1) continue;

        auto it = m_faceOffsetMap.find(static_cast<int64_t>(i));
        if (it == m_faceOffsetMap.end()) continue;

        const FaceRegion& region = it->second;

        HalfEdgeHandle start = faces[i].heh;
        HalfEdgeHandle curr = start;
        std::vector<glm::vec3> polyPts;
        std::vector<glm::vec3> polyNorms;
        std::vector<int64_t> polyVIndices;
        polyPts.reserve(region.indexedVertexCount);
        polyNorms.reserve(region.indexedVertexCount);
        polyVIndices.reserve(region.indexedVertexCount);
        int guard = 0;
        do {
            if (curr.index < 0 || curr.index >= heSize) break;
            const HalfEdge &he = halfEdges[curr.index];
            if (he.dst.index >= 0 && he.dst.index < posSize) {
                polyPts.push_back(positions[he.dst.index]);
                polyVIndices.push_back(he.dst.index);
            }
            if (het.hasCustomNormals()) polyNorms.push_back(het.getNormal(curr));
            curr = he.next;
        } while (curr != start && curr.index != -1 && ++guard < 64);

        if (polyPts.size() < 3 || polyPts.size() != region.indexedVertexCount) continue;

        glm::vec3 faceNorm = glm::cross(polyPts[1] - polyPts[0], polyPts[2] - polyPts[0]);
        float lenSq = glm::dot(faceNorm, faceNorm);
        if (lenSq > 0.0f) faceNorm /= std::sqrt(lenSq);
        else faceNorm = glm::vec3(0.0f, 0.0f, 1.0f);

        for (size_t j = 0; j < polyPts.size(); ++j)
        {
            Vertex& v = m_indexedSoup[region.indexedStart + j];
            v.position = polyPts[j];
            
            glm::vec3 vNormal = faceNorm;
            if (j < polyNorms.size() && glm::dot(polyNorms[j], polyNorms[j]) > 0.01f) {
                vNormal = polyNorms[j];
            } else if (j < polyVIndices.size() && polyVIndices[j] >= 0 && polyVIndices[j] < posSize) {
                vNormal = smoothNormals[polyVIndices[j]];
            }
            v.normal = vNormal;
        }
    }

    m_positionsDirty = true;
}

void GeometryBuffers::syncGpuBuffers(IRenderSystem &rs)
{
    if (m_triangleSoupDirty || m_faceOffsetMap.empty())
    {
        updateTriangleSoup();
        rs.uploadIndexedTriangleSoup(const_cast<Geometry*>(m_geometry), m_indexedSoup, m_triangleIndices);
        m_fullUploadNeeded = false;
        m_dirtyFaces.clear();
        m_positionsDirty = false;
        return;
    }

    if (m_fullUploadNeeded || m_positionsDirty)
    {
        rs.uploadIndexedTriangleSoup(const_cast<Geometry*>(m_geometry), m_indexedSoup, m_triangleIndices);
        m_fullUploadNeeded = false;
        m_dirtyFaces.clear();
        m_positionsDirty = false;
        return;
    }

    if (m_dirtyFaces.empty())
        return;

    const glm::vec3 defaultColor(1.0f);

    for (int64_t fi : m_dirtyFaces)
    {
        auto it = m_faceOffsetMap.find(fi);
        if (it == m_faceOffsetMap.end())
            continue;

        const FaceRegion &region = it->second;

        FaceHandle fh{fi};
        const Face &face = m_geometry->getHalfEdgeTable().deref(fh);
        if (face.heh.index == -1)
        {
            std::vector<Vertex> zeroTriangle(
                region.triangleVertexCount,
                Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0}, {0, 0, 0, 1}});
            std::vector<Vertex> zeroIndexed(
                region.indexedVertexCount,
                Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0}, {0, 0, 0, 1}});
            std::vector<unsigned int> zeroIndices(region.indexCount, 0u);
            std::copy(zeroTriangle.begin(), zeroTriangle.end(),
                      m_triangleSoup.begin() + region.triangleStart);
            std::copy(zeroIndexed.begin(), zeroIndexed.end(),
                      m_indexedSoup.begin() + region.indexedStart);
            std::copy(zeroIndices.begin(), zeroIndices.end(),
                      m_triangleIndices.begin() + region.indexStart);
            rs.updateIndexedTriangleSoupRange(
                const_cast<Geometry*>(m_geometry), zeroIndexed.data(), region.indexedStart,
                region.indexedVertexCount, zeroIndices.data(),
                region.indexStart, region.indexCount);
            continue;
        }

        glm::vec3 color = defaultColor;
        if (m_redFaces.count(fi))
            color = glm::vec3(1.0f, 0.0f, 0.0f);

        FaceRenderData faceData = buildFaceRenderData(fh, color);

        if (faceData.triangleSoup.size() != region.triangleVertexCount ||
            faceData.indexedSoup.size() != region.indexedVertexCount ||
            faceData.indices.size() != region.indexCount)
        {
            updateTriangleSoup();
            rs.uploadIndexedTriangleSoup(const_cast<Geometry*>(m_geometry), m_indexedSoup,
                                         m_triangleIndices);
            m_fullUploadNeeded = false;
            m_dirtyFaces.clear();
            return;
        }

        std::copy(faceData.triangleSoup.begin(), faceData.triangleSoup.end(),
                  m_triangleSoup.begin() + region.triangleStart);
        std::copy(faceData.indexedSoup.begin(), faceData.indexedSoup.end(),
                  m_indexedSoup.begin() + region.indexedStart);

        std::vector<unsigned int> absoluteIndices = faceData.indices;
        for (auto &index : absoluteIndices)
        {
            index += static_cast<unsigned int>(region.indexedStart);
        }

        std::copy(absoluteIndices.begin(), absoluteIndices.end(),
                  m_triangleIndices.begin() + region.indexStart);

        rs.updateIndexedTriangleSoupRange(
            const_cast<Geometry*>(m_geometry), faceData.indexedSoup.data(), region.indexedStart,
            region.indexedVertexCount, absoluteIndices.data(),
            region.indexStart, region.indexCount);
    }

    m_dirtyFaces.clear();
}

void GeometryBuffers::drawIndexed(IRenderSystem &rs, bool blackEdges)
{
    rs.drawIndexedTriangleSoup(const_cast<Geometry*>(m_geometry), m_triangleIndices.size(), blackEdges);
}

PrebuiltBuffers GeometryBuffers::buildBuffersFromHET(const HalfEdgeTable &het, const std::unordered_set<int64_t> &redFaces)
{
    PrebuiltBuffers out;
    const auto &faces = het.getFaces();
    const auto &halfEdges = het.getHalfEdges();
    const auto &positions = het.getPositions();
    const auto &uvs = het.getUVs();

    out.indexedSoup.reserve(faces.size() * 3);
    out.triangleIndices.reserve(faces.size() * 3);
    out.triangleToFace.reserve(faces.size());
    out.faceOffsetMap.reserve(faces.size());

    const glm::vec3 defaultColor(1.0f);
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());
    const int64_t uvSize = static_cast<int64_t>(uvs.size());

    glm::vec3 minPos(0.0f), maxPos(0.0f);
    if (!positions.empty()) {
        minPos = maxPos = positions[0];
        for (const auto &p : positions) {
            minPos = glm::min(minPos, p);
            maxPos = glm::max(maxPos, p);
        }
    }
    glm::vec3 extent = glm::max(maxPos - minPos, glm::vec3(1e-4f));

    // Use the explicit flag set by setUV() rather than inspecting coordinate
    // values, which fails for valid UVs that are legitimately at origin (0,0).
    bool hasAnyValidUV = het.hasCustomUVs();

    const std::vector<glm::vec3> smoothNormals = computeSmoothNormals(het);

    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (faces[i].heh.index == -1) continue;

        HalfEdgeHandle start = faces[i].heh;
        HalfEdgeHandle curr = start;

        std::vector<glm::vec3> polyPts;
        std::vector<glm::vec2> polyUVs;
        std::vector<glm::vec3> polyNorms;
        std::vector<glm::vec3> polyColors;
        std::vector<glm::vec4> polyTangents;
        std::vector<int64_t> polyVIndices;
        polyPts.reserve(4);
        polyUVs.reserve(4);
        polyNorms.reserve(4);
        polyColors.reserve(4);
        polyTangents.reserve(4);
        polyVIndices.reserve(4);
        int guard = 0;
        do {
            if (curr.index < 0 || curr.index >= heSize) break;
            const HalfEdge &he = halfEdges[curr.index];
            glm::vec2 uv(0.0f);
            if (he.dst.index >= 0 && he.dst.index < posSize) {
                const glm::vec3 &p = positions[he.dst.index];
                polyPts.push_back(p);
                polyVIndices.push_back(he.dst.index);
                uv = glm::vec2((p.x - minPos.x) / extent.x, (p.y - minPos.y) / extent.y);
            }
            if (hasAnyValidUV) {
                polyUVs.push_back(curr.index < uvSize ? uvs[curr.index] : glm::vec2(0.0f));
            } else {
                polyUVs.push_back(uv);
            }
            if (het.hasCustomNormals()) polyNorms.push_back(het.getNormal(curr));
            if (het.hasCustomColors()) polyColors.push_back(het.getColor(curr));
            if (het.hasCustomTangents()) polyTangents.push_back(het.getTangent(curr));
            curr = he.next;
        } while (curr != start && curr.index != -1 && ++guard < 64);

        if (polyPts.size() < 3) continue;

        glm::vec3 faceNorm = glm::cross(polyPts[1] - polyPts[0], polyPts[2] - polyPts[0]);
        float lenSq = glm::dot(faceNorm, faceNorm);
        if (lenSq > 0.0f) faceNorm /= std::sqrt(lenSq);
        else faceNorm = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 color = defaultColor;
        if (redFaces.count(static_cast<int64_t>(i)))
            color = glm::vec3(1.0f, 0.0f, 0.0f);

        const size_t indexedStart = out.indexedSoup.size();
        const size_t indexStart = out.triangleIndices.size();

        for (size_t j = 0; j < polyPts.size(); ++j) {
            glm::vec2 uv = (j < polyUVs.size()) ? polyUVs[j] : glm::vec2(0.0f);
            
            glm::vec3 vNormal = faceNorm;
            if (j < polyNorms.size() && glm::dot(polyNorms[j], polyNorms[j]) > 0.01f) {
                vNormal = polyNorms[j];
            } else if (j < polyVIndices.size() && polyVIndices[j] >= 0 && polyVIndices[j] < posSize) {
                vNormal = smoothNormals[polyVIndices[j]];
            }

            glm::vec3 vColor = color;
            if (j < polyColors.size()) {
                vColor *= polyColors[j];
            }

            glm::vec4 vTangent = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            if (j < polyTangents.size()) {
                vTangent = polyTangents[j];
            }

            out.indexedSoup.push_back(Vertex{polyPts[j], vNormal, vColor, uv, vTangent});
        }

        const size_t triCount = polyPts.size() - 2;
        for (size_t j = 1; j + 1 < polyPts.size(); ++j) {
            out.triangleIndices.push_back(static_cast<unsigned int>(indexedStart));
            out.triangleIndices.push_back(static_cast<unsigned int>(indexedStart + j));
            out.triangleIndices.push_back(static_cast<unsigned int>(indexedStart + j + 1));
        }

        out.triangleToFace.insert(out.triangleToFace.end(), triCount, static_cast<int64_t>(i));

        out.faceOffsetMap[static_cast<int64_t>(i)] = {
            0, 0,
            indexedStart, polyPts.size(),
            indexStart, triCount * 3
        };
    }

    return out;
}

std::vector<Vertex> GeometryBuffers::rebuildIndexedPositions(
    const HalfEdgeTable &het,
    const std::unordered_map<int64_t, FaceRegion> &faceOffsetMap,
    const std::vector<Vertex> &existingIndexedSoup)
{
    std::vector<Vertex> result = existingIndexedSoup;
    const auto &faces = het.getFaces();
    const auto &halfEdges = het.getHalfEdges();
    const auto &positions = het.getPositions();
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());

    const std::vector<glm::vec3> smoothNormals = computeSmoothNormals(het);

    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (faces[i].heh.index == -1) continue;

        auto it = faceOffsetMap.find(static_cast<int64_t>(i));
        if (it == faceOffsetMap.end()) continue;

        const FaceRegion &region = it->second;

        HalfEdgeHandle start = faces[i].heh;
        HalfEdgeHandle curr = start;
        std::vector<glm::vec3> polyPts;
        std::vector<glm::vec3> polyNorms;
        std::vector<int64_t> polyVIndices;
        polyPts.reserve(region.indexedVertexCount);
        polyNorms.reserve(region.indexedVertexCount);
        polyVIndices.reserve(region.indexedVertexCount);
        int guard = 0;
        do {
            if (curr.index < 0 || curr.index >= heSize) break;
            const HalfEdge &he = halfEdges[curr.index];
            if (he.dst.index >= 0 && he.dst.index < posSize) {
                polyPts.push_back(positions[he.dst.index]);
                polyVIndices.push_back(he.dst.index);
            }
            if (het.hasCustomNormals()) polyNorms.push_back(het.getNormal(curr));
            curr = he.next;
        } while (curr != start && curr.index != -1 && ++guard < 64);

        if (polyPts.size() < 3 || polyPts.size() != region.indexedVertexCount) continue;

        glm::vec3 faceNorm = glm::cross(polyPts[1] - polyPts[0], polyPts[2] - polyPts[0]);
        float lenSq = glm::dot(faceNorm, faceNorm);
        if (lenSq > 0.0f) faceNorm /= std::sqrt(lenSq);
        else faceNorm = glm::vec3(0.0f, 0.0f, 1.0f);

        for (size_t j = 0; j < polyPts.size(); ++j)
        {
            if (region.indexedStart + j < result.size())
            {
                result[region.indexedStart + j].position = polyPts[j];
                
                glm::vec3 vNormal = faceNorm;
                if (j < polyNorms.size() && glm::dot(polyNorms[j], polyNorms[j]) > 0.01f) {
                    vNormal = polyNorms[j];
                } else if (j < polyVIndices.size() && polyVIndices[j] >= 0 && polyVIndices[j] < posSize) {
                    vNormal = smoothNormals[polyVIndices[j]];
                }
                result[region.indexedStart + j].normal = vNormal;
            }
        }
    }

    return result;
}

void GeometryBuffers::adoptPrebuiltBuffers(PrebuiltBuffers &&prebuilt)
{
    m_indexedSoup = std::move(prebuilt.indexedSoup);
    m_triangleIndices = std::move(prebuilt.triangleIndices);
    m_triangleToFace = std::move(prebuilt.triangleToFace);
    m_faceOffsetMap = std::move(prebuilt.faceOffsetMap);
    m_triangleSoup.clear();

    m_triangleSoupDirty = false;
    m_fullUploadNeeded = true;
    m_dirtyFaces.clear();
    m_positionsDirty = false;
}

void GeometryBuffers::adoptPrebuiltBuffers(PrebuiltBuffers &&prebuilt, IRenderSystem &rs)
{
    adoptPrebuiltBuffers(std::move(prebuilt));
}

void GeometryBuffers::adoptPrebuiltPositions(std::vector<Vertex> &&indexedSoup, IRenderSystem &rs)
{
    m_indexedSoup = std::move(indexedSoup);
    m_fullUploadNeeded = true;
    m_dirtyFaces.clear();
    m_positionsDirty = false;
}
