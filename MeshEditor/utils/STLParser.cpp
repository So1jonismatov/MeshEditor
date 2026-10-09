#include "STLParser.h"

#include <array>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <map>
#include <sstream>
#include <iomanip>

bool STLParser::VertexKey::operator==(const VertexKey &other) const
{
    return x == other.x && y == other.y && z == other.z;
}

void STLParser::VertexKeyHash::hashCombine(std::size_t &seed, int64_t value)
{
    seed ^= std::hash<int64_t>{}(value) + 0x9e3779b97f4a7c15ull +
            (seed << 6) + (seed >> 2);
}

std::size_t STLParser::VertexKeyHash::operator()(const VertexKey &k) const
{
    std::size_t seed = 0;
    hashCombine(seed, k.x);
    hashCombine(seed, k.y);
    hashCombine(seed, k.z);
    return seed;
}

STLParser::VertexKey STLParser::toKey(const glm::vec3 &value)
{
    return {static_cast<int64_t>(std::round(value.x * 100000.0f)),
            static_cast<int64_t>(std::round(value.y * 100000.0f)),
            static_cast<int64_t>(std::round(value.z * 100000.0f))};
}

namespace
{
bool readVec3(std::istream &stream, glm::vec3 &value)
{
    std::array<float, 3> coords{};
    stream.read(reinterpret_cast<char *>(coords.data()),
                static_cast<std::streamsize>(sizeof(coords)));
    if (!stream)
        return false;

    value = {coords[0], coords[1], coords[2]};
    return true;
}
} // namespace

glm::vec3 STLParser::parseVertex(std::istream &stream)
{
    glm::vec3 value{0.0f};
    stream >> value.x >> value.y >> value.z;
    return value;
}

bool STLParser::isBinary(std::istream &stream, std::uint32_t &triangleCount)
{
    triangleCount = 0;

    stream.clear();
    stream.seekg(0, std::ios::end);
    const std::streampos end = stream.tellg();
    if (end < static_cast<std::streampos>(84))
        return false;

    stream.seekg(80, std::ios::beg);
    stream.read(reinterpret_cast<char *>(&triangleCount),
                static_cast<std::streamsize>(sizeof(triangleCount)));
    if (!stream)
        return false;

    const auto fileSize = static_cast<std::uint64_t>(end);
    const auto expectedSize =
        84ull + static_cast<std::uint64_t>(triangleCount) * 50ull;
    return fileSize == expectedSize;
}

void STLParser::readAscii(
    std::istream &stream, std::vector<glm::vec3> &vertices,
    std::vector<std::array<unsigned int, 3>> &faces,
    std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap)
{
    stream.clear();
    stream.seekg(0, std::ios::beg);

    std::string line;
    std::array<glm::vec3, 3> triangle{};
    std::size_t vertexCount = 0;

    while (std::getline(stream, line))
    {
        std::string token = line;
        const auto first = token.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            continue;
        const auto last = token.find_last_not_of(" \t\r\n");
        token = token.substr(first, last - first + 1);
        if (token.empty())
            continue;

        if (token.rfind("vertex", 0) == 0)
        {
            std::istringstream vertexStream(token.substr(6));
            if (vertexCount < 3)
            {
                triangle[vertexCount] = parseVertex(vertexStream);
                ++vertexCount;
            }
        }
        else if (token.rfind("endfacet", 0) == 0)
        {
            if (vertexCount == 3)
                addTriangle(vertices, faces, vertexMap, triangle);
            vertexCount = 0;
        }
    }
}

bool STLParser::readBinary(
    std::istream &stream, std::vector<glm::vec3> &vertices,
    std::vector<std::array<unsigned int, 3>> &faces,
    std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap,
    std::uint32_t triangleCount)
{
    stream.clear();
    stream.seekg(84, std::ios::beg);

    for (std::uint32_t triangleIndex = 0; triangleIndex < triangleCount;
         ++triangleIndex)
    {
        glm::vec3 normal{0.0f};
        std::array<glm::vec3, 3> triangle{};
        std::uint16_t attributeCount = 0;

        if (!readVec3(stream, normal))
            return false;
        if (!readVec3(stream, triangle[0]))
            return false;
        if (!readVec3(stream, triangle[1]))
            return false;
        if (!readVec3(stream, triangle[2]))
            return false;

        stream.read(reinterpret_cast<char *>(&attributeCount),
                    static_cast<std::streamsize>(sizeof(attributeCount)));
        if (!stream)
            return false;

        addTriangle(vertices, faces, vertexMap, triangle);
    }

    return true;
}

