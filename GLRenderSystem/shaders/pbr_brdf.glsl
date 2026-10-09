#ifndef PBR_BRDF_GLSL
#define PBR_BRDF_GLSL

// =============================================================================
// PBR BRDF Core Functions
// Conforms to Khronos glTF 2.0 Specification (Appendix B: BRDF Implementation)
// =============================================================================

#ifndef PI
#define PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------------
// 1. Normal Distribution Function (NDF): Trowbridge-Reitz (GGX)
// D(h) = alpha^2 / (PI * ((N.H)^2 * (alpha^2 - 1) + 1)^2)
// where alpha = roughness^2
// -----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

// -----------------------------------------------------------------------------
// 2. Visibility / Shadowing-Masking Function: Height-Correlated Smith Joint GGX
// V(l, v) = G(l, v, h) / (4 * (N.L) * (N.V))
// Reference: Heitz 2014, "Understanding the Masking-Shadowing Function"
// Khronos glTF 2.0 standard: avoids grazing-angle over-darkening and removes
// the separate 4 * (N.L) * (N.V) division in the denominator.
// -----------------------------------------------------------------------------
float V_SmithJointGGX(float NdotL, float NdotV, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    float v = 0.5 / max(lambdaV + lambdaL, 0.00001);
    return clamp(v, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// 3. Fresnel Reflectance: Schlick's Approximation
// F(v, h) = F0 + (1 - F0) * (1 - (V.H))^5
// -----------------------------------------------------------------------------
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// -----------------------------------------------------------------------------
// 4. KHR_materials_sheen (Charlie NDF + Ashikhmin Visibility)
// Specially modeled for microfiber, cloth, and velvet.
// -----------------------------------------------------------------------------
float D_Charlie(float roughness, float NdotH)
{
    float invAlpha = 1.0 / max(roughness, 0.000001);
    float cos2h = NdotH * NdotH;
    float sin2h = max(1.0 - cos2h, 0.0078125); // Prevent 0
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

float V_Ashikhmin(float NdotL, float NdotV)
{
    return 1.0 / max(4.0 * (NdotL + NdotV - NdotL * NdotV), 0.0001);
}

// -----------------------------------------------------------------------------
// 5. KHR_materials_anisotropy (Heitz Anisotropic GGX)
// Models directional microstructures such as brushed metals and hair.
// -----------------------------------------------------------------------------
float D_GGX_Anisotropic(float at, float ab, float TdotH, float BdotH, float NdotH)
{
    float a2 = at * ab;
    vec3 d = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
    float d2 = dot(d, d);
    float b2 = a2 / max(d2, 0.000001);
    return a2 * b2 * b2 / PI;
}

float V_GGX_Anisotropic(float at, float ab, float TdotV, float BdotV, float NdotV, float TdotL, float BdotL, float NdotL)
{
    float lambdaV = NdotL * length(vec3(at * TdotV, ab * BdotV, NdotV));
    float lambdaL = NdotV * length(vec3(at * TdotL, ab * BdotL, NdotL));
    float v = 0.5 / max(lambdaV + lambdaL, 0.00001);
    return clamp(v, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// 6. Split-Sum Environment BRDF Approximation (Karis 2013 / Lazarov 2013)
// Fits the integrated 2D LUT response for E(N.V, roughness).
// -----------------------------------------------------------------------------
vec2 EnvBRDFApprox(float roughness, float NoV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return AB;
}

// -----------------------------------------------------------------------------
// 7. KHR_materials_iridescence (Thin-Film Wave Interference)
// Computes optical path difference and Fraunhofer spectral wavelengths.
// -----------------------------------------------------------------------------
vec3 evalIridescence(float NoV, float iridIor, float thicknessNm)
{
    float cosTheta1 = clamp(NoV, 0.0, 1.0);
    float sinTheta1Sq = 1.0 - cosTheta1 * cosTheta1;
    float eta = 1.0 / max(iridIor, 1.0001);
    float sinTheta2Sq = eta * eta * sinTheta1Sq;
    if (sinTheta2Sq > 1.0) return vec3(1.0); // Total Internal Reflection
    float cosTheta2 = sqrt(max(1.0 - sinTheta2Sq, 0.0));

    // Optical path difference in nanometers
    float opd = 2.0 * iridIor * thicknessNm * cosTheta2;
    // Khronos Fraunhofer spectral wavelengths: Red (656.27 nm), Green (587.56 nm), Blue (486.13 nm)
    const vec3 lambda = vec3(656.27, 587.56, 486.13);
    vec3 phase = 2.0 * PI * opd / lambda;
    vec3 interference = 0.5 + 0.5 * cos(phase);

    float r0 = pow((1.0 - iridIor) / (1.0 + iridIor), 2.0);
    float f_film = r0 + (1.0 - r0) * pow(clamp(1.0 - cosTheta1, 0.0, 1.0), 5.0);
    return mix(vec3(f_film), interference, 0.75);
}

#endif // PBR_BRDF_GLSL
