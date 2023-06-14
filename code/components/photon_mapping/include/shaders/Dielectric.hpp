#pragma once
#ifndef __DIELECTRIC_HPP__
#define __DIELECTRIC_HPP__

#include "Shader.hpp"
#include "samplers/SamplerInstance.hpp"

namespace PhotonMapping {
	class Dielectric : public Shader {
	private:
		Vec3 absorbed;
		float ior;

	public:
		Dielectric(Material& material, vector<Texture>& textures);
		Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const;

		inline float schlick(float cosine, float refractionIndex) const
		{
			// approx fresnel
			float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
			r0 = r0 * r0;
			return r0 + (1.0f - r0) * glm::pow(1.0f - cosine, 5.0f);
		}

		inline Vec3 reflect(Vec3& incident, Vec3& normal) const
		{
			return glm::normalize(incident - 2.0f * glm::dot(incident, normal) * normal);
		}

		Vec3 scatterHelper(Vec3 incident, Vec3 normal, float n1, float n2) const
		{
			float cosi = clamp(glm::dot(incident, normal), -1.f, 1.f);
			float refractionIndexRatio = n1 / n2;

			if (cosi > 0.0f)
			{
				normal = -normal;
				refractionIndexRatio = n2 / n1;
			}

			float sinThetai = glm::sqrt(1.0f - cosi * cosi);
			float sinThetat = refractionIndexRatio * sinThetai;

			if (sinThetat >= 1.0f)
			{
				// Total internal reflection
				return reflect(incident, normal);
			}

			float cosThetat = glm::sqrt(1.0f - sinThetat * sinThetat);
			float r0 = schlick(fabs(cosi), refractionIndexRatio);

			/*cout << r0 << endl;*/

			if (defaultSamplerInstance<UniformSampler>().sample1d() < r0)
			{
				// Reflect
				return reflect(incident, normal);
			}
			else
			{
				// Refract
				/*cout << "refract" << endl;*/
				return glm::normalize(refractionIndexRatio * incident + (refractionIndexRatio * fabs(cosi) - cosThetat) * normal)
					/*glm::refract(incident, normal, 1.f/refractionIndexRatio)*/;
			}
		}

	};

}  // namespace PhotonMapping

#endif
