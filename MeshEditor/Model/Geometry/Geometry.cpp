#include "Geometry.h"
#include "Application.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <iostream>

Geometry::Geometry(const HalfEdgeTable &halfEdgeTable) : m_het(halfEdgeTable)
{
    m_bboxDirty = true;
    m_octree = std::make_unique<FaceOctree>(this);
    m_buffers = std::make_unique<GeometryBuffers>(this);
}

Geometry::Geometry(HalfEdgeTable &&halfEdgeTable)
    : m_het(std::move(halfEdgeTable))
{
    m_bboxDirty = true;
    m_octree = std::make_unique<FaceOctree>(this);
    m_buffers = std::make_unique<GeometryBuffers>(this);
}

Geometry::~Geometry()
{
    if (Application::getInstance() &&
        Application::getInstance()->getRenderSystem())
    {
        IRenderSystem *rs = Application::getInstance()->getRenderSystem();
        rs->releaseBuffer(this);
        rs->releaseBuffer(&m_holeBoundaryCache);
        rs->releaseBuffer(&m_polylineSoup);
    }
}

const HalfEdgeTable &Geometry::getHalfEdgeTable() const
{
    return m_het;
}

HalfEdgeTable &Geometry::getHalfEdgeTable()
{
    return m_het;
}

void Geometry::setHalfEdgeTable(const HalfEdgeTable &het)
{
    m_het = het;
    markGeometryDirty();
}

void Geometry::setHalfEdgeTable(HalfEdgeTable &&het)
{
    m_het = std::move(het);
    markGeometryDirty();
}

std::vector<glm::vec3> Geometry::collectFacePolygon(FaceHandle fh) const
{
    std::vector<glm::vec3> polygon;
    collectFacePolygon(fh, polygon);
    return polygon;
}

void Geometry::collectFacePolygon(FaceHandle fh,
                                  std::vector<glm::vec3> &polygon) const
{
    polygon.clear();
    if (fh.index < 0 ||
        fh.index >= static_cast<int64_t>(m_het.getFaces().size()))
        return;

    const Face &face = m_het.getFaces()[fh.index];
    if (face.heh.index < 0)
        return;

    const auto &halfEdges = m_het.getHalfEdges();
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
            polygon.clear();
            break;
        }

        const HalfEdge &edge = m_het.deref(current);
        if (edge.dst.index < 0 || 
            edge.dst.index >= static_cast<int64_t>(m_het.getVertices().size()))
        {
            polygon.clear();
            break;
        }

        polygon.push_back(m_het.getPoint(edge.dst));
        current = m_het.next(current);
    } while (current != start && current.index != -1);
}

void Geometry::markGeometryDirty()
{
    m_bboxDirty = true;
    if (m_octree) m_octree->reset();
    if (m_buffers) m_buffers->clearState();
    m_holeBoundaryDirty = true;
    ++m_topologyVersion;
    ++m_geometryVersion;
}

void Geometry::markPositionsDirty()
{
    m_bboxDirty = true;
    m_holeBoundaryDirty = true;
    ++m_geometryVersion;
    if (m_buffers) m_buffers->markPositionsDirty();
}

void Geometry::adoptPrebuilt(HalfEdgeTable &&het,
                             PrebuiltBuffers &&buffers,
                             PrebuiltOctree &&octree)
{
    m_het = std::move(het);
    m_bboxDirty = true;
    m_holeBoundaryDirty = true;
    ++m_topologyVersion;
    ++m_geometryVersion;

    if (m_buffers)
    {
        m_buffers->adoptPrebuiltBuffers(std::move(buffers));
    }
    if (m_octree && octree.root)
    {
        m_octree->adoptPrebuiltOctree(std::move(octree));
    }
}

void Geometry::adoptPrebuilt(HalfEdgeTable &&het,
                             PrebuiltBuffers &&buffers,
                             PrebuiltOctree &&octree,
                             IRenderSystem &rs)
{
    (void)rs;
    adoptPrebuilt(std::move(het), std::move(buffers), std::move(octree));
}

void Geometry::adoptPrebuilt(PrebuiltBuffers &&buffers,
                             PrebuiltOctree &&octree)
{
    m_bboxDirty = true;
    m_holeBoundaryDirty = true;
    ++m_topologyVersion;
    ++m_geometryVersion;

    if (m_buffers)
    {
        m_buffers->adoptPrebuiltBuffers(std::move(buffers));
    }
    if (m_octree && octree.root)
    {
        m_octree->adoptPrebuiltOctree(std::move(octree));
    }
}

