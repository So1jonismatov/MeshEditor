#include "Material.h"
#include <cmath>

Material::Material() = default;

Material::Material(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular,
                   float shininess)
    : m_sceneAmbient(ambient), m_sceneDiffuse(diffuse),
      m_sceneSpecular(specular), m_sceneShininess(shininess),
      m_ambient(ambient), m_diffuse(diffuse), m_specular(specular),
      m_shininess(shininess)
{
}

Material Material::preset(MaterialMode mode, const Material &sceneMaterial)
{
    Material material = sceneMaterial;
    material.applyPreset(mode, sceneMaterial);
    return material;
}

void Material::applyPreset(MaterialMode mode, const Material &sceneMaterial)
{
    switch (mode)
    {
    case MaterialMode::Metallic:
        m_ambient = glm::vec3(0.15f, 0.15f, 0.16f);
        m_diffuse = glm::vec3(0.95f, 0.95f, 0.96f);
        m_specular = glm::vec3(0.98f, 0.98f, 1.0f);
        m_shininess = 160.0f;
        m_extendedParams.metallic = 1.0f;
        m_extendedParams.roughness = 0.15f;
        break;
    case MaterialMode::Plastic:
        m_ambient = glm::vec3(0.18f, 0.18f, 0.19f);
        m_diffuse = glm::vec3(0.82f, 0.82f, 0.84f);
        m_specular = glm::vec3(0.45f, 0.45f, 0.48f);
        m_shininess = 18.0f;
        m_extendedParams.metallic = 0.0f;
        m_extendedParams.roughness = 0.35f;
        break;
    case MaterialMode::Matte:
        m_ambient = glm::vec3(0.22f, 0.22f, 0.22f);
        m_diffuse = glm::vec3(0.75f, 0.75f, 0.75f);
        m_specular = glm::vec3(0.05f, 0.05f, 0.05f);
        m_shininess = 4.0f;
        m_extendedParams.metallic = 0.0f;
        m_extendedParams.roughness = 0.90f;
        break;
    case MaterialMode::Scene:
    default:
        m_ambient = sceneMaterial.m_sceneAmbient;
        m_diffuse = sceneMaterial.m_sceneDiffuse;
        m_specular = sceneMaterial.m_sceneSpecular;
        m_shininess = sceneMaterial.m_sceneShininess;
        m_extendedParams = sceneMaterial.m_extendedParams;
        break;
    }

    m_Mode = mode;
}

const glm::vec3 &Material::getAmbient() const
{
    return m_ambient;
}

const glm::vec3 &Material::getDiffuse() const
{
    return m_diffuse;
}

const glm::vec3 &Material::getSpecular() const
{
    return m_specular;
}

float Material::getShininess() const
{
    return m_shininess;
}

MaterialMode Material::getMode() const
{
    return m_Mode;
}

void Material::setAmbient(glm::vec3 ambient)
{
    m_sceneAmbient = ambient;
    if (m_Mode == MaterialMode::Scene)
        m_ambient = ambient;
}

void Material::setDiffuse(glm::vec3 diffuse)
{
    m_sceneDiffuse = diffuse;
    if (m_Mode == MaterialMode::Scene)
        m_diffuse = diffuse;
}

void Material::setSpecular(glm::vec3 specular)
{
    m_sceneSpecular = specular;
    if (m_Mode == MaterialMode::Scene)
        m_specular = specular;
}

void Material::setShininess(float shininess)
{
    m_sceneShininess = shininess;
    if (m_Mode == MaterialMode::Scene)
        m_shininess = shininess;
}

void Material::setMode(MaterialMode mode)
{
    applyPreset(mode, *this);
    m_Mode = mode;
}

bool Material::operator==(const Material &other) const
{
    constexpr float eps = 1e-5f;
    auto vecEqual = [eps](const glm::vec3 &a, const glm::vec3 &b)
    {
        return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps &&
               std::abs(a.z - b.z) < eps;
    };
    return vecEqual(m_ambient, other.m_ambient) &&
           vecEqual(m_diffuse, other.m_diffuse) &&
           vecEqual(m_specular, other.m_specular) &&
           std::abs(m_shininess - other.m_shininess) < eps &&
           m_Mode == other.m_Mode;
}

bool Material::operator!=(const Material &other) const
{
    return !(*this == other);
}

float Material::getMetallic() const
{
    return m_extendedParams.metallic;
}
void Material::setMetallic(float metallic)
{
    m_extendedParams.metallic = metallic;
}

float Material::getRoughness() const
{
    return m_extendedParams.roughness;
}
void Material::setRoughness(float roughness)
{
    m_extendedParams.roughness = roughness;
}

const glm::vec3 &Material::getEmissive() const
{
    return m_extendedParams.emissive;
}
void Material::setEmissive(glm::vec3 emissive)
{
    m_extendedParams.emissive = emissive;
}

float Material::getIOR() const
{
    return m_extendedParams.ior;
}
void Material::setIOR(float ior)
{
    m_extendedParams.ior = ior;
}

float Material::getTransmission() const
{
    return m_extendedParams.transmission;
}
void Material::setTransmission(float transmission)
{
    m_extendedParams.transmission = transmission;
}

const PbrMaterialParams& Material::getExtendedParams() const
{
    return m_extendedParams;
}

PbrMaterialParams& Material::getExtendedParams()
{
    return m_extendedParams;
}

const std::string &Material::getDiffuseTexturePath() const
{
    return m_texturePaths[static_cast<size_t>(TextureSlot::Diffuse)];
}
void Material::setDiffuseTexturePath(const std::string &path)
{
    m_texturePaths[static_cast<size_t>(TextureSlot::Diffuse)] = path;
}

const std::string &Material::getNormalTexturePath() const
{
    return m_texturePaths[static_cast<size_t>(TextureSlot::Normal)];
}
void Material::setNormalTexturePath(const std::string &path)
{
    m_texturePaths[static_cast<size_t>(TextureSlot::Normal)] = path;
}

const std::string &Material::getBumpTexturePath() const
{
    return m_bumpTexturePath; // Legacy, kept separate
}
void Material::setBumpTexturePath(const std::string &path)
{
    m_bumpTexturePath = path;
}

const std::string &Material::getMetallicRoughnessTexturePath() const
{
    return m_texturePaths[static_cast<size_t>(TextureSlot::MetallicRoughness)];
}
void Material::setMetallicRoughnessTexturePath(const std::string &path)
{
    m_texturePaths[static_cast<size_t>(TextureSlot::MetallicRoughness)] = path;
}

const std::string &Material::getEmissiveTexturePath() const
{
    return m_texturePaths[static_cast<size_t>(TextureSlot::Emissive)];
}
void Material::setEmissiveTexturePath(const std::string &path)
{
    m_texturePaths[static_cast<size_t>(TextureSlot::Emissive)] = path;
}

const std::string &Material::getTexturePath(TextureSlot slot) const
{
    return m_texturePaths[static_cast<size_t>(slot)];
}
void Material::setTexturePath(TextureSlot slot, const std::string &path)
{
    m_texturePaths[static_cast<size_t>(slot)] = path;
}
