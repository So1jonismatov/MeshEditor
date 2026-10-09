#version 460 core

// Colour-id picking: transform position only. The fragment stage encodes the
// per-draw base id plus gl_PrimitiveID into an RGB colour.

layout(location = 0) in vec3 aPosition;

uniform mat4 worldViewProj;

void main()
{
    gl_Position = worldViewProj * vec4(aPosition, 1.0);
}
