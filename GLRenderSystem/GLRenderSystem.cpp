#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "GLRenderSystem.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <iomanip>
#include <string>

namespace
{
const char *debugSourceToString(GLenum source)
{
    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WindowSystem";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "ShaderCompiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "ThirdParty";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "Application";
    case GL_DEBUG_SOURCE_OTHER:
        return "Other";
    default:
        return "Unknown";
    }
}

const char *debugTypeToString(GLenum type)
{
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DeprecatedBehavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UndefinedBehavior";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "Performance";
    case GL_DEBUG_TYPE_MARKER:
        return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP:
        return "PushGroup";
    case GL_DEBUG_TYPE_POP_GROUP:
        return "PopGroup";
    case GL_DEBUG_TYPE_OTHER:
        return "Other";
    default:
        return "Unknown";
    }
}

// GL error checking is a performance trap in the render loop: glGetError()
// and GL_DEBUG_OUTPUT_SYNCHRONOUS both force the driver to synchronize with
// the GPU, collapsing the pipeline (the CPU stalls until multi-million-vertex
// draws finish before issuing the next call). Diagnostics are therefore
// opt-in: set MESHEDITOR_GL_DEBUG=1 to enable them.
bool glDiagnosticsEnabled()
{
    static const bool enabled = []
    {
        const char *env = std::getenv("MESHEDITOR_GL_DEBUG");
        return env && *env && *env != '0';
    }();
    return enabled;
}

const char *debugSeverityToString(GLenum severity)
{
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        return "High";
    case GL_DEBUG_SEVERITY_MEDIUM:
        return "Medium";
    case GL_DEBUG_SEVERITY_LOW:
        return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        return "Notification";
    default:
        return "Unknown";
    }
}
} // namespace

void APIENTRY GLRenderSystem::debugCallback(GLenum source, GLenum type,
                                            GLuint id, GLenum severity,
                                            GLsizei length,
                                            const GLchar *message,
                                            const void *userParam)
{
    static_cast<void>(length);
    static_cast<void>(userParam);

    if (type != GL_DEBUG_TYPE_ERROR || severity != GL_DEBUG_SEVERITY_HIGH)
        return;

    std::cerr << "[OpenGL] Error from " << debugSourceToString(source)
              << " (id=" << id << "): " << message << std::endl;
}

void GLRenderSystem::logOpenGLErrors(const char *stage)
{
    if (!glDiagnosticsEnabled())
        return;

    GLenum error = GL_NO_ERROR;
    bool foundError = false;
    while ((error = glGetError()) != GL_NO_ERROR)
    {
        foundError = true;
        std::cerr << "[OpenGL] Error after " << stage << ": 0x" << std::hex
                  << error << std::dec << std::endl;
    }

    static_cast<void>(foundError);
}

GLRenderSystem::~GLRenderSystem()
{
    if (m_SkyVao)
        glDeleteVertexArrays(1, &m_SkyVao);
    if (m_QuadVao)
        glDeleteVertexArrays(1, &m_QuadVao);
    for (auto &entry : m_Textures)
    {
        if (entry.second)
            glDeleteTextures(1, &entry.second);
    }
    if (m_PickColorTex)
        glDeleteTextures(1, &m_PickColorTex);
    if (m_PickDepthRbo)
        glDeleteRenderbuffers(1, &m_PickDepthRbo);
    if (m_PickFbo)
        glDeleteFramebuffers(1, &m_PickFbo);

    if (m_HdrFbo)
        glDeleteFramebuffers(1, &m_HdrFbo);
    if (m_HdrColorTex)
        glDeleteTextures(1, &m_HdrColorTex);
    if (m_HdrDepthRbo)
        glDeleteRenderbuffers(1, &m_HdrDepthRbo);
        
    if (m_PingPongFbo[0])
        glDeleteFramebuffers(2, m_PingPongFbo);
    if (m_PingPongColorTex[0])
        glDeleteTextures(2, m_PingPongColorTex);
}
GLRenderSystem::GLRenderSystem() = default;
void GLRenderSystem::init()
{
    // The GL context may come from a host that is not GLFW (e.g. a Qt
    // QOpenGLWidget). gladLoadGL() resolves function pointers through
    // opengl32.dll/wglGetProcAddress against whatever context is current.
    if (!GLVersion.major)
    {
        if (!gladLoadGL())
        {
            std::cerr << "GLRenderSystem::init: gladLoadGL failed (no current "
                         "GL context?)"
                      << std::endl;
            return;
        }
    }

    // Which GPU did the OS give us? On dual-GPU systems OpenGL defaults to
    // the integrated GPU unless the exe opts into the discrete one (see the
    // NvOptimusEnablement / AmdPowerXpressRequestHighPerformance exports in
    // MeshEditor/main.cpp). This line in the log console is the ground truth.
    std::cerr << "[GL] Renderer: " << glGetString(GL_RENDERER)
              << " | Vendor: " << glGetString(GL_VENDOR)
              << " | Version: " << glGetString(GL_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);

    if (glDiagnosticsEnabled() && GLAD_GL_VERSION_4_3 && glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLRenderSystem::debugCallback, nullptr);
    }

    logOpenGLErrors("GLRenderSystem::init setup");

    if (m_RenderShader)
        return;

    std::string shaderDir = "./shaders";
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length > 0)
    {
        std::string exePath(buffer, length);
        size_t pos = exePath.find_last_of("\\/");
        if (pos != std::string::npos)
        {
            shaderDir = exePath.substr(0, pos) + "/shaders";
        }
    }
#else
    try
    {
        auto exeDir = std::filesystem::canonical("/proc/self/exe").parent_path();
        if (std::filesystem::exists(exeDir / "shaders"))
        {
            shaderDir = (exeDir / "shaders").string();
        }
    }
    catch (...)
    {
    }
