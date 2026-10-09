#ifndef PBR_DIRECT_GLSL
#define PBR_DIRECT_GLSL

// =============================================================================
// Direct Lighting Evaluation (Cook-Torrance BRDF + Extensions)
// Evaluates point and directional lights according to the Khronos glTF 2.0 spec.
// =============================================================================

vec3 evaluateDirectLighting(
    vec3 N,
    vec3 V,
    float NdotV,
    vec3 worldPos,
    vec2 texCoord,
    vec3 albedoLinear,
    float roughness,
    float metallic,
    vec3 F0,
    float specularFactor,
    float clearcoat,
    float clearcoatRoughness,
    vec3 sheenColor,
    float sheenRoughness,
    float diffuseTransmission,
    vec3 diffuseTransmissionCol)
{
    vec3 Lo = vec3(0.0);
    float maxSheenColor = max(sheenColor.r, max(sheenColor.g, sheenColor.b));

    // Pre-calculate anisotropic tangent frame if anisotropy is active
    vec3 T_aniso = vec3(1.0, 0.0, 0.0);
    vec3 B_aniso = vec3(0.0, 1.0, 0.0);
    float at = roughness;
    float ab = roughness;
    bool useAniso = (uAnisotropyStrength > 0.0);

    if (useAniso)
    {
        at = max(roughness * (1.0 + uAnisotropyStrength), 0.001);
        ab = max(roughness * (1.0 - uAnisotropyStrength), 0.001);

        // Derive base tangent frame from screen derivatives
        vec3 dp1 = dFdx(worldPos);
        vec3 dp2 = dFdy(worldPos);
        vec2 duv1 = dFdx(texCoord);
        vec2 duv2 = dFdy(texCoord);
        vec3 dp2perp = cross(dp2, N);
        vec3 dp1perp = cross(N, dp1);
        vec3 T = normalize(dp2perp * duv1.x + dp1perp * duv2.x);
        vec3 B = normalize(cross(N, T));

        // Apply KHR_materials_anisotropy rotation
        float cosR = cos(uAnisotropyRotation);
        float sinR = sin(uAnisotropyRotation);
        T_aniso = normalize(cosR * T + sinR * B);
        B_aniso = normalize(cross(N, T_aniso));
    }

    // Specular F90 reflectance (KHR_materials_specular scales dielectric F90)
    vec3 F90 = mix(vec3(clamp(specularFactor, 0.0, 1.0)), vec3(1.0), metallic);

    for (int i = 0; i < 8; ++i)
    {
        if (lights[i].enabled != 1) continue;

        vec3 L;
        float attenuation = 1.0;
        if (lights[i].type == 0) // Point Light
        {
            float dist = length(lights[i].position - worldPos);
            L = normalize(lights[i].position - worldPos);
            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * (dist * dist));
        }
        else // Directional Light
        {
            L = normalize(-lights[i].direction);
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        // --- KHR_materials_diffuse_transmission (Subsurface back-penetration) ---
        if (NdotL <= 0.0 && diffuseTransmission > 0.0 && uDoubleSided == 0)
        {
            float backNdotL = max(dot(-N, L), 0.0);
            vec3 diffuseTransmissionEnergy = albedoLinear * diffuseTransmissionCol * diffuseTransmission;
            Lo += (diffuseTransmissionEnergy / PI) * (lights[i].color * (PI * attenuation)) * backNdotL;
            continue;
        }

        if (NdotL <= 0.0) continue;

        // --- 1. Base Layer Specular & Diffuse ---
        float VdotH = max(dot(V, H), 0.0);
        vec3 F = F0 + (F90 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
        vec3 specular;

        if (useAniso)
        {
            float TdotH = dot(T_aniso, H);
            float BdotH = dot(B_aniso, H);
            float TdotV = dot(T_aniso, V);
            float BdotV = dot(B_aniso, V);
            float TdotL = dot(T_aniso, L);
            float BdotL = dot(B_aniso, L);

            float NDF = D_GGX_Anisotropic(at, ab, TdotH, BdotH, max(dot(N, H), 0.0));
            float V_aniso = V_GGX_Anisotropic(at, ab, TdotV, BdotV, NdotV, TdotL, BdotL, NdotL);
            specular = NDF * V_aniso * F;
        }
        else
        {
            // Khronos glTF 2.0 Smith Joint Height-Correlated Specular
            float NDF = DistributionGGX(N, H, roughness);
            float V_vis = V_SmithJointGGX(NdotL, NdotV, roughness);
            specular = NDF * V_vis * F;
        }

        // Energy conservation: diffuse gets whatever fraction wasn't reflected as specular
        // KHR_materials_transmission replaces diffuse reflection
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 diffuseLayer = kD * albedoLinear * (1.0 - diffuseTransmission) * (1.0 - uTransmission) / PI;
        vec3 directBRDF = diffuseLayer + specular;

        // --- 2. KHR_materials_sheen (Velvet / Fabric layer) ---
        if (maxSheenColor > 0.001)
        {
            float D_sheen = D_Charlie(max(sheenRoughness, 0.04), max(dot(N, H), 0.0));
            float V_sheen = V_Ashikhmin(NdotL, NdotV);
            vec3 sheenSpecular = sheenColor * D_sheen * V_sheen;

            // Conty & Kulla (2017) albedo scaling: attenuate base layer under sheen fibers
            float sheenFr = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.5);
            float albedoScaling = clamp(1.0 - maxSheenColor * sheenFr * 0.75, 0.0, 1.0);
            directBRDF = directBRDF * albedoScaling + sheenSpecular;
        }

        // --- 3. KHR_materials_clearcoat (Top coat lacquer layer) ---
        if (clearcoat > 0.001)
        {
            float Dcc = DistributionGGX(N, H, clearcoatRoughness);
            float Vcc = V_SmithJointGGX(NdotL, NdotV, clearcoatRoughness);
            vec3 Fcc = (0.04 + 0.96 * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0)) * vec3(clearcoat);

            vec3 clearcoatSpecular = Dcc * Vcc * Fcc;
            float ccFresnel = clearcoat * (0.04 + 0.96 * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0));
            directBRDF = directBRDF * (1.0 - ccFresnel) + clearcoatSpecular;
        }

        vec3 radiance = lights[i].color * (PI * attenuation);
        Lo += directBRDF * radiance * NdotL;
    }

    return Lo;
}

#endif // PBR_DIRECT_GLSL
