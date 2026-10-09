#include "ColladaParser.h"
#include <tinyxml2.h>
#include <charconv>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace tinyxml2;

// A parsed geometry entry: the shared, reference-counted Geometry (half-edge
// mesh + standalone line primitives). Every instance_geometry/instance_node
// that references the same COLLADA geometry id receives the SAME shared_ptr,
// so instances share one HalfEdgeTable, one GPU buffer, and one octree
// instead of copying the geometry per instance.
using GeometryData = std::shared_ptr<Geometry>;

// Textured exporters usually pre-split the POSITION array per corner (one copy
// of each point per adjacent face, so positions/normals/UVs share one index
// stream). Twins can only connect through shared VertexHandles, so coincident
// positions must be welded back into a single vertex or every face becomes a
// topological island. Coordinates are quantized to a 1e-5 grid (same scheme as
// STLParser) and used as a hash key.
struct WeldKey
{
    int64_t x = 0;
    int64_t y = 0;
    int64_t z = 0;

    bool operator==(const WeldKey &other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct WeldKeyHash
{
    static void hashCombine(std::size_t &seed, int64_t value)
    {
        seed ^= std::hash<int64_t>{}(value) + 0x9e3779b97f4a7c15ull +
                (seed << 6) + (seed >> 2);
    }

    std::size_t operator()(const WeldKey &k) const
    {
        std::size_t seed = 0;
        hashCombine(seed, k.x);
        hashCombine(seed, k.y);
        hashCombine(seed, k.z);
        return seed;
    }
};

static WeldKey toWeldKey(const glm::vec3 &value)
{
    return {static_cast<int64_t>(std::round(value.x * 100000.0f)),
            static_cast<int64_t>(std::round(value.y * 100000.0f)),
            static_cast<int64_t>(std::round(value.z * 100000.0f))};
}

static std::string trimString(const std::string &s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string getBaseDir(const std::string &path)
{
    const size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
}

static bool isAbsolutePath(const std::string &s)
{
    if (s.size() >= 2 && s[1] == ':') // C:\ or C:/
        return true;
    if (!s.empty() && (s[0] == '/' || s[0] == '\\'))
        return true;
    return false;
}

// COLLADA image URIs are percent-encoded (e.g. spaces become "%20"), so decode
// them back into real filesystem characters before opening the file.
static std::string urlDecode(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2]))
        {
            const std::string hex = s.substr(i + 1, 2);
            out.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            i += 2;
        }
        else
        {
            out.push_back(s[i]);
        }
    }
    return out;
}

// Turn a COLLADA <init_from> reference into a filesystem path relative to the
// .dae's own directory. Handles "file://" URIs, percent-encoding and absolute
// paths.
static std::string resolveTexturePath(const std::string &baseDir,
                                      const std::string &ref)
{
    std::string p = urlDecode(trimString(ref));
    const std::string filePfx = "file://";
    if (p.rfind(filePfx, 0) == 0)
        p = p.substr(filePfx.size());
    if (p.size() >= 3 && p[0] == '/' && p[2] == ':') // /C:/...
        p = p.substr(1);
    if (isAbsolutePath(p))
        return p;
    return baseDir + p;
}

static void logMessage(const std::string &msg)
{
    (void)msg;
}

static void logParsedGeometry(const std::string &id, size_t vertexCount,
                              const std::string &primitiveType,
                              size_t faceCount)
{
    (void)id;
    (void)vertexCount;
    (void)primitiveType;
    (void)faceCount;
}

static void logParsedNode(const std::string &name, const glm::mat4 &transform,
                          const char *geomUrl)
{
    (void)name;
    (void)transform;
    (void)geomUrl;
}

static std::string getElementText(XMLElement *el)
{
    std::string res;
    if (!el)
        return res;
    for (XMLNode *node = el->FirstChild(); node; node = node->NextSibling())
    {
        if (XMLText *txt = node->ToText())
        {
            res += txt->Value();
            res += " "; // ensure separation
        }
    }
    return res;
}

// Whitespace-separated number parse via std::from_chars. COLLADA
// <float_array>/<p> blocks can hold millions of values (large exports), where
// stringstream extraction is 5-20x slower and allocation-heavy. Unparsable
// characters (e.g. a leading '+') are skipped one at a time to resync.
template <typename T> static std::vector<T> parseArray(const std::string &text)
{
    std::vector<T> result;
    const char *ptr = text.data();
    const char *const end = ptr + text.size();
    while (ptr < end)
    {
        while (ptr < end && std::isspace(static_cast<unsigned char>(*ptr)))
            ++ptr;
        if (ptr >= end)
            break;
        T value{};
        const auto [next, ec] = std::from_chars(ptr, end, value);
        if (ec != std::errc{} || next == ptr)
        {
            ++ptr;
            continue;
        }
        result.push_back(value);
        ptr = next;
    }
    return result;
}

struct InputData
{
    int stride = 1;
    int vertexOffset = 0;
    int texcoordOffset = -1;      // -1 == primitive has no TEXCOORD input
    std::string texcoordSourceId; // <source> id (leading '#' stripped)
};

struct ControllerBinding
{
    std::string sourceGeometry;
};

static std::string normalizeNodeUrl(const char *url)
{
    if (!url || !*url)
        return std::string();
    if (url[0] == '#')
        return url;
    return std::string("#") + url;
}

static std::unique_ptr<Node> parseNode(
    XMLElement *nodeEl, const std::map<std::string, GeometryData> &geometries,
    const std::map<std::string, ControllerBinding> &controllers,
    const std::map<std::string, XMLElement *> &libraryNodes,
    const std::map<std::string, std::string> &materialTextures,
    std::unordered_set<std::string> &activeLibraryNodes);

