#version 410 core

#define MAX_LIGHTS 16

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D gPosition;
uniform sampler2D gNormalShine;
uniform sampler2D gAlbedoSpec;

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

layout (std140) uniform LightData
{
	Light lights[MAX_LIGHTS];
};

uniform int numLights;
uniform int debugView;

void main()
{
	vec3 fragPos = texture(gPosition, fragTexCoord).rgb;
	vec4 normalData = texture(gNormalShine, fragTexCoord);
	vec4 albedoData = texture(gAlbedoSpec, fragTexCoord);

	// Debug views for G-buffer textures
	if (debugView == 0)
	{
		color = vec4(fragPos, 1.0);
		return;
	}
	else if (debugView == 1)
	{
		color = vec4(normalData.rgb, 1.0);
		return;
	}
	else if (debugView == 2)
	{
		color = vec4(albedoData.rgb, 1.0);
		return;
	}

	vec3 normal = normalize(normalData.rgb);
	float shininess = normalData.a * 256.0; // unpack from [0, 1]
	vec3 albedo = albedoData.rgb;
	float specStrength = albedoData.a;

	// Early return for skybox pixels
	// Positions will be zero
	if (length(fragPos) < 0.001)
	{
		color = vec4(0.0);
		return;
	}

	vec3 view = normalize(cameraPos.xyz - fragPos);

	vec3 ambient = albedo * 0.1;
	vec3 diffuseSum = vec3(0.0);
	vec3 specularSum = vec3(0.0);

	// Accumulate lights
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
		diffuseSum += albedo * dC * lightColor * attenuation;

		// Specular
		vec3 halfDir = normalize(lightDir + view);
		float sC = pow(max(dot(normal, halfDir), 0.0), shininess);
		specularSum += vec3(specStrength) * sC * lightColor * attenuation;

		// test
		// ambient = vec3(1.0, 0.0, 0.0);
	}

	vec3 lighting = ambient + diffuseSum + specularSum;
	color = vec4(lighting, 1.0);
}