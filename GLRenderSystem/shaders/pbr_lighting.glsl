// =============================================================================
// Physically Based Rendering (PBR) Lighting Pipeline Coordinator
// Follows Khronos glTF 2.0 Metallic-Roughness Specification & Extensions
// NOTE: Included inside main() - execution block only, no function declarations.
// =============================================================================

// 1. Base color is already in linear space: sRGB textures use GL_SRGB8_ALPHA8
//    so the GPU linearises them on read. No manual pow(x,2.2) needed here.
vec3 albedoLinear = albedo;

// 2. Metallic-Roughness parameter extraction
float metallic = uMetallic;
float roughness = uRoughness;

if (useMetallicRoughnessMap == 1)
{
    // glTF 2.0: Green = Roughness, Blue = Metallic (Linear space)
    vec4 mrSample = texture(uMetallicRoughnessTexture, TEX_COORD);
    roughness *= mrSample.g;
    metallic *= mrSample.b;
}
roughness = clamp(roughness, 0.04, 1.0);
metallic  = clamp(metallic, 0.0, 1.0);

// 3. Clearcoat (KHR_materials_clearcoat)
float clearcoat = uClearcoatFactor;
float clearcoatRoughness = uClearcoatRoughness;
if (useClearcoatMap == 1) clearcoat *= texture(uClearcoatTexture, TEX_COORD).r;
if (useClearcoatRoughnessMap == 1) clearcoatRoughness *= texture(uClearcoatRoughnessTexture, TEX_COORD).g;
clearcoatRoughness = clamp(clearcoatRoughness, 0.04, 1.0);

// 4. Sheen (KHR_materials_sheen)
//    sheenColorTexture is uploaded as GL_SRGB8_ALPHA8 — GPU linearises on read.
vec3 sheenColor = uSheenColor;
float sheenRoughness = uSheenRoughness;
if (useSheenColorMap == 1) sheenColor *= texture(uSheenColorTexture, TEX_COORD).rgb;
if (useSheenRoughnessMap == 1) sheenRoughness *= texture(uSheenRoughnessTexture, TEX_COORD).a;
sheenRoughness = clamp(sheenRoughness, 0.04, 1.0);

// 5. Specular (KHR_materials_specular)
//    specularColorTexture is uploaded as GL_SRGB8_ALPHA8 — GPU linearises on read.
float specularFactor = uSpecularFactor;
if (useSpecularMap == 1)
{
    vec4 specSample = texture(uSpecularTexture, TEX_COORD);
    // glTF 2.0 spec: specular factor in alpha channel
    specularFactor *= specSample.a;
}
vec3 specularColorFactor = uSpecularColor;
if (useSpecularColorMap == 1) specularColorFactor *= texture(uSpecularColorTexture, TEX_COORD).rgb;

// 6. Iridescence (KHR_materials_iridescence)
float iridescenceFactor = uIridescenceFactor;
if (useIridescenceMap == 1) iridescenceFactor *= texture(uIridescenceTexture, TEX_COORD).r;
float iridescenceThickness = uIridescenceThicknessMax;
if (useIridescenceThicknessMap == 1) {
    float t = texture(uIridescenceThicknessTexture, TEX_COORD).g;
    iridescenceThickness = mix(uIridescenceThicknessMin, uIridescenceThicknessMax, t);
}

// 7. Volume & Thickness (KHR_materials_volume) - thickness in Green channel
float thickness = uThickness;
if (useThicknessMap == 1)
{
    thickness *= texture(uThicknessTexture, TEX_COORD).g;
}

// 8. Emissive (Core & KHR_materials_emissive_strength)
//    emissiveTexture is uploaded as GL_SRGB8_ALPHA8 — GPU linearises on read.
vec3 emissive = uEmissive;
if (useEmissiveMap == 1)
{
    emissive *= texture(uEmissiveTexture, TEX_COORD).rgb;
}
emissive *= (uEmissiveStrength > 0.0) ? uEmissiveStrength : 1.0;

// 9. Diffuse Transmission (KHR_materials_diffuse_transmission)
float diffuseTransmission = uDiffuseTransmissionFactor;
vec3 diffuseTransmissionCol = uDiffuseTransmissionColor;
if (useDiffuseTransmissionMap == 1) diffuseTransmission *= texture(uDiffuseTransmissionTexture, TEX_COORD).r;
// diffuseTransmissionColorTexture is uploaded as GL_SRGB8_ALPHA8 — GPU linearises on read.
if (useDiffuseTransmissionColorMap == 1) diffuseTransmissionCol *= texture(uDiffuseTransmissionColorTexture, TEX_COORD).rgb;

// 10. View Geometry & Normal Incidence Reflectance (F0)
vec3 V = viewDir;
float NdotV = max(dot(N, V), 0.0001);

// Dielectric F0 from IOR (default IOR 1.5 -> F0 0.04) and specular extensions
float f0_ior = (uIOR > 1.0) ? pow((uIOR - 1.0) / (uIOR + 1.0), 2.0) : 0.04;
vec3 dielectricF0 = clamp(f0_ior * specularColorFactor * specularFactor, vec3(0.0), vec3(1.0));
vec3 F0 = mix(dielectricF0, albedoLinear, metallic);

// Iridescence modulates base F0
if (iridescenceThickness <= 0.0) {
    iridescenceThickness = 400.0;
}
if (iridescenceFactor > 0.001) {
    vec3 iridF0 = evalIridescence(NdotV, uIridescenceIor, iridescenceThickness);
    F0 = mix(F0, iridF0, iridescenceFactor);
}

// 11. Direct Lighting Evaluation
vec3 Lo = evaluateDirectLighting(
    N, V, NdotV, WORLD_POS, TEX_COORD, albedoLinear, roughness, metallic, F0,
    specularFactor,
    clearcoat, clearcoatRoughness, sheenColor, sheenRoughness,
    diffuseTransmission, diffuseTransmissionCol);

// 12. Ambient Image-Based Lighting Evaluation
vec3 ambient = evaluateIBLLighting(
    N, V, NdotV, albedoLinear, roughness, metallic, F0,
    clearcoat, clearcoatRoughness, sheenColor, sheenRoughness,
    diffuseTransmission, thickness, ao);
