#version 460 core
out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uSceneTexture;
uniform float uThreshold;

void main()
{
    vec4 color = texture(uSceneTexture, vTexCoord);
    
    // Check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > uThreshold)
        FragColor = vec4(color.rgb, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
