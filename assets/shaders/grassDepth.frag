#version 410 core

in vec2 TexCoord;

uniform sampler2D uGrassTex;

void main()
{
	vec4 color = texture(uGrassTex, TexCoord);
	//if (color.a < 0.35)
	//	discard;

	// Depth written automatically
}