#pragma once
#ifndef __CONDUCTOR_HPP__
#define __CONDUCTOR_HPP__

#include "Shader.hpp"
#include "samplers/SamplerInstance.hpp"

namespace PhotonMapping {
	class Conductor : public Shader {
	private:
		Vec3 albedo;

	public:
		Conductor(Material& material, vector<Texture>& textures);
		Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const;
	};

}  // namespace PhotonMapping

#endif