void Geometry::adoptPrebuiltPositions(std::vector<glm::vec3> &&positions,
                                      std::vector<Vertex> &&indexedSoup,
                                      IRenderSystem &rs)
{
    m_het.setPositions(std::move(positions));
    m_bboxDirty = true;
    m_holeBoundaryDirty = true;
    ++m_geometryVersion;

    if (m_buffers)
    {
        m_buffers->adoptPrebuiltPositions(std::move(indexedSoup), rs);
    }
}

void Geometry::markFaceDirty(FaceHandle fh)
{
    if (fh.index >= 0)
    {
        if (m_buffers) m_buffers->markFaceDirty(fh.index);
        if (m_octree) m_octree->invalidateFaceInOctree(static_cast<size_t>(fh.index));
    }
    m_bboxDirty = true;
    m_holeBoundaryDirty = true;
    ++m_geometryVersion;
}

void Geometry::markVertexDirty(VertexHandle vh)
{
    glm::vec3 pos = m_het.getPoint(vh);
    m_bbox.min = glm::min(m_bbox.min, pos);
    m_bbox.max = glm::max(m_bbox.max, pos);
    m_holeBoundaryDirty = true;
    ++m_geometryVersion;

    HalfEdgeHandle v_start = m_het.deref(vh).heh;
    if (v_start.index != -1)
    {
        const std::size_t maxSteps = m_het.getHalfEdges().size() + 1;

        HalfEdgeHandle v_curr = v_start;
        std::size_t guard = 0;
        do
        {
            if (m_het.deref(v_curr).fh.index != -1)
            {
                int64_t fIdx = m_het.deref(v_curr).fh.index;
                if (m_buffers) m_buffers->markFaceDirty(fIdx);
                if (m_octree) m_octree->invalidateFaceInOctree(static_cast<size_t>(fIdx));
            }
            HalfEdgeHandle twin = m_het.deref(v_curr).twin;
            if (twin.index == -1)
                break;
            v_curr = m_het.next(twin);
            if (++guard > maxSteps)
                break;
        } while (v_curr != v_start && v_curr.index != -1);

        v_curr = v_start;
        guard = 0;
        do
        {
            HalfEdgeHandle prev = m_het.prev(v_curr);
            if (prev.index == -1)
                break;
            HalfEdgeHandle twin = m_het.deref(prev).twin;
            if (twin.index == -1)
                break;
            v_curr = twin;
            if (v_curr == v_start)
                break;
            if (m_het.deref(v_curr).fh.index != -1)
            {
                int64_t fIdx = m_het.deref(v_curr).fh.index;
                if (m_buffers) m_buffers->markFaceDirty(fIdx);
                if (m_octree) m_octree->invalidateFaceInOctree(static_cast<size_t>(fIdx));
            }
            if (++guard > maxSteps)
                break;
        } while (v_curr != v_start && v_curr.index != -1);
    }
}

void Geometry::applyTransformation(FaceHandle fh, const glm::mat4 &trf)
{
    const auto &faces = m_het.getFaces();
    if (fh.index < 0 || fh.index >= static_cast<int64_t>(faces.size()))
        return;

    const Face &face = faces[fh.index];
    HalfEdgeHandle start_heh = face.heh;
    const auto &halfEdges = m_het.getHalfEdges();
    if (start_heh.index < 0 ||
        start_heh.index >= static_cast<int64_t>(halfEdges.size()))
        return;

    HalfEdgeHandle curr_heh = start_heh;
    do
    {
        if (curr_heh.index < 0 ||
            curr_heh.index >= static_cast<int64_t>(halfEdges.size()))
            return;

        VertexHandle vh = m_het.deref(curr_heh).dst;
        glm::vec3 pos = m_het.getPoint(vh);
        glm::vec4 newPos = trf * glm::vec4(pos, 1.0f);
        m_het.setPoint(vh, glm::vec3(newPos));

        markVertexDirty(vh);

        curr_heh = m_het.next(curr_heh);
    } while (curr_heh != start_heh);
}

