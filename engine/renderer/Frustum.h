#pragma once

#include <glm/glm.hpp>

namespace engine
{
	class BoundingVolume;
	struct CameraData;
}

namespace engine
{
	struct Frustum
	{
		glm::vec4 planes[6];

		static Frustum fromCamera(const CameraData& camData);
		bool intersects(const BoundingVolume& volume) const;
	};
}