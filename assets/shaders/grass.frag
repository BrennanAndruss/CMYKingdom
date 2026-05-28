#version 410 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uGrassTex;

void main()
{
    vec4 color = texture(uGrassTex, TexCoord);

    color.rgb *= 0.70;

    if (color.a < 0.35)
        discard;

    FragColor = color;
}