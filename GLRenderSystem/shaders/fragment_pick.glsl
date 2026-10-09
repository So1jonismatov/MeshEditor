#version 460 core

// Writes a unique id per triangle: (uBaseId + gl_PrimitiveID) packed into the
// low 24 bits of an RGB8 colour. The CPU reads it back with glReadPixels and
// maps the triangle index to a mesh face. White (0xFFFFFF) means "no hit".

uniform int uBaseId;

out vec4 FragColor;

void main()
{
    int id = uBaseId + gl_PrimitiveID;
    float r = float(id & 0xFF) / 255.0;
    float g = float((id >> 8) & 0xFF) / 255.0;
    float b = float((id >> 16) & 0xFF) / 255.0;
    FragColor = vec4(r, g, b, 1.0);
}