static std::unique_ptr<Node> instantiateLibraryNode(
    const std::string &url,
    const std::map<std::string, GeometryData> &geometries,
    const std::map<std::string, ControllerBinding> &controllers,
    const std::map<std::string, XMLElement *> &libraryNodes,
    const std::map<std::string, std::string> &materialTextures,
    std::unordered_set<std::string> &activeLibraryNodes)
{
    if (url.empty())
        return nullptr;

    const auto nodeIt = libraryNodes.find(url);
    if (nodeIt == libraryNodes.end())
    {
        logMessage("instance_node references missing node " + url);
        return nullptr;
    }

    if (!activeLibraryNodes.insert(url).second)
    {
        logMessage("Detected recursive instance_node cycle at " + url);
        return nullptr;
    }

    std::unique_ptr<Node> node =
        parseNode(nodeIt->second, geometries, controllers, libraryNodes,
                  materialTextures, activeLibraryNodes);
    activeLibraryNodes.erase(url);
    return node;
}

static void applyNodeTransformElement(XMLElement *transformEl,
                                      glm::mat4 &transform)
{
    const std::string elementName = transformEl->Name();
    std::vector<float> values = parseArray<float>(getElementText(transformEl));

    if (elementName == "translate" && values.size() >= 3)
    {
        transform = transform *
                    glm::translate(glm::mat4(1.0f),
                                   glm::vec3(values[0], values[1], values[2]));
    }
    else if (elementName == "rotate" && values.size() >= 4)
    {
        transform =
            transform * glm::rotate(glm::mat4(1.0f), glm::radians(values[3]),
                                    glm::vec3(values[0], values[1], values[2]));
    }
    else if (elementName == "scale" && values.size() >= 3)
    {
        transform =
            transform * glm::scale(glm::mat4(1.0f),
                                   glm::vec3(values[0], values[1], values[2]));
    }
    else if (elementName == "matrix" && values.size() >= 16)
    {
        glm::mat4 mat = glm::make_mat4(values.data());
        transform = transform * glm::transpose(mat);
    }
}

static bool attachMeshForUrl(
    Node *node, const std::string &url,
    const std::map<std::string, GeometryData> &geometries,
    const std::map<std::string, ControllerBinding> &controllers,
    std::string &resolvedGeometryUrl)
{
    const auto geometryIt = geometries.find(url);
    if (geometryIt != geometries.end())
    {
        // Share, don't copy: every instance holds the same Geometry.
        node->attachMesh(std::make_unique<Mesh>(geometryIt->second));
        resolvedGeometryUrl = url;
        return true;
    }

    const auto controllerIt = controllers.find(url);
    if (controllerIt != controllers.end())
    {
        const auto sourceIt =
            geometries.find(controllerIt->second.sourceGeometry);
        if (sourceIt != geometries.end())
        {
            node->attachMesh(std::make_unique<Mesh>(sourceIt->second));
            resolvedGeometryUrl = controllerIt->second.sourceGeometry;
            return true;
        }

        logMessage("Controller " + url + " references missing geometry " +
                   controllerIt->second.sourceGeometry);
    }

    return false;
}

// Resolve the diffuse texture bound to a mesh instance via <bind_material>.
// Falls back to the single scene material when there is no explicit binding.
static std::string findBoundTexture(
    XMLElement *instanceEl,
    const std::map<std::string, std::string> &materialTextures)
{
    if (instanceEl)
    {
        XMLElement *bind = instanceEl->FirstChildElement("bind_material");
        XMLElement *tech =
            bind ? bind->FirstChildElement("technique_common") : nullptr;
        if (tech)
        {
            for (XMLElement *im =
                     tech->FirstChildElement("instance_material");
                 im; im = im->NextSiblingElement("instance_material"))
            {
                const char *target = im->Attribute("target");
                if (!target)
                    continue;
                std::string matId = (target[0] == '#') ? target + 1 : target;
                const auto it = materialTextures.find(matId);
                if (it != materialTextures.end())
                    return it->second;
            }
        }
    }
    if (materialTextures.size() == 1)
        return materialTextures.begin()->second;
    return std::string();
}

static InputData getInputsInfo(XMLElement *primitiveEl)
{
    InputData info;
    for (XMLElement *input = primitiveEl->FirstChildElement("input"); input;
         input = input->NextSiblingElement("input"))
    {
        int offset = 0;
        if (input->QueryIntAttribute("offset", &offset) == XML_SUCCESS)
        {
            if (offset + 1 > info.stride)
                info.stride = offset + 1;
        }
        const char *semantic = input->Attribute("semantic");
        if (!semantic)
            continue;
        const std::string sem = semantic;
        if (sem == "VERTEX")
        {
            info.vertexOffset = offset;
        }
        else if (sem == "TEXCOORD")
        {
            info.texcoordOffset = offset;
            const char *src = input->Attribute("source");
            if (src)
                info.texcoordSourceId = (src[0] == '#') ? src + 1 : src;
        }
    }
    return info;
}

