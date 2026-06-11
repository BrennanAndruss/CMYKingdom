#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include "resources/Handle.h"

namespace engine
{
    class Shader;
    class Texture;
}

namespace engine
{
    enum class RenderMode
    {
        Opaque,
        Transparent,
        Terrain,
        Water,
    };

    struct Material
    {
        RenderMode renderMode = RenderMode::Opaque;

        Handle<Shader> shader;
        Handle<Texture> difTex;
        Handle<Texture> specTex;

        float shininess = 1.0f;
        glm::vec3 ambient = glm::vec3(0.0f);
        glm::vec3 diffuse = glm::vec3(0.0f);
        glm::vec3 specular = glm::vec3(0.0f);

        static constexpr int MAX_SPLATMAPS = 3;
        static constexpr int MAX_TERRAIN_TEXTURES = 11;

        std::array<Handle<Texture>, MAX_SPLATMAPS> splatMaps;
        std::array<Handle<Texture>, MAX_TERRAIN_TEXTURES> terrainTextures;

        int splatMapCount = 3;
        int terrainTextureCount = 11;

        Handle<Texture> terrainHeightTex;

        float terrainTextureTiling = 32.0f;
        float terrainPlaneLen = 500.0f;
        float terrainHeightScale = 400.0f;
    };
}