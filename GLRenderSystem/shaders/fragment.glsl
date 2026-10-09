#version 460 core

// High-Performance Physically Based Rendering (PBR) & Standard Phong Shader

struct Light
{
    int type; // 0 = point, 1 = directional
    vec3 position;
    vec3 direction;
    vec3 color;
    int enabled;
};

struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;
in vec4 vTangent;

// Transmission FBO uniforms removed as they are in pbr_uniforms.glsl

out vec4 FragColor;

const float PI = 3.14159265359;

uniform Light lights[8];
uniform Material material;
uniform vec3 viewPos;
uniform int forceFlatColor;
uniform vec3 flatColor;

// Shading mode: 0 = Standard (Phong), 1 = PBR (Physically Based), 2 = Flat / Unlit
uniform int shadingMode;

// PBR parameters
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmissive;
uniform float uIOR;
uniform float uTransmission;
uniform float uContrast;
uniform float uExposure;

// Texture samplers & flags
uniform int useTexture;
uniform sampler2D uTexture; // Unit 0: Diffuse / Base Color

uniform int useNormalMap;
uniform sampler2D uNormalTexture; // Unit 1: Normal Map

uniform int useBumpMap;
uniform sampler2D uBumpTexture; // Unit 2: Occlusion / Bump

uniform int useMetallicRoughnessMap;
uniform sampler2D uMetallicRoughnessTexture; // Unit 3: G=Roughness, B=Metallic

uniform int useEmissiveMap;
uniform sampler2D uEmissiveTexture; // Unit 4: Emissive

#include "pbr_uniforms.glsl"

vec3 perturbNormal(vec3 normal, vec3 viewDir, vec2 texCoord, sampler2D normalTex)
{
    vec3 mapNormal = texture(normalTex, texCoord).rgb * 2.0 - 1.0;

    // Use authored vertex tangents if present.
    // Re-orthogonalize T against N to remove interpolation drift at UV seam edges
    // sign(vTangent.w) preserves handedness for mirrored UV islands (glTF spec §3.7.2.1).
    float tLen = length(vTangent.xyz);
    if (tLen > 0.01)
    {
        vec3 T = vTangent.xyz / tLen;
        T = normalize(T - dot(T, normal) * normal); // Gram-Schmidt re-orthogonalization
        vec3 B = cross(normal, T) * sign(vTangent.w);
        return normalize(T * mapNormal.x + B * mapNormal.y + normal * mapNormal.z);
    }

    // Fallback: derive tangent frame from screen-space derivatives (dFdx/dFdy)
    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 duv1 = dFdx(texCoord);
    vec2 duv2 = dFdy(texCoord);

    vec3 dp2perp = cross(dp2, normal);
    vec3 dp1perp = cross(normal, dp1);
    vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    vec3 T0 = tangent * invmax;
    vec3 B0 = bitangent * invmax;

    return normalize(T0 * mapNormal.x + B0 * mapNormal.y + normal * mapNormal.z);
}

// Procedural environment lighting & reflection map
vec3 sampleEnvironment(vec3 rayDir, float roughness)
{
    vec3 dir = normalize(rayDir);
    float y = dir.y;

    // Natural studio / sky radiance
    vec3 zenithColor  = vec3(0.25, 0.45, 0.70);
    vec3 horizonColor = vec3(0.65, 0.70, 0.78);
    // Raised ground from 0.18→0.30 and horizon from 0.35→0.50 to provide
    // adequate ambient fill on downward-facing geometry (avocado belly, corset base).
    vec3 groundColor  = vec3(0.30, 0.30, 0.33);
    vec3 groundHorizon = vec3(0.50, 0.52, 0.56);

    vec3 sky;
    if (y > 0.0)
    {
        float h = pow(max(1.0 - y, 0.0), 3.0);
        sky = mix(zenithColor, horizonColor, h);
    }
    else
    {
        float h = pow(min(abs(y), 1.0), 0.5);
        sky = mix(groundHorizon, groundColor, h);
    }

    // Key light reflection / sun highlight
    vec3 sunDir = normalize(vec3(0.6, 0.8, 0.5));
    float sunDot = max(dot(dir, sunDir), 0.0);
    float power = mix(512.0, 16.0, roughness);
    float sunIntensity = pow(sunDot, power) * (1.0 - roughness * 0.8) * 2.0;
    sky += sunIntensity * vec3(1.2, 1.15, 1.05);

    return sky * 1.1;
}

