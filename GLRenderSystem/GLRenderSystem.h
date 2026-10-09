#pragma once
#include <array>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../Interfaces/IRenderSystem.h"

class Shader;
class IndexBuffer;
class VertexArray;
class VertexBuffer;

class GLRenderSystem : public IRenderSystem
{
public:
    GLRenderSystem();
    ~GLRenderSystem() override;

    void init() override;
    void clearDisplay(float r, float g, float b) override;
    void setViewport(double x, double y, double width, double height) override;

    void renderTriangleSoup(const std::vector<Vertex> &m_vertices) override;
    void renderLines(const std::vector<Vertex> &m_vertices) override;
    void renderPolyline(const std::vector<Vertex> &m_vertices) override;
    void setLineWidth(float value) override;

    // Incremental triangle soup API
    void uploadTriangleSoup(const void *meshKey,
                            const std::vector<Vertex> &vertices) override;
    void updateTriangleSoupRange(const void *meshKey, const Vertex *data,
                                 size_t startVertex,
                                 size_t vertexCount) override;
    void drawTriangleSoup(const void *meshKey, size_t vertexCount) override;
    void drawLineBuffer(const void *meshKey, size_t vertexCount) override;
    void uploadIndexedTriangleSoup(
        const void *meshKey, const std::vector<Vertex> &vertices,
        const std::vector<unsigned int> &indices) override;
    void updateIndexedTriangleSoupRange(const void *meshKey,
                                        const Vertex *vertexData,
                                        size_t startVertex, size_t vertexCount,
                                        const unsigned int *indexData,
                                        size_t startIndex,
                                        size_t indexCount) override;
    void drawIndexedTriangleSoup(const void *meshKey, size_t indexCount,
                                 bool renderWireframe = false) override;
    void drawIndexedWireframe(const void *meshKey, size_t indexCount) override;
    void releaseBuffer(const void *meshKey) override;

    // Shading Mode & PBR
    void setShadingMode(ShadingMode mode) override;
    ShadingMode getShadingMode() const override { return m_shadingMode; }

    void setEnvironmentBackground(bool enable) override { m_environmentBackground = enable; }
    bool getEnvironmentBackground() const override { return m_environmentBackground; }
    void renderEnvironmentBackground() override;

    void setContrast(float contrast) override;
    float getContrast() const override { return m_contrast; }
    void setExposure(float exposure) override;
    float getExposure() const override { return m_exposure; }

    void setPbrMaterial(float metallic, float roughness, glm::vec3 emissive,
                        float ior = 1.5f, float transmission = 0.0f) override;

    // Textures
    void uploadTexture(const void *texKey, int width, int height, int channels,
                       const unsigned char *pixels,
                       TextureSlot slot = TextureSlot::Diffuse) override;
    void setActiveTexture(const void *texKey) override;
    void releaseTexture(const void *texKey) override;

    // Overlay mode
    void setOverlayMode(bool enable) override;
    bool getOverlayMode() const override { return m_overlayMode; }

    // FBO colour-id picking
    void beginPickPass(int width, int height) override;
    void drawIndexedForPick(const void *meshKey, size_t indexCount,
                            uint32_t baseId) override;
    uint32_t readPickId(int x, int y) override;
    void endPickPass() override;

    void setupLight(unsigned int index, glm::vec3 position,
                    glm::vec3 color) override;
    void setupDirectionalLight(unsigned int index, glm::vec3 direction,
                               glm::vec3 color) override;
    void turnLight(unsigned int index, bool enable) override;

    void setMaterial(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular,
                     float shininess) override;
    void setShininess(float value) override;
    void setPbrExtendedParams(const PbrMaterialParams& params) override;
    void beginOpaqueCapture() override;
    void endOpaqueCapture() override;
    void setScreenSize(int width, int height) override;
    void setOpaquePassOnly(bool v) override { m_opaquePassOnly = v; }
    bool getOpaquePassOnly() const override { return m_opaquePassOnly; }
    
    // ── HDR & Post-Processing (Bloom & Tonemapping) ────────────────────────
    void beginHdrCapture() override;
    void renderPostProcessing() override;
    void setBloomThreshold(float threshold) override { m_bloomThreshold = threshold; }
    void setTonemapOperator(int op) override { m_tonemapOperator = op; }
    int getTonemapOperator() const override { return m_tonemapOperator; }

