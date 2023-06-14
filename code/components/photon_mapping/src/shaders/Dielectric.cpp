#include "shaders/Dielectric.hpp"
#include "samplers/SamplerInstance.hpp"

namespace PhotonMapping
{
	Dielectric::Dielectric(Material& material, vector<Texture>& textures)
		: Shader(material, textures)
	{
		ior = material.getProperty<Property::Wrapper::FloatType>("ior")->value;
		absorbed = material.getProperty<Property::Wrapper::RGBType>("absorbed")->value;
	}

	Scattered Dielectric::shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const {

		/*cout << "dielectric shade" << endl;*/

		float n1 = 1.f;
		float n2 = ior;

		auto in = glm::normalize(ray.direction);
		auto n = glm::normalize(normal);

		Vec3 dir = scatterHelper(in, n, n1, n2);

		return {
			Ray{hitPoint, dir},
			absorbed,
			Vec3{0},
			1.f
		};
	}
}