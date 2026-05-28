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

uniform sampler2D splat0;
uniform sampler2D terrainGrass;
uniform sampler2D terrainSand;
uniform sampler2D terrainRock;
uniform sampler2D terrainSnow;
uniform float terrainTextureTiling;

void main()
{
    vec4 weights = texture(splat0, fragTexCoord);

    float total = weights.r + weights.g + weights.b + weights.a;
    if (total > 0.0001)
    {
        weights /= total;
    }

    vec2 terrainUV = fragTexCoord * terrainTextureTiling;

    vec3 albedo =
        texture(terrainGrass, terrainUV).rgb * weights.r +
        texture(terrainSand, terrainUV).rgb  * weights.g +
        texture(terrainRock, terrainUV).rgb  * weights.b +
        texture(terrainSnow, terrainUV).rgb  * weights.a;
	
	gPosition = fragPos;
	gNormalShine = vec4(normalize(fragNor), mat.shininess / 256.0);
	gAlbedoSpec = vec4(albedo, 0.0);
}