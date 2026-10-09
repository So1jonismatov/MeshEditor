#version 460 core

layout (location = 0) in vec2 vTexCoord;
layout (location = 1) in vec3 vRayDir;
uniform vec3 viewPos;

out vec4 FragColor;

vec3 getSkyColor(vec3 rayDir)
{
    vec3 dir = normalize(rayDir);
    float y = dir.y;

    // Atmospheric colors - soft, balanced studio dome
    vec3 zenithColor = vec3(0.12, 0.25, 0.48);     // Soft deep blue zenith
    vec3 horizonColor = vec3(0.55, 0.64, 0.72);    // Gentle hazy horizon
    vec3 groundColor = vec3(0.14, 0.15, 0.17);     // Dark neutral ground
    vec3 groundHorizon = vec3(0.24, 0.26, 0.28);   // Ground horizon

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

    // Sun disc / directional glow
    vec3 sunDir = normalize(vec3(0.5, 0.7, 0.5));
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunDisc = pow(sunDot, 256.0) * 1.2;
    float sunGlow = pow(sunDot, 16.0) * 0.25;
    vec3 sunColor = vec3(1.0, 0.95, 0.85);

    sky += (sunDisc + sunGlow) * sunColor;

    // Subtle horizon haze line
    float horizonBand = exp(-abs(y) * 20.0) * 0.10;
    sky += vec3(horizonBand);

    return sky * 0.85;
}

void main()
{
    vec3 dir = vRayDir - viewPos;
    vec3 sky = getSkyColor(dir);
    FragColor = vec4(sky, 1.0);
}
