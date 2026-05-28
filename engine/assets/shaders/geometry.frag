#version 410 core

in vec3 fragPos;
in vec3 fragNor;
in vec2 fragTexCoord;

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec4 gNormalShine;
layout (location = 2) out vec4 gAlbedoSpec;

uniform struct Material
{
	vec3 ambient;
	vec3 diffuse;
	sampler2D difTex;
	vec3 specular;
	sampler2D specTex;
	float shininess;
} mat;

void main()
{
	vec3 albedo = mat.diffuse * texture(mat.difTex, fragTexCoord).rgb;
	float specStr = texture(mat.specTex, fragTexCoord).r;
	
	gPosition = fragPos;
	gNormalShine = vec4(normalize(fragNor), mat.shininess / 256.0); // pack to [0, 1]
	gAlbedoSpec = vec4(albedo, specStr);
}