#ifndef PBR_TONEMAP_GLSL
#define PBR_TONEMAP_GLSL

// =============================================================================
// PBR Tone Mapping Functions
// 1. Khronos PBR Neutral Tone Mapping (Khronos Group 3D Commerce Specification 2024)
//    Designed specifically for product and CAD rendering to faithfully preserve
//    the baseColor hue and saturation under neutral white light.
// 2. ACES Filmic Tone Mapping (Narkowicz 2015)
//    Cinematic S-curve for high-contrast film-like visuals.
// =============================================================================

// Khronos 2024 PBR Neutral Tone Mapper
vec3 PBRNeutralToneMapping(vec3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0), g);
}

// ACES Filmic Tone Mapping Curve (Narkowicz 2015)
vec3 ACESFilmic(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Unified Tone Mapping Evaluator
// operatorMode: 0 = Khronos PBR Neutral (default), 1 = ACES Filmic
vec3 evaluateToneMapping(vec3 hdrColor, int operatorMode)
{
    if (operatorMode == 1)
    {
        return ACESFilmic(hdrColor);
    }
    return PBRNeutralToneMapping(hdrColor);
}

#endif // PBR_TONEMAP_GLSL