// Reads a <source> of 2D data (texture coordinates) by id. Honours the
// technique_common/accessor stride so 3-component UVW sources are read
// correctly (we keep the first two components).
static std::vector<glm::vec2> readSourceVec2(XMLElement *meshEl,
                                             const std::string &sourceId)
{
    std::vector<glm::vec2> uvs;
    if (!meshEl || sourceId.empty())
        return uvs;
    for (XMLElement *source = meshEl->FirstChildElement("source"); source;
         source = source->NextSiblingElement("source"))
    {
        const char *srcId = source->Attribute("id");
        if (!srcId || sourceId != srcId)
            continue;
        XMLElement *fa = source->FirstChildElement("float_array");
        if (!fa)
            return uvs;
        std::vector<float> f = parseArray<float>(getElementText(fa));
        int stride = 2;
        XMLElement *tech = source->FirstChildElement("technique_common");
        XMLElement *acc = tech ? tech->FirstChildElement("accessor") : nullptr;
        if (acc)
            acc->QueryIntAttribute("stride", &stride);
        if (stride < 2)
            stride = 2;
        for (size_t i = 0; i + 1 < f.size(); i += stride)
            uvs.push_back(glm::vec2(f[i], f[i + 1]));
        return uvs;
    }
    return uvs;
}

// Assigns a texture coordinate to each corner of a freshly-added face. Each
// half-edge stores the UV of the corner at its destination vertex, so we match
// by destination-vertex handle (corners of one face have distinct vertices).
static void assignFaceCornerUVs(HalfEdgeTable &het, FaceHandle fh,
                                const std::vector<VertexHandle> &faceVerts,
                                const std::vector<glm::vec2> &cornerUVs)
{
    if (fh.index < 0 || faceVerts.size() != cornerUVs.size())
        return;
    const Face &face = het.getFaces()[fh.index];
    HalfEdgeHandle start = face.heh;
    if (start.index < 0)
        return;
    HalfEdgeHandle curr = start;
    size_t guard = 0;
    const size_t maxSteps = het.getHalfEdges().size() + 1;
    do
    {
        const VertexHandle dst = het.destVertex(curr);
        for (size_t i = 0; i < faceVerts.size(); ++i)
        {
            if (faceVerts[i].index == dst.index)
            {
                het.setUV(curr, cornerUVs[i]);
                break;
            }
        }
        curr = het.next(curr);
    } while (curr != start && curr.index != -1 && ++guard < maxSteps);
}

// Adds one polygon face to the table. Triangles and quads keep their native
// topology; polygons with 5+ corners are fan-triangulated (v0, vk, vk+1).
// True when any two corners collapsed into the same vertex (possible after
// position welding) — such a face would corrupt the half-edge loops.
static bool isDegenerateFace(const std::vector<VertexHandle> &faceVerts)
{
    for (size_t i = 0; i < faceVerts.size(); ++i)
        for (size_t j = i + 1; j < faceVerts.size(); ++j)
            if (faceVerts[i].index == faceVerts[j].index)
                return true;
    return false;
}

static void emitPolygonFace(HalfEdgeTable &het,
                            const std::vector<VertexHandle> &faceVerts,
                            const std::vector<glm::vec2> &cornerUVs, bool hasUV,
                            size_t &facesAdded)
{
    const size_t n = faceVerts.size();
    if (n < 3)
        return;

    if (n == 3)
    {
        if (isDegenerateFace(faceVerts))
            return;
        FaceHandle fh = het.addFace(faceVerts[0], faceVerts[1], faceVerts[2]);
        if (hasUV)
            assignFaceCornerUVs(het, fh, faceVerts, cornerUVs);
        facesAdded++;
    }
    else if (n == 4)
    {
        if (isDegenerateFace(faceVerts))
            return;
        FaceHandle fh = het.addFace(faceVerts[0], faceVerts[1], faceVerts[2],
                                    faceVerts[3]);
        if (hasUV)
            assignFaceCornerUVs(het, fh, faceVerts, cornerUVs);
        facesAdded++;
    }
    else
    {
        for (size_t k = 1; k + 1 < n; ++k)
        {
            std::vector<VertexHandle> tri = {faceVerts[0], faceVerts[k],
                                             faceVerts[k + 1]};
            if (isDegenerateFace(tri))
                continue;
            FaceHandle fh = het.addFace(tri[0], tri[1], tri[2]);
            if (hasUV)
            {
                std::vector<glm::vec2> triUV = {cornerUVs[0], cornerUVs[k],
                                                cornerUVs[k + 1]};
                assignFaceCornerUVs(het, fh, tri, triUV);
            }
            facesAdded++;
        }
    }
}

static void parsePolylist(XMLElement *el, XMLElement *meshEl, HalfEdgeTable &het,
                          const std::vector<VertexHandle> &vHandles,
                          const std::string &geomId)
{
    InputData info = getInputsInfo(el);
    std::vector<glm::vec2> uvs;
    if (info.texcoordOffset >= 0)
        uvs = readSourceVec2(meshEl, info.texcoordSourceId);
    const bool hasUV = info.texcoordOffset >= 0 && !uvs.empty();

    XMLElement *vcountEl = el->FirstChildElement("vcount");
    XMLElement *pEl = el->FirstChildElement("p");
    if (!vcountEl || !pEl)
        return;

    std::vector<int> vcounts = parseArray<int>(getElementText(vcountEl));
    std::vector<int> pVals = parseArray<int>(getElementText(pEl));

    size_t faceVertexTotal = 0;
    for (int vc : vcounts)
    {
        if (vc > 0)
            faceVertexTotal += static_cast<size_t>(vc);
    }
    het.reserve(0, faceVertexTotal, vcounts.size());

    size_t pIndex = 0;
    size_t facesAdded = 0;
    for (int vc : vcounts)
    {
        std::vector<VertexHandle> faceVerts;
        std::vector<glm::vec2> cornerUVs;
        for (int i = 0; i < vc; ++i)
        {
            if (pIndex + info.vertexOffset < pVals.size())
            {
                int vIdx = pVals[pIndex + info.vertexOffset];
                if (vIdx >= 0 && vIdx < static_cast<int>(vHandles.size()))
                {
                    faceVerts.push_back(vHandles[vIdx]);
                    if (hasUV && pIndex + info.texcoordOffset < pVals.size())
                    {
                        int tIdx = pVals[pIndex + info.texcoordOffset];
                        cornerUVs.push_back(
                            (tIdx >= 0 && tIdx < static_cast<int>(uvs.size()))
                                ? uvs[tIdx]
                                : glm::vec2(0.0f));
                    }
                }
            }
            pIndex += info.stride;
        }

        emitPolygonFace(het, faceVerts, cornerUVs,
                        hasUV && cornerUVs.size() == faceVerts.size(),
                        facesAdded);
    }
    logParsedGeometry(geomId, vHandles.size(), "polylist", facesAdded);
}

