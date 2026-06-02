#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "scene/Object.h"
#include "scene/components/Component.h"

namespace engine
{
	// Camera data struct for the Camera UBO
	struct CameraData
	{
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		glm::vec4 position = glm::vec4(0.0f);		// xyz + padding for UBO
	};

	class Camera : public Component
	{
	public:
		Camera(float fov, float aspect, float near = 0.1f, float far = 100.0f);
		~Camera() = default;

		void start() override;

		void setFov(float fov);
		float getFov() const { return _fov; }
		void setAspect(float aspect);
		float getAspect() const { return _aspect; }
		float getNearPlane() const { return _near; }
		float getFarPlane() const { return _far; }

		const CameraData& getCameraData() const { return _cameraData; }

		void updateViewMatrix();

	private:
		float _fov, _aspect, _near, _far;
		CameraData _cameraData;
		
		void updateProjectionMatrix();
	};
}