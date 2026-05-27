#include "renderer/Frustum.h"

#include <glm/gtc/matrix_access.hpp>
#include "renderer/BoundingVolume.h"
#include "scene/components/Camera.h"

namespace engine
{
	Frustum Frustum::fromCamera(const CameraData& camData)
	{
		Frustum f;

		// Extract planes from view-projection matrix
		glm::mat4 vp = camData.projection * camData.view;
		glm::vec4 r0 = glm::row(vp, 0);
		glm::vec4 r1 = glm::row(vp, 1);
		glm::vec4 r2 = glm::row(vp, 2);
		glm::vec4 r3 = glm::row(vp, 3);

		f.planes[0] = r3 + r0; // Left
		f.planes[1] = r3 - r0; // Right
		f.planes[2] = r3 + r1; // Bottom
		f.planes[3] = r3 - r1; // Top
		f.planes[4] = r3 + r2; // Near
		f.planes[5] = r3 - r2; // Far

		// Normalize planes
		for (auto& plane : f.planes)
		{
			plane /= glm::length(glm::vec3(plane));
		}

		return f;
	}

	bool Frustum::intersects(const BoundingVolume& volume) const
	{
		return volume.intersectsFrustum(*this);
	}
}