    // Opaque FBO for transmission
    GLuint m_OpaqueFbo = 0;
    GLuint m_OpaqueColorTex = 0;
    GLuint m_OpaqueDepthRbo = 0;
    int m_OpaqueWidth = 0;
    int m_OpaqueHeight = 0;
    GLint m_OpaqueSavedFbo = 0;
    bool m_useOpaqueFramebuffer = false;
    bool m_opaquePassOnly = false;
    glm::vec2 m_screenSize = {0.0f, 0.0f};

    GLint m_OpaqueSavedViewport[4] = {0, 0, 0, 0};
    void ensureOpaqueFramebuffer(int width, int height);
    
    // Post-Process FBOs
    GLuint m_HdrFbo = 0;
    GLuint m_HdrColorTex = 0;
    GLuint m_HdrDepthRbo = 0;
    int m_HdrWidth = 0;
    int m_HdrHeight = 0;
    GLint m_HdrSavedFbo = 0;
    GLint m_HdrSavedViewport[4] = {0, 0, 0, 0};
    GLuint m_PingPongFbo[2] = {0, 0};
    GLuint m_PingPongColorTex[2] = {0, 0};
    float m_bloomThreshold = 1.0f;
    void ensurePostProcessFramebuffers(int width, int height);

    void setWorldMatrix(const glm::mat4 &matrix) override;
    const glm::mat4 &getWorldMatrix() const override;

    void setViewProjectionMatrix(const glm::mat4 &matrix) override;
    const glm::mat4 &getViewProjectionMatrix() const override;

