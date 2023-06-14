#pragma once
#ifndef __NR_SCENE_HPP__
#define __NR_SCENE_HPP__

#include "Texture.hpp"
#include "Material.hpp"
#include "Model.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "../app/include/manager/RenderSettingsManager.hpp"

namespace NRenderer
{
	struct RenderOption
	{
		unsigned int width;
		unsigned int height;
		unsigned int depth;
		unsigned int samplesPerPixel;
		RenderSettings::Acceleration acc;
		unsigned int photonsPerLight;
		unsigned int neighborPhotons;
		RenderOption()
			: width(500)
			, height(500)
			, depth(4)
			, samplesPerPixel(16)
			, acc(RenderSettings::Acceleration::NONE)
			, photonsPerLight(10000)
			, neighborPhotons(250)
		{}
	};

	struct Ambient
	{
		enum class Type
		{
			CONSTANT, ENVIROMENT_MAP
		};
		Type type;
		Vec3 constant = {};
		Handle environmentMap = {};
	};

	struct Scene
	{
		Camera camera;

		RenderOption renderOption;

		Ambient ambient;

		// buffers
		vector<Material> materials;
		vector<Texture> textures;

		vector<Model> models;
		vector<Node> nodes;
		// object buffer
		vector<Sphere> sphereBuffer;
		vector<Triangle> triangleBuffer;
		vector<Plane> planeBuffer;
		vector<Mesh> meshBuffer;

		vector<Light> lights;
		// light buffer
		vector<PointLight> pointLightBuffer;
		vector<AreaLight> areaLightBuffer;
		vector<DirectionalLight> directionalLightBuffer;
		vector<SpotLight> spotLightBuffer;
	};
	using SharedScene = shared_ptr<Scene>;
} // namespace NRenderer


#endif