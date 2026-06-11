#version 410 core

#define MAX_LIGHTS 16
#define MAX_SPLATMAPS 3
#define MAX_TERRAIN_TEXTURES 11

in vec3 fragPos;
in vec3 fragNor;
in vec2 fragTexCoord;

uniform samplerCube irradianceMap;
uniform int useIrradianceMap;
uniform float irradianceStrength;

out vec4 color;

struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light
{
    vec4 color_intensity;
    vec4 position_range;
    vec4 direction_type;
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

uniform sampler2D splatMaps[MAX_SPLATMAPS];
uniform sampler2D terrainTextures[MAX_TERRAIN_TEXTURES];

uniform int splatMapCount;
uniform int terrainTextureCount;

uniform float terrainTextureTiling;

void main()
{
    vec2 terrainUV = fragTexCoord * terrainTextureTiling;

    vec3 blended = vec3(0.0);
    float totalWeight = 0.0;

    for (int s = 0; s < splatMapCount; s++)
    {
        vec4 weights = texture(splatMaps[s], fragTexCoord);

        for (int c = 0; c < 4; c++)
        {
            int texIndex = s * 4 + c;

            if (texIndex >= terrainTextureCount)
                continue;

            float weight = weights[c];

            blended += texture(terrainTextures[texIndex], terrainUV).rgb * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0001)
    {
        blended /= totalWeight;
    }
    else
    {
        blended = texture(terrainTextures[0], terrainUV).rgb;
    }

    vec3 normal = normalize(fragNor);
    vec3 viewDir = normalize(cameraPos.xyz - fragPos);

    vec3 sampledDif = mat.diffuse * blended;
    vec3 sampledSpec = mat.specular;

    vec3 diffuseSum = vec3(0.0);
    vec3 specularSum = vec3(0.0);
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

    for (int i = 0; i < numLights; i++)
    {
        float attenuation = 1.0;

        vec3 lightColor =
            lights[i].color_intensity.rgb *
            lights[i].color_intensity.a;

        int lightType = int(lights[i].direction_type.w);

        vec3 lightDir = vec3(0.0);

        if (lightType == LIGHT_DIRECTIONAL)
        {
            lightDir = normalize(-lights[i].direction_type.xyz);
        }
        else if (lightType == LIGHT_POINT)
        {
            vec3 toLight =
                lights[i].position_range.xyz - fragPos;

            float dist = length(toLight);

            lightDir = normalize(toLight);

            attenuation = 1.0 / (dist * dist);
        }

        float dC = max(dot(normal, lightDir), 0.0);

        diffuseSum +=
            sampledDif *
            dC *
            lightColor *
            attenuation;

        vec3 halfDir = normalize(lightDir + viewDir);

        float sC = max(dot(normal, halfDir), 0.0);

        specularSum +=
            sampledSpec *
            pow(sC, mat.shininess) *
            lightColor *
            attenuation;
    }

    vec3 reflection =
        ambient +
        diffuseSum +
        specularSum;

    color = vec4(reflection, 1.0);
}