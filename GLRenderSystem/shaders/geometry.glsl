#version 460 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Forwards per-vertex attributes to the fragment stage and injects
// per-triangle barycentric coordinates used for the black-edge wireframe.

in vec3 vWorldPos[];
in vec3 vNormal[];
in vec3 vColor[];
in vec2 vTexCoord[];
in vec4 vTangent[];

out vec3 fWorldPos;
out vec3 fNormal;
out vec3 fColor;
out vec2 fTexCoord;
out vec4 fTangent;
out vec3 barycentric;

void main()
{
    vec3 bary[3] = vec3[3](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
                           vec3(0.0, 0.0, 1.0));
    for (int i = 0; i < 3; ++i)
    {
        fWorldPos = vWorldPos[i];
        fNormal = vNormal[i];
        fColor = vColor[i];
        fTexCoord = vTexCoord[i];
        fTangent = vTangent[i];
        barycentric = bary[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