#endif

    // NOTE: no geometry stage here. m_RenderShader draws immediate-mode
    // triangle soup AND lines/polylines (axis handles, AABB/octree boxes). The
    // black-edge geometry shader takes `triangles` in, so attaching it here
    // makes every GL_LINES draw an INVALID_OPERATION. Black edges are handled
    // by m_MeshShader instead.
    m_RenderShader = std::make_unique<Shader>((shaderDir + "/vertex.glsl"),
                                              (shaderDir + "/fragment.glsl"));

    if (m_RenderShader->GetID() == 0)
    {
        std::cerr << "Failed to create render shader from: " << shaderDir
                  << std::endl;
        m_RenderShader.reset();
    }

    m_MeshShader = std::make_unique<Shader>((shaderDir + "/vertex.glsl"),
                                            (shaderDir + "/mesh_fragment.glsl"),
                                            (shaderDir + "/geometry.glsl"));

    if (m_MeshShader->GetID() == 0)
    {
        std::cerr << "Failed to create mesh shader from: " << shaderDir
                  << std::endl;
        m_MeshShader.reset();
    }

    // Colour-id picking program (position-only vertex + id-encoding fragment).
    m_PickShader = std::make_unique<Shader>(
        (shaderDir + "/vertex_pick.glsl"), (shaderDir + "/fragment_pick.glsl"));
    if (m_PickShader->GetID() == 0)
    {
        std::cerr << "Failed to create pick shader from: " << shaderDir
                  << std::endl;
        m_PickShader.reset();
    }

    // Environment panoramic sky background shader.
    m_SkyShader = std::make_unique<Shader>(
        (shaderDir + "/sky_vertex.glsl"), (shaderDir + "/sky_fragment.glsl"));
    if (m_SkyShader->GetID() == 0)
    {
        std::cerr << "Failed to create sky shader from: " << shaderDir
                  << std::endl;
        m_SkyShader.reset();
    }

    m_BloomExtractShader = std::make_unique<Shader>(
        (shaderDir + "/postprocess_vert.glsl"), (shaderDir + "/bloom_extract_frag.glsl"));
    
    m_BloomBlurShader = std::make_unique<Shader>(
        (shaderDir + "/postprocess_vert.glsl"), (shaderDir + "/bloom_blur_frag.glsl"));
        
    m_PostProcessShader = std::make_unique<Shader>(
        (shaderDir + "/postprocess_vert.glsl"), (shaderDir + "/postprocess_frag.glsl"));

    // Assign uniform texture units 0..18 for PBR textures
    const char* slotNames[19] = {
        "uTexture", "uNormalTexture", "uBumpTexture", "uMetallicRoughnessTexture", "uEmissiveTexture",
        "uClearcoatTexture", "uClearcoatRoughnessTexture", "uClearcoatNormalTexture",
        "uSheenColorTexture", "uSheenRoughnessTexture", "uTransmissionTexture", "uThicknessTexture",
        "uSpecularTexture", "uSpecularColorTexture", "uIridescenceTexture", "uIridescenceThicknessTexture",
        "uAnisotropyTexture", "uDiffuseTransmissionTexture", "uDiffuseTransmissionColorTexture"
    };

    if (m_RenderShader)
    {
        for (int i = 0; i < 19; ++i) m_RenderShader->SetInt(slotNames[i], i);
    }
    if (m_MeshShader)
    {
        for (int i = 0; i < 19; ++i) m_MeshShader->SetInt(slotNames[i], i);
    }
    // Configure default 3-point studio lighting
    m_Lights[0].type = 1; // Directional Key light
    m_Lights[0].direction = glm::normalize(glm::vec3(-0.6f, -0.8f, -0.5f));
    m_Lights[0].color = glm::vec3(1.0f, 0.98f, 0.95f);
    m_Lights[0].enabled = true;

    m_Lights[1].type = 1; // Directional Fill light
    m_Lights[1].direction = glm::normalize(glm::vec3(0.6f, -0.3f, -0.4f));
    m_Lights[1].color = glm::vec3(0.40f, 0.42f, 0.48f);
    m_Lights[1].enabled = true;

    m_Lights[2].type = 1; // Directional Back/Rim light
    m_Lights[2].direction = glm::normalize(glm::vec3(0.0f, 0.8f, 0.6f));
    m_Lights[2].color = glm::vec3(0.30f, 0.30f, 0.35f);
    m_Lights[2].enabled = true;

    for (size_t i = 3; i < m_Lights.size(); ++i)
    {
        m_Lights[i].enabled = false;
    }
    ++m_lightsVersion;
    
    glGenVertexArrays(1, &m_QuadVao);

    logOpenGLErrors("GLRenderSystem::init shader creation");
}

void GLRenderSystem::clearDisplay(float r, float g, float b)
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
        m_GarbageBuffers.clear();
        if (!m_GarbageTextures.empty())
        {
            glDeleteTextures(static_cast<GLsizei>(m_GarbageTextures.size()), m_GarbageTextures.data());
            m_GarbageTextures.clear();
        }
    }

    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // New frame — context state (bound program/VAO/texture) may have been
    // changed by the host (Qt compositing) since the last frame. Program
    // uniform values persist per-program, so the version caches stay valid.
    m_boundProgramId = 0;
    m_boundTextures.fill(0);
    m_lastBoundVaoKey = nullptr;
    logOpenGLErrors("clearDisplay");
}

void GLRenderSystem::uploadVertices(const std::vector<Vertex> &m_vertices)
{
    if (!m_RenderShader || m_RenderShader->GetID() == 0 || m_vertices.empty())
        return;

    applyShaderState(*m_RenderShader, m_RenderState);
    setForceFlatColor(*m_RenderShader, m_RenderState, 0);
    bindActiveTexture(*m_RenderShader, m_RenderState);

    if (!m_Buffer.vao)
    {
        m_Buffer.vbo =
            std::make_unique<VertexBuffer>(m_vertices.size() * sizeof(Vertex));
        m_Buffer.vao = std::make_unique<VertexArray>();
        m_Buffer.vao->attachVertexBuffer(*m_Buffer.vbo, sizeof(Vertex));
        m_Buffer.vao->addAttribute(0, 3, offsetof(Vertex, position));
        m_Buffer.vao->addAttribute(1, 3, offsetof(Vertex, normal));
        m_Buffer.vao->addAttribute(2, 3, offsetof(Vertex, color));
        m_Buffer.vao->addAttribute(3, 2, offsetof(Vertex, uv));
        m_Buffer.vao->addAttribute(4, 4, offsetof(Vertex, tangent));
    }

    m_Buffer.vbo->SetData(m_vertices.data(),
                          m_vertices.size() * sizeof(Vertex));

    m_Buffer.vao->bind();
    m_lastBoundVaoKey = nullptr; // immediate-mode VAO replaced the keyed one
    logOpenGLErrors("uploadVertices");
}

void GLRenderSystem::renderTriangleSoup(const std::vector<Vertex> &m_vertices)
{
    if (m_vertices.empty())
        return;
    uploadVertices(m_vertices);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    logOpenGLErrors("renderTriangleSoup");
}

void GLRenderSystem::renderLines(const std::vector<Vertex> &m_vertices)
{
    if (m_vertices.empty())
        return;
    uploadVertices(m_vertices);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    logOpenGLErrors("renderLines");
}

void GLRenderSystem::renderPolyline(const std::vector<Vertex> &m_vertices)
{
    if (m_vertices.empty())
        return;
    uploadVertices(m_vertices);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));
    logOpenGLErrors("renderPolyline");
}

// ── Incremental triangle soup API ──────────────────────────────────────