void Geometry::setPolylineSegments(const std::vector<glm::vec3> &segments)
{
    m_polylineSoup.clear();
    m_polylineSoup.reserve(segments.size());
    const glm::vec3 lineColor(0.0f, 1.0f, 0.0f); // Bright Green for measurements
    for (const glm::vec3 &p : segments)
        m_polylineSoup.push_back(Vertex{p, glm::vec3(0.0f), lineColor});
    m_polylineUploaded = false;
}

size_t Geometry::getIndexCount() const
{
    if (m_buffers) return m_buffers->getIndexCount();
    return 0;
}

FaceHandle Geometry::faceForTriangle(size_t triangleIndex) const
{
    if (m_buffers) return m_buffers->faceForTriangle(triangleIndex);
    return FaceHandle{-1};
}

void Geometry::deleteFaces(const std::vector<int64_t> &sortedDescending)
{
    const std::vector<int64_t> &sorted = sortedDescending;

    if (!m_buffers) return;

    bool canFastDelete = !m_buffers->isSoupDirty() && m_buffers->getFaceOffsetMap().size() > 0;
    int64_t simulatedFaceCount = static_cast<int64_t>(m_het.getFaces().size());
    for (int64_t faceIndex : sorted)
    {
        const int64_t currentLastIndex = simulatedFaceCount - 1;
        if (faceIndex < 0 || faceIndex >= simulatedFaceCount)
        {
            canFastDelete = false;
            break;
        }

        const auto faceIt = m_buffers->getFaceOffsetMap().find(faceIndex);
        const auto lastIt = m_buffers->getFaceOffsetMap().find(currentLastIndex);
        if (faceIt == m_buffers->getFaceOffsetMap().end() || lastIt == m_buffers->getFaceOffsetMap().end())
        {
            canFastDelete = false;
            break;
        }

        const FaceRegion &faceRegion = faceIt->second;
        const FaceRegion &lastRegion = lastIt->second;
        if (faceRegion.triangleVertexCount != lastRegion.triangleVertexCount ||
            faceRegion.indexedVertexCount != lastRegion.indexedVertexCount ||
            faceRegion.indexCount != lastRegion.indexCount)
        {
            canFastDelete = false;
            break;
        }

        --simulatedFaceCount;
    }

    if (!canFastDelete)
    {
        for (int64_t faceIndex : sorted)
        {
            if (faceIndex >= 0 &&
                faceIndex < static_cast<int64_t>(m_het.getFaces().size()))
            {
                m_het.deleteFaceLocally(FaceHandle{faceIndex});
            }
        }

        m_buffers->clearRedFaces();
        m_buffers->setBoundaryFacesHighlighted(false);
        markGeometryDirty(); 
        return;
    }

    for (int64_t faceIndex : sorted)
    {
        const int64_t currentLastIndex =
            static_cast<int64_t>(m_het.getFaces().size()) - 1;
        auto deletedIt = m_buffers->getFaceOffsetMap().find(faceIndex);
        auto movedIt = m_buffers->getFaceOffsetMap().find(currentLastIndex);
        if (deletedIt == m_buffers->getFaceOffsetMap().end() ||
            movedIt == m_buffers->getFaceOffsetMap().end())
        {
            for (int64_t remainingFace : sorted)
            {
                if (remainingFace >= 0 &&
                    remainingFace <
                        static_cast<int64_t>(m_het.getFaces().size()))
                {
                    m_het.deleteFaceLocally(FaceHandle{remainingFace});
                }
            }

            m_buffers->clearRedFaces();
            m_buffers->setBoundaryFacesHighlighted(false);
            markGeometryDirty();
            return;
        }

        const FaceRegion deletedRegion = deletedIt->second;
        const FaceRegion movedRegion = movedIt->second;

        m_het.deleteFaceLocally(FaceHandle{faceIndex});

        if (m_octree)
        {
            m_octree->removeFaceFromOctreeCache(static_cast<size_t>(faceIndex));
            m_octree->removeLooseFace(static_cast<size_t>(faceIndex));

            if (faceIndex != currentLastIndex)
            {
                m_octree->replaceFaceInOctreeCache(static_cast<size_t>(currentLastIndex),
                                                   static_cast<size_t>(faceIndex));
                if (m_octree->hasLooseFace(static_cast<size_t>(currentLastIndex)))
                {
                    m_octree->removeLooseFace(static_cast<size_t>(currentLastIndex));
                    m_octree->insertLooseFace(static_cast<size_t>(faceIndex));
                }
            }
        }

        if (faceIndex != currentLastIndex)
        {
            const int64_t indexDelta =
                static_cast<int64_t>(deletedRegion.indexedStart) -
                static_cast<int64_t>(movedRegion.indexedStart);

            std::copy(m_buffers->getTriangleSoup().begin() + movedRegion.triangleStart,
                      m_buffers->getTriangleSoup().begin() + movedRegion.triangleStart +
                          movedRegion.triangleVertexCount,
                      m_buffers->getTriangleSoup().begin() + deletedRegion.triangleStart);

            std::copy(m_buffers->getIndexedSoup().begin() + movedRegion.indexedStart,
                      m_buffers->getIndexedSoup().begin() + movedRegion.indexedStart +
                          movedRegion.indexedVertexCount,
                      m_buffers->getIndexedSoup().begin() + deletedRegion.indexedStart);

            for (size_t i = 0; i < movedRegion.indexCount; ++i)
            {
                const int64_t sourceIndex = static_cast<int64_t>(
                    m_buffers->getTriangleIndices()[movedRegion.indexStart + i]);
                m_buffers->getTriangleIndices()[deletedRegion.indexStart + i] =
                    static_cast<unsigned int>(sourceIndex + indexDelta);
            }

            std::fill(m_buffers->getTriangleSoup().begin() + movedRegion.triangleStart,
                      m_buffers->getTriangleSoup().begin() + movedRegion.triangleStart +
                          movedRegion.triangleVertexCount,
                      Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}});
            std::fill(m_buffers->getIndexedSoup().begin() + movedRegion.indexedStart,
                      m_buffers->getIndexedSoup().begin() + movedRegion.indexedStart +
                          movedRegion.indexedVertexCount,
                      Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}});
            std::fill(m_buffers->getTriangleIndices().begin() + movedRegion.indexStart,
                      m_buffers->getTriangleIndices().begin() + movedRegion.indexStart +
                          movedRegion.indexCount,
                      0u);

            std::fill(m_buffers->getTriangleToFace().begin() + deletedRegion.indexStart / 3,
                      m_buffers->getTriangleToFace().begin() +
                          (deletedRegion.indexStart + deletedRegion.indexCount) /
                              3,
                      faceIndex);
            std::fill(m_buffers->getTriangleToFace().begin() + movedRegion.indexStart / 3,
                      m_buffers->getTriangleToFace().begin() +
                          (movedRegion.indexStart + movedRegion.indexCount) / 3,
                      static_cast<int64_t>(-1));
        }
        else
        {
            std::fill(m_buffers->getTriangleSoup().begin() + deletedRegion.triangleStart,
                      m_buffers->getTriangleSoup().begin() + deletedRegion.triangleStart +
                          deletedRegion.triangleVertexCount,
                      Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}});
            std::fill(m_buffers->getIndexedSoup().begin() + deletedRegion.indexedStart,
                      m_buffers->getIndexedSoup().begin() + deletedRegion.indexedStart +
                          deletedRegion.indexedVertexCount,
                      Vertex{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}});
            std::fill(m_buffers->getTriangleIndices().begin() + deletedRegion.indexStart,
                      m_buffers->getTriangleIndices().begin() + deletedRegion.indexStart +
                          deletedRegion.indexCount,
                      0u);

            std::fill(m_buffers->getTriangleToFace().begin() + deletedRegion.indexStart / 3,
                      m_buffers->getTriangleToFace().begin() +
                          (deletedRegion.indexStart + deletedRegion.indexCount) /
                              3,
                      static_cast<int64_t>(-1));
        }

        m_buffers->removeFaceOffsetMapEntry(currentLastIndex);
    }

    m_buffers->clearRedFaces();
    m_buffers->setBoundaryFacesHighlighted(false);
    m_bboxDirty = true;
    m_buffers->requestFullUpload();
    m_buffers->clearDirtyFaces();
    m_holeBoundaryDirty = true; 
    ++m_topologyVersion;        
    ++m_geometryVersion;
}