void STLParser::addTriangle(
    std::vector<glm::vec3> &vertices,
    std::vector<std::array<unsigned int, 3>> &faces,
    std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap,
    const std::array<glm::vec3, 3> &triangle)
{
    std::array<unsigned int, 3> indices{};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const VertexKey key = toKey(triangle[i]);
        // we try to insert, but if the vertex already exists, we get the
        // existing index
        auto [it, inserted] = vertexMap.emplace(key, 0);
        if (inserted)
        {
            // if the vertex is new, we assign it the next available index and
            // add it to the vertices list
            it->second = static_cast<unsigned int>(vertices.size());
            vertices.push_back(triangle[i]);
        }
        indices[i] = it->second; // store the index of the vertex for the face
    }

    if (indices[0] != indices[1] && indices[1] != indices[2] &&
        indices[0] != indices[2])
    {
        faces.push_back(indices); // add the face to the list of faces
    }
}

std::pair<std::vector<glm::vec3>, std::vector<std::array<unsigned int, 3>>>
STLParser::read(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        return {};
    std::vector<glm::vec3> vertices; // list of unique vertices
    std::vector<std::array<unsigned int, 3>>
        faces; // list of triangle faces, each represented by 3 vertex indices
    std::unordered_map<VertexKey, unsigned int, VertexKeyHash>
        vertexMap; // map to store unique vertices and their indices

    std::uint32_t triangleCount = 0;
    if (isBinary(file, triangleCount))
    {
        vertices.reserve(static_cast<size_t>(triangleCount) * 3);
        faces.reserve(static_cast<size_t>(triangleCount));
        if (readBinary(file, vertices, faces, vertexMap, triangleCount))
            return {vertices, faces};
    }

    readAscii(file, vertices, faces, vertexMap);

    return {vertices, faces};
}

bool STLParser::write(const HalfEdgeTable &halfEdgeTable,
                      const std::string &filename)
{
    std::ofstream file(filename);
    if (!file)
    {
        return false;
    }

    file << "solid mesh\n";
    file << std::fixed << std::setprecision(6);

    for (std::size_t faceIndex = 0; faceIndex < halfEdgeTable.getFaces().size();
         ++faceIndex)
    {
        const FaceHandle faceHandle{static_cast<int64_t>(faceIndex)};
        const Face &face = halfEdgeTable.deref(faceHandle);
        if (face.heh.index == -1)
            continue;

        std::vector<glm::vec3> polygon;
        HalfEdgeHandle start = face.heh;
        HalfEdgeHandle current = start;
        std::size_t guard = 0;
        const std::size_t maxSteps = halfEdgeTable.getHalfEdges().size() + 1;

        do
        {
            if (current.index < 0 ||
                current.index >=
                    static_cast<int64_t>(halfEdgeTable.getHalfEdges().size()) ||
                ++guard > maxSteps)
            {
                polygon.clear();
                break;
            }

            const HalfEdge &edge = halfEdgeTable.deref(current);
            polygon.push_back(halfEdgeTable.getPoint(edge.dst));
            current = halfEdgeTable.next(current);
        } while (current != start);

        if (polygon.size() < 3)
            continue;

        for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
        {
            const glm::vec3 a = polygon[0];
            const glm::vec3 b = polygon[i];
            const glm::vec3 c = polygon[i + 1];
            glm::vec3 normal = glm::cross(b - a, c - a);
            if (glm::dot(normal, normal) > 0.0f)
                normal = glm::normalize(normal);
            else
                normal = glm::vec3(0.0f);

            file << "  facet normal " << normal.x << " " << normal.y << " "
                 << normal.z << "\n";
            file << "    outer loop\n";
            file << "      vertex " << a.x << " " << a.y << " " << a.z << "\n";
            file << "      vertex " << b.x << " " << b.y << " " << b.z << "\n";
            file << "      vertex " << c.x << " " << c.y << " " << c.z << "\n";
            file << "    endloop\n";
            file << "  endfacet\n";
        }
    }

    file << "endsolid mesh\n";
    return true;
}