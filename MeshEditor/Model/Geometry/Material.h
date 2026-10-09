#pragma once

#include <glm/glm.hpp>
#include <string>

enum class MaterialMode
{
    Scene,
    Metallic,
    Plastic,
    Matte,
};

#include "../../Interfaces/IRenderSystem.h"
#include <array>

class Material
{
public:
    Material();
    Material(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular,
             float shininess);

    static Material preset(MaterialMode mode,
                           const Material &sceneMaterial = Material());

    void applyPreset(MaterialMode mode,
                     const Material &sceneMaterial = Material());

    bool operator==(const Material &other) const;
    bool operator!=(const Material &other) const;

    const glm::vec3 &getAmbient() const;
    const glm::vec3 &getDiffuse() const;
    const glm::vec3 &getSpecular() const;
    float getShininess() const;
    MaterialMode getMode() const;

    void setAmbient(glm::vec3 ambient);
    void setDiffuse(glm::vec3 diffuse);
    void setSpecular(glm::vec3 specular);
    void setShininess(float shininess);
    void setMode(MaterialMode mode);

    // Legacy PBR properties
    float getMetallic() const;
    void setMetallic(float metallic);

    float getRoughness() const;
    void setRoughness(float roughness);

    const glm::vec3 &getEmissive() const;
    void setEmissive(glm::vec3 emissive);

    float getIOR() const;
    void setIOR(float ior);

    float getTransmission() const;
    void setTransmission(float transmission);

    // Extended PBR Params
    const PbrMaterialParams& getExtendedParams() const;
    PbrMaterialParams& getExtendedParams();

    // Absolute paths to textures
    const std::string &getDiffuseTexturePath() const;
    void setDiffuseTexturePath(const std::string &path);

    const std::string &getNormalTexturePath() const;
    void setNormalTexturePath(const std::string &path);

    const std::string &getBumpTexturePath() const;
    void setBumpTexturePath(const std::string &path);

    const std::string &getMetallicRoughnessTexturePath() const;
    void setMetallicRoughnessTexturePath(const std::string &path);

    const std::string &getEmissiveTexturePath() const;
    void setEmissiveTexturePath(const std::string &path);

    const std::string &getTexturePath(TextureSlot slot) const;
    void setTexturePath(TextureSlot slot, const std::string &path);

private:
    glm::vec3 m_sceneAmbient{0.2f, 0.2f, 0.2f};
    glm::vec3 m_sceneDiffuse{0.8f, 0.8f, 0.8f};
    glm::vec3 m_sceneSpecular{0.5f, 0.5f, 0.5f};
    float m_sceneShininess = 32.0f;

    glm::vec3 m_ambient{0.2f, 0.2f, 0.2f};
    glm::vec3 m_diffuse{0.8f, 0.8f, 0.8f};
    glm::vec3 m_specular{0.5f, 0.5f, 0.5f};
    float m_shininess = 32.0f;
    MaterialMode m_Mode = MaterialMode::Scene;

    // Extended PBR Params
    PbrMaterialParams m_extendedParams;

    std::string m_diffuseTexturePath;
    std::string m_normalTexturePath;
    std::string m_bumpTexturePath;
    std::string m_metallicRoughnessTexturePath;
    std::string m_emissiveTexturePath;
    
    std::array<std::string, static_cast<size_t>(TextureSlot::Count)> m_texturePaths;
};
