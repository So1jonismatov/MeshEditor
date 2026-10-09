#version 460 core

layout (location = 0) out vec2 vTexCoord;
layout (location = 1) out vec3 vRayDir;

uniform mat4 invViewProj;

void main()
{
    // Full screen triangle: (-1,-1), (3,-1), (-1,3)
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                    (gl_VertexID == 2) ? 3.0 : -1.0);
    vTexCoord = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.999999, 1.0); // at z = 1.0 (far plane)

    vec4 unprojected = invViewProj * vec4(pos, 1.0, 1.0);
    vRayDir = unprojected.xyz / unprojected.w;
}