void Geometry::paintBoundaryFaces()
{
    if (m_buffers) m_buffers->paintBoundaryFaces();
}

void Geometry::moveEvenFaces(glm::vec3 offset)
{
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), offset);
    const auto &faces = m_het.getFaces();
    for (size_t i = 0; i < faces.size(); i += 2)
    {
        if (faces[i].heh.index != -1)
            applyTransformation(FaceHandle{static_cast<int64_t>(i)}, transform);
    }
    flushOctreeLooseFaces();
}

void Geometry::moveOrthogonalFaces(glm::vec3 normal, glm::vec3 offset)
{
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), offset);
    const auto &faces = m_het.getFaces();
    std::unordered_set<int64_t> matchingFaces;
    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (faces[i].heh.index == -1)
            continue;

        std::vector<glm::vec3> pts;
        HalfEdgeHandle start = faces[i].heh;
        HalfEdgeHandle curr = start;
        do
        {
            pts.push_back(m_het.getStartPoint(curr));
            curr = m_het.next(curr);
        } while (curr != start && curr.index != -1);

        if (pts.size() >= 3)
        {
            glm::vec3 n =
                glm::normalize(glm::cross(pts[1] - pts[0], pts[2] - pts[0]));
            if (glm::abs(glm::dot(n, normal)) > 0.999f)
            {
                matchingFaces.insert(static_cast<int64_t>(i));
            }
        }
    }

    if (matchingFaces.empty())
        return;

    for (int64_t faceIndex : matchingFaces)
    {
        applyTransformation(FaceHandle{faceIndex}, transform);
    }
}

