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
	// Properties indexed per cascade
	vec4 cascadeRadii;
	vec4 cascadeSplits;
	vec4 cascadeBiases;
};

uniform int numLights;

uniform int shadowSampleCount;
uniform float shadowPenumbraSize;

uniform bool useIrradianceMap;
uniform samplerCube irradianceMap;
uniform float irradianceStrength;

// Poisson disk distribution
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// Debug variables
uniform int debugView;
uniform bool showCascades;

const vec3 cascadeTints[3] = vec3[](
    vec3(1.4, 0.6, 0.6),
    vec3(0.6, 1.4, 0.6),
    vec3(0.6, 0.6, 1.4)
);

// Random number generator returning values [0, 1)
float random(vec3 seed, int i)
{
	float dotProd = dot(vec4(seed, float(i)), vec4(12.9898, 78.233, 45.164, 94.673));
	return fract(sin(dotProd) * 43758.5453);
}

float sampleShadow(vec3 worldPos, float NdotL, int layer)
{
	// Transform to light space, perspective divide, and transform to [0, 1]
	vec4 lightSpacePos = cascadeLightSpaces[layer] * vec4(worldPos, 1.0);
	lightSpacePos.xyz /= lightSpacePos.w;
	vec3 shifted = lightSpacePos.xyz * 0.5 + 0.5;

	if (shifted.z > 1.0) return 0.0;

	// Compute sloped-scaled bias to increase bias for surfaces at glancing angles
	float shadowBias = cascadeBiases[layer];
	float biasModifier = cascadeRadii[0] / cascadeRadii[layer];
	float bias = max(shadowBias * (1.0 - NdotL), shadowBias * 0.1) * biasModifier;

	// Apply bias to reference for sample compare
	float compareDepth = shifted.z - bias;

	// Get texel sizes for this cascade's shadow map and scale to texel space
	vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMaps, 0).xy);
	vec2 texelQuantized = shifted.xy / texelSize;

	// Rotate the poisson disk randomly per fragment
	// Seed with screen space to keep grain structure static
	float angle = random(vec3(gl_FragCoord.xy, float(layer)), 0) * 6.2831853;
	float c = cos(angle), s = sin(angle);
	mat2 rotMat = mat2(c, -s, s, c);

	// Scale down penumbra coverage relative to cascade layer
	float cascadeScale = cascadeRadii[0] / cascadeRadii[layer];
	float spread = shadowPenumbraSize * cascadeScale;
	
	// Perform percentage-closer filtering for smooth shadows
	float visibility = 0.0;
	for (int i = 0; i < shadowSampleCount; i++)
	{
		vec2 offset = rotMat * poissonDisk[i] * spread;
		vec2 texelCoord = texelQuantized + offset;
		
        // Compute manual bilinear weights based on the fraction within the texel
        vec2 f = fract(texelCoord);
        vec2 baseCoord = (floor(texelCoord) + 0.5) * texelSize;

        // Gather the 4 adjacent texel comparison samples
        float s00 = texture(shadowMaps, vec4(
			baseCoord + vec2(-texelSize.x, -texelSize.y) * 0.5, 
			float(layer), compareDepth));
        float s10 = texture(shadowMaps, vec4(
			baseCoord + vec2(texelSize.x, -texelSize.y) * 0.5, 
			float(layer), compareDepth));
        float s01 = texture(shadowMaps, vec4(
			baseCoord + vec2(-texelSize.x, texelSize.y) * 0.5, 
			float(layer), compareDepth));
        float s11 = texture(shadowMaps, vec4(
			baseCoord + vec2(texelSize.x, texelSize.y) * 0.5, 
			float(layer), compareDepth));

        // Bilinear interpolation of the shadow samples
        float bilinearPCF = mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
        visibility += bilinearPCF;
	}

	return visibility / float(shadowSampleCount);
}

int getCascade(float viewDepth)
{
	for (int i = 0; i < NUM_CASCADES; i++)
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
			float visibilityNext = 1.0;
			float visibilityCurrent = sampleShadow(fragPos, NdotL, layer);

			// Define blend zone width
			float blendBand = 5.0;
			float currentSplit = cascadeSplits[layer];
			float distToSplit = currentSplit - viewDepth;

			// Blend if we are close to the edge and a further cascade exists
			if (distToSplit < blendBand && layer < (NUM_CASCADES - 1))
			{
				visibilityNext = sampleShadow(fragPos, NdotL, layer + 1);
				float blendFactor = clamp(distToSplit / blendBand, 0.0, 1.0);
            
				// Linear interpolation blends the boundary away smoothly
				visibility = mix(visibilityNext, visibilityCurrent, blendFactor);
			}
			else
			{
				visibility = visibilityCurrent;
			}

			// Shadow maps only match the first directional light
			primaryShadowApplied = true;
		}

		if (visibility < 1.0) 
		{
			// color = vec4(1, 0, 0, 1);
			// return;
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