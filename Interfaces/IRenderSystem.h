#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv{0.0f, 0.0f};
    glm::vec4 tangent{0.0f, 0.0f, 0.0f, 1.0f};
};

enum class ShadingMode
{
    Standard = 0, // Phong lighting / classic CAD face shading
    Pbr = 1,      // Physically Based Rendering (Cook-Torrance GGX + IBL)
    Flat = 2      // Flat / unlit shading
};

enum class TextureSlot : size_t
{
    Diffuse = 0,
    Normal = 1,
    Occlusion = 2,
    MetallicRoughness = 3,
    Emissive = 4,
    Clearcoat = 5,
    ClearcoatRoughness = 6,
    ClearcoatNormal = 7,
    SheenColor = 8,
    SheenRoughness = 9,
    Transmission = 10,
    Thickness = 11,
    Specular = 12,
    SpecularColor = 13,
    Iridescence = 14,
    IridescenceThickness = 15,
    Anisotropy = 16,
    DiffuseTransmission = 17,
    DiffuseTransmissionColor = 18,
    Count = 19
};

enum class AlphaMode
{
    Opaque,
    Mask,
    Blend
};

struct PbrMaterialParams
{
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emissive{0.0f};
    float ior = 1.5f;
    float transmission = 0.0f;
    
    // Extensions
    float clearcoatFactor = 0.0f;
    float clearcoatRoughness = 0.0f;
    glm::vec3 sheenColor{0.0f};
    float sheenRoughness = 0.0f;
    float thickness = 0.0f;
    float attenuationDistance = 0.0f;
    glm::vec3 attenuationColor{1.0f};
    float specularFactor = 1.0f;
    glm::vec3 specularColor{1.0f};
    float emissiveStrength = 1.0f;
    float iridescenceFactor = 0.0f;
    float iridescenceIor = 1.3f;
    float iridescenceThicknessMin = 100.0f;
    float iridescenceThicknessMax = 400.0f;
    float anisotropyStrength = 0.0f;
    float anisotropyRotation = 0.0f;
    float dispersion = 0.0f;
    float diffuseTransmissionFactor = 0.0f;
    glm::vec3 diffuseTransmissionColor{1.0f};
    
    // Core parameters
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool unlit = false;

    // UV Transform
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};
    float uvRotation = 0.0f;
};

class IRenderSystem
{
public:
    virtual ~IRenderSystem();
    virtual void init() = 0;

    virtual void clearDisplay(float r, float g, float b) = 0;
    virtual void setViewport(double x, double y, double width,
                             double height) = 0;

    virtual void renderTriangleSoup(const std::vector<Vertex> &m_vertices) = 0;
    virtual void renderLines(const std::vector<Vertex> &m_vertices) = 0;
    virtual void renderPolyline(const std::vector<Vertex> &m_vertices) = 0;
    virtual void setLineWidth(float value) = 0;

    // Incremental triangle soup API: upload once, update sub-regions, draw
    virtual void uploadTriangleSoup(const void *meshKey,
                                    const std::vector<Vertex> &vertices) = 0;
    virtual void updateTriangleSoupRange(const void *meshKey,
                                         const Vertex *data, size_t startVertex,
                                         size_t vertexCount) = 0;
    virtual void drawTriangleSoup(const void *meshKey, size_t vertexCount) = 0;
    // Draws a keyed vertex buffer (uploaded via uploadTriangleSoup) as
    // GL_LINES. Lets overlays (hole boundaries, AABB/octree boxes, selection
    // outlines) live in a persistent VBO instead of being re-uploaded through
    // renderPolyline's shared scratch buffer every frame.
    virtual void drawLineBuffer(const void *meshKey, size_t vertexCount) = 0;
    virtual void uploadIndexedTriangleSoup(
        const void *meshKey, const std::vector<Vertex> &vertices,
        const std::vector<unsigned int> &indices) = 0;
    virtual void updateIndexedTriangleSoupRange(
        const void *meshKey, const Vertex *vertexData, size_t startVertex,
        size_t vertexCount, const unsigned int *indexData, size_t startIndex,
        size_t indexCount) = 0;
    virtual void drawIndexedTriangleSoup(const void *meshKey,
                                         size_t indexCount,
                                         bool renderWireframe = false) = 0;
    virtual void drawIndexedWireframe(const void *meshKey,
                                      size_t indexCount) = 0;
    virtual void releaseBuffer(const void *meshKey) = 0;

