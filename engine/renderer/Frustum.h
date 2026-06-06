#pragma once

#include <glm/glm.hpp>

namespace engine
{
	class BoundingVolume;
}

namespace engine
{
	struct Frustum
	{
		glm::vec4 planes[6];

		static Frustum fromMatrix(const glm::mat4& matrix);
		bool intersects(const BoundingVolume& volume) const;
	};
}