void GLRenderSystem::uploadTriangleSoup(const void *meshKey,
                                        const std::vector<Vertex> &vertices)
{
    if (!m_RenderShader || m_RenderShader->GetID() == 0 || vertices.empty() ||
        !meshKey)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto &buf = m_PersistentBuffers[meshKey];
    if (!buf.vao)
    {
        buf.vbo =
            std::make_unique<VertexBuffer>(vertices.size() * sizeof(Vertex));
        buf.vao = std::make_unique<VertexArray>();
        buf.vao->attachVertexBuffer(*buf.vbo, sizeof(Vertex));
        buf.vao->addAttribute(0, 3, offsetof(Vertex, position));
        buf.vao->addAttribute(1, 3, offsetof(Vertex, normal));
        buf.vao->addAttribute(2, 3, offsetof(Vertex, color));
        buf.vao->addAttribute(3, 2, offsetof(Vertex, uv));
        buf.vao->addAttribute(4, 4, offsetof(Vertex, tangent));
    }

    buf.vbo->SetData(vertices.data(), vertices.size() * sizeof(Vertex));
    logOpenGLErrors("uploadTriangleSoup");
}

void GLRenderSystem::uploadIndexedTriangleSoup(
    const void *meshKey, const std::vector<Vertex> &vertices,
    const std::vector<unsigned int> &indices)
{
    if (!m_RenderShader || m_RenderShader->GetID() == 0 || vertices.empty() ||
        indices.empty() || !meshKey)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto &buf = m_PersistentBuffers[meshKey];
    if (!buf.vao)
    {
        buf.vbo =
            std::make_unique<VertexBuffer>(vertices.size() * sizeof(Vertex));
        buf.ibo = std::make_unique<IndexBuffer>(indices.size() *
                                                sizeof(unsigned int));
        buf.vao = std::make_unique<VertexArray>();
        buf.vao->attachVertexBuffer(*buf.vbo, sizeof(Vertex));
        buf.vao->attachIndexBuffer(*buf.ibo);
        buf.vao->addAttribute(0, 3, offsetof(Vertex, position));
        buf.vao->addAttribute(1, 3, offsetof(Vertex, normal));
        buf.vao->addAttribute(2, 3, offsetof(Vertex, color));
        buf.vao->addAttribute(3, 2, offsetof(Vertex, uv));
        buf.vao->addAttribute(4, 4, offsetof(Vertex, tangent));
    }

    buf.vbo->SetData(vertices.data(), vertices.size() * sizeof(Vertex));
    buf.ibo->SetData(indices.data(), indices.size() * sizeof(unsigned int));
    logOpenGLErrors("uploadIndexedTriangleSoup");
}

void GLRenderSystem::updateTriangleSoupRange(const void *meshKey,
                                             const Vertex *data,
                                             size_t startVertex,
                                             size_t vertexCount)
{
    if (!meshKey || !data || vertexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vbo)
        return;

    const size_t byteOffset = startVertex * sizeof(Vertex);
    const size_t byteSize = vertexCount * sizeof(Vertex);
    it->second.vbo->SetSubData(data, byteOffset, byteSize);
    logOpenGLErrors("updateTriangleSoupRange");
}

void GLRenderSystem::updateIndexedTriangleSoupRange(
    const void *meshKey, const Vertex *vertexData, size_t startVertex,
    size_t vertexCount, const unsigned int *indexData, size_t startIndex,
    size_t indexCount)
{
    if (!meshKey || !vertexData || !indexData || vertexCount == 0 ||
        indexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vbo || !it->second.ibo)
        return;

    const size_t vertexByteOffset = startVertex * sizeof(Vertex);
    const size_t vertexByteSize = vertexCount * sizeof(Vertex);
    const size_t indexByteOffset = startIndex * sizeof(unsigned int);
    const size_t indexByteSize = indexCount * sizeof(unsigned int);

    it->second.vbo->SetSubData(vertexData, vertexByteOffset, vertexByteSize);
    it->second.ibo->SetSubData(indexData, indexByteOffset, indexByteSize);
    logOpenGLErrors("updateIndexedTriangleSoupRange");
}

void GLRenderSystem::drawTriangleSoup(const void *meshKey, size_t vertexCount)
{
    if (!m_RenderShader || m_RenderShader->GetID() == 0 || !meshKey ||
        vertexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vao)
        return;

    applyShaderState(*m_RenderShader, m_RenderState);
    bindActiveTexture(*m_RenderShader, m_RenderState);

    it->second.vao->bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
    m_lastBoundVaoKey = meshKey;
    logOpenGLErrors("drawTriangleSoup");
}

void GLRenderSystem::drawLineBuffer(const void *meshKey, size_t vertexCount)
{
    if (!m_RenderShader || m_RenderShader->GetID() == 0 || !meshKey ||
        vertexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vao)
        return;

    applyShaderState(*m_RenderShader, m_RenderState);
    setForceFlatColor(*m_RenderShader, m_RenderState, 0);
    bindActiveTexture(*m_RenderShader, m_RenderState);

    it->second.vao->bind();
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount));
    m_lastBoundVaoKey = meshKey;
    logOpenGLErrors("drawLineBuffer");
}

void GLRenderSystem::drawIndexedTriangleSoup(const void *meshKey,
                                             size_t indexCount,
                                             bool renderWireframe)
{
    if (!meshKey || indexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vao || !it->second.ibo)
        return;

    // Fast path: when the black-edge overlay is OFF, draw with the plain
    // vertex+fragment program (m_RenderShader). The geometry shader in
    // m_MeshShader exists only to inject per-triangle barycentric coords for
    // the wireframe; routing every triangle through a geometry stage each
    // frame throttles GPU throughput dramatically (an order of magnitude on
    // multi-million-triangle meshes), so it is used ONLY when actually needed.
    if (!renderWireframe)
    {
        if (!m_RenderShader || m_RenderShader->GetID() == 0)
            return;

        applyShaderState(*m_RenderShader, m_RenderState);
        setForceFlatColor(*m_RenderShader, m_RenderState, 0);
        bindActiveTexture(*m_RenderShader, m_RenderState);

        it->second.vao->bind();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                       GL_UNSIGNED_INT, nullptr);

        m_lastBoundVaoKey = meshKey;
        logOpenGLErrors("drawIndexedTriangleSoup");
        return;
    }

    if (!m_MeshShader || m_MeshShader->GetID() == 0)
        return;

    applyShaderState(*m_MeshShader, m_MeshState);
    setForceFlatColor(*m_MeshShader, m_MeshState, 0);
    m_MeshShader->SetInt("renderWireframe", 1);
    bindActiveTexture(*m_MeshShader, m_MeshState);

    it->second.vao->bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                   GL_UNSIGNED_INT, nullptr);

    m_lastBoundVaoKey = meshKey; // VAO is still bound on GPU
    logOpenGLErrors("drawIndexedTriangleSoup");
}

