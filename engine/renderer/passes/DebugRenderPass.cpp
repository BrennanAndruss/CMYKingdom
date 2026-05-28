#include "renderer/passes/DebugRenderPass.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <glm/gtc/quaternion.hpp>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "scene/components/Component.h"
#include "scene/components/Collider.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"
#include "scene/components/CharacterController.h"

namespace
{
	glm::vec3 transformPoint(const glm::vec3& origin, const glm::mat4& rotation, const glm::vec3& localPoint)
	{
		return origin + glm::vec3(rotation * glm::vec4(localPoint, 0.0f));
	}

	void appendLine(std::vector<glm::vec3>& verts, const glm::vec3& a, const glm::vec3& b)
	{
		verts.push_back(a);
		verts.push_back(b);
	}

	void appendBoxWireframe(std::vector<glm::vec3>& verts,
		const glm::vec3& origin,
		const glm::mat4& rotation,
		const glm::vec3& halfExtents)
	{
		const glm::vec3 corners[8] = {
			{-halfExtents.x, -halfExtents.y, -halfExtents.z},
			{ halfExtents.x, -halfExtents.y, -halfExtents.z},
			{ halfExtents.x,  halfExtents.y, -halfExtents.z},
			{-halfExtents.x,  halfExtents.y, -halfExtents.z},
			{-halfExtents.x, -halfExtents.y,  halfExtents.z},
			{ halfExtents.x, -halfExtents.y,  halfExtents.z},
			{ halfExtents.x,  halfExtents.y,  halfExtents.z},
			{-halfExtents.x,  halfExtents.y,  halfExtents.z}
		};

		const int edges[][2] = {
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		for (const auto& edge : edges)
		{
			appendLine(verts,
				transformPoint(origin, rotation, corners[edge[0]]),
				transformPoint(origin, rotation, corners[edge[1]]));
		}
	}

	glm::vec3 capsuleAxisPoint(engine::Axis axis, float x, float y, float z)
	{
		switch (axis)
		{
		case engine::Axis::X: return glm::vec3(y, x, z);
		case engine::Axis::Z: return glm::vec3(x, z, y);
		case engine::Axis::Y:
		default: return glm::vec3(x, y, z);
		}
	}

	void appendRing(std::vector<glm::vec3>& verts,
		const glm::vec3& origin,
		const glm::mat4& rotation,
		engine::Axis axis,
		const glm::vec3& ringCenter,
		float radius,
		int segments = 16)
	{
		for (int i = 0; i < segments; ++i)
		{
			const float angle0 = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
			const float angle1 = glm::two_pi<float>() * static_cast<float>(i + 1) / static_cast<float>(segments);
			const glm::vec3 local0 = ringCenter + capsuleAxisPoint(axis, std::cos(angle0) * radius, std::sin(angle0) * radius, 0.0f);
			const glm::vec3 local1 = ringCenter + capsuleAxisPoint(axis, std::cos(angle1) * radius, std::sin(angle1) * radius, 0.0f);
			appendLine(verts, transformPoint(origin, rotation, local0), transformPoint(origin, rotation, local1));
		}
	}

	void appendCapsuleWireframe(std::vector<glm::vec3>& verts,
		const glm::vec3& origin,
		const glm::mat4& rotation,
		engine::Axis axis,
		float radius,
		float height)
	{
		const float cylinderHeight = std::max(height - (2.0f * radius), 0.0f);
		const float halfCylinder = cylinderHeight * 0.5f;
		const glm::vec3 axisOffset = capsuleAxisPoint(axis, 0.0f, halfCylinder, 0.0f);
		const glm::vec3 topCenter = axisOffset;
		const glm::vec3 bottomCenter = -axisOffset;

		appendRing(verts, origin, rotation, axis, topCenter, radius);
		appendRing(verts, origin, rotation, axis, bottomCenter, radius);

		const glm::vec3 cross0 = capsuleAxisPoint(axis, radius, 0.0f, 0.0f);
		const glm::vec3 cross1 = capsuleAxisPoint(axis, 0.0f, 0.0f, radius);
		appendLine(verts,
			transformPoint(origin, rotation, topCenter + cross0),
			transformPoint(origin, rotation, bottomCenter + cross0));
		appendLine(verts,
			transformPoint(origin, rotation, topCenter - cross0),
			transformPoint(origin, rotation, bottomCenter - cross0));
		appendLine(verts,
			transformPoint(origin, rotation, topCenter + cross1),
			transformPoint(origin, rotation, bottomCenter + cross1));
		appendLine(verts,
			transformPoint(origin, rotation, topCenter - cross1),
			transformPoint(origin, rotation, bottomCenter - cross1));
	}

	void appendSphereWireframe(std::vector<glm::vec3>& verts,
		const glm::vec3& origin,
		const glm::mat4& rotation,
		float radius)
	{
		appendRing(verts, origin, rotation, engine::Axis::Z, glm::vec3(0.0f), radius);
		appendRing(verts, origin, rotation, engine::Axis::X, glm::vec3(0.0f), radius);
		appendRing(verts, origin, rotation, engine::Axis::Y, glm::vec3(0.0f), radius);
	}
}

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

