#pragma once

#include <glm/glm.hpp>
#include <vector>
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

		Handle<Texture> splat0;

		Handle<Texture> terrainGrass;
		Handle<Texture> terrainSand;
		Handle<Texture> terrainRock;
		Handle<Texture> terrainSnow;
		Handle<Texture> terrainHeightTex;

		float terrainTextureTiling = 32.0f;
		float terrainPlaneLen = 500.0f;
		float terrainHeightScale = 400.0f;
	};
}