static void parseTriangles(XMLElement *el, XMLElement *meshEl,
                           HalfEdgeTable &het,
                           const std::vector<VertexHandle> &vHandles,
                           const std::string &geomId)
{
    InputData info = getInputsInfo(el);
    std::vector<glm::vec2> uvs;
    if (info.texcoordOffset >= 0)
        uvs = readSourceVec2(meshEl, info.texcoordSourceId);
    const bool hasUV = info.texcoordOffset >= 0 && !uvs.empty();

    XMLElement *pEl = el->FirstChildElement("p");
    if (!pEl)
        return;

    std::vector<int> pVals = parseArray<int>(getElementText(pEl));
    int count = 0;
    el->QueryIntAttribute("count", &count);

    if (count > 0)
        het.reserve(0, static_cast<size_t>(count) * 3,
                    static_cast<size_t>(count));

    size_t pIndex = 0;
    size_t facesAdded = 0;
    for (int f = 0; f < count; ++f)
    {
        std::vector<VertexHandle> faceVerts;
        std::vector<glm::vec2> cornerUVs;
        for (int i = 0; i < 3; ++i)
        {
            if (pIndex + info.vertexOffset < pVals.size())
            {
                int vIdx = pVals[pIndex + info.vertexOffset];
                if (vIdx >= 0 && vIdx < static_cast<int>(vHandles.size()))
                {
                    faceVerts.push_back(vHandles[vIdx]);
                    if (hasUV && pIndex + info.texcoordOffset < pVals.size())
                    {
                        int tIdx = pVals[pIndex + info.texcoordOffset];
                        cornerUVs.push_back(
                            (tIdx >= 0 && tIdx < static_cast<int>(uvs.size()))
                                ? uvs[tIdx]
                                : glm::vec2(0.0f));
                    }
                }
            }
            pIndex += info.stride;
        }

        emitPolygonFace(het, faceVerts, cornerUVs,
                        hasUV && cornerUVs.size() == faceVerts.size(),
                        facesAdded);
    }
    logParsedGeometry(geomId, vHandles.size(), "triangles", facesAdded);
}

static void parsePolygons(XMLElement *el, XMLElement *meshEl, HalfEdgeTable &het,
                          const std::vector<VertexHandle> &vHandles,
                          const std::string &geomId)
{
    InputData info = getInputsInfo(el);
    std::vector<glm::vec2> uvs;
    if (info.texcoordOffset >= 0)
        uvs = readSourceVec2(meshEl, info.texcoordSourceId);
    const bool hasUV = info.texcoordOffset >= 0 && !uvs.empty();

    size_t facesAdded = 0;

    for (XMLElement *pEl = el->FirstChildElement("p"); pEl;
         pEl = pEl->NextSiblingElement("p"))
    {
        std::vector<int> pVals = parseArray<int>(getElementText(pEl));
        std::vector<VertexHandle> faceVerts;
        std::vector<glm::vec2> cornerUVs;
        size_t pIndex = 0;

        while (pIndex + info.vertexOffset < pVals.size())
        {
            int vIdx = pVals[pIndex + info.vertexOffset];
            if (vIdx >= 0 && vIdx < static_cast<int>(vHandles.size()))
            {
                faceVerts.push_back(vHandles[vIdx]);
                if (hasUV && pIndex + info.texcoordOffset < pVals.size())
                {
                    int tIdx = pVals[pIndex + info.texcoordOffset];
                    cornerUVs.push_back(
                        (tIdx >= 0 && tIdx < static_cast<int>(uvs.size()))
                            ? uvs[tIdx]
                            : glm::vec2(0.0f));
                }
            }
            pIndex += info.stride;
        }

        emitPolygonFace(het, faceVerts, cornerUVs,
                        hasUV && cornerUVs.size() == faceVerts.size(),
                        facesAdded);
    }
    logParsedGeometry(geomId, vHandles.size(), "polygons", facesAdded);
}

// Standalone line primitives. <lines> stores index pairs; <linestrips> stores
// one polyline per <p>. Both are emitted as GL_LINES endpoint pairs.
static void parseLines(XMLElement *el, const std::vector<glm::vec3> &positions,
                       std::vector<glm::vec3> &outSegments)
{
    InputData info = getInputsInfo(el);
    XMLElement *pEl = el->FirstChildElement("p");
    if (!pEl)
        return;
    std::vector<int> pVals = parseArray<int>(getElementText(pEl));

    std::vector<int> verts;
    for (size_t pIndex = 0; pIndex + info.vertexOffset < pVals.size();
         pIndex += info.stride)
        verts.push_back(pVals[pIndex + info.vertexOffset]);

    for (size_t i = 0; i + 1 < verts.size(); i += 2)
    {
        int a = verts[i], b = verts[i + 1];
        if (a >= 0 && a < static_cast<int>(positions.size()) && b >= 0 &&
            b < static_cast<int>(positions.size()))
        {
            outSegments.push_back(positions[a]);
            outSegments.push_back(positions[b]);
        }
    }
}

