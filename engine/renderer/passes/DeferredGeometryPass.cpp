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

	DeferredGeometryPass::DeferredGeometryPass(int width, int height, Handle<Shader> baseShader,
		Handle<Shader> terrainShader, Handle<Shader> skinnedShader) :
		_baseShader(baseShader),
		_terrainShader(terrainShader),
		_skinnedShader(skinnedShader),
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

		// Build view frustum
		const CameraData& camData = camera->getCameraData();
		Frustum frustum = Frustum::fromMatrix(camData.projection * camData.view);

		// Clear render queues
		baseQueue.clear();
		stencilQueue.clear();
		terrainQueue.clear();
		skinnedQueue.clear();

		// Populate render queues and handle frustum culling
		for (const auto& object : scene.getRootObjects())
		{
			collectVisibleObjects(object, scene, assets, frustum);
		}

		// Render terrain first to optimize for early depth test
		auto* shader = assets.getShader(_terrainShader);
		shader->bind();

		for (auto* object : terrainQueue)
		{
			auto* mr = object->getComponent<MeshRenderer>();
			auto* mat = assets.getMaterial(mr->material);
			auto* mesh = assets.getMesh(mr->mesh);

			shader->setMat4("model", object->transform.getWorldMatrix());
			//shader->setVec3("mat.ambient", mat->ambient); // currently unused in shader
			//shader->setVec3("mat.diffuse", mat->diffuse);
			//shader->setVec3("mat.specular", mat->specular);
			shader->setFloat("mat.shininess", mat->shininess);

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

			mesh->draw();
		}

		// Render standard opaque meshes
		shader = assets.getShader(_baseShader);
		shader->bind();

		for (auto* object : baseQueue)
		{
			auto* mr = object->getComponent<MeshRenderer>();
			auto* mat = assets.getMaterial(mr->material);
			auto* mesh = assets.getMesh(mr->mesh);

			shader->setMat4("model", object->transform.getWorldMatrix());
			shader->setVec3("mat.ambient", mat->ambient);
			shader->setVec3("mat.diffuse", mat->diffuse);
			shader->setVec3("mat.specular", mat->specular);
			shader->setFloat("mat.shininess", mat->shininess);

			auto* difTex = assets.getTexture(mat->difTex);
			auto* specTex = assets.getTexture(mat->specTex);
			difTex->bind(shader->getUniform("mat.difTex"));
			specTex->bind(shader->getUniform("mat.specTex"));

			mesh->draw();
		}

		// Render opaque meshes marked to write to stencil buffer
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);

		for (auto* object : stencilQueue)
		{
			auto* mr = object->getComponent<MeshRenderer>();
			auto* mat = assets.getMaterial(mr->material);
			auto* mesh = assets.getMesh(mr->mesh);

			shader->setMat4("model", object->transform.getWorldMatrix());
			shader->setVec3("mat.ambient", mat->ambient);
			shader->setVec3("mat.diffuse", mat->diffuse);
			shader->setVec3("mat.specular", mat->specular);
			shader->setFloat("mat.shininess", mat->shininess);

			auto* difTex = assets.getTexture(mat->difTex);
			auto* specTex = assets.getTexture(mat->specTex);
			difTex->bind(shader->getUniform("mat.difTex"));
			specTex->bind(shader->getUniform("mat.specTex"));

			mesh->draw();
		}

		// Render skinned meshes
		// (currently all skinned meshes in the game write to stencil buffer)
		shader = assets.getShader(_skinnedShader);
		shader->bind();

		for (auto* object : skinnedQueue)
		{
			auto* mr = object->getComponent<MeshRenderer>();
			auto* mat = assets.getMaterial(mr->material);
			auto* mesh = assets.getMesh(mr->mesh);

			shader->setMat4("model", object->transform.getWorldMatrix());
			shader->setVec3("mat.ambient", mat->ambient);
			shader->setVec3("mat.diffuse", mat->diffuse);
			shader->setVec3("mat.specular", mat->specular);
			shader->setFloat("mat.shininess", mat->shininess);

			auto* difTex = assets.getTexture(mat->difTex);
			auto* specTex = assets.getTexture(mat->specTex);
			difTex->bind(shader->getUniform("mat.difTex"));
			specTex->bind(shader->getUniform("mat.specTex"));

			auto* animator = object->getComponent<Animator>();
			const auto& boneMatrices = animator->getBoneMatrices();
			GLsizei numBones = static_cast<GLsizei>(
				std::min(boneMatrices.size(), MAX_SHADER_BONES));
			shader->setInt("numBones", numBones);
			shader->setMat4Array("bones[0]", boneMatrices.data(), numBones);

			mesh->draw();
		}

		// Render instanced grass
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

	void DeferredGeometryPass::collectVisibleObjects(Object* object, const Scene& scene,
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

			// Sort objects into appropriate render queue
			auto* mesh = assets.getMesh(mr->mesh);
			auto* mat = assets.getMaterial(mr->material);
			if (!mesh || !mat || mat->renderMode == RenderMode::Transparent
				|| mat->renderMode == RenderMode::Water) return;

			auto* animator = object->getComponent<Animator>();

			if (mat->renderMode == RenderMode::Terrain)
			{
				terrainQueue.push_back(object);
			}
			else if (mesh && mesh->isSkinned() && animator)
			{
				skinnedQueue.push_back(object);
			}
			else if (mr->writeStencil)
			{
				stencilQueue.push_back(object);
			}
			else
			{
				baseQueue.push_back(object);
			}
		}

		for (auto* childTransform : object->transform.getChildren())
		{
			if (Object* child = childTransform->owner)
				collectVisibleObjects(child, scene, assets, frustum);
		}
	}
}