#include "pbr_brdf.glsl"
#include "pbr_direct.glsl"
#include "pbr_ibl.glsl"

vec3 applyPhongLight(Light light, vec3 normal, vec3 viewDir, vec3 worldPos, vec3 baseColor)
{
    vec3 lightDir;
    float attenuation = 1.0;

    if (light.type == 0)
    {
        float dist = length(light.position - worldPos);
        lightDir = (light.position - worldPos) / max(dist, 0.001);
        attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * (dist * dist));
    }
    else
    {
        lightDir = normalize(-light.direction);
    }

    float diff = max(dot(normal, lightDir), 0.0);

    float spec = 0.0;
    if (diff > 0.0)
    {
        vec3 halfDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfDir), 0.0), max(material.shininess, 1.0));
    }

    vec3 diffuse = light.color * diff * baseColor;
    vec3 specular = light.color * spec * material.specular;

    return (diffuse + specular) * attenuation;
}

void main()
{
    if (forceFlatColor == 1)
    {
        FragColor = vec4(flatColor, 1.0);
        return;
    }

    // Line primitives & coordinate axes (zero normals) preserve pure unaltered vertex colors
    if (length(vNormal) < 0.0001 && useTexture != 1)
    {
        FragColor = vec4(vColor, 1.0);
        return;
    }

    vec4 baseColorMap = (useTexture == 1) ? texture(uTexture, vTexCoord) : vec4(1.0);
    vec4 albedoAlpha = baseColorMap * vec4(vColor, 1.0);
    albedoAlpha.rgb *= material.diffuse;

    // Alpha mode handling
    if (uAlphaMode == 1) // MASK
    {
        if (albedoAlpha.a < uAlphaCutoff) discard;
    }

    vec3 albedo = albedoAlpha.rgb;

    // 2. Normal vector
    vec3 viewDir = normalize(viewPos - vWorldPos);

    // Flat geometric face normal via screen-space derivatives (fallback when vertex normals missing):
    vec3 faceN = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    if (dot(faceN, viewDir) < 0.0) faceN = -faceN;

    // Use smooth interpolated per-fragment vertex normals whenever available:
    vec3 N = (length(vNormal) > 0.001) ? normalize(vNormal) : faceN;
    
    if (uDoubleSided == 1 && dot(N, viewDir) < 0.0) {
        N = -N;
    }

    if (useNormalMap == 1)
    {
        N = perturbNormal(N, viewDir, vTexCoord, uNormalTexture);
    }

    // 3. Occlusion / AO
    float ao = 1.0;
    if (useBumpMap == 1)
    {
        ao = texture(uBumpTexture, vTexCoord).r;
    }

    // ── Shading Mode Branch ───────────────────────────────────────────────
    if (shadingMode == 2 || uUnlit == 1) // Flat / Unlit (standard normal flat color)
    {
        FragColor = vec4(albedo * ao, albedoAlpha.a);
        return;
    }
    else if (shadingMode == 0) // Standard Phong (Face shading - normal classic lighting)
    {
        vec3 ambient = 0.15 * albedo;
        vec3 direct = vec3(0.0);

        for (int i = 0; i < 8; ++i)
        {
            if (lights[i].enabled == 1)
            {
                direct += applyPhongLight(lights[i], N, viewDir, vWorldPos, albedo);
            }
        }

        vec3 phongColor = (ambient + direct) * ao;
        FragColor = vec4(clamp(phongColor, 0.0, 1.0), albedoAlpha.a);
        return;
    }

    float contrast = (uContrast > 0.001) ? uContrast : 1.0;
    float exposure = (uExposure > 0.001) ? uExposure : 1.0;

    // --- Shading Mode 1: Physically Based Rendering (PBR) ---
    #define TEX_COORD vTexCoord
    #define WORLD_POS vWorldPos
    #include "pbr_lighting.glsl"

    vec3 hdrColor = ambient + Lo + emissive;

    float alpha = albedoAlpha.a;
    if (uAlphaMode != 2 && uTransmission > 0.001) {
        alpha = 1.0;
    }
    // Output raw linear HDR — post-process pass does tonemapping + gamma
    FragColor = vec4(hdrColor, alpha);
}
