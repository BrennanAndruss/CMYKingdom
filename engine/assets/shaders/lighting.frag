#version 410 core

#define NUM_CASCADES 3
#define MAX_LIGHTS 16

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D gPosition;
uniform sampler2D gNormalShine;
uniform sampler2D gAlbedoSpec;

uniform sampler2DArrayShadow shadowMaps;

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

layout (std140) uniform ShadowData
{
	mat4 cascadeLightSpaces[NUM_CASCADES];
	vec4 cascadeRadii;
	vec4 cascadeSplits;
	int numCascades;
	float shadowBias;
	vec2 _pad;
};

uniform int numLights;

uniform bool useIrradianceMap;
uniform samplerCube irradianceMap;
uniform float irradianceStrength;

// Debug variables
uniform int debugView;
uniform bool showCascades;

// 1.0, 0.5, 0.2
const float biasScales[3] = float[](1.0, 0.5, 0.2);

const vec3 cascadeTints[3] = vec3[](
    vec3(1.4, 0.6, 0.6),
    vec3(0.6, 1.4, 0.6),
    vec3(0.6, 0.6, 1.4)
);

float sampleShadow(vec3 worldPos, float NdotL, int layer)
{
	// Transform to light space, perspective divide, and transform to [0, 1]
	vec4 lightSpacePos = cascadeLightSpaces[layer] * vec4(worldPos, 1.0);
	lightSpacePos.xyz /= lightSpacePos.w;
	vec3 shifted = lightSpacePos.xyz * 0.5 + 0.5;

	if (shifted.z > 1.0) return 0.0;

	// Sloped-scaled bias to increase bias for surfaces at glancing angles
	// Further reduce bias for lower cascades to resolve peter panning
	float biasModifier = cascadeRadii[layer] / cascadeRadii[0] * biasScales[layer];
	float bias = max(shadowBias * (1.0 - NdotL), shadowBias * 0.1) * biasModifier;
	
	// Get lookup coordinate for 2D Array Shadow sampler
	// Apply bias to reference for sample compare
	vec4 projCoord = vec4(shifted.xy, float(layer), shifted.z - bias);

	// Compare mode returns visibility
	// Linear filtering performs hardware PCF for smooth shadows
	float visibility = texture(shadowMaps, projCoord);
	return visibility;
}

int getCascade(float viewDepth)
{
	for (int i = 0; i < numCascades; i++)
	{
		if (viewDepth < cascadeSplits[i])
		{
			return i;
		}
	}
}

void main()
{
	vec3 fragPos = texture(gPosition, fragTexCoord).rgb;
	vec4 normalData = texture(gNormalShine, fragTexCoord);
	vec4 albedoData = texture(gAlbedoSpec, fragTexCoord);

	// Early return for skybox pixels (positions will be zero)
	if (length(fragPos) < 0.001)
	{
		color = vec4(0.0);
		return;
	}

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

	vec3 ambient = albedo * 0.1;
	vec3 diffuseSum = vec3(0.0);
	vec3 specularSum = vec3(0.0);

	if (useIrradianceMap)
    {
        vec3 skyAmbient = texture(irradianceMap, normal).rgb;
        ambient = skyAmbient * albedo * irradianceStrength;
    }
	
	// Resolve cascade layer index
	vec4 viewPos = view * vec4(fragPos, 1.0);
	float viewDepth = -viewPos.z;
	int layer = getCascade(viewDepth);

	vec3 viewDir = normalize(cameraPos.xyz - fragPos);
	bool primaryShadowApplied = false;

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

		float NdotL = max(dot(normal, lightDir), 0.0);

		// Shadow factor
		float visibility = 1.0;
		if (lightType == LIGHT_DIRECTIONAL && !primaryShadowApplied)
		{
			visibility = sampleShadow(fragPos, NdotL, layer);

			// Shadow maps only match the first directional light
			primaryShadowApplied = true;
		}

		// Diffuse
		float dC = max(NdotL, 0.0);
		diffuseSum += albedo * dC * lightColor * attenuation * visibility;

		// Specular
		vec3 halfDir = normalize(lightDir + viewDir);
		float sC = pow(max(dot(normal, halfDir), 0.0), shininess);
		specularSum += vec3(specStrength) * sC * lightColor * attenuation * visibility;
	}

	vec3 lighting = ambient + diffuseSum + specularSum;
	color = vec4(lighting, 1.0);

	if (showCascades) color.rgb *= cascadeTints[layer];
}