static void parseLinestrips(XMLElement *el,
                            const std::vector<glm::vec3> &positions,
                            std::vector<glm::vec3> &outSegments)
{
    InputData info = getInputsInfo(el);
    for (XMLElement *pEl = el->FirstChildElement("p"); pEl;
         pEl = pEl->NextSiblingElement("p"))
    {
        std::vector<int> pVals = parseArray<int>(getElementText(pEl));
        std::vector<int> strip;
        for (size_t pIndex = 0; pIndex + info.vertexOffset < pVals.size();
             pIndex += info.stride)
            strip.push_back(pVals[pIndex + info.vertexOffset]);

        for (size_t i = 0; i + 1 < strip.size(); ++i)
        {
            int a = strip[i], b = strip[i + 1];
            if (a >= 0 && a < static_cast<int>(positions.size()) && b >= 0 &&
                b < static_cast<int>(positions.size()))
            {
                outSegments.push_back(positions[a]);
                outSegments.push_back(positions[b]);
            }
        }
    }
}

static std::unique_ptr<Node> parseNode(
    XMLElement *nodeEl, const std::map<std::string, GeometryData> &geometries,
    const std::map<std::string, ControllerBinding> &controllers,
    const std::map<std::string, XMLElement *> &libraryNodes,
    const std::map<std::string, std::string> &materialTextures,
    std::unordered_set<std::string> &activeLibraryNodes)
{
    if (!nodeEl)
        return nullptr;

    auto node = std::make_unique<Node>();
    const char *id = nodeEl->Attribute("id");
    const char *name = nodeEl->Attribute("name");
    if (!name)
        name = id;

    std::string nodeName = name ? name : "UnnamedNode";
    node->setName(nodeName);

    glm::mat4 transform(1.0f);
    for (XMLElement *childEl = nodeEl->FirstChildElement(); childEl;
         childEl = childEl->NextSiblingElement())
    {
        const std::string childName = childEl->Name();
        if (childName == "translate" || childName == "rotate" ||
            childName == "scale" || childName == "matrix")
        {
            applyNodeTransformElement(childEl, transform);
        }
    }
    node->setRelativeTransform(transform);

    std::string attachmentUrl;
    XMLElement *meshInstanceEl = nullptr; // element carrying <bind_material>
    XMLElement *instanceController =
        nodeEl->FirstChildElement("instance_controller");
    if (instanceController)
    {
        const char *url = instanceController->Attribute("url");
        if (url)
        {
            attachMeshForUrl(node.get(), url, geometries, controllers,
                             attachmentUrl);
            meshInstanceEl = instanceController;
        }
    }

    if (attachmentUrl.empty())
    {
        XMLElement *instanceGeom =
            nodeEl->FirstChildElement("instance_geometry");
        if (instanceGeom)
        {
            const char *url = instanceGeom->Attribute("url");
            if (url)
            {
                attachMeshForUrl(node.get(), url, geometries, controllers,
                                 attachmentUrl);
                meshInstanceEl = instanceGeom;
            }
        }
    }

    // Bind a diffuse texture (if any) to the freshly attached mesh.
    if (node->getMesh())
    {
        std::string texPath = findBoundTexture(meshInstanceEl, materialTextures);
        if (!texPath.empty())
            node->getMesh()->material.setDiffuseTexturePath(texPath);
    }

    logParsedNode(nodeName, transform,
                  attachmentUrl.empty() ? nullptr : attachmentUrl.c_str());

    for (XMLElement *childEl = nodeEl->FirstChildElement(); childEl;
         childEl = childEl->NextSiblingElement())
    {
        const std::string childName = childEl->Name();
        if (childName == "node")
        {
            std::unique_ptr<Node> child =
                parseNode(childEl, geometries, controllers, libraryNodes,
                          materialTextures, activeLibraryNodes);
            if (child)
                node->attachNode(std::move(child));
        }
        else if (childName == "instance_node")
        {
            const char *url = childEl->Attribute("url");
            std::unique_ptr<Node> child = instantiateLibraryNode(
                normalizeNodeUrl(url), geometries, controllers, libraryNodes,
                materialTextures, activeLibraryNodes);
            if (child)
                node->attachNode(std::move(child));
        }
    }

    return node;
}