void GLRenderSystem::drawIndexedWireframe(const void *meshKey,
                                          size_t indexCount)
{
    if (!m_MeshShader || m_MeshShader->GetID() == 0 || !meshKey ||
        indexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vao || !it->second.ibo)
        return;

    // When drawIndexedTriangleSoup just ran with the same shader, every step
    // below is a version-checked no-op except the flat-colour override.
    applyShaderState(*m_MeshShader, m_MeshState);

    setForceFlatColor(*m_MeshShader, m_MeshState, 1);
    m_MeshShader->SetVec3("flatColor", glm::vec3(0.0f));
    m_MeshShader->SetInt("renderWireframe", 0);

    // Skip vao->bind() if it's already bound from drawIndexedTriangleSoup.
    if (m_lastBoundVaoKey != meshKey)
    {
        it->second.vao->bind();
        m_lastBoundVaoKey = meshKey;
    }
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                   GL_UNSIGNED_INT, nullptr);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);

    // Restore forceFlatColor for subsequent draw calls.
    setForceFlatColor(*m_MeshShader, m_MeshState, 0);
    logOpenGLErrors("drawIndexedWireframe");
}

void GLRenderSystem::releaseBuffer(const void *meshKey)
{
    if (meshKey)
    {
        std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
        if (m_lastBoundVaoKey == meshKey)
            m_lastBoundVaoKey = nullptr;
        auto it = m_PersistentBuffers.find(meshKey);
        if (it != m_PersistentBuffers.end())
        {
            m_GarbageBuffers.push_back(std::move(it->second));
            m_PersistentBuffers.erase(it);
        }
    }
}

// ── Textures ───────────────────────────────────────────────────────────────

void GLRenderSystem::bindActiveTexture(Shader &shader, ShaderStateCache &state)
{
    std::array<GLuint, 19> tex = {0};
    {
        std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
        if (m_activeTextureKey)
        {
            for (int i = 0; i < 19; ++i)
            {
                auto it = m_Textures.find(reinterpret_cast<const char *>(m_activeTextureKey) + i);
                if (it != m_Textures.end())
                    tex[i] = it->second;
            }
        }
    }

    for (int i = 0; i < 19; ++i)
    {
        if (tex[i] && tex[i] != m_boundTextures[i])
        {
            glBindTextureUnit(i, tex[i]);
            m_boundTextures[i] = tex[i];
        }

        const int useTex = tex[i] ? 1 : 0;
        if (state.useTexture[i] != useTex)
        {
            shader.SetInt(state.locUseTexture[i], useTex);
            state.useTexture[i] = useTex;
        }
    }
}

void GLRenderSystem::uploadTexture(const void *texKey, int width, int height,
                                   int channels, const unsigned char *pixels,
                                   TextureSlot slot)
{
    if (!texKey || !pixels || width <= 0 || height <= 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    GLuint &tex = m_Textures[texKey];
    if (tex == 0)
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    // Per glTF 2.0 spec these slots store sRGB-encoded data; the GPU
    // must linearise on read.  All other slots (normal, occlusion,
    // metallic-roughness, transmission, etc.) are linear.
    const bool isSrgb = (slot == TextureSlot::Diffuse              ||
                         slot == TextureSlot::Emissive              ||
                         slot == TextureSlot::SheenColor            ||
                         slot == TextureSlot::SpecularColor         ||
                         slot == TextureSlot::DiffuseTransmissionColor);

    GLenum internalFormat = GL_RGBA8;
    GLenum dataFormat     = GL_RGBA;
    if (channels == 1)
    {
        internalFormat = GL_R8;
        dataFormat = GL_RED;
    }
    else if (channels == 2)
    {
        internalFormat = GL_RG8;
        dataFormat = GL_RG;
    }
    else if (channels == 3)
    {
        internalFormat = isSrgb ? GL_SRGB8 : GL_RGB8;
        dataFormat = GL_RGB;
    }
    else // channels == 4
    {
        internalFormat = isSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        dataFormat = GL_RGBA;
    }

    // Tightly-packed rows may not be 4-byte aligned (e.g. RGB or 1-channel).
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat,
                 GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for (int i = 0; i < 20; ++i)
    {
        if (m_boundTextures[i] == tex)
            m_boundTextures[i] = 0;
    }
    logOpenGLErrors("uploadTexture");
}

void GLRenderSystem::setActiveTexture(const void *texKey)
{
    m_activeTextureKey = texKey;
}

void GLRenderSystem::releaseTexture(const void *texKey)
{
    if (!texKey)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_Textures.find(texKey);
    if (it == m_Textures.end())
        return;
    if (it->second)
    {
        for (int i = 0; i < 20; ++i)
        {
            if (m_boundTextures[i] == it->second)
                m_boundTextures[i] = 0;
        }
        m_GarbageTextures.push_back(it->second);
    }
    m_Textures.erase(it);
    if (m_activeTextureKey == texKey)
        m_activeTextureKey = nullptr;
}

void GLRenderSystem::setOverlayMode(bool enable)
{
    m_overlayMode = enable;
    if (enable)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// ── FBO colour-id picking ────────────────────────────────────────────────────

void GLRenderSystem::ensurePickFramebuffer(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    if (m_PickFbo != 0 && width == m_PickWidth && height == m_PickHeight)
        return;

    if (m_PickColorTex)
        glDeleteTextures(1, &m_PickColorTex);
    if (m_PickDepthRbo)
        glDeleteRenderbuffers(1, &m_PickDepthRbo);
    if (m_PickFbo)
        glDeleteFramebuffers(1, &m_PickFbo);

    glGenFramebuffers(1, &m_PickFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PickFbo);

    glGenTextures(1, &m_PickColorTex);
    glBindTexture(GL_TEXTURE_2D, m_PickColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_PickColorTex, 0);

    glGenRenderbuffers(1, &m_PickDepthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_PickDepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_PickDepthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[GL] Pick framebuffer incomplete" << std::endl;

    m_PickWidth = width;
    m_PickHeight = height;
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    // Caller (beginPickPass) rebinds to the pick FBO; leave it bound here.
}

void GLRenderSystem::beginPickPass(int width, int height)
{
    // Remember the caller's framebuffer + viewport (Qt renders into its own
    // FBO, never framebuffer 0) so endPickPass can restore them exactly.
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_SavedFbo);
    glGetIntegerv(GL_VIEWPORT, m_SavedViewport);

    ensurePickFramebuffer(width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PickFbo);
    glViewport(0, 0, width, height);
    // Ids must be written verbatim: nearest triangle wins (depth test), no
    // blending or dithering to perturb the encoded colour.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    // Clear to 0xFFFFFF (== "no hit"): white encodes id 0x00FFFFFF.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_boundProgramId = 0; // picks can run before clearDisplay (deferred mouse)
    m_lastBoundVaoKey = nullptr;
    logOpenGLErrors("beginPickPass");
}

void GLRenderSystem::drawIndexedForPick(const void *meshKey, size_t indexCount,
                                        uint32_t baseId)
{
    if (!m_PickShader || m_PickShader->GetID() == 0 || !meshKey ||
        indexCount == 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_resourceMutex);
    auto it = m_PersistentBuffers.find(meshKey);
    if (it == m_PersistentBuffers.end() || !it->second.vao || !it->second.ibo)
        return;

    const glm::mat4 worldViewProj = m_ViewProjectionMatrix * m_WorldMatrix;
    bindProgram(*m_PickShader);
    m_PickShader->SetMat4("worldViewProj", worldViewProj);
    m_PickShader->SetInt("uBaseId", static_cast<int>(baseId));

    it->second.vao->bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                   GL_UNSIGNED_INT, nullptr);
    logOpenGLErrors("drawIndexedForPick");
}

uint32_t GLRenderSystem::readPickId(int x, int y)
{
    if (m_PickFbo == 0)
        return 0xFFFFFFFFu;
    if (x < 0 || y < 0 || x >= m_PickWidth || y >= m_PickHeight)
        return 0xFFFFFFFFu;

    unsigned char px[4] = {255, 255, 255, 255};
    glBindFramebuffer(GL_FRAMEBUFFER, m_PickFbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    logOpenGLErrors("readPickId");

    return static_cast<uint32_t>(px[0]) | (static_cast<uint32_t>(px[1]) << 8) |
           (static_cast<uint32_t>(px[2]) << 16);
}

void GLRenderSystem::endPickPass()
{
    glEnable(GL_DITHER); // restore default state for normal rendering
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_SavedFbo));
    glViewport(m_SavedViewport[0], m_SavedViewport[1], m_SavedViewport[2],
               m_SavedViewport[3]);
    m_lastBoundVaoKey = nullptr;
    logOpenGLErrors("endPickPass");
}