void Geometry::updateBoundingBox() const
{
    if (m_het.getVertices().empty())
    {
        m_bbox.min = m_bbox.max = glm::vec3(0.0f);
        return;
    }

    bool first = true;
    const auto &vertices = m_het.getVertices();
    const auto &positions = m_het.getPositions();
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        if (vertices[i].heh.index != -1)
        {
            const glm::vec3 &p = positions[i];
            if (first)
            {
                m_bbox.min = m_bbox.max = p;
                first = false;
            }
            else
            {
                m_bbox.min = glm::min(m_bbox.min, p);
                m_bbox.max = glm::max(m_bbox.max, p);
            }
        }
    }
    if (first)
        m_bbox.min = m_bbox.max = glm::vec3(0.0f);

    m_bboxDirty = false;
}

void Geometry::syncGpuBuffers(IRenderSystem &rs)
{
    if (m_buffers) m_buffers->syncGpuBuffers(rs);
}

void Geometry::drawIndexed(IRenderSystem &rs, bool blackEdges)
{
    if (m_buffers) m_buffers->drawIndexed(rs, blackEdges);
}

void Geometry::drawPolyline(IRenderSystem &rs)
{
    if (m_polylineSoup.empty())
        return;
    if (!m_polylineUploaded)
    {
        rs.uploadTriangleSoup(&m_polylineSoup, m_polylineSoup);
        m_polylineUploaded = true;
    }
    rs.setOverlayMode(true);
    rs.setLineWidth(6.5f); // 6-7 pixels wide for clear measurement visibility
    rs.drawLineBuffer(&m_polylineSoup, m_polylineSoup.size());
    rs.setLineWidth(1.0f); // Restore
    rs.setOverlayMode(false);
}

void Geometry::drawHoleBoundaries(IRenderSystem &rs)
{
    if (m_holeBoundaryDirty)
    {
        rebuildHoleBoundaryCache();
        m_holeBoundaryDirty = false;
        if (!m_holeBoundaryCache.empty())
            rs.uploadTriangleSoup(&m_holeBoundaryCache, m_holeBoundaryCache);
    }
    if (!m_holeBoundaryCache.empty())
    {
        rs.setLineWidth(4.0f);
        rs.drawLineBuffer(&m_holeBoundaryCache, m_holeBoundaryCache.size());
        rs.setLineWidth(1.0f);
    }
}

