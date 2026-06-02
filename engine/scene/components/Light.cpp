#include "scene/components/Light.h"

#include "scene/Scene.h"
#include "scene/Object.h"

namespace engine
{
	void Light::start()
	{
		owner->getScene()->addLight(this);
	}

	LightData DirectionalLight::getLightData() const
	{
		return {
			glm::vec4(_color, _intensity),
			glm::vec4(0.0f),
			glm::vec4(owner->transform.getForward(), static_cast<float>(LightType::Directional))
		};
	}

	glm::vec3 DirectionalLight::getDirection() const
	{
		return owner->transform.getForward();
	}

	LightData PointLight::getLightData() const
	{
		return {
			glm::vec4(_color, _intensity),
			glm::vec4(owner->transform.getWorldPosition(), _range),
			glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(LightType::Point))
		};
	}
}