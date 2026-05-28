#version 410 core

#define MAX_LIGHTS 16

in vec3 fragPos;
in vec3 fragNor;
in vec2 fragTexCoord;

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
    vec4 color_intensity;    // rgb = color, a = intensity
    vec4 position_range;     // xyz = position, w = range
    vec4 direction_type;     // xyz = direction, w = type
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

    vec3 grass = texture(terrainGrass, terrainUV).rgb;
    vec3 sand  = texture(terrainSand, terrainUV).rgb;
    vec3 rock  = texture(terrainRock, terrainUV).rgb;
    vec3 snow  = texture(terrainSnow, terrainUV).rgb;

    vec3 blended =
        grass * weights.r +
        sand  * weights.g +
        rock  * weights.b +
        snow  * weights.a;

    vec3 normal = normalize(fragNor);
    vec3 viewDir = normalize(cameraPos.xyz - fragPos);

    vec3 sampledDif = mat.diffuse * blended;
    vec3 sampledSpec = mat.specular;
    vec3 diffuseSum = vec3(0.0);
    vec3 specularSum = vec3(0.0);
    vec3 ambient = mat.ambient * sampledDif;


    for (int i = 0; i < numLights; i++)
    {
        float attenuation = 1.0;
        vec3 lightColor = lights[i].color_intensity.rgb * lights[i].color_intensity.a;
        int lightType = int(lights[i].direction_type.w);

        vec3 lightDir = vec3(0.0);

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

        float dC = max(dot(normal, lightDir), 0.0);
        diffuseSum += sampledDif * dC * lightColor * attenuation;

        vec3 halfDir = normalize(lightDir + viewDir);
        float sC = max(dot(normal, halfDir), 0.0);
        specularSum += sampledSpec * pow(sC, mat.shininess) * lightColor * attenuation;
    }

    vec3 reflection = ambient + diffuseSum + specularSum;
    color = vec4(reflection, 1.0);
}