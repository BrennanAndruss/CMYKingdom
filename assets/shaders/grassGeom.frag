#version 410 core

in vec3 WorldPos;
in vec3 WorldNor;
in vec2 TexCoord;

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec4 gNormalShine;
layout (location = 2) out vec4 gAlbedoSpec;

uniform sampler2D uGrassTex;

void main()
{
	vec4 albedo = texture(uGrassTex, TexCoord);
	albedo.rgb *= 0.70;
	//if (albedo.a < 0.35)
	//	discard;

	// Flip normal for back faces
	vec3 normal = gl_FrontFacing ? WorldNor : -WorldNor;

	// Blend face normal toward world up for softer lighting
	float normalBend = 0.5;
	vec3 bentNormal = normalize(mix(normal, vec3(0.0, 1.0, 0.0), normalBend));

	gPosition = WorldPos;
	gNormalShine = vec4(bentNormal, 32.0 / 256.0);
	gAlbedoSpec = vec4(albedo.rgb, 0.3);
}