#include "server/Server.hpp"

#include "PhotonMapping.hpp"

#include "VertexTransformer.hpp"
#include "intersections/intersections.hpp"

#include "glm/gtc/matrix_transform.hpp"

#include "Onb.hpp"

namespace PhotonMapping
{
	RGB PhotonMappingRenderer::gamma(const RGB& rgb) {
		return glm::sqrt(rgb);
	}

	void PhotonMappingRenderer::renderTask(RGBA* pixels, int width, int height, int off, int step) {
		for (int i = off; i < height; i += step) {
			for (int j = 0; j < width; j++) {
				Vec3 color{ 0, 0, 0 };
				for (int k = 0; k < samples; k++) {
					auto r = defaultSamplerInstance<UniformInSquare>().sample2d();
					float rx = r.x;
					float ry = r.y;
					float x = (float(j) + rx) / float(width);
					float y = (float(i) + ry) / float(height);
					auto ray = camera.shoot(x, y);
					color += trace(ray, 0);
				}
				color /= samples;
				color = gamma(color);
				pixels[(height - i - 1) * width + j] = { color, 1 };
			}
			/*cout << "line " << i << " rendered" << endl;*/
		}
		cout << "thread " << off << " ended" << endl;
	}

	auto PhotonMappingRenderer::render() -> RenderResult {

		cout << "photons/light = " << photonsPerLight << "\tneighborsNum = " << neighborsNum << endl;

		// shaders
		shaderPrograms.clear();
		ShaderCreator shaderCreator{};
		for (auto& m : scene.materials) {
			shaderPrograms.push_back(shaderCreator.create(m, scene.textures));
		}

		RGBA* pixels = new RGBA[width * height]{};

		// 局部坐标转换成世界坐标
		VertexTransformer vertexTransformer{};
		vertexTransformer.exec(spScene);

		// 
		buildPhotonMap();

		const auto taskNums = 8;
		thread t[taskNums];
		for (int i = 0; i < taskNums; i++) {
			t[i] = thread(&PhotonMappingRenderer::renderTask,
				this, pixels, width, height, i, taskNums);
		}
		for (int i = 0; i < taskNums; i++) {
			t[i].join();
		}
		getServer().logger.log("Done...");
		return { pixels, width, height };
	}

	void PhotonMappingRenderer::release(const RenderResult& r) {
		auto [p, w, h] = r;
		delete[] p;
	}