std::unique_ptr<Model> loadModel(const std::string &filename)
{
    logMessage("Attempting to load COLLADA file: " + filename);
    XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != XML_SUCCESS)
    {
        logMessage("Failed to load or parse XML file: " + filename);
        return nullptr;
    }

    XMLElement *collada = doc.FirstChildElement("collada");
    if (!collada)
        collada = doc.FirstChildElement("COLLADA");
    if (!collada)
    {
        logMessage("File is missing the <COLLADA> root tag!");
        return nullptr;
    }

    std::map<std::string, GeometryData> geometries;
    XMLElement *libGeom = collada->FirstChildElement("library_geometries");
    if (libGeom)
    {
        logMessage("Parsing <library_geometries>...");
        for (XMLElement *geom = libGeom->FirstChildElement("geometry"); geom;
             geom = geom->NextSiblingElement("geometry"))
        {
            const char *geomId = geom->Attribute("id");
            if (!geomId)
                continue;

            XMLElement *meshEl = geom->FirstChildElement("mesh");
            if (!meshEl)
                continue;

            // Find the proper POSITION source ID
            std::string posSourceId;
            XMLElement *verticesEl = meshEl->FirstChildElement("vertices");
            if (verticesEl)
            {
                for (XMLElement *input = verticesEl->FirstChildElement("input");
                     input; input = input->NextSiblingElement("input"))
                {
                    const char *sem = input->Attribute("semantic");
                    if (sem && std::string(sem) == "POSITION")
                    {
                        const char *src = input->Attribute("source");
                        if (src && src[0] == '#')
                            posSourceId = src + 1;
                    }
                }
            }

            // Find the float_array associated with the position source
            XMLElement *posFloatArray = nullptr;
            for (XMLElement *source = meshEl->FirstChildElement("source");
                 source; source = source->NextSiblingElement("source"))
            {
                const char *srcId = source->Attribute("id");
                if (srcId && posSourceId == srcId)
                {
                    posFloatArray = source->FirstChildElement("float_array");
                    break;
                }
            }
            // Fallback if not found correctly
            if (!posFloatArray)
            {
                XMLElement *firstSource = meshEl->FirstChildElement("source");
                if (firstSource)
                    posFloatArray =
                        firstSource->FirstChildElement("float_array");
            }

            std::vector<glm::vec3> positions;
            if (posFloatArray)
            {
                std::vector<float> floats =
                    parseArray<float>(getElementText(posFloatArray));
                positions.reserve(floats.size() / 3);
                for (size_t i = 0; i + 2 < floats.size(); i += 3)
                {
                    positions.push_back(
                        glm::vec3(floats[i], floats[i + 1], floats[i + 2]));
                }
            }

            HalfEdgeTable het;
            std::vector<glm::vec3> polylineSegments;
            het.reserve(positions.size(), 0, 0);
            // Weld coincident positions: every COLLADA position index maps to
            // the handle of the first vertex seen at that coordinate, so faces
            // referencing duplicated positions share vertices and connectTwins
            // can stitch them. UVs are unaffected — they are stored per
            // half-edge corner, not per vertex.
            std::vector<VertexHandle> vHandles;
            vHandles.reserve(positions.size());
            std::unordered_map<WeldKey, VertexHandle, WeldKeyHash> weldMap;
            weldMap.reserve(positions.size());
            for (const auto &p : positions)
            {
                const WeldKey key = toWeldKey(p);
                auto it = weldMap.find(key);
                if (it == weldMap.end())
                    it = weldMap.emplace(key, het.addVertex(p)).first;
                vHandles.push_back(it->second);
            }

            // Route parsing logic dynamically to the appropriate variant
            for (XMLElement *child = meshEl->FirstChildElement(); child;
                 child = child->NextSiblingElement())
            {
                std::string childName = child->Name();
                if (childName == "polylist")
                {
                    parsePolylist(child, meshEl, het, vHandles, geomId);
                }
                else if (childName == "triangles")
                {
                    parseTriangles(child, meshEl, het, vHandles, geomId);
                }
                else if (childName == "polygons")
                {
                    parsePolygons(child, meshEl, het, vHandles, geomId);
                }
                else if (childName == "lines")
                {
                    parseLines(child, positions, polylineSegments);
                }
                else if (childName == "linestrips")
                {
                    parseLinestrips(child, positions, polylineSegments);
                }
            }

            het.connectTwins();
            auto geometry = std::make_shared<Geometry>(std::move(het));
            geometry->setPolylineSegments(polylineSegments);
            geometries[std::string("#") + geomId] = std::move(geometry);
        }
    }

    std::map<std::string, ControllerBinding> controllers;
    XMLElement *libControllers =
        collada->FirstChildElement("library_controllers");
    if (libControllers)
    {
        logMessage("Parsing <library_controllers>...");
        for (XMLElement *controller =
                 libControllers->FirstChildElement("controller");
             controller;
             controller = controller->NextSiblingElement("controller"))
        {
            const char *controllerId = controller->Attribute("id");
            if (!controllerId)
                continue;

            XMLElement *skin = controller->FirstChildElement("skin");
            if (!skin)
                continue;

            const char *source = skin->Attribute("source");
            if (!source)
                continue;

            controllers[std::string("#") + controllerId] =
                ControllerBinding{source};
        }
    }

    std::map<std::string, XMLElement *> libraryNodes;
    XMLElement *libNodes = collada->FirstChildElement("library_nodes");
    if (libNodes)
    {
        logMessage("Parsing <library_nodes>...");
        for (XMLElement *libNode = libNodes->FirstChildElement("node"); libNode;
             libNode = libNode->NextSiblingElement("node"))
        {
            const char *nodeId = libNode->Attribute("id");
            if (!nodeId)
                continue;

            libraryNodes[normalizeNodeUrl(nodeId)] = libNode;
        }
    }

    // ── Materials → diffuse texture path ──────────────────────────────────
    // Resolve the chain material → effect → sampler/surface → image → file.
    const std::string baseDir = getBaseDir(filename);

    std::map<std::string, std::string> imagePaths; // image id -> raw file ref
    if (XMLElement *libImages =
            collada->FirstChildElement("library_images"))
    {
        for (XMLElement *image = libImages->FirstChildElement("image"); image;
             image = image->NextSiblingElement("image"))
        {
            const char *id = image->Attribute("id");
            if (!id)
                continue;
            XMLElement *initFrom = image->FirstChildElement("init_from");
            std::string path = initFrom ? getElementText(initFrom) : "";
            // COLLADA 1.5 wraps the path in an <init_from><ref>...</ref>.
            if (initFrom)
            {
                if (XMLElement *ref = initFrom->FirstChildElement("ref"))
                    path = getElementText(ref);
            }
            imagePaths[id] = trimString(path);
        }
    }

    std::map<std::string, std::string> effectImage; // effect id -> image id
    if (XMLElement *libEffects =
            collada->FirstChildElement("library_effects"))
    {
        for (XMLElement *effect = libEffects->FirstChildElement("effect");
             effect; effect = effect->NextSiblingElement("effect"))
        {
            const char *id = effect->Attribute("id");
            XMLElement *prof =
                effect ? effect->FirstChildElement("profile_COMMON") : nullptr;
            if (!id || !prof)
                continue;

            // newparam sids: sampler2D -> surface, surface -> image id.
            std::map<std::string, std::string> surfaceImage;
            std::map<std::string, std::string> samplerSurface;
            for (XMLElement *np = prof->FirstChildElement("newparam"); np;
                 np = np->NextSiblingElement("newparam"))
            {
                const char *sid = np->Attribute("sid");
                if (!sid)
                    continue;
                if (XMLElement *surf = np->FirstChildElement("surface"))
                {
                    if (XMLElement *initFrom =
                            surf->FirstChildElement("init_from"))
                        surfaceImage[sid] = trimString(getElementText(initFrom));
                }
                if (XMLElement *samp = np->FirstChildElement("sampler2D"))
                {
                    if (XMLElement *src = samp->FirstChildElement("source"))
                        samplerSurface[sid] = trimString(getElementText(src));
                }
            }

            // Look for the first <diffuse><texture> under the technique.
            std::string imageId;
            XMLElement *tech = prof->FirstChildElement("technique");
            for (XMLElement *shade = tech ? tech->FirstChildElement() : nullptr;
                 shade && imageId.empty();
                 shade = shade->NextSiblingElement())
            {
                XMLElement *diffuse = shade->FirstChildElement("diffuse");
                XMLElement *texture =
                    diffuse ? diffuse->FirstChildElement("texture") : nullptr;
                const char *texRef =
                    texture ? texture->Attribute("texture") : nullptr;
                if (!texRef)
                    continue;
                std::string ref = texRef;
                if (samplerSurface.count(ref))
                {
                    const std::string surf = samplerSurface[ref];
                    if (surfaceImage.count(surf))
                        imageId = surfaceImage[surf];
                }
                else if (surfaceImage.count(ref))
                    imageId = surfaceImage[ref];
                else
                    imageId = ref; // some exporters point straight at the image
            }

            if (!imageId.empty())
                effectImage[id] = imageId;
        }
    }

    std::map<std::string, std::string> materialTextures; // material id -> path
    if (XMLElement *libMaterials =
            collada->FirstChildElement("library_materials"))
    {
        for (XMLElement *material =
                 libMaterials->FirstChildElement("material");
             material; material = material->NextSiblingElement("material"))
        {
            const char *id = material->Attribute("id");
            XMLElement *inst =
                material ? material->FirstChildElement("instance_effect")
                         : nullptr;
            const char *url = inst ? inst->Attribute("url") : nullptr;
            if (!id || !url)
                continue;
            std::string effectId = (url[0] == '#') ? url + 1 : url;
            const auto eit = effectImage.find(effectId);
            if (eit == effectImage.end())
                continue;
            const auto iit = imagePaths.find(eit->second);
            if (iit == imagePaths.end())
                continue;
            materialTextures[id] = resolveTexturePath(baseDir, iit->second);
        }
    }

    auto model = std::make_unique<Model>();
    XMLElement *libVisualScenes =
        collada->FirstChildElement("library_visual_scenes");
    if (libVisualScenes)
    {
        logMessage("Parsing <library_visual_scenes>...");
        XMLElement *visualScene =
            libVisualScenes->FirstChildElement("visual_scene");
        if (visualScene)
        {
            std::unordered_set<std::string> activeLibraryNodes;
            for (XMLElement *nodeEl = visualScene->FirstChildElement(); nodeEl;
                 nodeEl = nodeEl->NextSiblingElement())
            {
                const std::string nodeName = nodeEl->Name();
                if (nodeName == "node")
                {
                    std::unique_ptr<Node> node =
                        parseNode(nodeEl, geometries, controllers, libraryNodes,
                                  materialTextures, activeLibraryNodes);
                    if (node)
                        model->attachNode(std::move(node));
                }
                else if (nodeName == "instance_node")
                {
                    const char *url = nodeEl->Attribute("url");
                    std::unique_ptr<Node> node = instantiateLibraryNode(
                        normalizeNodeUrl(url), geometries, controllers,
                        libraryNodes, materialTextures, activeLibraryNodes);
                    if (node)
                        model->attachNode(std::move(node));
                }
            }
        }
    }

    logMessage("Successfully loaded COLLADA model.");
    return model;
}

