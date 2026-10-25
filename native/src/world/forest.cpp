#include "world/forest.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "core/rng.h"
#include "mesh/mesh_builder.h"

using namespace godot;

namespace tot {

namespace {

// Stacked cones on a short trunk; tiers shrink and darken toward the tip.
Ref<ArrayMesh> pine(float height, int tiers, uint32_t seed) {
	MeshBuilder m;
	Rng rng(seed);
	const int bark = mat_cell(pal::BARK_DARK, mat::BARK);
	m.set_ground_ao(true);
	m.cylinder(Vector3(0, 0, 0), 0.26f, 1.4f, 7, bark, 0.2f);
	m.set_ground_ao(false);
	float y = 1.0f;
	const float tier_h = (height - 1.0f) / (tiers * 0.72f);
	for (int t = 0; t < tiers; ++t) {
		const float k = 1.0f - float(t) / tiers;
		const float r = (0.9f + k * 1.5f) * (height / 7.5f);
		const int cell = mat_cell(t % 2 ? pal::HEDGE : pal::HEDGE_DARK, mat::LEAVES);
		m.set_transform(Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)), Vector3(rng.range(-0.05f, 0.05f), 0, rng.range(-0.05f, 0.05f))));
		m.cylinder(Vector3(0, y, 0), r, tier_h, 7, cell, 0.0f, mat_cell(pal::HEDGE_DARK, mat::LEAVES));
		y += tier_h * 0.72f;
	}
	m.set_transform(Transform3D());
	return m.build();
}

// A twisted bare tree: bent trunk segments and forked branches, a few last leaves clinging on.
Ref<ArrayMesh> spooky(uint32_t seed) {
	MeshBuilder m;
	Rng rng(seed);
	const int bark = mat_cell(pal::BARK_DARK, mat::BARK);
	Vector3 p(0, 0, 0);
	float w = 0.42f;
	Vector3 dir(0, 1, 0);
	std::vector<std::pair<Vector3, Vector3>> tips;
	for (int s = 0; s < 4; ++s) {
		dir = (dir + Vector3(rng.range(-0.35f, 0.35f), 0, rng.range(-0.35f, 0.35f))).normalized();
		const Vector3 q = p + dir * rng.range(1.1f, 1.5f);
		m.beam(p, q, w, w, bark, Vector3(1, 0, 0));
		if (s >= 1) {
			const int branches = rng.irange(1, 2);
			for (int b = 0; b < branches; ++b) {
				const float a = rng.range(0.0f, Math_TAU);
				const Vector3 bd = (Vector3(std::cos(a), rng.range(0.3f, 0.9f), std::sin(a))).normalized();
				const Vector3 e = q + bd * rng.range(1.0f, 1.8f);
				m.beam(q, e, w * 0.45f, w * 0.45f, bark, Vector3(0, 1, 0));
				const Vector3 f = e + (bd + Vector3(rng.range(-0.6f, 0.6f), 0.4f, rng.range(-0.6f, 0.6f))).normalized() * rng.range(0.5f, 0.9f);
				m.beam(e, f, w * 0.22f, w * 0.22f, bark, Vector3(0, 1, 0));
				tips.push_back({ e, f });
			}
		}
		p = q;
		w *= 0.78f;
	}
	const int leaves[] = { pal::MAPLE_ORANGE, pal::RUST_LEAF, pal::GOLDEN_LEAF };
	for (const auto &t : tips) {
		if (rng.chance(0.6f)) {
			m.blob(t.second, Vector3(0.22f, 0.16f, 0.22f), mat_cell(leaves[rng.irange(0, 2)], mat::LEAVES), 0);
		}
	}
	// Roots flaring at the base.
	for (int r = 0; r < 4; ++r) {
		const float a = r * Math_TAU / 4.0f + rng.range(-0.3f, 0.3f);
		m.beam(Vector3(0, 0.25f, 0), Vector3(std::cos(a) * 0.9f, 0.0f, std::sin(a) * 0.9f), 0.2f, 0.18f, bark, Vector3(0, 1, 0));
	}
	return m.build();
}

} // namespace

