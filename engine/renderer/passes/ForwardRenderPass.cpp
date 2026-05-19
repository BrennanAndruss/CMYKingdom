#include "renderer/passes/ForwardRenderPass.h"

#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <memory>
#include <vector>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "resources/AssetManager.h"

#include "scene/Scene.h"
#include "scene/components/Component.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"

namespace engine
{
	static constexpr std::size_t MAX_SHADER_BONES = 100;
	static std::unique_ptr<Shader> g_debugLineShader;

	ForwardRenderPass::ForwardRenderPass(int width, int height, Handle<Shader> shader) :
		_shader(shader),
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24Stencil8 }
		}) 
	{
		// Create debug line shader for skeletal animation
		const std::string vsrc =
			"#version 410 core\n"
			"layout(location=0) in vec3 vertPos;\n"
			"layout(std140) uniform CameraData{ mat4 view; mat4 projection; vec4 cameraPos; };\n"
			"uniform mat4 model;\n"
			"void main(){ gl_Position = projection * view * model * vec4(vertPos, 1.0); }";
		const std::string fsrc =
			"#version 410 core\n"
			"out vec4 fragColor;\n"
			"void main(){ fragColor = vec4(1.0, 0.0, 0.0, 1.0); }";

		g_debugLineShader = std::make_unique<Shader>(vsrc, fsrc);
	}

	ForwardRenderPass::~ForwardRenderPass()
	{
		if (_vbo != 0) glDeleteBuffers(1, &_vbo);
		if (_vao != 0) glDeleteVertexArrays(1, &_vao);
	}

	void ForwardRenderPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
	}

	void ForwardRenderPass::execute(const Scene& scene, const AssetManager& assets, RenderContext& ctx)
	{
		static bool showSkeletonVisualizer;
		if (Input::isKeyPressed(GLFW_KEY_F3))
		{
			showSkeletonVisualizer = !showSkeletonVisualizer;
			std::cout << "[ForwardRenderPass] Skeleton visualizer "
		 		<< (showSkeletonVisualizer ? "enabled" : "disabled") << " (F3)\n";
		}

		_framebuffer.bind();
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		for (const auto& object : scene.getObjects())
		{
			auto* meshRenderer = object->getComponent<MeshRenderer>();
			if (!meshRenderer) continue;

			auto* mesh = assets.getMesh(meshRenderer->mesh);
			auto* mat = assets.getMaterial(meshRenderer->material);
			if (!mesh || !mat) continue;

			auto* shader = assets.getShader(mat->shader);
			if (!shader) continue;

			shader->bind();


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
			// All materials have a default texture so no need for null check
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
				terrainGrass  = assets.getTexture(mat->terrainGrass);
				terrainSand   = assets.getTexture(mat->terrainSand);
				terrainRock   = assets.getTexture(mat->terrainRock);
				terrainSnow   = assets.getTexture(mat->terrainSnow);

				if (terrainSplat0) terrainSplat0->bindToUnit(shader->getUniform("splat0"), 0);
				if (terrainGrass)  terrainGrass->bindToUnit(shader->getUniform("terrainGrass"), 1);
				if (terrainSand)   terrainSand->bindToUnit(shader->getUniform("terrainSand"), 2);
				if (terrainRock)   terrainRock->bindToUnit(shader->getUniform("terrainRock"), 3);
				if (terrainSnow)   terrainSnow->bindToUnit(shader->getUniform("terrainSnow"), 4);

				shader->setFloat("terrainTextureTiling", mat->terrainTextureTiling);
			}
			else
			{
				difTex = assets.getTexture(mat->difTex);
				specTex = assets.getTexture(mat->specTex);

				if (difTex)  difTex->bind(shader->getUniform("mat.difTex"));
				if (specTex) specTex->bind(shader->getUniform("mat.specTex"));
			}

			Animator* animator;
			if (mesh->isSkinned() && (animator = object->getComponent<Animator>()))
			{
				const auto& boneMatrices = animator->getBoneMatrices();
				const int numBones = static_cast<int>(std::min(boneMatrices.size(), MAX_SHADER_BONES));
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

			shader->unbind();

			// Show skeleton visualizer
			Skeleton* skeleton;
			if (showSkeletonVisualizer && mesh->isSkinned() && animator && animator->hasPose() &&
				(skeleton = assets.getSkeleton(animator->skeleton)))
			{
				const auto& globals = animator->getGlobalMatrices();
				const glm::mat4 objectWorld = object->transform.getWorldMatrix();
				std::vector<glm::vec3> lineVerts;
				lineVerts.reserve(skeleton->nodes.size() * 2);

				for (const auto& node : skeleton->nodes)
				{
					if (!node.isBone || node.parentIndex < 0) continue;
					const int parentIdx = skeleton->nodes[node.parentIndex].boneIndex;
					const int childIdx = node.boneIndex;
					if (parentIdx < 0 || childIdx < 0) continue;
					if (static_cast<std::size_t>(parentIdx) >= globals.size() ||
						static_cast<std::size_t>(childIdx) >= globals.size()) continue;

					glm::vec3 parentPos = glm::vec3(
						objectWorld * globals[static_cast<std::size_t>(parentIdx)][3]);
					glm::vec3 childPos = glm::vec3(
						objectWorld * globals[static_cast<std::size_t>(childIdx)][3]);
					lineVerts.push_back(parentPos);
					lineVerts.push_back(childPos);
				}

				if (!lineVerts.empty())
				{
					GLuint vao = 0, vbo = 0;
					glGenVertexArrays(1, &vao);
					glGenBuffers(1, &vbo);
					glBindVertexArray(vao);
					glBindBuffer(GL_ARRAY_BUFFER, vbo);
					glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(glm::vec3), 
						lineVerts.data(), GL_DYNAMIC_DRAW);
					glEnableVertexAttribArray(0);
					glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

					g_debugLineShader->bind();
					g_debugLineShader->setMat4("model", glm::mat4(1.0f));
					glDisable(GL_DEPTH_TEST);
					glLineWidth(2.0f);
					glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVerts.size()));
					glEnable(GL_DEPTH_TEST);
					g_debugLineShader->unbind();

					glDisableVertexAttribArray(0);
					glBindBuffer(GL_ARRAY_BUFFER, 0);
					glBindVertexArray(0);
					glDeleteBuffers(1, &vbo);
					glDeleteVertexArrays(1, &vao);
				}
			}
		}

		// Cleanup state for next pass
		glDisable(GL_STENCIL_TEST);
		glStencilMask(0x00);
		_framebuffer.unbind();

		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getAttachment(AttachmentFormat::RGBA8));
		ctx.setBuffer(BufferNames::SceneDepth, _framebuffer.getAttachment(AttachmentFormat::Depth24Stencil8));
		ctx.sceneFramebuffer = &_framebuffer;
	}
}