void GLRenderSystem::setupLight(unsigned int index, glm::vec3 position,
                                glm::vec3 color)
{
    if (index >= m_Lights.size())
        return;

    m_Lights[index].type = 0;
    m_Lights[index].position = position;
    m_Lights[index].direction = glm::vec3(0.0f, -1.0f, 0.0f);
    m_Lights[index].color = color;
    ++m_lightsVersion;
}

void GLRenderSystem::setupDirectionalLight(unsigned int index,
                                           glm::vec3 direction, glm::vec3 color)
{
    if (index >= m_Lights.size())
        return;

    m_Lights[index].type = 1;
    m_Lights[index].position = glm::vec3(0.0f);
    m_Lights[index].direction = direction;
    m_Lights[index].color = color;
    ++m_lightsVersion;
}

void GLRenderSystem::setMaterial(glm::vec3 ambient, glm::vec3 diffuse,
                                 glm::vec3 specular, float shininess)
{
    // Value-compare: with many meshes sharing one material (or the default),
    // most calls are redundant — skip the version bump so the per-draw sync
    // stays a no-op instead of re-uploading identical uniforms per mesh.
    if (m_Material.ambient == ambient && m_Material.diffuse == diffuse &&
        m_Material.specular == specular && m_Material.shininess == shininess)
        return;

    m_Material.ambient = ambient;
    m_Material.diffuse = diffuse;
    m_Material.specular = specular;
    m_Material.shininess = shininess;
    ++m_materialVersion;
}

void GLRenderSystem::turnLight(unsigned int index, bool enable)
{
    if (index >= m_Lights.size())
        return;

    m_Lights[index].enabled = enable;
    ++m_lightsVersion;
}

void GLRenderSystem::setShininess(float value)
{
    if (m_Material.shininess == value)
        return;
    m_Material.shininess = value;
    ++m_materialVersion;
}

void GLRenderSystem::setViewport(double x, double y, double width,
                                 double height)
{
    glViewport((unsigned int)x, (unsigned int)y, (unsigned int)width,
               (unsigned int)height);
    logOpenGLErrors("setViewport");
}

void GLRenderSystem::setLineWidth(float value)
{
    // Core-profile GL only guarantees a line width of 1.0; passing anything
    // outside GL_ALIASED_LINE_WIDTH_RANGE raises GL_INVALID_VALUE. Clamp so
    // thick-line requests degrade gracefully instead of spamming errors.
    static float s_minWidth = 0.0f;
    static float s_maxWidth = 0.0f;
    if (s_maxWidth == 0.0f)
    {
        GLfloat range[2] = {1.0f, 1.0f};
        glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
        s_minWidth = range[0];
        s_maxWidth = range[1];
        if (s_maxWidth < 1.0f)
            s_maxWidth = 1.0f;
    }

    glLineWidth(std::clamp(value, s_minWidth, s_maxWidth));
    logOpenGLErrors("setLineWidth");
}

void GLRenderSystem::setWorldMatrix(const glm::mat4 &matrix)
{
    m_WorldMatrix = matrix;
    ++m_matricesVersion;
}

const glm::mat4 &GLRenderSystem::getWorldMatrix() const
{
    return m_WorldMatrix;
}

void GLRenderSystem::setViewProjectionMatrix(const glm::mat4 &matrix)
{
    m_ViewProjectionMatrix = matrix;
    ++m_matricesVersion;
}

const glm::mat4 &GLRenderSystem::getViewProjectionMatrix() const
{
    return m_ViewProjectionMatrix;
}

void GLRenderSystem::setViewPosition(glm::vec3 value)
{
    if (m_ViewPosition == value)
        return;
    m_ViewPosition = value;
    ++m_matricesVersion;
}

const glm::vec3 &GLRenderSystem::getViewPosition() const
{
    return m_ViewPosition;
}

void GLRenderSystem::ensureDerivedMatrices()
{
    if (m_derivedMatricesVersion == m_matricesVersion)
        return;
    m_WorldViewProj = m_ViewProjectionMatrix * m_WorldMatrix;
    m_NormalMatrix = glm::transpose(glm::inverse(glm::mat3(m_WorldMatrix)));
    m_derivedMatricesVersion = m_matricesVersion;
}

void GLRenderSystem::setContrast(float contrast)
{
    m_contrast = std::clamp(contrast, 0.1f, 3.0f);
    ++m_colorGradingVersion;
}

void GLRenderSystem::setExposure(float exposure)
{
    m_exposure = std::clamp(exposure, 0.1f, 5.0f);
    ++m_colorGradingVersion;
}

void GLRenderSystem::setShadingMode(ShadingMode mode)
{
    if (m_shadingMode == mode)
        return;
    m_shadingMode = mode;
    ++m_shadingModeVersion;
}

