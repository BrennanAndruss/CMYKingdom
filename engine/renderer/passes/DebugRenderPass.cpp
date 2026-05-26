#include "renderer/passes/DebugRenderPass.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "scene/components/Component.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"

namespace engine
{
	DebugRenderPass::DebugRenderPass()
	{
		// Setup VAO and VBO
		glGenVertexArrays(1, &_vao);
		glGenBuffers(1, &_vbo);
		glBindVertexArray(_vao);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glBindVertexArray(0);

		// Skeletal animation debug line shader
		const std::string vertSrc =
			"#version 410 core\n"
			"layout(location=0) in vec3 vertPos;\n"
			"layout(std140) uniform CameraData {\n"
			"	mat4 view; mat4 projection; vec4 cameraPos;\n"
			"};\n"
			"void main() {\n"
			"	gl_Position = projection * view * vec4(vertPos, 1.0);\n"
			"}";
		
		const std::string fragSrc =
			"#version 410 core\n"
			"uniform vec3 lineColor;\n"
			"out vec4 fragColor;\n"
			"void main(){ fragColor = vec4(lineColor, 1.0); }";

		_lineShader = std::make_unique<Shader>(vertSrc, fragSrc);
	}

	DebugRenderPass::~DebugRenderPass()
	{
		if (_vbo != 0) glDeleteBuffers(1, &_vbo);
		if (_vao != 0) glDeleteVertexArrays(1, &_vao);
	}

	void DebugRenderPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		if (Input::isKeyPressed(GLFW_KEY_F3))
		{
			showSkeletons = !showSkeletons;
			std::cout << "[DebugRenderPass] Skeleton visualizer "
				<< (showSkeletons ? "enabled" : "disabled") << " (F3)\n";
		}

		if (!showSkeletons) return;

		// Draw into scene framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());

		glDisable(GL_DEPTH_TEST);
		glLineWidth(2.0f);

		drawSkeletons(scene, assets);

		glEnable(GL_DEPTH_TEST);
		glLineWidth(1.0f);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void DebugRenderPass::drawSkeletons(const Scene& scene, const AssetManager& assets)
	{
		_lineShader->bind();
		_lineShader->setVec3("lineColor", glm::vec3(1.0f, 0.0f, 0.0f));

		std::vector<glm::vec3> lineVerts;

		for (const auto& obj : scene.getObjects())
		{
			auto* mr = obj->getComponent<MeshRenderer>();
			if (!mr) continue;
			Mesh* mesh = assets.getMesh(mr->mesh);
			if (!mesh || !mesh->isSkinned()) continue;

			auto* animator = obj->getComponent<Animator>();
			if (!animator || !animator->hasPose()) continue;

			Skeleton* skeleton = assets.getSkeleton(animator->skeleton);
			if (!skeleton) continue;

			const auto& globals = animator->getGlobalMatrices();
			const glm::mat4 world = obj->transform.getWorldMatrix();

			lineVerts.clear();
			lineVerts.reserve(skeleton->nodes.size() * 2);

			for (const auto& node : skeleton->nodes)
			{
				if (!node.isBone || node.parentIndex < 0) continue;
				int parentIdx = skeleton->nodes[node.parentIndex].boneIndex;
				int childIdx = node.boneIndex;
				if (parentIdx < 0 || childIdx < 0) continue;
				if (static_cast<size_t>(parentIdx) >= globals.size() ||
					static_cast<size_t>(childIdx) >= globals.size()) continue;

				lineVerts.push_back(glm::vec3(world * globals[parentIdx][3]));
				lineVerts.push_back(glm::vec3(world * globals[childIdx][3]));
			}

			if (lineVerts.empty()) continue;
			uploadLineVerts(lineVerts);

			glBindVertexArray(_vao);
			glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVerts.size()));
			glBindVertexArray(0);
		}

		_lineShader->unbind();
	}

	void DebugRenderPass::uploadLineVerts(const std::vector<glm::vec3>& verts)
	{
		size_t byteSize = verts.size() * sizeof(glm::vec3);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);

		// Only realloc if new data exceeds current capacity
		if (byteSize > _vboCapacity)
		{
			// Orphan old buffer, upload new data
			glBufferData(GL_ARRAY_BUFFER, byteSize, verts.data(), GL_DYNAMIC_DRAW);
			_vboCapacity = byteSize;
		}
		else
		{
			// Sub-upload data
			glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, verts.data());
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}