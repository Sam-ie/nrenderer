#pragma once
#ifndef __PHOTON_MAPPING_HPP__
#define __PHOTON_MAPPING_HPP__

#include "scene/Scene.hpp"
#include "Ray.hpp"
#include "Camera.hpp"
#include "intersections/HitRecord.hpp"

#include "shaders/ShaderCreator.hpp"

#include "../app/include/manager/RenderSettingsManager.hpp"

#include <tuple>
#include <queue>
#include <vector>
#include <algorithm>
#include <list>
namespace PhotonMapping
{
	using namespace NRenderer;
	using namespace std;

	// 光子类定义
	class Photon {
	public:
		Vec3 position;
		Vec3 direction;
		Vec3 power;
		Photon() {}
		Photon(Vec3 pos, Vec3 dir, Vec3 power)
			: position(pos), direction(dir), power(power) {}
	};

	class KDTree {
	public:
		struct Node {
			Photon p;
			Node* left;
			Node* right;
			size_t axis;
			Vec3 boundary_min;
			Vec3 boundary_max;

			Node(Photon& p, const size_t& axis) : p(p), left(nullptr), right(nullptr), axis(axis) {}
		};

		KDTree() :root(nullptr)
		{

		}

		void buildTree(vector<Photon>& photons) {
			root = buildTree(photons, 0, photons.size() - 1, 0);
		}

		~KDTree() {
			destroyTree(root);
		}

		tuple<vector<Photon>, float> search(const Vec3& target, size_t k) {
			list<Neighbor> result;
			searchHelper(root, target, k, result);
			vector<Photon> res_p;
			float res_dist = result.back().distanceSqr;
			for (auto item : result)
			{
				res_p.push_back(item.p);
			}
			return{ res_p, res_dist };
		}

	private:
		Node* root;

		struct Neighbor {
			float distanceSqr;
			Photon p;

			bool operator<(const Neighbor& other) const {
				return distanceSqr < other.distanceSqr;
			}
		};

		Node* buildTree(vector<Photon>& photons, size_t start, size_t end, size_t depth) {
			if (start > end) {
				return nullptr;
			}

			else if (start == end) {
				Node* node = new Node(photons[start], depth % 3);
				node->boundary_min = node->boundary_max = photons[start].position;
				return node;
			}

			size_t axis = depth % 3;
			size_t mid = (start + end) / 2;

			/*cout << start << " " << mid << " " << end << endl;*/

			nth_element(photons.begin() + start, photons.begin() + mid, photons.begin() + end + 1,
				[axis](Photon& a, Photon& b) {
					return a.position[axis] < b.position[axis];
				});

			Node* newNode = new Node(photons[mid], axis);
			newNode->left = buildTree(photons, start, mid < 1 ? 0 : mid - 1, depth + 1);
			newNode->right = buildTree(photons, mid + 1, end, depth + 1);

			float float_min = numeric_limits<float>::lowest(), float_max = numeric_limits<float>::max();
			Vec3 min1 = newNode->left == nullptr ? Vec3(float_max, float_max, float_max) : newNode->left->boundary_min;
			Vec3 min2 = newNode->right == nullptr ? Vec3(float_max, float_max, float_max) : newNode->right->boundary_min;
			newNode->boundary_min = glm::min(glm::min(min1, min2), photons[mid].position);
			Vec3 max1 = newNode->left == nullptr ? Vec3(float_min, float_min, float_min) : newNode->left->boundary_max;
			Vec3 max2 = newNode->right == nullptr ? Vec3(float_min, float_min, float_min) : newNode->right->boundary_max;
			newNode->boundary_max = glm::max(glm::max(max1, max2), photons[mid].position);

			return newNode;
		}

		void destroyTree(Node* node) {
			if (node == nullptr) {
				return;
			}

			destroyTree(node->left);
			destroyTree(node->right);

			delete node;
		}

		void searchHelper(Node* node, const Vec3& target, size_t k, list<Neighbor>& result) {
			if (node == nullptr) {
				return;
			}

			auto neighbor = Neighbor(glm::dot(((node->p).position - target), ((node->p).position - target)), node->p);

			result.insert(lower_bound(result.begin(), result.end(), neighbor), neighbor);

			if (result.size() > k) {
				result.pop_back();
			}

			float axisDist = target[node->axis] - ((node->p).position)[node->axis];

			if (axisDist <= 0) {
				searchHelper(node->left, target, k, result);
				if (node->right != nullptr) {
					axisDist = target[node->axis] - (node->right->boundary_min)[node->axis];
					if (axisDist * axisDist < result.back().distanceSqr)
						searchHelper(node->right, target, k, result);
				}
			}
			else {
				searchHelper(node->right, target, k, result);
				if (node->left != nullptr) {
					axisDist = target[node->axis] - (node->left->boundary_max)[node->axis];
					if (axisDist * axisDist < result.back().distanceSqr)
						searchHelper(node->left, target, k, result);
				}
			}
		}
	};

	class PhotonMappingRenderer
	{
	public:
	private:
		SharedScene spScene;
		Scene& scene;

		// 
		vector<Photon> photons;
		// todo ： 从UI传进来
		unsigned int photonsPerLight;
		float russianRoulette;
		unsigned int neighborsNum;

		RenderSettings::Acceleration acc;
		KDTree kd_tree;

		unsigned int width;
		unsigned int height;
		unsigned int depth;
		unsigned int samples;

		using SCam = PhotonMapping::Camera;
		SCam camera;

		vector<SharedShader> shaderPrograms;

	public:
		PhotonMappingRenderer(SharedScene spScene)
			: spScene(spScene)
			, scene(*spScene)
			, camera(spScene->camera)
		{
			width = scene.renderOption.width;
			height = scene.renderOption.height;
			depth = scene.renderOption.depth;
			samples = scene.renderOption.samplesPerPixel;
			acc = scene.renderOption.acc;
			photonsPerLight = scene.renderOption.photonsPerLight;
			russianRoulette = 0.8;
			neighborsNum = scene.renderOption.neighborPhotons;
			kd_tree = KDTree();
		}
		~PhotonMappingRenderer() = default;

		using RenderResult = tuple<RGBA*, unsigned int, unsigned int>;
		RenderResult render();
		void release(const RenderResult& r);

	private:
		void renderTask(RGBA* pixels, int width, int height, int off, int step);

		RGB gamma(const RGB& rgb);
		RGB trace(const Ray& ray, int currDepth);
		HitRecord closestHitObject(const Ray& r);
		tuple<float, Vec3> closestHitLight(const Ray& r);

		//
		void buildPhotonMap();
		void buildPhotonMapTask(int step);
		void tracePhoton(const Ray& r, Vec3 currPower, int currDepth);
		tuple<vector<Photon>, float> findNearestPhotons(Vec3& point);
		void buildKDTree();
	};
}

#endif