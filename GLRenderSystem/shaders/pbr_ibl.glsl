#ifndef PBR_IBL_GLSL
#define PBR_IBL_GLSL

// =============================================================================
// Image-Based Lighting (IBL) & Ambient Environment Evaluation
// Evaluates split-sum specular reflection, diffuse irradiance, multiple-scattering
// energy compensation, sheen IBL, clearcoat IBL, and volumetric transmission.
// =============================================================================

vec3 evaluateIBLLighting(
    vec3 N,
    vec3 V,
    float NdotV,
    vec3 albedoLinear,
    float roughness,
    float metallic,
    vec3 F0,
    float clearcoat,
    float clearcoatRoughness,
    vec3 sheenColor,
    float sheenRoughness,
    float diffuseTransmission,
    float thickness,
    float ao)
{
    vec3 R = reflect(-V, N);
    vec2 envBRDF = EnvBRDFApprox(roughness, NdotV);

    // Roughness-dependent Fresnel for IBL (Fdez-Agüera 2020)
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 kS_ibl = F0 + Fr * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    vec3 FssEss = kS_ibl * envBRDF.x + F0 * envBRDF.y;

    // Multiple-scattering compensation (Fdez-Agüera 2020, Khronos ibl.glsl)
    // Prevents severe energy loss on rough metallic/dielectric materials.
    float Ems = 1.0 - (envBRDF.x + envBRDF.y);
    vec3 F_avg = F0 + (vec3(1.0) - F0) / 21.0;
    vec3 FmsEms = Ems * FssEss * F_avg / max(vec3(1.0) - F_avg * Ems, vec3(0.0001));

    vec3 kD_ibl = (1.0 - metallic) * (vec3(1.0) - kS_ibl);

    vec3 irradiance = sampleEnvironment(N, 1.0);
    vec3 diffuseIBL = kD_ibl * irradiance * albedoLinear * (1.0 - diffuseTransmission);

    vec3 prefilteredColor = sampleEnvironment(R, roughness);
    vec3 specularIBL = prefilteredColor * (FssEss + FmsEms);

    // --- KHR_materials_sheen IBL ---
    float maxSheenColor = max(sheenColor.r, max(sheenColor.g, sheenColor.b));
    if (maxSheenColor > 0.001)
    {
        vec3 sheenPre = sampleEnvironment(N, 0.9); // Broad hemisphere irradiance without sun burst
        float sheenFr = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.5);
        vec3 sheenIBL = sheenColor * sheenPre * (0.15 + 0.85 * sheenFr);

        diffuseIBL = diffuseIBL * (1.0 - maxSheenColor * sheenFr * 0.75) + sheenIBL;
    }

    // --- KHR_materials_clearcoat IBL ---
    if (clearcoat > 0.001)
    {
        vec2 ccAB = EnvBRDFApprox(clearcoatRoughness, NdotV);
        vec3 ccPre = sampleEnvironment(R, clearcoatRoughness);
        vec3 ccSpec = ccPre * (0.04 * ccAB.x + ccAB.y) * clearcoat;
        float ccFr = clearcoat * (0.04 + 0.96 * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0));
        specularIBL = specularIBL * (1.0 - ccFr) + ccSpec;
    }

    // --- KHR_materials_transmission & KHR_materials_volume ---
    if (uTransmission > 0.001)
    {
        float nd = max(uIOR, 1.0001);
        float D  = max(uDispersion, 0.0);

        float n_r = max(nd + (nd - 1.0) * (D / 20.0) * (-0.3010), 1.0);
        float n_g = nd;
        float n_b = max(nd + (nd - 1.0) * (D / 20.0) * ( 0.6991), 1.0);

        vec3 T_g = refract(-V, N, 1.0 / n_g);
        vec3 col_g;

        if (uUseOpaqueFramebuffer == 1 && dot(T_g, T_g) > 0.001)
        {
            vec2 baseUV = gl_FragCoord.xy / max(uScreenSize, vec2(1.0));
            vec3 T_n = normalize(T_g);
            vec2 refractOffset = T_n.xy * (1.0 / max(n_g, 1.0001)) * 0.12;

            vec2 refractUV   = clamp(baseUV + refractOffset,                       vec2(0.001), vec2(0.999));
            vec2 refractUV_r = clamp(baseUV + refractOffset * (n_r / max(n_g, 0.001)), vec2(0.001), vec2(0.999));
            vec2 refractUV_b = clamp(baseUV + refractOffset * (n_b / max(n_g, 0.001)), vec2(0.001), vec2(0.999));

            // Frosted blur: sample background mipmap chain according to surface roughness
            float maxLod = 6.0;
            float lod = roughness * maxLod;
            vec3 col_refract_g = textureLod(uOpaqueFramebuffer, refractUV, lod).rgb;
            vec3 col_refract_r = textureLod(uOpaqueFramebuffer, refractUV_r, lod).rgb;
            vec3 col_refract_b = textureLod(uOpaqueFramebuffer, refractUV_b, lod).rgb;
            col_g = vec3(col_refract_r.r, col_refract_g.g, col_refract_b.b);
        }
        else
        {
            col_g = sampleEnvironment(T_g, roughness);
        }

        // Fresnel conservation: light reflected as specular cannot also be transmitted
        vec3 transmittedColor = col_g * albedoLinear * (vec3(1.0) - kS_ibl);

        // Volumetric absorption (Beer-Lambert Law)
        if (thickness > 0.0)
        {
            vec3 attenuation = -log(max(uAttenuationColor, vec3(0.0001))) / max(uAttenuationDistance, 0.0001);
            vec3 absorbance = exp(-attenuation * thickness);
            transmittedColor *= absorbance;
        }

        diffuseIBL = mix(diffuseIBL, transmittedColor, uTransmission * (1.0 - metallic));
    }

    // Minimum ambient floor — prevents absolute black on fully shadowed / downward-facing
    // surfaces where both diffuse and specular IBL evaluate near zero.
    vec3 result = (diffuseIBL + specularIBL) * ao;
    result = max(result, albedoLinear * 0.03);
    return result;
}

#endif // PBR_IBL_GLSL
