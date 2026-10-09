#include "CreatePrimitives.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>

namespace Utils
{
std::vector<Vertex> buildCoordinateAxes()
{
    const float axisLength = 100.0f;

    return {
        {glm::vec3(-axisLength, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.35f, 0.0f, 0.0f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(1.0f, 0.0f, 0.0f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(1.0f, 0.0f, 0.0f)},
        {glm::vec3(axisLength, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(1.0f, 0.35f, 0.35f)},
        {glm::vec3(0.0f, -axisLength, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.0f, 0.35f, 0.0f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3(0.0f, axisLength, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.35f, 1.0f, 0.35f)},
        {glm::vec3(0.0f, 0.0f, -axisLength), glm::vec3(0.0f),
         glm::vec3(0.0f, 0.0f, 0.35f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f),
         glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(0.0f, 0.0f, axisLength), glm::vec3(0.0f),
         glm::vec3(0.35f, 0.35f, 1.0f)},
    };
}

HalfEdgeTable createCube()
{
    HalfEdgeTable het;

    VertexHandle v000 = het.addVertex({-1.0f, -1.0f, -1.0f});
    VertexHandle v001 = het.addVertex({-1.0f, -1.0f, 1.0f});
    VertexHandle v010 = het.addVertex({-1.0f, 1.0f, -1.0f});
    VertexHandle v011 = het.addVertex({-1.0f, 1.0f, 1.0f});
    VertexHandle v100 = het.addVertex({1.0f, -1.0f, -1.0f});
    VertexHandle v101 = het.addVertex({1.0f, -1.0f, 1.0f});
    VertexHandle v110 = het.addVertex({1.0f, 1.0f, -1.0f});
    VertexHandle v111 = het.addVertex({1.0f, 1.0f, 1.0f});

    het.addFace(v001, v101, v111);
    het.addFace(v001, v111, v011);

    het.addFace(v100, v000, v010);
    het.addFace(v100, v010, v110);

    het.addFace(v000, v001, v011);
    het.addFace(v000, v011, v010);

    het.addFace(v101, v100, v110);
    het.addFace(v101, v110, v111);

    het.addFace(v011, v111, v110);
    het.addFace(v011, v110, v010);

    het.addFace(v000, v100, v101);
    het.addFace(v000, v101, v001);

    het.connectTwins();
    return het;
}

std::vector<glm::mat4> createCubeWallTransforms(unsigned int rows,
                                                unsigned int columns,
                                                float spacing)
{
    std::vector<glm::mat4> transforms;
    transforms.reserve(rows * columns * 2);

    const float rowCenter = (static_cast<float>(rows) - 1.0f) * 0.5f;
    const float columnCenter = (static_cast<float>(columns) - 1.0f) * 0.5f;
    const float wallOffset = spacing * 1.75f;

    for (unsigned int row = 0; row < rows; ++row)
    {
        const float y = (rowCenter - static_cast<float>(row)) * spacing;

        for (unsigned int column = 0; column < columns; ++column)
        {
            const float x =
                (static_cast<float>(column) - columnCenter) * spacing;
            const float scale = 0.4f;

            glm::mat4 transform(1.0f);
            transform = glm::translate(transform, glm::vec3(x, y, -wallOffset));
            transform = glm::scale(transform, glm::vec3(scale));
            transforms.push_back(transform);

            transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(x, y, wallOffset));
            transform = glm::scale(transform, glm::vec3(scale));
            transforms.push_back(transform);
        }
    }

    return transforms;
}

HalfEdgeTable createTessellatedCircle(unsigned int segments, float radius)
{
    if (segments < 3)
        segments = 3;

    HalfEdgeTable het;
    VertexHandle center = het.addVertex({0.0f, 0.0f, 0.0f});

    std::vector<VertexHandle> perimeter;
    for (unsigned int i = 0; i < segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        perimeter.push_back(het.addVertex(
            {std::cos(angle) * radius, std::sin(angle) * radius, 0.0f}));
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        het.addFace(center, perimeter[i], perimeter[(i + 1) % segments]);
    }

    het.connectTwins();
    return het;
}

HalfEdgeTable meshCylinder(double R, double h, uint32_t numSubdivisions)
{
    HalfEdgeTable het;
    if (numSubdivisions < 3)
        numSubdivisions = 3;

    std::vector<VertexHandle> bottomVerts(numSubdivisions);
    std::vector<VertexHandle> topVerts(numSubdivisions);

    VertexHandle bottomCenter = het.addVertex({0.0f, -h / 2.0f, 0.0f});
    VertexHandle topCenter = het.addVertex({0.0f, h / 2.0f, 0.0f});

    for (uint32_t i = 0; i < numSubdivisions; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / numSubdivisions;
        float x = static_cast<float>(R * std::cos(angle));
        float z = static_cast<float>(R * std::sin(angle));

        bottomVerts[i] = het.addVertex({x, -h / 2.0f, z});
        topVerts[i] = het.addVertex({x, h / 2.0f, z});
    }

    for (uint32_t i = 0; i < numSubdivisions; ++i)
    {
        uint32_t next = (i + 1) % numSubdivisions;

        // Side face (quad)
        het.addFace(bottomVerts[i], topVerts[i], topVerts[next],
                    bottomVerts[next]);

        // Bottom face (triangle) - pointing down, so clockwise from top
        het.addFace(bottomCenter, bottomVerts[next], bottomVerts[i]);

        // Top face (triangle) - pointing up, counter-clockwise
        het.addFace(topCenter, topVerts[i], topVerts[next]);
    }

    het.connectTwins();
    return het;
}

HalfEdgeTable meshCone(double R, double h, uint32_t numSubdivisions)
{
    HalfEdgeTable het;
    if (numSubdivisions < 3)
        numSubdivisions = 3;

    std::vector<VertexHandle> bottomVerts(numSubdivisions);
    VertexHandle bottomCenter = het.addVertex({0.0f, 0.0f, 0.0f});
    VertexHandle apex = het.addVertex({0.0f, static_cast<float>(h), 0.0f});

    for (uint32_t i = 0; i < numSubdivisions; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / numSubdivisions;
        float x = static_cast<float>(R * std::cos(angle));
        float z = static_cast<float>(R * std::sin(angle));
        bottomVerts[i] = het.addVertex({x, 0.0f, z});
    }

    for (uint32_t i = 0; i < numSubdivisions; ++i)
    {
        uint32_t next = (i + 1) % numSubdivisions;
        // Side face
        het.addFace(bottomVerts[i], apex, bottomVerts[next]);
        // Bottom face
        het.addFace(bottomCenter, bottomVerts[next], bottomVerts[i]);
    }

    het.connectTwins();
    return het;
}

HalfEdgeTable meshTorus(double minorRadius, double majorRadius,
                        uint32_t majorSegments)
{
    HalfEdgeTable het;
    uint32_t minorSegments = majorSegments;
    if (majorSegments < 3)
        majorSegments = 3;

    std::vector<std::vector<VertexHandle>> grid(
        majorSegments, std::vector<VertexHandle>(minorSegments));

    for (uint32_t i = 0; i < majorSegments; ++i)
    {
        float u = 2.0f * glm::pi<float>() * i / majorSegments;
        glm::vec3 center(std::cos(u) * majorRadius, 0.0f,
                         std::sin(u) * majorRadius);

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 normal = glm::normalize(center);

        for (uint32_t j = 0; j < minorSegments; ++j)
        {
            float v = 2.0f * glm::pi<float>() * j / minorSegments;
            glm::vec3 pt =
                center +
                normal * static_cast<float>(minorRadius * std::cos(v)) +
                up * static_cast<float>(minorRadius * std::sin(v));
            grid[i][j] = het.addVertex(pt);
        }
    }

    for (uint32_t i = 0; i < majorSegments; ++i)
    {
        uint32_t nextI = (i + 1) % majorSegments;
        for (uint32_t j = 0; j < minorSegments; ++j)
        {
            uint32_t nextJ = (j + 1) % minorSegments;
            het.addFace(grid[i][j], grid[i][nextJ], grid[nextI][nextJ],
                        grid[nextI][j]);
        }
    }

    het.connectTwins();
    return het;
}

HalfEdgeTable meshArrow()
{
    HalfEdgeTable cyl = meshCylinder(0.05, 1.0, 24);
    HalfEdgeTable con = meshCone(0.16, 0.5, 24);

    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                 glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 trfCyl =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f)) * rotX;
    glm::mat4 trfCon =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * rotX;

    HalfEdgeTable arrow;

    auto appendHET = [&](const HalfEdgeTable &src, const glm::mat4 &trf)
    {
        std::vector<VertexHandle> vMap(src.getVertices().size());
        for (size_t i = 0; i < src.getVertices().size(); ++i)
        {
            if (src.getVertices()[i].heh.index != -1)
            {
                glm::vec4 p = trf * glm::vec4(src.getPositions()[i], 1.0f);
                vMap[i] = arrow.addVertex(glm::vec3(p));
            }
        }
        for (const auto &f : src.getFaces())
        {
            if (f.heh.index != -1)
            {
                std::vector<VertexHandle> fVerts;
                HalfEdgeHandle start = f.heh;
                HalfEdgeHandle curr = start;
                do
                {
                    fVerts.push_back(vMap[src.deref(curr).dst.index]);
                    curr = src.next(curr);
                } while (curr != start);

                if (fVerts.size() == 3)
                    arrow.addFace(fVerts[0], fVerts[1], fVerts[2]);
                else if (fVerts.size() == 4)
                    arrow.addFace(fVerts[0], fVerts[1], fVerts[2], fVerts[3]);
            }
        }
    };

    appendHET(cyl, trfCyl);
    appendHET(con, trfCon);

    arrow.connectTwins();
    return arrow;
}

HalfEdgeTable meshScaleArrow()
{
    HalfEdgeTable cyl = meshCylinder(0.05, 1.0, 24);
    HalfEdgeTable cube = createCube();

    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                 glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 trfCyl =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f)) * rotX;
    glm::mat4 trfCube =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

