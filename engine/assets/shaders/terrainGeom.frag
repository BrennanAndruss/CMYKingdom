#version 410 core

#define MAX_SPLATMAPS 3
#define MAX_TERRAIN_TEXTURES 11

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

uniform sampler2D splatMaps[MAX_SPLATMAPS];
uniform sampler2D terrainTextures[MAX_TERRAIN_TEXTURES];

uniform int splatMapCount;
uniform int terrainTextureCount;

uniform float terrainTextureTiling;

void main()
{
    vec2 terrainUV = fragTexCoord * terrainTextureTiling;

    vec3 albedo = vec3(0.0);
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

            albedo +=
                texture(
                    terrainTextures[texIndex],
                    terrainUV
                ).rgb * weight;

            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0001)
    {
        albedo /= totalWeight;
    }
    else
    {
        albedo =
            texture(
                terrainTextures[0],
                terrainUV
            ).rgb;
    }

    gPosition = fragPos;

    gNormalShine =
        vec4(
            normalize(fragNor),
            mat.shininess / 256.0
        );

    gAlbedoSpec = vec4(albedo, 0.0);
}