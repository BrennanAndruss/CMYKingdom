#include "renderer/Frustum.h"

#include <glm/gtc/matrix_access.hpp>
#include "renderer/BoundingVolume.h"
#include "scene/components/Camera.h"

namespace engine
{
	Frustum Frustum::fromMatrix(const glm::mat4& matrix)
	{
		Frustum f;

		// Extract planes from matrix
		glm::vec4 r0 = glm::row(matrix, 0);
		glm::vec4 r1 = glm::row(matrix, 1);
		glm::vec4 r2 = glm::row(matrix, 2);
		glm::vec4 r3 = glm::row(matrix, 3);

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