static void exportNode(XMLDocument &doc, XMLElement *parentXml,
                       const Node *node, int &geomCounter, XMLElement *libGeom,
                       std::map<const Geometry *, std::string> &exportedGeoms)
{
    Mesh *mesh = node->getMesh();
    std::string geomId = "geom_" + std::to_string(geomCounter++);

    XMLElement *xmlNode = doc.NewElement("node");
    xmlNode->SetAttribute("id", node->getName().empty()
                                    ? geomId.c_str()
                                    : node->getName().c_str());

    const glm::mat4 &trf = node->getRelativeTransform();
    const float *pSource = (const float *)glm::value_ptr(trf);
    std::stringstream ssMat;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            ssMat << pSource[c * 4 + r] << " ";
        }
    }
    XMLElement *matEl = doc.NewElement("matrix");
    matEl->SetText(ssMat.str().c_str());
    xmlNode->InsertEndChild(matEl);

    // A geometry shared by several instance nodes is written once; every
    // further node just references it with <instance_geometry> — mirroring
    // how the file was loaded.
    const auto alreadyExported =
        mesh ? exportedGeoms.find(mesh->getGeometry()) : exportedGeoms.end();
    if (mesh && alreadyExported != exportedGeoms.end())
    {
        XMLElement *instGeom = doc.NewElement("instance_geometry");
        instGeom->SetAttribute("url",
                               ("#" + alreadyExported->second).c_str());
        xmlNode->InsertEndChild(instGeom);
    }
    else if (mesh)
    {
        exportedGeoms[mesh->getGeometry()] = geomId;
        XMLElement *geom = doc.NewElement("geometry");
        geom->SetAttribute("id", geomId.c_str());
        XMLElement *xmlMesh = doc.NewElement("mesh");
        geom->InsertEndChild(xmlMesh);

        XMLElement *source = doc.NewElement("source");
        std::string sourceId = geomId + "-positions";
        source->SetAttribute("id", sourceId.c_str());

        XMLElement *floatArray = doc.NewElement("float_array");
        floatArray->SetAttribute("id", (sourceId + "-array").c_str());

        const auto &positions = mesh->getHalfEdgeTable().getPositions();
        std::stringstream ssPos;
        for (const auto &p : positions)
        {
            ssPos << p.x << " " << p.y << " " << p.z << " ";
        }
        floatArray->SetText(ssPos.str().c_str());
        floatArray->SetAttribute("count", (int)(positions.size() * 3));
        source->InsertEndChild(floatArray);
        xmlMesh->InsertEndChild(source);

        XMLElement *xmlVertices = doc.NewElement("vertices");
        xmlVertices->SetAttribute("id", (geomId + "-vertices").c_str());
        XMLElement *input = doc.NewElement("input");
        input->SetAttribute("semantic", "POSITION");
        input->SetAttribute("source", ("#" + sourceId).c_str());
        xmlVertices->InsertEndChild(input);
        xmlMesh->InsertEndChild(xmlVertices);

        XMLElement *polylist = doc.NewElement("polylist");
        XMLElement *inputPoly = doc.NewElement("input");
        inputPoly->SetAttribute("semantic", "VERTEX");
        inputPoly->SetAttribute("source", ("#" + geomId + "-vertices").c_str());
        inputPoly->SetAttribute("offset", "0");
        polylist->InsertEndChild(inputPoly);

        std::stringstream ssVcount;
        std::stringstream ssP;
        const auto &faces = mesh->getHalfEdgeTable().getFaces();
        int faceCount = 0;
        for (const auto &f : faces)
        {
            if (f.heh.index == -1)
                continue;

            int vc = 0;
            HalfEdgeHandle start = f.heh;
            HalfEdgeHandle curr = start;
            do
            {
                ssP << mesh->getHalfEdgeTable().deref(curr).dst.index << " ";
                vc++;
                curr = mesh->getHalfEdgeTable().next(curr);
            } while (curr != start && curr.index != -1);

            ssVcount << vc << " ";
            faceCount++;
        }

        polylist->SetAttribute("count", faceCount);

        XMLElement *xmlVcount = doc.NewElement("vcount");
        xmlVcount->SetText(ssVcount.str().c_str());
        polylist->InsertEndChild(xmlVcount);

        XMLElement *xmlP = doc.NewElement("p");
        xmlP->SetText(ssP.str().c_str());
        polylist->InsertEndChild(xmlP);

        xmlMesh->InsertEndChild(polylist);
        libGeom->InsertEndChild(geom);

        XMLElement *instGeom = doc.NewElement("instance_geometry");
        instGeom->SetAttribute("url", ("#" + geomId).c_str());
        xmlNode->InsertEndChild(instGeom);
    }

    // Recursively export child nodes
    for (const auto &child : node->getChildren())
    {
        exportNode(doc, xmlNode, child.get(), geomCounter, libGeom,
                   exportedGeoms);
    }

    parentXml->InsertEndChild(xmlNode);
}