    // Upload raw pixels (decoded app-side) as a 2D texture keyed by texKey.
    // channels is 3 (RGB) or 4 (RGBA). slot controls internal format: Diffuse,
    // Emissive, SheenColor and SpecularColor are sRGB and use GL_SRGB8_ALPHA8;
    // all other slots are linear data and use GL_RGBA8.
    // setActiveTexture(key) binds it for the following draws and enables
    // texture sampling; setActiveTexture(nullptr) disables sampling.
    // releaseTexture frees the GPU texture.
    virtual void uploadTexture(const void *texKey, int width, int height,
                               int channels, const unsigned char *pixels,
                               TextureSlot slot = TextureSlot::Diffuse) = 0;
    virtual void setActiveTexture(const void *texKey) = 0;
    virtual void releaseTexture(const void *texKey) = 0;

    // ── Overlay / Always-On-Top Mode ──────────────────────────────────────
    virtual void setOverlayMode(bool enable) = 0;
    virtual bool getOverlayMode() const = 0;

    // ── Shading Mode & PBR Parameters ─────────────────────────────────────
    virtual void setShadingMode(ShadingMode mode) = 0;
    virtual ShadingMode getShadingMode() const = 0;

    virtual void setEnvironmentBackground(bool enable) = 0;
    virtual bool getEnvironmentBackground() const = 0;
    virtual void renderEnvironmentBackground() = 0;

    virtual void setContrast(float contrast) = 0;
    virtual float getContrast() const = 0;
    virtual void setExposure(float exposure) = 0;
    virtual float getExposure() const = 0;

    virtual void setPbrMaterial(float metallic, float roughness, glm::vec3 emissive,
                                float ior = 1.5f, float transmission = 0.0f) = 0;
    virtual void setPbrExtendedParams(const PbrMaterialParams& params) = 0;

    // ── Opaque Capture for Transmission ───────────────────────────────────
    virtual void beginOpaqueCapture() = 0;
    virtual void endOpaqueCapture() = 0;
    virtual void setScreenSize(int width, int height) = 0;
    virtual void setOpaquePassOnly(bool v) = 0;
    virtual bool getOpaquePassOnly() const = 0;

    // ── FBO colour-id picking ─────────────────────────────────────────────
    // beginPickPass binds an offscreen id-framebuffer (saving the currently
    // bound one), clears it, and sizes it to width×height. drawIndexedForPick
    // renders a mesh writing (baseId + gl_PrimitiveID) as an RGB colour.
    // readPickId reads back one pixel and decodes the id (0xFFFFFF == miss).
    // endPickPass restores the previously bound framebuffer and viewport.
    virtual void beginPickPass(int width, int height) = 0;
    virtual void drawIndexedForPick(const void *meshKey, size_t indexCount,
                                    uint32_t baseId) = 0;
    virtual uint32_t readPickId(int x, int y) = 0;
    virtual void endPickPass() = 0;

    // ── HDR & Post-Processing (Bloom & Tonemapping) ────────────────────────
    virtual void beginHdrCapture() = 0;
    virtual void renderPostProcessing() = 0;
    virtual void setBloomThreshold(float threshold) = 0;
    virtual void setTonemapOperator(int op) = 0;
    virtual int getTonemapOperator() const = 0;

    virtual void setupLight(unsigned int index, glm::vec3 position,
                            glm::vec3 color) = 0;
    virtual void setupDirectionalLight(unsigned int index, glm::vec3 direction,
                                       glm::vec3 color) = 0;
    virtual void turnLight(unsigned int index, bool enable) = 0;

    virtual void setMaterial(glm::vec3 ambient, glm::vec3 diffuse,
                             glm::vec3 specular, float shininess) = 0;
    virtual void setShininess(float value) = 0;

    virtual void setWorldMatrix(const glm::mat4 &matrix) = 0;
    virtual const glm::mat4 &getWorldMatrix() const = 0;

    virtual void setViewProjectionMatrix(const glm::mat4 &matrix) = 0;
    virtual const glm::mat4 &getViewProjectionMatrix() const = 0;

    virtual void setViewPosition(glm::vec3 value) = 0;
    virtual const glm::vec3 &getViewPosition() const = 0;
};