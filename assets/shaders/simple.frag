#version 410 core

#define MAX_LIGHTS 16

in vec3 fragPos;
in vec3 fragNor;
in vec2 fragTexCoord;

//commented out for now
/*
uniform samplerCube irradianceMap;
uniform int useIrradianceMap;
uniform float irradianceStrength;
*/

out vec4 color;

struct Material
{
	vec3 ambient;
	vec3 diffuse;
	sampler2D difTex;
	vec3 specular;
	sampler2D specTex;
	float shininess;
};

struct Light
{
	vec4 color_intensity;	// rgb = color, a = intensity
	vec4 position_range;	// xyz = position, w = range
	vec4 direction_type;	// xyz = direction, w = type
};

const int LIGHT_DIRECTIONAL = 0;
const int LIGHT_POINT = 1;

layout (std140) uniform CameraData
{
	mat4 view;
	mat4 projection;
	vec4 cameraPos;
};

layout(std140) uniform LightData
{
	Light lights[MAX_LIGHTS];
};

uniform int numLights;

uniform Material mat;

void main()
{
	vec3 normal = normalize(fragNor);
	vec3 view = normalize(cameraPos.xyz - fragPos);
	
	vec3 sampledDif = mat.diffuse * texture(mat.difTex, fragTexCoord).rgb;
	vec3 sampledSpec = mat.specular * texture(mat.specTex, fragTexCoord).rgb;
	vec3 ambient = mat.ambient * sampledDif;

	//commented out for now 
	/*
	vec3 ambient;
	if (useIrradianceMap == 1)
	{
		vec3 skyAmbient = texture(irradianceMap, normal).rgb;
		ambient = skyAmbient * sampledDif * irradianceStrength;
	}
	else
	{
		ambient = mat.ambient * sampledDif;
	}
	*/
	
	vec3 diffuseSum = vec3(0.0);
	vec3 specularSum = vec3(0.0);

	for (int i = 0; i < numLights; i++)
	{
		float attenuation = 1.0;
		vec3 lightColor = lights[i].color_intensity.rgb * lights[i].color_intensity.a;
		int lightType = int(lights[i].direction_type.w);

		vec3 lightDir;
		if (lightType == LIGHT_DIRECTIONAL)
		{
			lightDir = normalize(-lights[i].direction_type.xyz);
		}
		else if (lightType == LIGHT_POINT)
		{
			vec3 toLight = lights[i].position_range.xyz - fragPos;
			float dist = length(toLight);
			lightDir = normalize(toLight);
			attenuation = 1.0 / (dist * dist);
		}

		// Diffuse
		float dC = max(dot(normal, lightDir), 0.0);
		diffuseSum += sampledDif * dC * lightColor * attenuation;

		// Specular
		vec3 halfDir = normalize(lightDir + view);
		float sC = max(dot(normal, halfDir), 0.0);
		specularSum += sampledSpec * pow(sC, mat.shininess) * lightColor * attenuation;
	}

	vec3 reflection = ambient + diffuseSum + specularSum;
	color = vec4(reflection, 1.0);
}