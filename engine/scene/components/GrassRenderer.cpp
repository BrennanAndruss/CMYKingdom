#include "scene/components/GrassRenderer.h"

#include <random>
#include <glm/gtc/constants.hpp>
#include <core/Time.h>
#include "resources/AssetManager.h"
#include "resources/Heightmap.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Texture.h"
#include <glm/common.hpp>
#include <iostream>

namespace engine
{
	void GrassRenderer::start()
	{
		buildBladeMesh();

		if (usePatchStreaming)
		{
			_currentCenterPatch = worldToPatchCoord(centerPosition);
			updatePatchesAround(centerPosition);
		}
		else
		{
			generateInstances();

			glBindBuffer(GL_ARRAY_BUFFER, _instanceVbo);

			if (!_instances.empty())
			{
				glBufferData(
					GL_ARRAY_BUFFER,
					_instances.size() * sizeof(GrassInstance),
					_instances.data(),
					GL_STATIC_DRAW
				);
			}

			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	void GrassRenderer::setSplatTextureCpu(const Texture* texture)
	{
		_splatTextureCpu = texture;
	}

	void GrassRenderer::buildBladeMesh()
	{
		// curved vertical blade strip: position.xyz + uv.xy
		struct BladeVertex
		{
			glm::vec3 pos;
			glm::vec2 uv;
		};

		std::vector<BladeVertex> verts = {
			{{-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{ 0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{-0.4f, 0.33f, 0.0f}, {0.0f, 0.33f}},
			{{ 0.4f, 0.33f, 0.0f}, {1.0f, 0.33f}},
			{{-0.25f, 0.66f, 0.0f}, {0.0f, 0.66f}},
			{{ 0.25f, 0.66f, 0.0f}, {1.0f, 0.66f}},
			{{ 0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}},
		};

		std::vector<unsigned int> indices = {
			0, 1, 2,
			1, 3, 2,
			2, 3, 4,
			3, 5, 4,
			4, 5, 6
		};

		_indexCount = static_cast<GLsizei>(indices.size());

		glGenVertexArrays(1, &_vao);
		glBindVertexArray(_vao);

		glGenBuffers(1, &_bladeVbo);
		glBindBuffer(GL_ARRAY_BUFFER, _bladeVbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(BladeVertex), verts.data(), GL_STATIC_DRAW);

		glGenBuffers(1, &_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BladeVertex), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BladeVertex), (void*)offsetof(BladeVertex, uv));

		glGenBuffers(1, &_instanceVbo);
		glBindBuffer(GL_ARRAY_BUFFER, _instanceVbo);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)offsetof(GrassInstance, posScale));
		glVertexAttribDivisor(2, 1);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)offsetof(GrassInstance, rotBendSeed));
		glVertexAttribDivisor(3, 1);

		glBindVertexArray(0);
	}

	glm::ivec2 GrassRenderer::worldToPatchCoord(const glm::vec3& worldPos) const
	{
		return glm::ivec2(
			static_cast<int>(floor(worldPos.x / patchSize)),
			static_cast<int>(floor(worldPos.z / patchSize))
		);
	}

	glm::vec3 GrassRenderer::patchCoordToCenter(const glm::ivec2& coord) const
	{
		return glm::vec3(
			(coord.x + 0.5f) * patchSize,
			0.0f,
			(coord.y + 0.5f) * patchSize
		);
	}

	static float sampleGrassSplatWeight(const engine::Texture* splatTexture, float u, float v)
	{
		if (!splatTexture)
		{
			return 1.0f;
		}

		const auto& pixels = splatTexture->getPixels();
		const int width = splatTexture->getWidth();
		const int height = splatTexture->getHeight();
		const int channels = splatTexture->getChannels();

		if (pixels.empty() || width <= 0 || height <= 0 || channels <= 0)
		{
			return 1.0f;
		}

		u = glm::clamp(u, 0.0f, 1.0f);
		v = glm::clamp(v, 0.0f, 1.0f);

		const int x = static_cast<int>(u * static_cast<float>(width - 1));
		const int y = static_cast<int>(v * static_cast<float>(height - 1));

		const int index = (y * width + x) * channels;

		// terrain.frag says:
		// red   = grass
		// green = sand
		// blue  = rock
		// alpha = snow
		const unsigned char red = pixels[index + 0];

		return static_cast<float>(red) / 255.0f;
	}

	void GrassRenderer::updatePatchesAround(const glm::vec3& playerPos)
	{
		glm::ivec2 playerPatch = worldToPatchCoord(playerPos);

		_patches.clear();

		for (int dz = -patchRadius; dz <= patchRadius; ++dz)
		{
			for (int dx = -patchRadius; dx <= patchRadius; ++dx)
			{
				glm::ivec2 coord = playerPatch + glm::ivec2(dx, dz);

				GrassPatch patch;
				patch.coord = coord;
				patch.center = patchCoordToCenter(coord);

				glGenBuffers(1, &patch.instanceVbo);

				generatePatch(patch);

				if (!patch.instances.empty())
				{
					glBindBuffer(GL_ARRAY_BUFFER, patch.instanceVbo);
					glBufferData(
						GL_ARRAY_BUFFER,
						patch.instances.size() * sizeof(GrassInstance),
						patch.instances.data(),
						GL_STATIC_DRAW
					);
				}

				_patches.push_back(std::move(patch));
			}
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void GrassRenderer::generatePatch(GrassPatch& patch)
	{
		if (!heightmap) return;

		std::mt19937 rng(
			static_cast<unsigned int>(
				patch.coord.x * 73856093 ^
				patch.coord.y * 19349663
			)
		);

		std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
		std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());
		std::uniform_real_distribution<float> distHeight(minHeight, maxHeight);
		std::uniform_real_distribution<float> distWidth(minWidth, maxWidth);
		std::uniform_real_distribution<float> distBend(0.0f, maxBend);

		float minX = patch.coord.x * patchSize;
		float minZ = patch.coord.y * patchSize;

		patch.instances.clear();
		patch.instances.reserve(bladesPerPatch);
		patch.bounds = BBox{};

		int attempts = 0;
		int maxAttempts = bladesPerPatch * maxPlacementAttemptsMultiplier;

		while (static_cast<int>(patch.instances.size()) < bladesPerPatch && attempts < maxAttempts)
		{
			attempts++;

			float x = minX + dist01(rng) * patchSize;
			float z = minZ + dist01(rng) * patchSize;

			float u = (x / terrainPlaneLen) + 0.5f;
			float v = (z / terrainPlaneLen) + 0.5f;

			if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
				continue;

			float grassWeight = 1.0f;

			if (useSplatmapPlacement && _splatTextureCpu)
			{
				grassWeight = sampleGrassSplatWeight(_splatTextureCpu, u, v);

				if (grassWeight < minGrassSplatWeight)
					continue;
			}

			if (useDensityNoise)
			{
				float densityNoise =
					sin(x * densityNoiseScale) *
					sin(z * densityNoiseScale);

				densityNoise = densityNoise * 0.5f + 0.5f;

				float density = glm::mix(
					1.0f,
					densityNoise,
					densityNoiseStrength
				);

				if (dist01(rng) > grassWeight * density)
					continue;
			}

			float y = heightmap->sample(u, v) * heightmap->getHeightScale();

			GrassInstance instance;
			instance.posScale = glm::vec4(x, y, z, distHeight(rng));
			instance.rotBendSeed = glm::vec4(
				distRot(rng),
				distBend(rng),
				distWidth(rng),
				dist01(rng) * 100.0f
			);

			patch.instances.push_back(instance);

			patch.bounds.expand(glm::vec3(x, y, z));
			patch.bounds.expand(glm::vec3(x, y + maxHeight + maxBend, z));
		}
	}
	void GrassRenderer::updateStreaming(const glm::vec3& playerPos)
	{
		if (!usePatchStreaming)
			return;

		glm::ivec2 playerPatch = worldToPatchCoord(playerPos);

		if (playerPatch == _currentCenterPatch)
			return;

		_currentCenterPatch = playerPatch;
		centerPosition = playerPos;

		//std::cout << "Grass streaming update. Player patch: "
		//		<< playerPatch.x << ", " << playerPatch.y << "\n";

		updatePatchesAround(playerPos);
	}

	/* 
	Purpose Statement: generating instanced of grass 
		- generate random x/z
		- convert x/z to splatmap UV
		- sample splatmap color
		- if grass channel is weak, reject point
		- sample heightmap
		- create grass
	*/
	void GrassRenderer::generateInstances()
	{
		if (!heightmap) return;

		std::mt19937 rng(1337);
		std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
		std::uniform_real_distribution<float> distAngle(0.0f, glm::two_pi<float>());
		std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);
		std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());
		std::uniform_real_distribution<float> distHeight(minHeight, maxHeight);
		std::uniform_real_distribution<float> distWidth(minWidth, maxWidth);
		std::uniform_real_distribution<float> distBend(0.0f, maxBend);

		_instances.clear();
		_instances.reserve(bladeCount);

		worldBounds = BBox{};
	
		int attempts = 0;
		const int maxAttempts = bladeCount * maxPlacementAttemptsMultiplier;

		while (static_cast<int>(_instances.size()) < bladeCount && attempts < maxAttempts)
		{
			attempts++;

			float angle = distAngle(rng);
			float radius = sqrt(distRadius(rng)) * spawnRadius;

			float x = centerPosition.x + cos(angle) * radius;
			float z = centerPosition.z + sin(angle) * radius;

			float u = (x / terrainPlaneLen) + 0.5f;
			float v = (z / terrainPlaneLen) + 0.5f;

			if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
			{
				continue;
			}

			

		float grassWeight = 1.0f;

		if (useSplatmapPlacement && _splatTextureCpu)
		{
			grassWeight = sampleGrassSplatWeight(_splatTextureCpu, u, v);

			if (grassWeight < minGrassSplatWeight)
			{
				continue;
			}
		}

		if (useDensityNoise)
		{
			float densityNoise =
				sin(x * densityNoiseScale) *
				sin(z * densityNoiseScale);

			densityNoise = densityNoise * 0.5f + 0.5f;

			float density = glm::mix(
				1.0f,
				densityNoise,
				densityNoiseStrength
			);

			float finalSpawnChance = grassWeight * density;

			if (dist01(rng) > finalSpawnChance)
			{
				continue;
			}
		}

			float y = heightmap->sample(u, v) * heightmap->getHeightScale();

			GrassInstance instance;
			instance.posScale = glm::vec4(x, y, z, distHeight(rng));
			instance.rotBendSeed = glm::vec4(
				distRot(rng),
				distBend(rng),
				distWidth(rng),
				dist01(rng) * 100.0f
			);

			float densityNoise =
				sin(x * 0.08f) *
				sin(z * 0.08f);

			densityNoise = densityNoise * 0.5f + 0.5f;

			float combinedDensity = grassWeight * densityNoise;

			if (dist01(rng) > combinedDensity)
			{
				continue;
			}

			_instances.push_back(instance);

			worldBounds.expand(glm::vec3(x, y, z));
			worldBounds.expand(glm::vec3(x, y + maxHeight + maxBend, z));
		}

		std::cout << "Generated grass instances: "
				<< _instances.size()
				<< " / "
				<< bladeCount
				<< " after "
				<< attempts
				<< " attempts\n";
	}

	void GrassRenderer::draw(const AssetManager& assets, const Frustum& frustum) const
	{
		if (_vao == 0 || _indexCount <= 0) return;

		auto* shaderPtr = assets.getShader(shader);
		if (!shaderPtr) return;

		shaderPtr->bind();

		// Shader Variables 
		shaderPtr->setFloat("uTime", Time::time());
		shaderPtr->setVec2("uWindDirection", windDirection);
		shaderPtr->setFloat("uWindStrength", windStrength);
		shaderPtr->setFloat("uWindSpeed", windSpeed);

		if (auto* tex = assets.getTexture(texture))
		{
			tex->bindToUnit(shaderPtr->getUniform("uGrassTex"), 0);
		}

		glBindVertexArray(_vao);

		if (usePatchStreaming)
		{
			//int totalPatches = 0;
			//int drawnPatches = 0;
			//int drawnInstances = 0;

			for (const GrassPatch& patch : _patches)
			{
				//totalPatches++;

				if (patch.instances.empty()) continue;
				if (!patch.bounds.intersectsFrustum(frustum)) continue;
				if (patch.instanceVbo == 0) continue;

				glBindBuffer(GL_ARRAY_BUFFER, patch.instanceVbo);

				glVertexAttribPointer(
					2,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(GrassInstance),
					(void*)offsetof(GrassInstance, posScale)
				);

				glVertexAttribPointer(
					3,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(GrassInstance),
					(void*)offsetof(GrassInstance, rotBendSeed)
				);

				glDrawElementsInstanced(
					GL_TRIANGLES,
					_indexCount,
					GL_UNSIGNED_INT,
					(void*)0,
					static_cast<GLsizei>(patch.instances.size())
				);

				//drawnPatches++;
				//drawnInstances += static_cast<int>(patch.instances.size());
			}

			//static int frameCounter = 0;
			//frameCounter++;

			//if (frameCounter % 120 == 0)
			//{
			//	std::cout << "Grass VFC: drawn "
			//			<< drawnPatches
			//			<< " / "
			//			<< totalPatches
			//			<< " patches, instances: "
			//			<< drawnInstances
			//			<< "\n";
			//}
		}
		else
		{
			if (!_instances.empty() && worldBounds.intersectsFrustum(frustum))
			{
				glDrawElementsInstanced(
					GL_TRIANGLES,
					_indexCount,
					GL_UNSIGNED_INT,
					(void*)0,
					static_cast<GLsizei>(_instances.size())
				);
			}
		}		

		glBindVertexArray(0);
		shaderPtr->unbind();
	}

	void GrassRenderer::drawShadow(const AssetManager& assets,
		const Frustum& lightFrustum, const glm::mat4& lightSpace) const
	{
		if (_vao == 0 || _indexCount <= 0) return;

		auto* shaderPtr = assets.getShader(shadowShader);
		if (!shaderPtr) return;

		shaderPtr->bind();

		shaderPtr->setMat4("lightSpace", lightSpace);
		shaderPtr->setFloat("uTime", Time::time());
		shaderPtr->setVec2("uWindDirection", windDirection);
		shaderPtr->setFloat("uWindStrength", windStrength);
		shaderPtr->setFloat("uWindSpeed", windSpeed);

		if (auto* tex = assets.getTexture(texture))
		{
			tex->bindToUnit(shaderPtr->getUniform("uGrassTex"), 0);
		}

		glBindVertexArray(_vao);

		if (usePatchStreaming)
		{
			for (const GrassPatch& patch : _patches)
			{
				if (patch.instances.empty()) continue;
				if (!patch.bounds.intersectsFrustum(lightFrustum)) continue;
				if (patch.instanceVbo == 0) continue;

				glBindBuffer(GL_ARRAY_BUFFER, patch.instanceVbo);

				glVertexAttribPointer(
					2,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(GrassInstance),
					(void*)offsetof(GrassInstance, posScale)
				);

				glVertexAttribPointer(
					3,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(GrassInstance),
					(void*)offsetof(GrassInstance, rotBendSeed)
				);

				glDrawElementsInstanced(
					GL_TRIANGLES,
					_indexCount,
					GL_UNSIGNED_INT,
					(void*)0,
					static_cast<GLsizei>(patch.instances.size())
				);
			}
		}
		else
		{
			if (!_instances.empty() && worldBounds.intersectsFrustum(lightFrustum))
			{
				glDrawElementsInstanced(
					GL_TRIANGLES,
					_indexCount,
					GL_UNSIGNED_INT,
					(void*)0,
					static_cast<GLsizei>(_instances.size())
				);
			}
		}

		glBindVertexArray(0);
		shaderPtr->unbind();
	}

	void GrassRenderer::regenerate(const glm::vec3& newCenter)
	{
		centerPosition = newCenter;

		if (usePatchStreaming)
		{
			updatePatchesAround(newCenter);
			return;
		}

		generateInstances();

		glBindBuffer(GL_ARRAY_BUFFER, _instanceVbo);

		if (!_instances.empty())
		{
			glBufferData(
				GL_ARRAY_BUFFER,
				_instances.size() * sizeof(GrassInstance),
				_instances.data(),
				GL_STATIC_DRAW
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}