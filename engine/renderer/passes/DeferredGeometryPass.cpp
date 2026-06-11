#include "renderer/passes/DeferredGeometryPass.h"

#include <glad/glad.h>
#include <algorithm>
#include <string>
#include <vector>

#include "core/Time.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"
#include "scene/components/GrassRenderer.h"

namespace engine
{
	static constexpr std::size_t MAX_SHADER_BONES = 100;

	DeferredGeometryPass::DeferredGeometryPass(int width, int height) :
		_gBuffer(width, height, {
			{ AttachmentFormat::RGB32F },
			{ AttachmentFormat::RGBA16F },
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24Stencil8 }
		}) {}

	DeferredGeometryPass::~DeferredGeometryPass() = default;

	void DeferredGeometryPass::resize(int width, int height)
	{
		_gBuffer.resize(width, height);
	}

	void DeferredGeometryPass::execute(const Scene& scene, const AssetManager& assets, RenderContext& ctx)
	{
		_gBuffer.bind();
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		Camera* camera = scene.getMainCamera();
		if (!camera) return;

		const CameraData& camData = camera->getCameraData();
		Frustum frustum = Frustum::fromMatrix(camData.projection * camData.view);

		for (const auto& object : scene.getRootObjects())
		{
			drawObjectCulled(object, scene, assets, frustum);
		}

		glStencilMask(0x00);

		glDisable(GL_CULL_FACE);
		for (const auto& object : scene.getObjects())
		{
			if (auto* grass = object->getComponent<GrassRenderer>())
				grass->draw(assets, frustum);
		}

		glEnable(GL_CULL_FACE);
		glDisable(GL_STENCIL_TEST);
		_gBuffer.unbind();

		ctx.setBuffer(BufferNames::GPosition, _gBuffer.getColorAttachment(0));
		ctx.setBuffer(BufferNames::GNormal, _gBuffer.getColorAttachment(1));
		ctx.setBuffer(BufferNames::GAlbedo, _gBuffer.getColorAttachment(2));
		ctx.setBuffer(BufferNames::SceneDepth, _gBuffer.getDepthAttachment());
		ctx.sceneFramebuffer = &_gBuffer;
	}

	void DeferredGeometryPass::drawObjectCulled(Object* object, const Scene& scene,
		const AssetManager& assets, const Frustum& frustum)
	{
		if (!object->transform.getChildren().empty())
		{
			BBox hierarchyBBox = object->getHierarchyBBox(assets);
			if (!hierarchyBBox.intersectsFrustum(frustum))
				return;
		}

		if (auto* mr = object->getComponent<MeshRenderer>())
		{
			const BBox& worldBBox = object->getWorldBBox(assets);
			if (!worldBBox.intersectsFrustum(frustum))
				return;

			drawObject(object, scene, assets);
		}

		for (auto* childTransform : object->transform.getChildren())
		{
			if (Object* child = childTransform->owner)
				drawObjectCulled(child, scene, assets, frustum);
		}
	}

	void DeferredGeometryPass::drawObject(Object* object, const Scene& scene, const AssetManager& assets)
	{
		auto* meshRenderer = object->getComponent<MeshRenderer>();
		if (!meshRenderer) return;

		auto* mesh = assets.getMesh(meshRenderer->mesh);
		auto* mat = assets.getMaterial(meshRenderer->material);

		if (!mesh || !mat ||
			mat->renderMode == RenderMode::Transparent ||
			mat->renderMode == RenderMode::Water)
		{
			return;
		}

		auto* shader = assets.getShader(mat->shader);
		if (!shader) return;

		if (meshRenderer->writeStencil)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 1, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0xFF);
		}
		else
		{
			glStencilMask(0x00);
		}

		shader->bind();

		shader->setMat4("model", object->transform.getWorldMatrix());
		shader->setVec3("mat.ambient", mat->ambient);
		shader->setVec3("mat.diffuse", mat->diffuse);
		shader->setVec3("mat.specular", mat->specular);
		shader->setFloat("mat.shininess", mat->shininess);

		Texture* difTex = nullptr;
		Texture* specTex = nullptr;

		if (mat->renderMode == RenderMode::Terrain)
		{
			const int splatBaseUnit = 0;
			const int terrainBaseUnit = 3;

			for (int i = 0; i < mat->splatMapCount; i++)
			{
				if (auto* t = assets.getTexture(mat->splatMaps[i]))
				{
					t->bindToUnit(
						shader->getUniform("splatMaps[" + std::to_string(i) + "]"),
						splatBaseUnit + i
					);
				}
			}

			for (int i = 0; i < mat->terrainTextureCount; i++)
			{
				if (auto* t = assets.getTexture(mat->terrainTextures[i]))
				{
					t->bindToUnit(
						shader->getUniform("terrainTextures[" + std::to_string(i) + "]"),
						terrainBaseUnit + i
					);
				}
			}

			shader->setInt("splatMapCount", mat->splatMapCount);
			shader->setInt("terrainTextureCount", mat->terrainTextureCount);
			shader->setFloat("terrainTextureTiling", mat->terrainTextureTiling);
		}
		else
		{
			difTex = assets.getTexture(mat->difTex);
			specTex = assets.getTexture(mat->specTex);

			if (difTex) difTex->bind(shader->getUniform("mat.difTex"));
			if (specTex) specTex->bind(shader->getUniform("mat.specTex"));
		}

		Animator* animator = nullptr;
		if (mesh->isSkinned() && (animator = object->getComponent<Animator>()))
		{
			const auto& boneMatrices = animator->getBoneMatrices();
			const int numBones = static_cast<int>(
				std::min(boneMatrices.size(), MAX_SHADER_BONES));

			shader->setInt("isSkinned", 1);
			shader->setInt("numBones", numBones);

			for (int i = 0; i < numBones; ++i)
			{
				shader->setMat4(
					"bones[" + std::to_string(i) + "]",
					boneMatrices[static_cast<std::size_t>(i)]
				);
			}
		}

		mesh->draw();

		if (mat->renderMode == RenderMode::Terrain)
		{
			const int splatBaseUnit = 0;
			const int terrainBaseUnit = 3;

			for (int i = 0; i < mat->splatMapCount; i++)
			{
				if (auto* t = assets.getTexture(mat->splatMaps[i]))
					t->unbindFromUnit(splatBaseUnit + i);
			}

			for (int i = 0; i < mat->terrainTextureCount; i++)
			{
				if (auto* t = assets.getTexture(mat->terrainTextures[i]))
					t->unbindFromUnit(terrainBaseUnit + i);
			}
		}

		if (difTex) difTex->unbind();
		if (specTex) specTex->unbind();

		shader->unbind();
	}
}