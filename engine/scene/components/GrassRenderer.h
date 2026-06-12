#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "scene/components/Component.h"
#include "resources/Handle.h"
#include "renderer/BoundingVolume.h"

namespace engine
{
	class Shader;
	class Texture;
	class Heightmap;

	struct GrassInstance
	{
		glm::vec4 posScale;     // xyz = world position, w = height scale
		glm::vec4 rotBendSeed;  // x = rotation, y = bend, z = width scale, w = random seed
	};

	struct GrassPatch
	{
		glm::ivec2 coord = glm::ivec2(0);
		glm::vec3 center = glm::vec3(0.0f);
		BBox bounds;

		GLuint instanceVbo = 0;
		std::vector<GrassInstance> instances;
	};

	class GrassRenderer : public Component
	{
	public:
		Handle<Shader> shader;
		Handle<Shader> shadowShader;
		Handle<Texture> texture;
		Handle<Texture> splatmap;
		const Texture* _splatTextureCpu = nullptr;

		// Heightmap Variables
		Heightmap* heightmap = nullptr;
		float terrainPlaneLen = 500.0f;
		

		// Splatmapping Filtering Variables 
		bool useSplatmapPlacement = true;
		float minGrassSplatWeight = 0.35f;
		int maxPlacementAttemptsMultiplier = 10;

		// Grass Clumping Variables
		bool useDensityNoise = true;
		float densityNoiseScale = 0.08f;
		float densityNoiseStrength = 0.65f;

		// Wind Variables
		glm::vec2 windDirection = glm::vec2(1.0f, 0.25f);
		float windStrength = 0.25f;
		float windSpeed = 1.8f;

		// Grass Instancing Data
		int bladeCount = 20000; 
		float minHeight = 0.8f;
		float maxHeight = 1.8f;
		float minWidth = 0.04f;
		float maxWidth = 0.10f;
		float maxBend = 0.35f;

		glm::vec3 centerPosition = glm::vec3(0.0f);
		float spawnRadius = 800.0f;
		float reloadDistance = 20.0f;

		// Patch Variables
		BBox worldBounds;
		bool usePatchStreaming = false;

		float patchSize = 25.0f;
		int patchRadius = 3;
		int bladesPerPatch = 1200;

		void regenerate(const glm::vec3& newCenter);
		void updateStreaming(const glm::vec3& playerPos);

		void start() override;
		void draw(const AssetManager& assets, const Frustum& frustum) const;
		void drawShadow(const AssetManager& assets, const Frustum& lightFrustum, 
			const glm::mat4& lightSpace) const;
		void setSplatTextureCpu(const Texture* texture);


	private:
		GLuint _vao = 0;
		GLuint _bladeVbo = 0;
		GLuint _ebo = 0;
		GLuint _instanceVbo = 0;
		

		GLsizei _indexCount = 0;
		std::vector<GrassInstance> _instances;

		std::vector<GrassPatch> _patches;

		glm::ivec2 worldToPatchCoord(const glm::vec3& worldPos) const;
		glm::vec3 patchCoordToCenter(const glm::ivec2& coord) const;
		glm::ivec2 _currentCenterPatch = glm::ivec2(999999);

		void updatePatchesAround(const glm::vec3& playerPos);
		void generatePatch(GrassPatch& patch);
		void buildBladeMesh();
		void generateInstances();
	};
}