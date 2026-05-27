#include "renderer/passes/ForwardRenderPass.h"

#include <algorithm>
#include <glad/glad.h>
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"

namespace engine
{
	static constexpr std::size_t MAX_SHADER_BONES = 100;

	ForwardRenderPass::ForwardRenderPass(int width, int height, Handle<Shader> shader) :
		_shader(shader),
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24Stencil8 }
		}) {}

	ForwardRenderPass::~ForwardRenderPass() = default;

	void ForwardRenderPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
	}

	void ForwardRenderPass::execute(const Scene& scene, const AssetManager& assets, 
		RenderContext& ctx)
	{
		_framebuffer.bind();
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		Camera* camera = scene.getMainCamera();
		if (!camera) return;

		const CameraData& camData = camera->getCameraData();
		Frustum frustum = Frustum::fromCamera(camData);

		// Cull and draw
		for (const auto& object : scene.getRootObjects())
		{
			drawObjectCulled(object, scene, assets, frustum);
		}

		// Cleanup state for next pass
		glDisable(GL_STENCIL_TEST);
		glStencilMask(0x00);
		_framebuffer.unbind();

		ctx.setBuffer(BufferNames::SceneColor, 
			_framebuffer.getAttachment(AttachmentFormat::RGBA8));
		ctx.setBuffer(BufferNames::SceneDepth, 
			_framebuffer.getAttachment(AttachmentFormat::Depth24Stencil8));
		ctx.sceneFramebuffer = &_framebuffer;
	}

	void ForwardRenderPass::drawObjectCulled(Object* object, const Scene& scene,
		const AssetManager& assets, const Frustum& frustum)
	{
		// Test object's hierarchy bbox
		if (!object->transform.getChildren().empty())
		{
			// Test combined bbox to cull the subtree
			BBox hierarchyBBox = object->getHierarchyBBox(assets);
			if (!hierarchyBBox.intersectsFrustum(frustum))
			{
				// std::cout << "Culled " << object->name << " and its subtree\n";
				return;
			}
		}

		// Test object's own bbox
		if (auto* mr = object->getComponent<MeshRenderer>())
		{
			const BBox& worldBBox = object->getWorldBBox(assets);
			if (worldBBox.intersectsFrustum(frustum))
			{
				drawObject(object, scene, assets);
			}
			else
			{
				// std::cout << "Culled " << object->name << "\n";
				return;
			}
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

	void ForwardRenderPass::drawObject(Object* object, const Scene& scene,
		const AssetManager& assets)
	{
		auto* meshRenderer = object->getComponent<MeshRenderer>();
		if (!meshRenderer) return;

		auto* mesh = assets.getMesh(meshRenderer->mesh);
		auto* mat = assets.getMaterial(meshRenderer->material);
		if (!mesh || !mat) return;

		auto* shader = assets.getShader(mat->shader);
		if (!shader) return;

		shader->bind();

		//Cubemap* irradianceMap = nullptr;
		//if (scene.hasIrradianceMap())
		//{
		//	irradianceMap = assets.getCubemap(scene.getIrradianceMap());
		//	if (irradianceMap)
		//	{
		//		irradianceMap->bindToUnit(shader->getUniform("irradianceMap"), 10);
		//		shader->setInt("useIrradianceMap", 1);
		//		shader->setFloat("irradianceStrength", 1.2f);
		//	}
		//}
		//else
		//{
		//	shader->setInt("useIrradianceMap", 0);
		//}

		shader->setMat4("model", object->transform.getWorldMatrix());
		shader->setVec3("mat.ambient", mat->ambient);
		shader->setVec3("mat.diffuse", mat->diffuse);
		shader->setVec3("mat.specular", mat->specular);
		shader->setFloat("mat.shininess", mat->shininess);
		shader->setInt("numLights", scene.getLights().size());

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

		// Bind textures
		Texture* terrainSplat0 = nullptr;
		Texture* terrainGrass = nullptr;
		Texture* terrainSand = nullptr;
		Texture* terrainRock = nullptr;
		Texture* terrainSnow = nullptr;

		Texture* difTex = nullptr;
		Texture* specTex = nullptr;

		if (mat->isTerrain)
		{
			terrainSplat0 = assets.getTexture(mat->splat0);
			terrainGrass = assets.getTexture(mat->terrainGrass);
			terrainSand = assets.getTexture(mat->terrainSand);
			terrainRock = assets.getTexture(mat->terrainRock);
			terrainSnow = assets.getTexture(mat->terrainSnow);

			if (terrainSplat0)
				terrainSplat0->bindToUnit(shader->getUniform("splat0"), 0);
			if (terrainGrass)
				terrainGrass->bindToUnit(shader->getUniform("terrainGrass"), 1);
			if (terrainSand)
				terrainSand->bindToUnit(shader->getUniform("terrainSand"), 2);
			if (terrainRock)
				terrainRock->bindToUnit(shader->getUniform("terrainRock"), 3);
			if (terrainSnow)
				terrainSnow->bindToUnit(shader->getUniform("terrainSnow"), 4);

			shader->setFloat("terrainTextureTiling", mat->terrainTextureTiling);
		}
		else
		{
			difTex = assets.getTexture(mat->difTex);
			specTex = assets.getTexture(mat->specTex);

			if (difTex)  difTex->bind(shader->getUniform("mat.difTex"));
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
				shader->setMat4("bones[" + std::to_string(i) + "]",
					boneMatrices[static_cast<std::size_t>(i)]);
			}
		}

		mesh->draw();

		if (mat->isTerrain)
		{
			if (terrainSplat0) terrainSplat0->unbindFromUnit(0);
			if (terrainGrass)  terrainGrass->unbindFromUnit(1);
			if (terrainSand)   terrainSand->unbindFromUnit(2);
			if (terrainRock)   terrainRock->unbindFromUnit(3);
			if (terrainSnow)   terrainSnow->unbindFromUnit(4);
		}
		else
		{
			if (difTex)  difTex->unbind();
			if (specTex) specTex->unbind();
		}

		//if (irradianceMap)
		//{
		//	irradianceMap->unbindFromUnit(10);
		//}

		shader->unbind();
	}
}