void GLRenderSystem::setPbrMaterial(float metallic, float roughness,
                                    glm::vec3 emissive, float ior,
                                    float transmission)
{
    if (m_PbrMaterial.metallic == metallic &&
        m_PbrMaterial.roughness == roughness &&
        m_PbrMaterial.emissive == emissive &&
        m_PbrMaterial.ior == ior &&
        m_PbrMaterial.transmission == transmission)
        return;

    m_PbrMaterial.metallic = metallic;
    m_PbrMaterial.roughness = roughness;
    m_PbrMaterial.emissive = emissive;
    m_PbrMaterial.ior = ior;
    m_PbrMaterial.transmission = transmission;
    ++m_pbrMaterialVersion;
}

void GLRenderSystem::setPbrExtendedParams(const PbrMaterialParams& params)
{
    m_PbrExtendedParams = params;
    ++m_pbrExtendedVersion;
}

void GLRenderSystem::ensureOpaqueFramebuffer(int width, int height)
{
    if (m_OpaqueFbo != 0 && width == m_OpaqueWidth && height == m_OpaqueHeight)
        return;

    if (m_OpaqueColorTex) glDeleteTextures(1, &m_OpaqueColorTex);
    if (m_OpaqueDepthRbo) glDeleteRenderbuffers(1, &m_OpaqueDepthRbo);
    if (m_OpaqueFbo)      glDeleteFramebuffers(1, &m_OpaqueFbo);

    glGenFramebuffers(1, &m_OpaqueFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_OpaqueFbo);

    // Color attachment
    glGenTextures(1, &m_OpaqueColorTex);
    glBindTexture(GL_TEXTURE_2D, m_OpaqueColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_OpaqueColorTex, 0);

    // Depth attachment
    glGenRenderbuffers(1, &m_OpaqueDepthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_OpaqueDepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_OpaqueDepthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[GLRenderSystem] Opaque FBO incomplete\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_OpaqueWidth  = width;
    m_OpaqueHeight = height;
    logOpenGLErrors("ensureOpaqueFramebuffer");
}

void GLRenderSystem::beginOpaqueCapture()
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_OpaqueSavedFbo);
    glGetIntegerv(GL_VIEWPORT, m_OpaqueSavedViewport);

    int w = m_OpaqueSavedViewport[2];
    int h = m_OpaqueSavedViewport[3];
    if (w <= 0 || h <= 0)
    {
        w = static_cast<int>(m_screenSize.x);
        h = static_cast<int>(m_screenSize.y);
    }
    if (w <= 0 || h <= 0) return;
    
    m_screenSize = glm::vec2(w, h);
    ensureOpaqueFramebuffer(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, m_OpaqueFbo);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_opaquePassOnly = true;
    m_useOpaqueFramebuffer = false; // glass sampling off during capture
    logOpenGLErrors("beginOpaqueCapture");
}

void GLRenderSystem::endOpaqueCapture()
{
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_OpaqueSavedFbo));
    glViewport(m_OpaqueSavedViewport[0], m_OpaqueSavedViewport[1], m_OpaqueSavedViewport[2], m_OpaqueSavedViewport[3]);
    
    m_opaquePassOnly = false;
    m_useOpaqueFramebuffer = (m_OpaqueColorTex != 0);

    if (m_useOpaqueFramebuffer) {
        // Generate mipmaps for the frosted-glass roughness LOD sampling
        glBindTexture(GL_TEXTURE_2D, m_OpaqueColorTex);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        // Bind to dedicated unit 19 using DSA so the active-unit state is not
        // disturbed and the bind is tracked for proper cleanup.
        glBindTextureUnit(19, m_OpaqueColorTex);
        m_boundTextures[19] = m_OpaqueColorTex;
    }
    
    logOpenGLErrors("endOpaqueCapture");
}

void GLRenderSystem::setScreenSize(int width, int height)
{
    m_screenSize = glm::vec2(width, height);
}