		if (Input::isKeyPressed(GLFW_KEY_F5))
		{
			showColliders = !showColliders;
			std::cout << "[DebugRenderPass] Collider visualizer "
				<< (showColliders ? "enabled" : "disabled") << " (F4)\n";
		}

		if (!showSkeletons && !showColliders) return;

		// Draw into scene framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());

		glDisable(GL_DEPTH_TEST);
		glLineWidth(2.0f);

		if (showSkeletons)
		{
			drawSkeletons(scene, assets);
		}
		if (showColliders)
		{
			drawColliders(scene);
		}

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

	void DebugRenderPass::drawColliders(const Scene& scene)
	{
		std::vector<glm::vec3> lineVerts;

		for (const auto& obj : scene.getObjects())
		{
			if (!obj || obj->markedForDeletion)
			{
				continue;
			}

			const glm::vec3 worldPos = obj->transform.getWorldPosition();
			const glm::quat worldRot = obj->transform.getWorldRotation();
			const glm::mat4 rotation = glm::mat4_cast(worldRot);

			lineVerts.clear();

			if (auto* boxCollider = obj->getComponent<BoxCollider>())
			{
				const glm::vec3 worldScale = glm::abs(obj->transform.getWorldScale());
				const glm::vec3 origin = worldPos + (glm::mat3_cast(worldRot) * (boxCollider->center * worldScale));
				const glm::vec3 halfExtents = boxCollider->size * worldScale;
				appendBoxWireframe(lineVerts, origin, rotation, halfExtents);
				if (!lineVerts.empty())
				{
					_lineShader->bind();
					_lineShader->setVec3("lineColor", glm::vec3(0.1f, 0.9f, 0.2f));
					uploadLineVerts(lineVerts);
					glBindVertexArray(_vao);
					glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVerts.size()));
					glBindVertexArray(0);
					_lineShader->unbind();
				}
			}
			lineVerts.clear();
			if (auto* characterController = obj->getComponent<CharacterController>())
			{
				const glm::vec3 origin = worldPos;
				const glm::vec3 halfExtents(characterController->radius, characterController->height * 0.5f, characterController->radius);
				appendBoxWireframe(lineVerts, origin, rotation, halfExtents);
				if (!lineVerts.empty())
				{
					_lineShader->bind();
					_lineShader->setVec3("lineColor", glm::vec3(1.0f, 0.3f, 0.9f));
					uploadLineVerts(lineVerts);
					glBindVertexArray(_vao);
					glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVerts.size()));
					glBindVertexArray(0);
					_lineShader->unbind();
				}
			}
		}
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