void Geometry::rebuildHoleBoundaryCache()
{
    m_holeBoundaryCache.clear();

    const auto &halfEdges = m_het.getHalfEdges();
    const auto &vertices = m_het.getVertices();

    std::vector<bool> visited(halfEdges.size(), false);
    int holeIndex = 0;

    const glm::vec3 palette[] = {
        glm::vec3(1.0f, 0.23f, 0.18f),  glm::vec3(0.20f, 0.78f, 0.35f),
        glm::vec3(0.00f, 0.48f, 1.00f), glm::vec3(1.0f, 0.58f, 0.00f),
        glm::vec3(0.69f, 0.32f, 0.87f), glm::vec3(1.0f, 0.80f, 0.00f),
        glm::vec3(0.35f, 0.78f, 0.98f)};
    const int paletteSize = sizeof(palette) / sizeof(palette[0]);

    std::vector<Vertex> &boundaryLines = m_holeBoundaryCache;

    for (size_t i = 0; i < halfEdges.size(); ++i)
    {
        if (halfEdges[i].fh.index == -1 && !visited[i])
        {
            glm::vec3 color = palette[holeIndex % paletteSize];
            holeIndex++;

            HalfEdgeHandle startHh{static_cast<int64_t>(i)};
            HalfEdgeHandle currHh = startHh;

            std::size_t stepGuard = 0;
            const std::size_t maxSteps = halfEdges.size() + 1;

            do
            {
                if (currHh.index < 0 ||
                    currHh.index >= static_cast<int64_t>(halfEdges.size()))
                    break;

                visited[currHh.index] = true;

                const HalfEdge &he = halfEdges[currHh.index];
                VertexHandle vDst = he.dst;

                if (he.twin.index >= 0 &&
                    he.twin.index < static_cast<int64_t>(halfEdges.size()))
                {
                    VertexHandle vSrc = m_het.deref(he.twin).dst;

                    if (vDst.index >= 0 &&
                        vDst.index < (int64_t)vertices.size() &&
                        vSrc.index >= 0 && vSrc.index < (int64_t)vertices.size())
                    {
                        glm::vec3 pDst = m_het.getPoint(vDst);
                        glm::vec3 pSrc = m_het.getPoint(vSrc);

                        glm::vec3 normal(0.0f, 0.0f, 1.0f);
                        const HalfEdge &twinHe = m_het.deref(he.twin);
                        if (twinHe.fh.index != -1)
                        {
                            const Face &face = m_het.deref(twinHe.fh);
                            if (face.heh.index != -1)
                            {
                                HalfEdgeHandle faceCurr = face.heh;
                                glm::vec3 pts[3];
                                int count = 0;
                                do
                                {
                                    if (faceCurr.index < 0 ||
                                        faceCurr.index >=
                                            (int64_t)halfEdges.size())
                                        break;
                                    pts[count++] = m_het.getPoint(
                                        m_het.deref(faceCurr).dst);
                                    faceCurr = m_het.next(faceCurr);
                                } while (faceCurr != face.heh && count < 3);

                                if (count >= 3)
                                {
                                    glm::vec3 cross = glm::cross(
                                        pts[1] - pts[0], pts[2] - pts[0]);
                                    if (glm::dot(cross, cross) > 0.0f)
                                        normal = glm::normalize(cross);
                                }
                            }
                        }

                        glm::vec3 offset = normal * 0.005f;
                        boundaryLines.push_back(
                            Vertex{pSrc + offset, glm::vec3(0.0f), color});
                        boundaryLines.push_back(
                            Vertex{pDst + offset, glm::vec3(0.0f), color});
                    }
                }

                currHh = he.next;
                stepGuard++;
            } while (currHh != startHh && currHh.index != -1 &&
                     !visited[currHh.index] && stepGuard < maxSteps);
        }
    }
}

const bbox &Geometry::getBoundingBox() const
{
    if (m_bboxDirty)
        updateBoundingBox();
    return m_bbox;
}

const std::vector<::Vertex> &Geometry::getTriangleSoup() const
{
    if (m_buffers)
    {
        if (m_buffers->isSoupDirty() || m_buffers->getFaceOffsetMap().empty())
            m_buffers->updateTriangleSoup();
        return m_buffers->getTriangleSoup();
    }
    static std::vector<Vertex> empty;
    return empty;
}

const OctreeNode *Geometry::getFaceOctree() const
{
    if (m_octree) return m_octree->getFaceOctree();
    return nullptr;
}

const std::unordered_set<size_t> &Geometry::getLooseFaces() const
{
    if (m_octree) return m_octree->getLooseFaces();
    static std::unordered_set<size_t> empty;
    return empty;
}

void Geometry::markOctreeDirty()
{
    if (m_octree) m_octree->markOctreeDirty();
}

void Geometry::flushOctreeLooseFaces()
{
    if (m_octree) m_octree->flushOctreeLooseFaces();
}

uint64_t Geometry::geometryVersion() const
{
    return m_geometryVersion;
}
uint64_t Geometry::topologyVersion() const
{
    return m_topologyVersion;
}
