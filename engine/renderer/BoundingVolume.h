#pragma once

#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <vector>
#include "renderer/Frustum.h"

namespace engine
{
	struct BoundingVolume
	{
		virtual bool intersectsFrustum(const Frustum& frustum) const = 0;
	};

	struct Sphere : public BoundingVolume
	{
		glm::vec3 center = glm::vec3(0.0f);
		float radius = 1.0f;

		bool intersectsFrustum(const Frustum& frustum) const override
		{
			for (const auto& plane : frustum.planes)
			{
				if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
					return false;
			}

			return true;
		}
	};

	struct BBox : public BoundingVolume
	{
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max = glm::vec3(-std::numeric_limits<float>::max());

		glm::vec3 center() const
		{
			return max - min;
		}

		// Expand to include a point
		void expand(glm::vec3 point)
		{
			min = glm::min(min, point);
			max = glm::max(max, point);
		}

		// Expand to include another bbox
		void expand(const BBox& other)
		{
			min = glm::min(min, other.min);
			max = glm::max(max, other.max);
		}

		// Transform to world space
		BBox transformed(const glm::mat4& transform) const
		{
			BBox result;

			// Extract translation component
			glm::vec3 translation = glm::vec3(transform[3]);
			result.min = translation;
			result.max = translation;

			// Form the new bounding box by checking the extent of each basis axis
			for (int col = 0; col < 3; ++col)
			{
				glm::vec3 axis = glm::vec3(transform[col]);

				float e1 = axis.x * min[col];
				float e2 = axis.x * max[col];
				result.min.x += std::min(e1, e2);
				result.max.x += std::max(e1, e2);

				e1 = axis.y * min[col];
				e2 = axis.y * max[col];
				result.min.y += std::min(e1, e2);
				result.max.y += std::max(e1, e2);

				e1 = axis.z * min[col];
				e2 = axis.z * max[col];
				result.min.z += std::min(e1, e2);
				result.max.z += std::max(e1, e2);
			}

			return result;
		}

		bool intersectsFrustum(const Frustum& frustum) const override
		{
			for (const auto& plane : frustum.planes)
			{
				// Find the furthest point in plane normal direction
				glm::vec3 pVertex(
					plane.x >= 0 ? max.x : min.x,
					plane.y >= 0 ? max.y : min.y,
					plane.z >= 0 ? max.z : min.z
				);

				// If the furthest point is outside the plane, the box is outside the frustum
				if (glm::dot(glm::vec3(plane), pVertex) + plane.w < 0)
					return false;
			}

			return true;
		}

		static BBox fromPositions(const std::vector<glm::vec3>& positions)
		{
			BBox bbox;
			for (const auto& pos : positions)
			{
				bbox.expand(pos);
			}

			return bbox;
		}
	};
}