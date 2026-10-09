#version 460 core

// Per-fragment Phong / PBR vertex stage:
// Transforms and forwards vertex attributes (world position, normal, colour, UV, tangent)
// to the downstream geometry or fragment stage.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aTexCoord;
layout(location = 4) in vec4 aTangent;

uniform mat4 world;
uniform mat4 worldViewProj;
uniform mat3 normalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vTexCoord;
out vec4 vTangent;

void main()
{
    vec4 worldPos = world * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalMatrix * aNormal;
    vColor = aColor;
    vTexCoord = aTexCoord;
    vTangent = vec4(normalMatrix * aTangent.xyz, aTangent.w);
    gl_Position = worldViewProj * vec4(aPosition, 1.0);
}
