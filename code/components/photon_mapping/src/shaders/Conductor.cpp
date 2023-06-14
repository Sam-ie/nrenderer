#include "shaders/Conductor.hpp"
#include "samplers/SamplerInstance.hpp"

namespace PhotonMapping
{
	Conductor::Conductor(Material& material, vector<Texture>& textures)
		: Shader(material, textures)
	{
		albedo = material.getProperty<Property::Wrapper::RGBType>("reflect")->value;
	}

	Scattered Conductor::shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const {
		auto in = glm::normalize(ray.direction);
		auto n = glm::normalize(normal);

		auto dir = glm::normalize(in - 2.0f * glm::dot(in, n) * n);

		return {
			Ray{hitPoint, dir},
			albedo,
			Vec3{0},
			1.f
		};
	}
}