void GLRenderSystem::renderEnvironmentBackground()
{
    if (!m_SkyShader || m_SkyShader->GetID() == 0 || !m_environmentBackground)
        return;

    if (!m_SkyVao)
    {
        glGenVertexArrays(1, &m_SkyVao);
    }

    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    bindProgram(*m_SkyShader);
    const glm::mat4 invViewProj = glm::inverse(m_ViewProjectionMatrix);
    m_SkyShader->SetMat4("invViewProj", invViewProj);
    m_SkyShader->SetVec3("viewPos", m_ViewPosition);

    glBindVertexArray(m_SkyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    m_lastBoundVaoKey = nullptr;
    logOpenGLErrors("renderEnvironmentBackground");
}

void GLRenderSystem::resolveLocations(Shader &shader, ShaderStateCache &state)
{
    if (state.locationsResolved)
        return;
    state.locWorld = shader.uniformLocation("world");
    state.locWorldViewProj = shader.uniformLocation("worldViewProj");
    state.locNormalMatrix = shader.uniformLocation("normalMatrix");
    state.locViewPos = shader.uniformLocation("viewPos");
    state.locMatAmbient = shader.uniformLocation("material.ambient");
    state.locMatDiffuse = shader.uniformLocation("material.diffuse");
    state.locMatSpecular = shader.uniformLocation("material.specular");
    state.locMatShininess = shader.uniformLocation("material.shininess");
    state.locMetallic = shader.uniformLocation("uMetallic");
    state.locRoughness = shader.uniformLocation("uRoughness");
    state.locEmissive = shader.uniformLocation("uEmissive");
    state.locIOR = shader.uniformLocation("uIOR");
    state.locTransmission = shader.uniformLocation("uTransmission");
    state.locShadingMode = shader.uniformLocation("shadingMode");
    state.locContrast = shader.uniformLocation("uContrast");
    state.locExposure = shader.uniformLocation("uExposure");
    state.locForceFlatColor = shader.uniformLocation("forceFlatColor");

    state.locAlphaMode = shader.uniformLocation("uAlphaMode");
    state.locAlphaCutoff = shader.uniformLocation("uAlphaCutoff");
    state.locDoubleSided = shader.uniformLocation("uDoubleSided");
    state.locUnlit = shader.uniformLocation("uUnlit");
    state.locClearcoatFactor = shader.uniformLocation("uClearcoatFactor");
    state.locClearcoatRoughness = shader.uniformLocation("uClearcoatRoughness");
    state.locSheenColor = shader.uniformLocation("uSheenColor");
    state.locSheenRoughness = shader.uniformLocation("uSheenRoughness");
    state.locThickness = shader.uniformLocation("uThickness");
    state.locAttenuationDistance = shader.uniformLocation("uAttenuationDistance");
    state.locAttenuationColor = shader.uniformLocation("uAttenuationColor");
    state.locSpecularFactor = shader.uniformLocation("uSpecularFactor");
    state.locSpecularColor = shader.uniformLocation("uSpecularColor");
    state.locEmissiveStrength = shader.uniformLocation("uEmissiveStrength");
    state.locIridescenceFactor = shader.uniformLocation("uIridescenceFactor");
    state.locIridescenceIor = shader.uniformLocation("uIridescenceIor");
    state.locIridescenceThicknessMin = shader.uniformLocation("uIridescenceThicknessMin");
    state.locIridescenceThicknessMax = shader.uniformLocation("uIridescenceThicknessMax");
    state.locAnisotropyStrength = shader.uniformLocation("uAnisotropyStrength");
    state.locAnisotropyRotation = shader.uniformLocation("uAnisotropyRotation");
    state.locDispersion = shader.uniformLocation("uDispersion");
    state.locDiffuseTransmissionFactor = shader.uniformLocation("uDiffuseTransmissionFactor");
    state.locDiffuseTransmissionColor = shader.uniformLocation("uDiffuseTransmissionColor");

    state.locOpaqueFramebuffer = shader.uniformLocation("uOpaqueFramebuffer");
    state.locUseOpaqueFramebuffer = shader.uniformLocation("uUseOpaqueFramebuffer");
    state.locScreenSize = shader.uniformLocation("uScreenSize");

    const char* slotUseNames[19] = {
        "useTexture", "useNormalMap", "useBumpMap", "useMetallicRoughnessMap", "useEmissiveMap",
        "useClearcoatMap", "useClearcoatRoughnessMap", "useClearcoatNormalMap",
        "useSheenColorMap", "useSheenRoughnessMap", "useTransmissionMap", "useThicknessMap",
        "useSpecularMap", "useSpecularColorMap", "useIridescenceMap", "useIridescenceThicknessMap",
        "useAnisotropyMap", "useDiffuseTransmissionMap", "useDiffuseTransmissionColorMap"
    };
    for (int i = 0; i < 19; ++i) {
        state.locUseTexture[i] = shader.uniformLocation(slotUseNames[i]);
    }

    state.locationsResolved = true;
}

void GLRenderSystem::bindProgram(Shader &shader)
{
    const GLuint id = shader.GetID();
    if (id == 0 || id == m_boundProgramId)
        return;
    shader.bind();
    m_boundProgramId = id;
}

void GLRenderSystem::applyShaderState(Shader &shader, ShaderStateCache &state)
{
    resolveLocations(shader, state);
    bindProgram(shader);
    uploadMatrices(shader, state);
    uploadLights(shader, state);
    uploadMaterial(shader, state);
    uploadPbrMaterial(shader, state);
}

void GLRenderSystem::setForceFlatColor(Shader &shader, ShaderStateCache &state,
                                       int value)
{
    if (state.forceFlatColor == value)
        return;
    shader.SetInt(state.locForceFlatColor, value);
    state.forceFlatColor = value;
}

void GLRenderSystem::uploadMatrices(Shader &shader, ShaderStateCache &state)
{
    if (state.matricesVersion == m_matricesVersion)
        return;

    ensureDerivedMatrices();
    shader.SetMat4(state.locWorld, m_WorldMatrix);
    shader.SetMat4(state.locWorldViewProj, m_WorldViewProj);
    shader.SetMat3(state.locNormalMatrix, m_NormalMatrix);
    shader.SetVec3(state.locViewPos, m_ViewPosition);
    state.matricesVersion = m_matricesVersion;
}

void GLRenderSystem::uploadLights(Shader &shader, ShaderStateCache &state)
{
    if (state.lightsVersion == m_lightsVersion)
        return;

    for (std::size_t i = 0; i < m_Lights.size(); ++i)
    {
        const auto &light = m_Lights[i];
        const std::string base = "lights[" + std::to_string(i) + "]";
        shader.SetInt(base + ".type", light.type);
        shader.SetVec3(base + ".position", light.position);
        shader.SetVec3(base + ".direction", light.direction);
        shader.SetVec3(base + ".color", light.color);
        shader.SetInt(base + ".enabled", light.enabled ? 1 : 0);
    }

    state.lightsVersion = m_lightsVersion;
}

void GLRenderSystem::uploadMaterial(Shader &shader, ShaderStateCache &state)
{
    if (state.materialVersion == m_materialVersion)
        return;

    shader.SetVec3(state.locMatAmbient, m_Material.ambient);
    shader.SetVec3(state.locMatDiffuse, m_Material.diffuse);
    shader.SetVec3(state.locMatSpecular, m_Material.specular);
    shader.SetFloat(state.locMatShininess, m_Material.shininess);
    state.materialVersion = m_materialVersion;
}

void GLRenderSystem::uploadPbrMaterial(Shader &shader, ShaderStateCache &state)
{
    if (state.pbrMaterialVersion != m_pbrMaterialVersion)
    {
        shader.SetFloat(state.locMetallic, m_PbrMaterial.metallic);
        shader.SetFloat(state.locRoughness, m_PbrMaterial.roughness);
        shader.SetVec3(state.locEmissive, m_PbrMaterial.emissive);
        shader.SetFloat(state.locIOR, m_PbrMaterial.ior);
        shader.SetFloat(state.locTransmission, m_PbrMaterial.transmission);
        state.pbrMaterialVersion = m_pbrMaterialVersion;
    }

    if (state.pbrExtendedVersion != m_pbrExtendedVersion)
    {
        shader.SetInt(state.locAlphaMode, static_cast<int>(m_PbrExtendedParams.alphaMode));
        shader.SetFloat(state.locAlphaCutoff, m_PbrExtendedParams.alphaCutoff);
        shader.SetInt(state.locDoubleSided, m_PbrExtendedParams.doubleSided ? 1 : 0);
        shader.SetInt(state.locUnlit, m_PbrExtendedParams.unlit ? 1 : 0);
        shader.SetFloat(state.locClearcoatFactor, m_PbrExtendedParams.clearcoatFactor);
        shader.SetFloat(state.locClearcoatRoughness, m_PbrExtendedParams.clearcoatRoughness);
        shader.SetVec3(state.locSheenColor, m_PbrExtendedParams.sheenColor);
        shader.SetFloat(state.locSheenRoughness, m_PbrExtendedParams.sheenRoughness);
        shader.SetFloat(state.locThickness, m_PbrExtendedParams.thickness);
        shader.SetFloat(state.locAttenuationDistance, m_PbrExtendedParams.attenuationDistance);
        shader.SetVec3(state.locAttenuationColor, m_PbrExtendedParams.attenuationColor);
        shader.SetFloat(state.locSpecularFactor, m_PbrExtendedParams.specularFactor);
        shader.SetVec3(state.locSpecularColor, m_PbrExtendedParams.specularColor);
        shader.SetFloat(state.locEmissiveStrength, m_PbrExtendedParams.emissiveStrength);
        shader.SetFloat(state.locIridescenceFactor, m_PbrExtendedParams.iridescenceFactor);
        shader.SetFloat(state.locIridescenceIor, m_PbrExtendedParams.iridescenceIor);
        shader.SetFloat(state.locIridescenceThicknessMin, m_PbrExtendedParams.iridescenceThicknessMin);
        shader.SetFloat(state.locIridescenceThicknessMax, m_PbrExtendedParams.iridescenceThicknessMax);
        shader.SetFloat(state.locAnisotropyStrength, m_PbrExtendedParams.anisotropyStrength);
        shader.SetFloat(state.locAnisotropyRotation, m_PbrExtendedParams.anisotropyRotation);
        shader.SetFloat(state.locDispersion, m_PbrExtendedParams.dispersion);
        shader.SetFloat(state.locDiffuseTransmissionFactor, m_PbrExtendedParams.diffuseTransmissionFactor);
        shader.SetVec3(state.locDiffuseTransmissionColor, m_PbrExtendedParams.diffuseTransmissionColor);
        state.pbrExtendedVersion = m_pbrExtendedVersion;
    }

    if (state.shadingModeVersion != m_shadingModeVersion)
    {
        shader.SetInt(state.locShadingMode, static_cast<int>(m_shadingMode));
        state.shadingModeVersion = m_shadingModeVersion;
    }

    if (state.colorGradingVersion != m_colorGradingVersion)
    {
        if (state.locContrast != -1)
            shader.SetFloat(state.locContrast, m_contrast);
        if (state.locExposure != -1)
            shader.SetFloat(state.locExposure, m_exposure);
        state.colorGradingVersion = m_colorGradingVersion;
    }

    if (state.locOpaqueFramebuffer != -1)
        shader.SetInt(state.locOpaqueFramebuffer, 19); // always bound to dedicated unit 19
    if (state.locUseOpaqueFramebuffer != -1)
        shader.SetInt(state.locUseOpaqueFramebuffer, m_useOpaqueFramebuffer ? 1 : 0);
    if (state.locScreenSize != -1)
        shader.SetVec2(state.locScreenSize, m_screenSize);
}

void GLRenderSystem::ensurePostProcessFramebuffers(int width, int height)
{
    if (m_HdrFbo != 0 && width == m_HdrWidth && height == m_HdrHeight)
        return;

    m_HdrWidth  = width;
    m_HdrHeight = height;

    if (m_HdrFbo) glDeleteFramebuffers(1, &m_HdrFbo);
    if (m_HdrColorTex) glDeleteTextures(1, &m_HdrColorTex);
    if (m_HdrDepthRbo) glDeleteRenderbuffers(1, &m_HdrDepthRbo);

    if (m_PingPongFbo[0]) glDeleteFramebuffers(2, m_PingPongFbo);
    if (m_PingPongColorTex[0]) glDeleteTextures(2, m_PingPongColorTex);

    // HDR FBO
    glGenFramebuffers(1, &m_HdrFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_HdrFbo);
    
    glGenTextures(1, &m_HdrColorTex);
    glBindTexture(GL_TEXTURE_2D, m_HdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HdrColorTex, 0);

    glGenRenderbuffers(1, &m_HdrDepthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_HdrDepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_HdrDepthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "HDR Framebuffer not complete!" << std::endl;

    // Ping-Pong FBOs for Bloom
    glGenFramebuffers(2, m_PingPongFbo);
    glGenTextures(2, m_PingPongColorTex);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFbo[i]);
        glBindTexture(GL_TEXTURE_2D, m_PingPongColorTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_PingPongColorTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "PingPong Framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderSystem::beginHdrCapture()
{
    // Remember the caller's framebuffer + viewport (Qt renders into its own
    // FBO, never framebuffer 0) so renderPostProcessing can composite into it.
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_HdrSavedFbo);
    glGetIntegerv(GL_VIEWPORT, m_HdrSavedViewport);

    int w = m_HdrSavedViewport[2];
    int h = m_HdrSavedViewport[3];
    if (w <= 0 || h <= 0)
    {
        w = static_cast<int>(m_screenSize.x);
        h = static_cast<int>(m_screenSize.y);
    }
    if (w <= 0 || h <= 0)
        return;

    m_screenSize = glm::vec2(w, h);
    ensurePostProcessFramebuffers(w, h);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_HdrFbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, w, h);
}

void GLRenderSystem::renderPostProcessing()
{
    if (!m_BloomExtractShader || !m_BloomBlurShader || !m_PostProcessShader || m_HdrWidth <= 0 || m_HdrHeight <= 0)
        return;

    const int w = m_HdrWidth;
    const int h = m_HdrHeight;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    bool horizontal = true, first_iteration = true;
    int amount = 10;
    
    // 1. Extract Bright Colors
    glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFbo[0]);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_BloomExtractShader->bind();
    m_BloomExtractShader->SetInt("uSceneTexture", 0);
    m_BloomExtractShader->SetFloat("uThreshold", m_bloomThreshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HdrColorTex);
    
    glBindVertexArray(m_QuadVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    
    // 2. Blur the Bright Colors
    m_BloomBlurShader->bind();
    m_BloomBlurShader->SetInt("image", 0);
    for (unsigned int i = 0; i < amount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFbo[horizontal]);
        glViewport(0, 0, w, h);
        m_BloomBlurShader->SetInt("horizontal", horizontal);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? m_PingPongColorTex[0] : m_PingPongColorTex[!horizontal]); 
        
        glDrawArrays(GL_TRIANGLES, 0, 3);
        horizontal = !horizontal;
        if (first_iteration)
            first_iteration = false;
    }

    // 3. Composite and Tone Map into the caller's (Qt widget's) framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_HdrSavedFbo));
    glViewport(m_HdrSavedViewport[0], m_HdrSavedViewport[1], m_HdrSavedViewport[2], m_HdrSavedViewport[3]);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    
    m_PostProcessShader->bind();
    m_PostProcessShader->SetInt("uSceneTexture", 0);
    m_PostProcessShader->SetInt("uBloomTexture", 1);
    m_PostProcessShader->SetFloat("uExposure", m_exposure);
    m_PostProcessShader->SetFloat("uContrast", m_contrast);
    m_PostProcessShader->SetInt("uIsPbr", m_shadingMode == ShadingMode::Pbr ? 1 : 0);
    m_PostProcessShader->SetInt("uTonemapOperator", m_tonemapOperator);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HdrColorTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_PingPongColorTex[!horizontal]);
    
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Restore standard pipeline state
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    m_boundProgramId = 0;
    m_lastBoundVaoKey = nullptr;
}