void ForestKit::build() {
	mesh[PINE_A] = pine(7.5f, 4, 11);
	mesh[PINE_B] = pine(9.5f, 5, 12);
	mesh[PINE_C] = pine(5.5f, 3, 13);
	for (int k : { PINE_A, PINE_B, PINE_C }) {
		trunk_radius[k] = 0.35f;
		trunk_height[k] = 3.0f;
	}
	mesh[SPOOKY_A] = spooky(21);
	mesh[SPOOKY_B] = spooky(22);
	trunk_radius[SPOOKY_A] = trunk_radius[SPOOKY_B] = 0.38f;
	trunk_height[SPOOKY_A] = trunk_height[SPOOKY_B] = 3.0f;
	{
		MeshBuilder m;
		m.blob(Vector3(0, 0.45f, 0), Vector3(0.75f, 0.5f, 0.7f), mat_cell(pal::HEDGE, mat::HEDGE), 1, 0.12f, 3);
		m.blob(Vector3(0.55f, 0.32f, 0.2f), Vector3(0.5f, 0.36f, 0.5f), mat_cell(pal::HEDGE_DARK, mat::HEDGE), 1, 0.12f, 4);
		m.blob(Vector3(-0.45f, 0.3f, -0.15f), Vector3(0.45f, 0.32f, 0.45f), mat_cell(pal::MOSS, mat::HEDGE), 1, 0.12f, 5);
		mesh[BUSH] = m.build();
	}
	for (int v = 0; v < 2; ++v) {
		MeshBuilder m;
		const float s = v == 0 ? 0.8f : 1.3f;
		m.blob(Vector3(0, 0.3f * s, 0), Vector3(0.8f, 0.55f, 0.65f) * s, mat_cell(pal::STONE, mat::STONE), 1, 0.18f, 7 + v);
		m.blob(Vector3(0.1f * s, 0.62f * s, 0.05f), Vector3(0.5f, 0.12f, 0.4f) * s, mat_cell(pal::MOSS, mat::GRASS), 0, 0.1f, 9 + v);
		mesh[v == 0 ? ROCK_A : ROCK_B] = m.build();
		trunk_radius[v == 0 ? ROCK_A : ROCK_B] = 0.6f * s;
		trunk_height[v == 0 ? ROCK_A : ROCK_B] = 0.85f * s;
	}
	{
		MeshBuilder m;
		m.cylinder(Vector3(0, 0, 0), 0.45f, 0.55f, 9, mat_cell(pal::BARK, mat::BARK), 0.4f, mat_cell(pal::WOOD_LIGHT, mat::WOOD));
		for (int r = 0; r < 3; ++r) {
			const float a = r * 2.1f;
			m.beam(Vector3(0, 0.2f, 0), Vector3(std::cos(a) * 0.7f, 0, std::sin(a) * 0.7f), 0.16f, 0.14f, mat_cell(pal::BARK, mat::BARK));
		}
		mesh[STUMP] = m.build();
		trunk_radius[STUMP] = 0.45f;
		trunk_height[STUMP] = 0.55f;
	}
	{
		// Glowing mushrooms: teal and purple caps on pale stems, the woods' night lights.
		MeshBuilder m;
		Rng rng(31);
		for (int k = 0; k < 6; ++k) {
			const float a = k * 1.05f + rng.range(-0.3f, 0.3f), r = rng.range(0.1f, 0.6f);
			const Vector3 base(std::cos(a) * r, 0, std::sin(a) * r);
			const float h = rng.range(0.15f, 0.45f);
			m.cylinder(base, 0.04f + h * 0.08f, h, 6, mat_cell(pal::BONE, mat::FLAT), 0.03f);
			const int cap = k % 3 == 0 ? pal::GLOW_PURPLE : pal::GLOW_MAGIC_TEAL;
			m.blob(base + Vector3(0, h, 0), Vector3(0.12f + h * 0.25f, 0.06f + h * 0.1f, 0.12f + h * 0.25f), cap, 1);
		}
		mesh[SHROOMS] = m.build();
	}
	{
		MeshBuilder m;
		m.set_transform(Transform3D(Basis(Vector3(0, 0, 1), Math_PI * 0.5f), Vector3(1.6f, 0.35f, 0)));
		m.cylinder(Vector3(0, 0, 0), 0.36f, 3.2f, 8, mat_cell(pal::BARK, mat::BARK), 0.32f, mat_cell(pal::WOOD_LIGHT, mat::WOOD));
		m.set_transform(Transform3D());
		m.blob(Vector3(-0.6f, 0.68f, 0.1f), Vector3(0.4f, 0.1f, 0.25f), mat_cell(pal::MOSS, mat::GRASS), 0);
		mesh[LOG] = m.build();
		trunk_radius[LOG] = 0.0f; // box collision, handled by the streamer
		trunk_height[LOG] = 0.7f;
	}
}

} // namespace tot
