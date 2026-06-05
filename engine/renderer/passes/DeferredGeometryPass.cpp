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
			{ AttachmentFormat::RGB16F },	// 0: Position
			{ AttachmentFormat::RGBA16F },	// 1: Normal + Shininess
			{ AttachmentFormat::RGBA8 },	// 2: Albedo + Specular
			{ AttachmentFormat::Depth24Stencil8 }
		}) {}

	DeferredGeometryPass::~DeferredGeometryPass() = default;

	void DeferredGeometryPass::resize(int width, int height)
	{
		_gBuffer.resize(width, height);
	}

	void DeferredGeometryPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		_gBuffer.bind();
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		Camera* camera = scene.getMainCamera();
		if (!camera) return;
		Frustum frustum = Frustum::fromCamera(camera->getCameraData());

		// Cull and draw regular objects
		for (const auto& object : scene.getRootObjects())
		{
			drawObjectCulled(object, scene, assets, frustum);
		}

		glStencilMask(0x00);

		// Draw instanced grass
		glDisable(GL_CULL_FACE);
		for (const auto& object : scene.getObjects())
		{
			if (auto* grass = object->getComponent<GrassRenderer>())
			{
				grass->draw(assets, frustum);
			}
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
		// Test object's hierarchy bbox
		if (!object->transform.getChildren().empty())
		{
			// Test combined bbox to cull the subtree
			BBox hierarchyBBox = object->getHierarchyBBox(assets);
			if (!hierarchyBBox.intersectsFrustum(frustum))
			{
				return;
			}
		}

		// Test object's own bbox
		if (auto* mr = object->getComponent<MeshRenderer>())
		{
			const BBox& worldBBox = object->getWorldBBox(assets);
			if (!worldBBox.intersectsFrustum(frustum))
			{
				return;
			}

			drawObject(object, scene, assets);
		}

		// Recurse into children
		for (auto* childTransform : object->transform.getChildren())
		{
			if (Object* child = childTransform->owner)
			{
				drawObjectCulled(child, scene, assets, frustum);
			}
		}
	}

	void DeferredGeometryPass::drawObject(Object* object, const Scene& scene,
		const AssetManager& assets)
	{
		auto* meshRenderer = object->getComponent<MeshRenderer>();
		if (!meshRenderer) return;

		auto* mesh = assets.getMesh(meshRenderer->mesh);
		auto* mat = assets.getMaterial(meshRenderer->material);
		if (!mesh || !mat || mat->renderMode == RenderMode::Transparent
			|| mat->renderMode == RenderMode::Water) return;

		auto* shader = assets.getShader(mat->shader);
		if (!shader) return;

		// Write stencil
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

		// Bind textures
		if (mat->renderMode == RenderMode::Terrain)
		{
			if (auto* t = assets.getTexture(mat->splat0))
				t->bindToUnit(shader->getUniform("splat0"), 0);
			if (auto* t = assets.getTexture(mat->terrainGrass))
				t->bindToUnit(shader->getUniform("terrainGrass"), 1);
			if (auto* t = assets.getTexture(mat->terrainSand))
				t->bindToUnit(shader->getUniform("terrainSand"), 2);
			if (auto* t = assets.getTexture(mat->terrainRock))
				t->bindToUnit(shader->getUniform("terrainRock"), 3);
			if (auto* t = assets.getTexture(mat->terrainSnow))
				t->bindToUnit(shader->getUniform("terrainSnow"), 4);

			shader->setFloat("terrainTextureTiling", mat->terrainTextureTiling);
		}
		else
		{
			if (auto* t = assets.getTexture(mat->difTex))
				t->bind(shader->getUniform("mat.difTex"));
			if (auto* t = assets.getTexture(mat->specTex))
				t->bind(shader->getUniform("mat.specTex"));
		}

		// Bone matrices for skinned meshes
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
				shader->setMat4("bones[" + std::to_string(i) + "]",
					boneMatrices[static_cast<std::size_t>(i)]);
			}
		}

		mesh->draw();
		shader->unbind();
	}
}