	HitRecord PhotonMappingRenderer::closestHitObject(const Ray& r) {
		HitRecord closestHit = nullopt;
		float closest = FLOAT_INF;
		for (auto& s : scene.sphereBuffer) {
			auto hitRecord = Intersection::xSphere(r, s, 0.000001, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		for (auto& t : scene.triangleBuffer) {
			auto hitRecord = Intersection::xTriangle(r, t, 0.000001, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		for (auto& p : scene.planeBuffer) {
			auto hitRecord = Intersection::xPlane(r, p, 0.000001, closest);
			if (hitRecord && hitRecord->t < closest) {
				closest = hitRecord->t;
				closestHit = hitRecord;
			}
		}
		return closestHit;
	}

	tuple<float, Vec3> PhotonMappingRenderer::closestHitLight(const Ray& r) {
		Vec3 v = {};
		HitRecord closest = getHitRecord(FLOAT_INF, {}, {}, {});
		for (auto& a : scene.areaLightBuffer) {
			auto hitRecord = Intersection::xAreaLight(r, a, 0.000001, closest->t);
			if (hitRecord && closest->t > hitRecord->t) {
				closest = hitRecord;
				v = a.radiance;
			}
		}
		return { closest->t, v };
	}

	void PhotonMappingRenderer::buildPhotonMap()
	{
		// emit photons
		for (int j = 0; j < photonsPerLight; j++) {
			for (auto area_light : scene.areaLightBuffer) {

				// random pos
				Vec2 random = defaultSamplerInstance<UniformInSquare>().sample2d();
				Vec3 origin = area_light.position + random.x * area_light.u + random.y * area_light.v;

				// random dir
				Vec3 random3d = defaultSamplerInstance<HemiSphere>().sample3d();
				Vec3 normal = glm::normalize(glm::cross(area_light.u, area_light.v));
				Vec3 direction = glm::normalize(Onb{ normal }.local(random3d));

				// simplified
				auto power = area_light.radiance * glm::length(glm::cross(area_light.u, area_light.v)) * PI;

				// trace photons recursively
				tracePhoton(Ray(origin, direction), power, 0);
			}
		}
		if (acc == RenderSettings::Acceleration::KD_TREE)
		{
			kd_tree.buildTree(photons);
		}
		cout << "photon map(size " << photons.size() << ") built...\n";
	}

	void PhotonMappingRenderer::tracePhoton(const Ray& r, Vec3 currPower, int currDepth) {
		if (currDepth == depth) return;
		auto hitObject = closestHitObject(r);
		auto [t, emitted] = closestHitLight(r);
		// hit object
		if (hitObject && hitObject->t < t) {
			auto mtlHandle = hitObject->material;
			if (scene.materials[mtlHandle.index()].hasProperty("diffuseColor") || scene.materials[mtlHandle.index()].hasProperty("diffuseMap"))
			{
				/*cout << currPower * fabs(glm::dot(r.direction, hitObject->normal)) << endl;*/
				if (glm::dot(r.direction, hitObject->normal) < 0)
					photons.push_back(Photon(hitObject->hitPoint, r.direction, currPower * fabs(glm::dot(r.direction, hitObject->normal))));
			}
			if (scene.materials[mtlHandle.index()].hasProperty("diffuseColor") || scene.materials[mtlHandle.index()].hasProperty("diffuseMap") ||
				scene.materials[mtlHandle.index()].hasProperty("ior") || scene.materials[mtlHandle.index()].hasProperty("reflect"))
			{
				if (defaultSamplerInstance<UniformSampler>().sample1d() < russianRoulette) {
					auto scattered = shaderPrograms[mtlHandle.index()]->shade(r, hitObject->hitPoint, hitObject->normal);
					auto scatteredRay = scattered.ray;
					auto attenuation = scattered.attenuation;
					/*auto emitted = scattered.emitted;*/
					float n_dot_in = fabs(glm::dot(hitObject->normal, scatteredRay.direction));
					float pdf = scattered.pdf;
					tracePhoton(scatteredRay, attenuation * currPower * n_dot_in / pdf / russianRoulette, currDepth + 1);
				}
			}
		}
		else if (t != FLOAT_INF) {
			return;
		}
		else {
			return;
		}
	}

	RGB PhotonMappingRenderer::trace(const Ray& r, int currDepth) {
		if (currDepth == depth) return scene.ambient.constant;
		auto hitObject = closestHitObject(r);
		auto [t, emitted] = closestHitLight(r);
		// hit object
		if (hitObject && hitObject->t < t) {
			auto mtlHandle = hitObject->material;
			if (scene.materials[mtlHandle.index()].hasProperty("diffuseColor") || scene.materials[mtlHandle.index()].hasProperty("diffuseMap"))
			{
				// diffuse
				Vec3 flux{ 0,0,0 }, dir{ 0, 0, 0 };
				auto [knn, radius2] = findNearestPhotons(hitObject->hitPoint);
				/*cout << radius2 << endl;*/
				for (auto& p : knn)
				{
					if (glm::dot(p.direction, hitObject->normal) < 0)
					{
						flux += p.power;
						dir -= p.direction;
					}
				}
				auto n_dot_in = glm::dot(hitObject->normal, glm::normalize(dir));

				//auto scattered = shaderPrograms[mtlHandle.index()]->shade(r, hitObject->hitPoint, hitObject->normal);
				//auto scatteredRay = scattered.ray;
				//auto emitted = scattered.emitted;

				auto c = scene.materials[mtlHandle.index()].getProperty<Property::Wrapper::RGBType>("diffuseColor")->value;
				/*cout << c * n_dot_in * (flux / (PI * radius2 * photonsPerLight)) << endl;*/
				return /*emitted +*/ c * n_dot_in * (flux / (PI * radius2 * photonsPerLight));
			}
			else if (scene.materials[mtlHandle.index()].hasProperty("ior") || scene.materials[mtlHandle.index()].hasProperty("reflect"))
			{
				/*cout << "ior " << mtlHandle.index() << endl;*/
				auto scattered = shaderPrograms[mtlHandle.index()]->shade(r, hitObject->hitPoint, hitObject->normal);
				auto scatteredRay = scattered.ray;
				auto attenuation = scattered.attenuation;
				auto next = trace(scatteredRay, currDepth + 1);
				float n_dot_in = glm::dot(hitObject->normal, scatteredRay.direction);
				float pdf = scattered.pdf;
				/*cout << attenuation << " " << n_dot_in << " " << pdf << endl;
				return attenuation * next * n_dot_in / pdf;*/
				return attenuation * next ;
			}
			else
				assert(0);
		}
		//
		else if (t != FLOAT_INF) {
			return emitted;
		}
		else {
			return Vec3{ 0 };
		}
	}

	tuple<vector<Photon>, float> PhotonMappingRenderer::findNearestPhotons(Vec3& point) {
		switch (acc)
		{
		case RenderSettings::Acceleration::NONE:
		{
			priority_queue<pair<float, int>, vector<pair<float, int>>, greater<pair<float, int>>> q;
			for (int i = 0; i != photons.size(); i++)
			{
				q.push({ glm::dot(photons[i].position - point, photons[i].position - point), i });
				/*if (q.size() > neighborsNum)
					q.pop();*/
			}
			vector<Photon> res;
			float radius2 = 0;
			for (int i = 0; i < neighborsNum && i < photons.size() && !q.empty(); i++)
			{
				if (i == neighborsNum - 1)
					radius2 = q.top().first;
				res.push_back(photons[q.top().second]);
				q.pop();
			}
			return{ res, radius2 };
		}
		case RenderSettings::Acceleration::KD_TREE:
		{
			return kd_tree.search(point, neighborsNum);
		}
		default:
			assert(0);
		}
	}
}