    HalfEdgeTable arrow;

    auto appendHET = [&](const HalfEdgeTable &src, const glm::mat4 &trf)
    {
        std::vector<VertexHandle> vMap(src.getVertices().size());
        for (size_t i = 0; i < src.getVertices().size(); ++i)
        {
            if (src.getVertices()[i].heh.index != -1)
            {
                glm::vec4 p = trf * glm::vec4(src.getPositions()[i], 1.0f);
                vMap[i] = arrow.addVertex(glm::vec3(p));
            }
        }
        for (const auto &f : src.getFaces())
        {
            if (f.heh.index != -1)
            {
                std::vector<VertexHandle> fVerts;
                HalfEdgeHandle start = f.heh;
                HalfEdgeHandle curr = start;
                do
                {
                    fVerts.push_back(vMap[src.deref(curr).dst.index]);
                    curr = src.next(curr);
                } while (curr != start);

                if (fVerts.size() == 3)
                    arrow.addFace(fVerts[0], fVerts[1], fVerts[2]);
                else if (fVerts.size() == 4)
                    arrow.addFace(fVerts[0], fVerts[1], fVerts[2], fVerts[3]);
            }
        }
    };

    appendHET(cyl, trfCyl);
    appendHET(cube, trfCube);

    arrow.connectTwins();
    return arrow;
}
} // namespace Utils
