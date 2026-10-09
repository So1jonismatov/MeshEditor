#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <utility>
#include <unordered_map>
#include <istream>
#include <glm/glm.hpp>
#include <HalfEdge.h>

class STLParser
{
public:
    static std::pair<std::vector<glm::vec3>,
                     std::vector<std::array<unsigned int, 3>>>
    read(const std::string &filename);
    static bool write(const HalfEdgeTable &halfEdgeTable,
                      const std::string &filename);

private:
    struct VertexKey
    {
        int64_t x = 0;
        int64_t y = 0;
        int64_t z = 0;

        bool operator==(const VertexKey &other) const;
    };

    struct VertexKeyHash
    {
        // boost::hash_combine-style mixing: spreads each component's bits so
        // near-identical coordinates don't collapse into the same bucket the
        // way a plain `h1 ^ (h2 << 1) ^ (h3 << 2)` would.
        static void hashCombine(std::size_t &seed, int64_t value);

        std::size_t operator()(const VertexKey &k) const;
    };

    static VertexKey toKey(const glm::vec3 &value);

    static glm::vec3 parseVertex(std::istream &stream);

    static bool isBinary(std::istream &stream, std::uint32_t &triangleCount);

    static void readAscii(
        std::istream &stream, std::vector<glm::vec3> &vertices,
        std::vector<std::array<unsigned int, 3>> &faces,
        std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap);

    static bool readBinary(
        std::istream &stream, std::vector<glm::vec3> &vertices,
        std::vector<std::array<unsigned int, 3>> &faces,
        std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap,
        std::uint32_t triangleCount);

    static void addTriangle(
        std::vector<glm::vec3> &vertices,
        std::vector<std::array<unsigned int, 3>> &faces,
        std::unordered_map<VertexKey, unsigned int, VertexKeyHash> &vertexMap,
        const std::array<glm::vec3, 3> &triangle);
};