void saveModel(const Model &model, const std::string &filename)
{
    logMessage("Exporting COLLADA to: " + filename);
    XMLDocument doc;

    // 1. Add standard XML declaration
    XMLDeclaration *decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    // 2. Add proper uppercase COLLADA root tag with namespace and version
    XMLElement *collada = doc.NewElement("COLLADA");
    collada->SetAttribute("xmlns",
                          "http://www.collada.org/2005/11/COLLADASchema");
    collada->SetAttribute("version", "1.4.1");
    doc.InsertEndChild(collada);

    // 3. Add required <asset> block
    XMLElement *asset = doc.NewElement("asset");
    XMLElement *upAxis = doc.NewElement("up_axis");
    upAxis->SetText("Y_UP");
    asset->InsertEndChild(upAxis);
    collada->InsertEndChild(asset);

    // Library Geometries
    XMLElement *libGeom = doc.NewElement("library_geometries");
    collada->InsertEndChild(libGeom);

    // Library Visual Scenes
    XMLElement *libVisScenes = doc.NewElement("library_visual_scenes");
    collada->InsertEndChild(libVisScenes);
    XMLElement *visualScene = doc.NewElement("visual_scene");
    visualScene->SetAttribute("id", "Scene");
    libVisScenes->InsertEndChild(visualScene);

    int geomCounter = 1;
    std::map<const Geometry *, std::string> exportedGeoms;
    for (const auto &node : model.getNodes())
    {
        exportNode(doc, visualScene, node.get(), geomCounter, libGeom,
                   exportedGeoms);
    }

    // 4. Add the required <scene> block at the end to instantiate the scene
    XMLElement *sceneEl = doc.NewElement("scene");
    XMLElement *instVisScene = doc.NewElement("instance_visual_scene");
    instVisScene->SetAttribute("url", "#Scene");
    sceneEl->InsertEndChild(instVisScene);
    collada->InsertEndChild(sceneEl);

    // Save
    doc.SaveFile(filename.c_str());
    logMessage("Successfully exported COLLADA model.");
}