    void setViewPosition(glm::vec3 value) override;
    const glm::vec3 &getViewPosition() const override;

private:
    static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
                                       GLenum severity, GLsizei length,
                                       const GLchar *message,
                                       const void *userParam);
    static void logOpenGLErrors(const char *stage);

    struct LightState
    {
        int type = 0;
        glm::vec3 position{0.0f};
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        glm::vec3 color{1.0f};
        bool enabled = false;
    };

    struct MaterialState
    {
        glm::vec3 ambient{0.2f, 0.2f, 0.2f};
        glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
        glm::vec3 specular{0.5f, 0.5f, 0.5f};
        float shininess = 32.0f;
    };

    struct PbrMaterialState
    {
        float metallic = 0.0f;
        float roughness = 0.5f;
        glm::vec3 emissive{0.0f, 0.0f, 0.0f};
        float ior = 1.5f;
        float transmission = 0.0f;
    };

    struct GpuBuffer
    {
        std::unique_ptr<VertexArray> vao;
        std::unique_ptr<VertexBuffer> vbo;
        std::unique_ptr<IndexBuffer> ibo;
    };

    std::unique_ptr<Shader> m_RenderShader;
    std::unique_ptr<Shader> m_MeshShader;
    std::unique_ptr<Shader> m_PickShader;
    std::unique_ptr<Shader> m_SkyShader;
    std::unique_ptr<Shader> m_BloomExtractShader;
    std::unique_ptr<Shader> m_BloomBlurShader;
    std::unique_ptr<Shader> m_PostProcessShader;
    GLuint m_SkyVao = 0;
    GLuint m_QuadVao = 0;

    GpuBuffer m_Buffer;
    mutable std::recursive_mutex m_resourceMutex;
    std::unordered_map<const void *, GpuBuffer> m_PersistentBuffers;
    std::unordered_map<const void *, GLuint> m_Textures;
    std::vector<GpuBuffer> m_GarbageBuffers;
    std::vector<GLuint> m_GarbageTextures;
    const void *m_activeTextureKey = nullptr;
    std::array<LightState, 8> m_Lights{};
    MaterialState m_Material{};
    PbrMaterialState m_PbrMaterial{};
    PbrMaterialParams m_PbrExtendedParams{};

    ShadingMode m_shadingMode = ShadingMode::Standard;
    bool m_environmentBackground = false;
    float m_contrast = 1.0f;
    float m_exposure = 1.0f;
    int m_tonemapOperator = 0; // 0 = Khronos PBR Neutral (default), 1 = ACES Filmic
    bool m_overlayMode = false;

    // Offscreen framebuffer for colour-id picking (created lazily, resized to
    // match the viewport). m_SavedFbo/m_SavedViewport restore the caller's
    // state (Qt's own FBO) in endPickPass.
    GLuint m_PickFbo = 0;
    GLuint m_PickColorTex = 0;
    GLuint m_PickDepthRbo = 0;
    int m_PickWidth = 0;
    int m_PickHeight = 0;
    GLint m_SavedFbo = 0;
    GLint m_SavedViewport[4] = {0, 0, 0, 0};
    void ensurePickFramebuffer(int width, int height);

    glm::mat4 m_WorldMatrix = glm::mat4(1.0f);
    glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    glm::vec3 m_ViewPosition = glm::vec3(0.0f);

    uint64_t m_matricesVersion = 1;
    uint64_t m_lightsVersion = 1;
    uint64_t m_materialVersion = 1;
    uint64_t m_pbrMaterialVersion = 1;
    uint64_t m_pbrExtendedVersion = 1;
    uint64_t m_shadingModeVersion = 1;
    uint64_t m_colorGradingVersion = 1;

    glm::mat4 m_WorldViewProj = glm::mat4(1.0f);
    glm::mat3 m_NormalMatrix = glm::mat3(1.0f);
    uint64_t m_derivedMatricesVersion = 0;
    void ensureDerivedMatrices();

    struct ShaderStateCache
    {
        uint64_t matricesVersion = 0;
        uint64_t lightsVersion = 0;
        uint64_t materialVersion = 0;
        uint64_t pbrMaterialVersion = 0;
        uint64_t pbrExtendedVersion = 0;
        uint64_t shadingModeVersion = 0;
        uint64_t colorGradingVersion = 0;
        int forceFlatColor = -1;
        int shadingMode = -1;
        std::array<int, 19> useTexture = {-1}; // tracks if each texture is used
        bool locationsResolved = false;
        GLint locWorld = -1;
        GLint locWorldViewProj = -1;
        GLint locNormalMatrix = -1;
        GLint locViewPos = -1;
        GLint locMatAmbient = -1;
        GLint locMatDiffuse = -1;
        GLint locMatSpecular = -1;
        GLint locMatShininess = -1;
        GLint locMetallic = -1;
        GLint locRoughness = -1;
        GLint locEmissive = -1;
        GLint locIOR = -1;
        GLint locTransmission = -1;
        GLint locShadingMode = -1;
        GLint locContrast = -1;
        GLint locExposure = -1;
        GLint locForceFlatColor = -1;
        
        // PBR Extended locations
        GLint locAlphaMode = -1;
        GLint locAlphaCutoff = -1;
        GLint locDoubleSided = -1;
        GLint locUnlit = -1;
        GLint locClearcoatFactor = -1;
        GLint locClearcoatRoughness = -1;
        GLint locSheenColor = -1;
        GLint locSheenRoughness = -1;
        GLint locThickness = -1;
        GLint locAttenuationDistance = -1;
        GLint locAttenuationColor = -1;
        GLint locSpecularFactor = -1;
        GLint locSpecularColor = -1;
        GLint locEmissiveStrength = -1;
        GLint locIridescenceFactor = -1;
        GLint locIridescenceIor = -1;
        GLint locIridescenceThicknessMin = -1;
        GLint locIridescenceThicknessMax = -1;
        GLint locAnisotropyStrength = -1;
        GLint locAnisotropyRotation = -1;
        GLint locDispersion = -1;
        GLint locDiffuseTransmissionFactor = -1;
        GLint locDiffuseTransmissionColor = -1;

        std::array<GLint, 19> locUseTexture = {-1};

        GLint locOpaqueFramebuffer = -1;
        GLint locUseOpaqueFramebuffer = -1;
        GLint locScreenSize = -1;
    };
    ShaderStateCache m_RenderState;
    ShaderStateCache m_MeshState;
    void resolveLocations(Shader &shader, ShaderStateCache &state);

    GLuint m_boundProgramId = 0;
    void bindProgram(Shader &shader);

    std::array<GLuint, 20> m_boundTextures = {0}; // units 0-18: material textures; unit 19: opaque FBO

    const void *m_lastBoundVaoKey = nullptr;

    void uploadVertices(const std::vector<Vertex> &m_vertices);
    void uploadMatrices(Shader &shader, ShaderStateCache &state);
    void uploadLights(Shader &shader, ShaderStateCache &state);
    void uploadMaterial(Shader &shader, ShaderStateCache &state);
    void uploadPbrMaterial(Shader &shader, ShaderStateCache &state);
    void applyShaderState(Shader &shader, ShaderStateCache &state);
    void setForceFlatColor(Shader &shader, ShaderStateCache &state, int value);
    void bindActiveTexture(Shader &shader, ShaderStateCache &state);
};
