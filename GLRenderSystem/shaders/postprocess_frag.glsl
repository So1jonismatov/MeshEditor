#version 460 core
out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform float uExposure = 1.0;
uniform float uContrast = 1.0;

// 1 = PBR HDR pipeline (full tonemap), 0 = Phong/Flat (already LDR, pass-through)
uniform int uIsPbr = 0;

// Tone mapping operator: 0 = Khronos PBR Neutral (default), 1 = ACES Filmic
uniform int uTonemapOperator = 0;

#include "pbr_tonemap.glsl"

void main()
{
    vec3 hdrColor = texture(uSceneTexture, vTexCoord).rgb;
    vec3 bloomColor = texture(uBloomTexture, vTexCoord).rgb;
    hdrColor += bloomColor; // Additive bloom blending

    vec3 finalColor;

    if (uIsPbr == 1)
    {
        // Natural HDR exposure + tone mapping + gamma correction
        float expFactor = max(uExposure, 0.001);
        vec3 exposed = max(hdrColor * expFactor, vec3(0.0));
        vec3 tonemapped = evaluateToneMapping(exposed, uTonemapOperator);
        finalColor = pow(tonemapped, vec3(1.0 / 2.2));
    }
    else
    {
        // Phong / Flat: pass-through
        finalColor = hdrColor;
    }

    // Post-tonemap contrast adjustment (only if contrast is not 1.0)
    if (abs(uContrast - 1.0) > 0.01)
    {
        finalColor = clamp((finalColor - vec3(0.5)) * uContrast + vec3(0.5), 0.0, 1.0);
    }

    FragColor = vec4(finalColor, 1.0);
}
