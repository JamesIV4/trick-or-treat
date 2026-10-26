#include "play/monster_art.h"

#include "gen/asset_library.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "core/rng.h"
#include "mesh/mesh_builder.h"

using namespace godot;

namespace tot {

const char *monster_name(MonsterType t) {
	static const char *n[] = { "Pumpkin Hopper", "Bat", "Rattlebones", "Ghost", "Candy-Corn Gremlin", "Mummy", "Goo Slime", "Scarecrow", "Spider",
		"Toadstool Imp" };
	return n[int(t)];
}

const char *nest_name(MonsterType t) {
	static const char *n[] = { "Cracked Pumpkin", "Bat Belfry", "Crooked Graves", "Haunted Lantern", "Candy-Corn Hive", "Sarcophagus",
		"Bubbling Cauldron", "Crow Post", "Web Nest", "Fairy Ring" };
	return n[int(t)];
}

namespace {

// Big cute eye: white, dark pupil looking forward, tiny glint.
void cute_eye(MeshBuilder &m, Vector3 p, float r) {
	m.blob(p, Vector3(r, r * 1.15f, r * 0.6f), pal::EYE_WHITE, 1);
	m.blob(p + Vector3(0, -r * 0.1f, r * 0.45f), Vector3(r * 0.55f, r * 0.65f, r * 0.3f), pal::INK, 1);
	m.blob(p + Vector3(r * 0.2f, r * 0.25f, r * 0.62f), Vector3(r * 0.16f, r * 0.16f, r * 0.1f), pal::GLOW_WHITE, 0);
}

// A thorn: a cone pointing along `dir` from `base`.
void thorn(MeshBuilder &m, Vector3 base, Vector3 dir, float len, float r, int cell) {
	dir = dir.normalized();
	Vector3 side = std::abs(dir.y) > 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
	const Vector3 x = side.cross(dir).normalized();
	const Vector3 z = x.cross(dir).normalized();
	const Transform3D save = m.transform();
	m.set_transform(save * Transform3D(Basis(x, dir, z), base));
	// Bone-white spine with a brighter tip.
	m.cylinder(Vector3(), r, len * 0.72f, 5, cell, r * 0.3f);
	m.cylinder(Vector3(0, len * 0.72f, 0), r * 0.3f, len * 0.28f, 5, pal::TRIM_WHITE, 0.0f);
	m.set_transform(save);
}

// A round tapered stem segment from a to b (an oriented cylinder; joints get a knuckle blob).
void tube(MeshBuilder &m, Vector3 a, Vector3 b, float ra, float rb, int cell) {
	const Vector3 d = b - a;
	const float len = d.length();
	if (len < 1e-3f) {
		return;
	}
	const Vector3 y = d / len;
	const Vector3 side = std::abs(y.y) > 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
	const Vector3 x = side.cross(y).normalized();
	const Vector3 z = x.cross(y).normalized();
	const Transform3D save = m.transform();
	m.set_transform(save * Transform3D(Basis(x, y, z), a));
	m.cylinder(Vector3(), ra, len, 7, cell, rb);
	m.set_transform(save);
}

// A small pointed leaf lying along `dir` from `base`, tilted up a little.
void leaf(MeshBuilder &m, Vector3 base, Vector3 dir, Vector3 side, float size, int cell) {
	const Vector3 tip = base + dir * size;
	const Vector3 mid = base + dir * (size * 0.45f) + Vector3(0, size * 0.12f, 0);
	m.tri_out(base, mid + side * size * 0.28f, tip, cell, mid - Vector3(0, 1, 0));
	m.tri_out(base, tip, mid - side * size * 0.28f, cell, mid - Vector3(0, 1, 0));
}

Ref<ArrayMesh> finish(MeshBuilder &m) {
	return m.empty() ? Ref<ArrayMesh>() : m.build();
}

void build_pumpkin(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int skin = mat_cell(pal::PUMPKIN, mat::PUMPKIN);
	b.blob(Vector3(0, 0.42f, 0), Vector3(0.48f, 0.4f, 0.46f), skin, 1);
	for (float sx : { -1.0f, 1.0f }) {
		b.blob(Vector3(sx * 0.2f, 0.42f, 0), Vector3(0.3f, 0.39f, 0.42f), skin, 1);
	}
	b.cylinder(Vector3(0, 0.76f, 0), 0.07f, 0.2f, 6, mat_cell(pal::STEM_BROWN, mat::WOOD), 0.05f);
	b.blob(Vector3(0.13f, 0.86f, 0), Vector3(0.15f, 0.03f, 0.08f), mat_cell(pal::STEM_GREEN, mat::LEAVES), 0);
	for (float sx : { -1.0f, 1.0f }) {
		b.tri_out(Vector3(sx * 0.24f, 0.5f, 0.42f), Vector3(sx * 0.08f, 0.5f, 0.45f), Vector3(sx * 0.15f, 0.64f, 0.43f), pal::GLOW_JACK_HOT,
				Vector3(sx * 0.15f, 0.55f, 0.0f));
	}
	b.box(Vector3(0, 0.31f, 0.43f), Vector3(0.2f, 0.045f, 0.03f), pal::GLOW_JACK_HOT);
	for (int k = -1; k <= 1; ++k) {
		b.box(Vector3(k * 0.1f, 0.345f, 0.455f), Vector3(0.03f, 0.022f, 0.02f), skin);
	}
	l.beam(Vector3(0, 0, 0), Vector3(-0.22f, -0.12f, 0.12f), 0.07f, 0.07f, mat_cell(pal::VINE, mat::LEAVES));
	l.blob(Vector3(-0.24f, -0.14f, 0.14f), Vector3(0.08f, 0.04f, 0.06f), mat_cell(pal::STEM_GREEN, mat::LEAVES), 0);
	r.beam(Vector3(0, 0, 0), Vector3(0.22f, -0.12f, 0.12f), 0.07f, 0.07f, mat_cell(pal::VINE, mat::LEAVES));
	r.blob(Vector3(0.24f, -0.14f, 0.14f), Vector3(0.08f, 0.04f, 0.06f), mat_cell(pal::STEM_GREEN, mat::LEAVES), 0);
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.44f, 0.4f, 0.05f);
	a.pivot_b = Vector3(0.44f, 0.4f, 0.05f);
	a.height = 0.9f;
	a.radius = 0.42f;
	// Nest: a giant cracked pumpkin glowing from inside, vines sprawling out.
	MeshBuilder n;
	const int dark = mat_cell(pal::PUMPKIN_DARK, mat::PUMPKIN);
	n.blob(Vector3(0, 0.85f, 0), Vector3(1.2f, 0.85f, 1.15f), dark, 1, 0.05f, 3);
	for (int k = 0; k < 4; ++k) {
		const float ang = k * Math_PI * 0.5f + 0.4f;
		n.blob(Vector3(std::cos(ang) * 0.45f, 0.85f, std::sin(ang) * 0.45f), Vector3(0.75f, 0.82f, 0.75f), dark, 1, 0.04f, 4 + k);
	}
	n.blob(Vector3(0, 1.62f, 0), Vector3(0.5f, 0.12f, 0.5f), pal::INK, 1);
	n.cylinder(Vector3(0.1f, 1.6f, 0), 0.14f, 0.35f, 6, mat_cell(pal::STEM_BROWN, mat::WOOD), 0.1f);
	Vector3 c(-0.55f, 0.35f, 1.08f);
	for (int k = 0; k < 7; ++k) {
		const Vector3 d(c.x + 0.18f, c.y + ((k & 1) ? -0.16f : 0.2f), 1.1f - std::abs(c.x) * 0.15f);
		n.beam(c, d, 0.09f, 0.06f, pal::GLOW_JACK_HOT, Vector3(0, 0, 1));
		c = d;
	}
	Rng rng(5);
	for (int k = 0; k < 6; ++k) {
		const float ang = k * Math_TAU / 6.0f + rng.range(-0.3f, 0.3f);
		Vector3 p(std::cos(ang) * 1.1f, 0.05f, std::sin(ang) * 1.1f);
		for (int s = 0; s < 3; ++s) {
			const Vector3 q = p + Vector3(std::cos(ang + s * 0.5f), 0, std::sin(ang + s * 0.5f)) * 0.55f;
			n.beam(p, q, 0.08f, 0.06f, mat_cell(pal::VINE_DARK, mat::LEAVES));
			p = q;
		}
		n.blob(p, Vector3(0.16f, 0.13f, 0.16f), mat_cell(pal::PUMPKIN, mat::PUMPKIN), 0);
	}
	a.nest = finish(n);
	a.nest_radius = 1.2f;
	a.nest_height = 1.7f;
}

void build_bat(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int fur = mat_cell(pal::SHADOW_NAVY, mat::FELT);
	b.blob(Vector3(0, 0, 0), Vector3(0.22f, 0.24f, 0.2f), fur, 1);
	b.blob(Vector3(0, -0.04f, 0.1f), Vector3(0.14f, 0.15f, 0.1f), mat_cell(pal::WITCH_PURPLE, mat::FELT), 0);
	for (float sx : { -1.0f, 1.0f }) {
		b.tri_out(Vector3(sx * 0.06f, 0.17f, 0), Vector3(sx * 0.18f, 0.16f, 0), Vector3(sx * 0.15f, 0.36f, -0.02f), fur, Vector3(sx * 0.12f, 0.2f, -1.0f));
		b.tri_out(Vector3(sx * 0.06f, 0.17f, 0), Vector3(sx * 0.18f, 0.16f, 0), Vector3(sx * 0.15f, 0.36f, -0.02f), fur, Vector3(sx * 0.12f, 0.2f, 1.0f));
		b.blob(Vector3(sx * 0.08f, 0.06f, 0.17f), Vector3(0.05f, 0.055f, 0.03f), pal::GLOW_JACK_HOT, 0);
		b.tri_out(Vector3(sx * 0.05f, -0.05f, 0.19f), Vector3(sx * 0.02f, -0.05f, 0.2f), Vector3(sx * 0.035f, -0.11f, 0.19f), pal::BONE,
				Vector3(sx * 0.03f, -0.07f, 0.0f));
	}
	auto wing = [&](MeshBuilder &m, float sx) {
		const int membrane = mat_cell(pal::ROOF_PLUM, mat::FELT);
		const Vector3 root(0, 0, 0), tip(sx * 0.7f, 0.18f, -0.04f), mid(sx * 0.5f, -0.14f, 0.0f), inner(sx * 0.24f, -0.17f, 0.0f);
		for (float face : { 1.0f, -1.0f }) {
			m.tri_out(root, tip, mid, membrane, Vector3(sx * 0.4f, 0.0f, -face));
			m.tri_out(root, mid, inner, membrane, Vector3(sx * 0.2f, -0.05f, -face));
		}
		m.beam(root, tip, 0.03f, 0.03f, fur);
	};
	wing(l, -1.0f);
	wing(r, 1.0f);
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.15f, 0.04f, 0);
	a.pivot_b = Vector3(0.15f, 0.04f, 0);
	a.height = 0.5f;
	a.radius = 0.3f;
	// Nest: a crooked little belfry on a post, red light in its windows.
	MeshBuilder n;
	n.rounded_box(Vector3(0, 0.12f, 0), Vector3(0.6f, 0.12f, 0.6f), 0.04f, mat_cell(pal::STONE, mat::STONE));
	n.set_transform(Transform3D(Basis(Vector3(0, 0, 1), 0.06f), Vector3()));
	n.cylinder(Vector3(0, 0.2f, 0), 0.14f, 1.4f, 6, mat_cell(pal::WOOD_DARK, mat::WOOD));
	n.rounded_box(Vector3(0, 2.0f, 0), Vector3(0.55f, 0.48f, 0.55f), 0.04f, mat_cell(pal::WOOD_DARK, mat::PLANKS));
	for (int k = 0; k < 4; ++k) {
		const float ang = k * Math_PI * 0.5f;
		const Vector3 f(std::sin(ang), 0, std::cos(ang));
		n.box(Vector3(0, 2.0f, 0) + f * 0.56f, Vector3(std::abs(f.z) * 0.16f + 0.01f, 0.22f, std::abs(f.x) * 0.16f + 0.01f), pal::GLOW_LANTERN_RED);
	}
	n.cylinder(Vector3(0, 2.48f, 0), 0.85f, 0.9f, 4, mat_cell(pal::ROOF_PLUM, mat::SHINGLES), 0.02f);
	n.blob(Vector3(0, 1.72f, 0), Vector3(0.14f, 0.16f, 0.14f), mat_cell(pal::FOIL_GOLD, mat::METAL), 0);
	n.set_transform(Transform3D());
	a.nest = finish(n);
	a.nest_radius = 0.65f;
	a.nest_height = 3.3f;
}

void build_skeleton(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int bone = mat_cell(pal::BONE, mat::FLAT);
	for (float sx : { -1.0f, 1.0f }) {
		b.beam(Vector3(sx * 0.1f, 0.05f, 0), Vector3(sx * 0.1f, 0.48f, 0), 0.08f, 0.08f, bone, Vector3(0, 0, 1));
		b.rounded_box(Vector3(sx * 0.1f, 0.04f, 0.05f), Vector3(0.06f, 0.04f, 0.1f), 0.02f, bone);
	}
	b.rounded_box(Vector3(0, 0.52f, 0), Vector3(0.16f, 0.06f, 0.09f), 0.02f, bone);
	b.beam(Vector3(0, 0.52f, -0.02f), Vector3(0, 0.88f, -0.02f), 0.06f, 0.06f, bone, Vector3(0, 0, 1));
	b.box(Vector3(0, 0.74f, -0.01f), Vector3(0.12f, 0.12f, 0.06f), pal::INK);
	for (int k = 0; k < 3; ++k) {
		b.rounded_box(Vector3(0, 0.64f + k * 0.08f, 0.0f), Vector3(0.17f - k * 0.01f, 0.025f, 0.1f), 0.01f, bone);
	}
	b.blob(Vector3(0, 1.05f, 0.02f), Vector3(0.2f, 0.19f, 0.19f), bone, 1);
	for (float sx : { -1.0f, 1.0f }) {
		b.blob(Vector3(sx * 0.075f, 1.07f, 0.16f), Vector3(0.055f, 0.06f, 0.04f), pal::INK, 1);
		b.blob(Vector3(sx * 0.075f, 1.07f, 0.19f), Vector3(0.024f, 0.024f, 0.015f), pal::GLOW_MAGIC_TEAL, 0);
	}
	b.rounded_box(Vector3(0, 0.92f, 0.06f), Vector3(0.11f, 0.04f, 0.09f), 0.015f, bone);
	b.box(Vector3(0, 0.95f, 0.15f), Vector3(0.08f, 0.006f, 0.005f), pal::INK);
	l.beam(Vector3(0, 0, 0), Vector3(0, -0.36f, 0.04f), 0.06f, 0.06f, bone, Vector3(0, 0, 1));
	l.blob(Vector3(0, -0.39f, 0.05f), Vector3(0.05f, 0.05f, 0.05f), bone, 0);
	r.beam(Vector3(0, 0, 0), Vector3(0, -0.36f, 0.04f), 0.06f, 0.06f, bone, Vector3(0, 0, 1));
	r.beam(Vector3(-0.02f, -0.38f, -0.1f), Vector3(-0.02f, -0.38f, 0.26f), 0.05f, 0.05f, bone);
	r.blob(Vector3(-0.02f, -0.38f, 0.28f), Vector3(0.06f, 0.05f, 0.05f), bone, 0);
	r.blob(Vector3(-0.02f, -0.38f, -0.12f), Vector3(0.06f, 0.05f, 0.05f), bone, 0);
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.22f, 0.84f, 0);
	a.pivot_b = Vector3(0.22f, 0.84f, 0);
	a.height = 1.2f;
	a.radius = 0.3f;
	// Nest: crooked headstones on a fresh dirt mound, runes glowing.
	MeshBuilder n;
	n.blob(Vector3(0, 0.05f, 0), Vector3(1.1f, 0.28f, 0.85f), mat_cell(pal::CINNAMON, mat::SOIL), 1, 0.08f, 9);
	const float tilt[3] = { -0.12f, 0.08f, 0.18f };
	for (int k = 0; k < 3; ++k) {
		const float x = (k - 1) * 0.65f;
		n.set_transform(Transform3D(Basis(Vector3(0, 0, 1), tilt[k]) * Basis(Vector3(1, 0, 0), -0.08f * k), Vector3(x, 0.15f, -0.35f + k * 0.1f)));
		const float h = k == 1 ? 1.2f : 0.85f;
		n.rounded_box(Vector3(0, h * 0.5f, 0), Vector3(0.28f, h * 0.5f, 0.09f), 0.04f, mat_cell(pal::STONE, mat::STONE), mat_cell(pal::MOSS, mat::GRASS));
		n.blob(Vector3(0, h, 0), Vector3(0.28f, 0.12f, 0.09f), mat_cell(pal::STONE, mat::STONE), 0);
		n.box(Vector3(0, h * 0.6f, 0.095f), Vector3(0.12f, 0.025f, 0.01f), pal::GLOW_MAGIC_TEAL);
		n.box(Vector3(0, h * 0.6f, 0.095f), Vector3(0.025f, 0.14f, 0.01f), pal::GLOW_MAGIC_TEAL);
	}
	n.set_transform(Transform3D());
	n.blob(Vector3(0.1f, 0.25f, 0.45f), Vector3(0.28f, 0.08f, 0.2f), pal::INK, 1);
	a.nest = finish(n);
	a.nest_radius = 1.0f;
	a.nest_height = 1.4f;
}

void build_ghost(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int sheet = mat_cell(pal::GHOST_WHITE, mat::FABRIC);
	b.blob(Vector3(0, 0.78f, 0), Vector3(0.42f, 0.5f, 0.4f), sheet, 2);
	for (int k = 0; k < 6; ++k) {
		const float ang = k * Math_TAU / 6.0f;
		b.cylinder(Vector3(std::cos(ang) * 0.27f, 0.08f, std::sin(ang) * 0.25f), 0.02f, 0.4f, 6, sheet, 0.17f);
	}
	for (float sx : { -1.0f, 1.0f }) {
		b.blob(Vector3(sx * 0.13f, 0.88f, 0.34f), Vector3(0.07f, 0.1f, 0.04f), pal::INK, 1);
		b.blob(Vector3(sx * 0.21f, 0.74f, 0.32f), Vector3(0.06f, 0.035f, 0.02f), pal::CHEEK_PINK, 0);
	}
	b.blob(Vector3(0, 0.66f, 0.36f), Vector3(0.06f, 0.08f, 0.03f), pal::INK, 1);
	l.blob(Vector3(-0.1f, 0, 0.02f), Vector3(0.15f, 0.08f, 0.09f), sheet, 1);
	r.blob(Vector3(0.1f, 0, 0.02f), Vector3(0.15f, 0.08f, 0.09f), sheet, 1);
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.38f, 0.72f, 0);
	a.pivot_b = Vector3(0.38f, 0.72f, 0);
	a.height = 1.3f;
	a.radius = 0.4f;
	// Nest: a tall haunted lantern burning purple.
	MeshBuilder n;
	n.rounded_box(Vector3(0, 0.18f, 0), Vector3(0.5f, 0.18f, 0.5f), 0.05f, mat_cell(pal::STONE_DARK, mat::STONE), mat_cell(pal::MOSS, mat::GRASS));
	n.cylinder(Vector3(0, 0.36f, 0), 0.09f, 2.0f, 8, mat_cell(pal::LAMP_BLACK, mat::METAL));
	n.rounded_box(Vector3(0, 2.6f, 0), Vector3(0.36f, 0.42f, 0.36f), 0.03f, mat_cell(pal::IRON, mat::METAL));
	n.blob(Vector3(0, 2.6f, 0), Vector3(0.33f, 0.38f, 0.33f), pal::GLOW_PURPLE, 1);
	for (int k = 0; k < 4; ++k) {
		const float ang = Math_PI * 0.25f + k * Math_PI * 0.5f;
		n.beam(Vector3(std::cos(ang) * 0.36f, 2.18f, std::sin(ang) * 0.36f), Vector3(std::cos(ang) * 0.36f, 3.02f, std::sin(ang) * 0.36f), 0.05f, 0.05f,
				mat_cell(pal::LAMP_BLACK, mat::METAL));
	}
	n.cylinder(Vector3(0, 3.02f, 0), 0.48f, 0.35f, 4, mat_cell(pal::LAMP_BLACK, mat::METAL), 0.05f);
	a.nest = finish(n);
	a.nest_radius = 0.55f;
	a.nest_height = 3.3f;
}

void build_gremlin(MonsterArt &a) {
	MeshBuilder b, l, r;
	b.cylinder(Vector3(0, 0.12f, 0), 0.32f, 0.22f, 8, mat_cell(pal::CANDY_CORN_YELLOW, mat::FLAT), 0.27f);
	b.cylinder(Vector3(0, 0.34f, 0), 0.27f, 0.24f, 8, mat_cell(pal::PUMPKIN, mat::FLAT), 0.17f);
	b.cylinder(Vector3(0, 0.58f, 0), 0.17f, 0.24f, 8, mat_cell(pal::CANDY_CORN_WHITE, mat::FLAT), 0.03f);
	cute_eye(b, Vector3(-0.09f, 0.44f, 0.2f), 0.07f);
	cute_eye(b, Vector3(0.09f, 0.44f, 0.2f), 0.07f);
	b.box(Vector3(0, 0.27f, 0.27f), Vector3(0.12f, 0.025f, 0.02f), pal::INK);
	for (int k = -2; k <= 2; ++k) {
		b.tri_out(Vector3(k * 0.045f - 0.02f, 0.29f, 0.29f), Vector3(k * 0.045f + 0.02f, 0.29f, 0.29f), Vector3(k * 0.045f, 0.255f, 0.29f), pal::BONE,
				Vector3(k * 0.045f, 0.27f, 0.0f));
	}
	for (MeshBuilder *m : { &l, &r }) {
		m->beam(Vector3(0, 0, 0), Vector3(0, -0.12f, 0.02f), 0.05f, 0.05f, mat_cell(pal::CHARCOAL, mat::FLAT));
		m->rounded_box(Vector3(0, -0.13f, 0.06f), Vector3(0.05f, 0.025f, 0.08f), 0.01f, mat_cell(pal::CHARCOAL, mat::FLAT));
	}
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.12f, 0.13f, 0);
	a.pivot_b = Vector3(0.12f, 0.13f, 0);
	a.height = 0.8f;
	a.radius = 0.32f;
	// Nest: a dirt burrow with a dark hole and candy-corn crumbs.
	MeshBuilder n;
	n.blob(Vector3(0, 0.15f, 0), Vector3(1.05f, 0.6f, 1.0f), mat_cell(pal::CINNAMON, mat::SOIL), 1, 0.1f, 13);
	n.blob(Vector3(0, 0.5f, 0.55f), Vector3(0.38f, 0.3f, 0.3f), pal::INK, 1);
	Rng rng(14);
	for (int k = 0; k < 7; ++k) {
		const float ang = rng.range(0.0f, Math_TAU), rr = rng.range(1.0f, 1.4f);
		const Vector3 p(std::cos(ang) * rr, 0.02f, std::sin(ang) * rr);
		n.cylinder(p, 0.07f, 0.06f, 6, mat_cell(pal::CANDY_CORN_YELLOW, mat::FLAT), 0.05f);
		n.cylinder(p + Vector3(0, 0.06f, 0), 0.05f, 0.06f, 6, mat_cell(pal::PUMPKIN, mat::FLAT), 0.03f);
		n.cylinder(p + Vector3(0, 0.12f, 0), 0.03f, 0.05f, 6, mat_cell(pal::CANDY_CORN_WHITE, mat::FLAT), 0.0f);
	}
	a.nest = finish(n);
	a.nest_radius = 1.0f;
	a.nest_height = 1.6f; // the authored Candy-Corn Hive stands 1.6 m
}

void build_mummy(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int wrap = mat_cell(pal::CREAM, mat::FABRIC), band = mat_cell(pal::BONE, mat::FABRIC);
	for (float sx : { -1.0f, 1.0f }) {
		b.rounded_box(Vector3(sx * 0.11f, 0.26f, 0), Vector3(0.09f, 0.26f, 0.09f), 0.03f, wrap);
	}
	b.rounded_box(Vector3(0, 0.8f, 0), Vector3(0.23f, 0.3f, 0.15f), 0.05f, wrap);
	Rng rng(7);
	for (int k = 0; k < 6; ++k) {
		b.set_transform(Transform3D(Basis(Vector3(0, 0, 1), rng.range(-0.25f, 0.25f)), Vector3(0, 0.55f + k * 0.1f, 0)));
		b.box(Vector3(0, 0, 0), Vector3(0.245f, 0.022f, 0.16f), band);
	}
	b.set_transform(Transform3D());
	b.rounded_box(Vector3(0, 1.2f, 0), Vector3(0.17f, 0.16f, 0.16f), 0.05f, wrap);
	b.set_transform(Transform3D(Basis(Vector3(0, 0, 1), 0.2f), Vector3(0, 1.16f, 0)));
	b.box(Vector3(0, 0, 0), Vector3(0.18f, 0.025f, 0.17f), band);
	b.set_transform(Transform3D());
	b.blob(Vector3(0.065f, 1.24f, 0.155f), Vector3(0.045f, 0.045f, 0.02f), pal::GLOW_JACK_HOT, 0);
	b.box(Vector3(-0.065f, 1.24f, 0.16f), Vector3(0.045f, 0.01f, 0.01f), pal::INK);
	for (MeshBuilder *m : { &l, &r }) {
		m->rounded_box(Vector3(0, 0, 0.24f), Vector3(0.07f, 0.07f, 0.26f), 0.02f, wrap);
		m->box(Vector3(0, 0, 0.36f), Vector3(0.075f, 0.075f, 0.02f), band);
	}
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.29f, 0.98f, 0);
	a.pivot_b = Vector3(0.29f, 0.98f, 0);
	a.height = 1.4f;
	a.radius = 0.32f;
	// Nest: an upright painted sarcophagus with its lid ajar.
	MeshBuilder n;
	n.rounded_box(Vector3(0, 0.1f, 0), Vector3(0.75f, 0.1f, 0.5f), 0.03f, mat_cell(pal::STONE, mat::STONE));
	n.rounded_box(Vector3(0, 1.15f, -0.05f), Vector3(0.5f, 1.0f, 0.28f), 0.08f, mat_cell(pal::STONE_LIGHT, mat::STONE));
	n.box(Vector3(0, 1.15f, 0.2f), Vector3(0.36f, 0.88f, 0.05f), pal::INK);
	n.set_transform(Transform3D(Basis(Vector3(0, 1, 0), -0.55f), Vector3(0.42f, 0, 0.32f)));
	n.rounded_box(Vector3(-0.3f, 1.15f, 0), Vector3(0.42f, 0.98f, 0.07f), 0.06f, mat_cell(pal::FOIL_GOLD, mat::METAL));
	n.rounded_box(Vector3(-0.3f, 1.6f, 0.07f), Vector3(0.22f, 0.26f, 0.03f), 0.04f, mat_cell(pal::ROOF_TEAL, mat::FLAT));
	for (float sx : { -1.0f, 1.0f }) {
		n.blob(Vector3(-0.3f + sx * 0.09f, 1.66f, 0.1f), Vector3(0.04f, 0.03f, 0.02f), pal::GLOW_JACK, 0);
	}
	n.set_transform(Transform3D());
	a.nest = finish(n);
	a.nest_radius = 0.75f;
	a.nest_height = 2.2f;
}

void build_slime(MonsterArt &a) {
	MeshBuilder b;
	const int goo = mat_cell(pal::WITCH_GREEN, mat::VINYL);
	b.blob(Vector3(0, 0.34f, 0), Vector3(0.45f, 0.34f, 0.42f), goo, 2);
	b.blob(Vector3(-0.12f, 0.55f, -0.05f), Vector3(0.14f, 0.07f, 0.12f), mat_cell(pal::GOURD_GREEN, mat::VINYL), 0);
	for (int k = 0; k < 5; ++k) {
		const float ang = k * Math_TAU / 5.0f + 0.3f;
		b.blob(Vector3(std::cos(ang) * 0.4f, 0.06f, std::sin(ang) * 0.38f), Vector3(0.1f, 0.07f, 0.1f), goo, 0);
	}
	cute_eye(b, Vector3(-0.13f, 0.42f, 0.33f), 0.09f);
	cute_eye(b, Vector3(0.13f, 0.42f, 0.33f), 0.09f);
	b.box(Vector3(0, 0.26f, 0.4f), Vector3(0.09f, 0.02f, 0.02f), pal::INK);
	a.body = finish(b);
	a.height = 0.7f;
	a.radius = 0.42f;
	// Nest: a big black cauldron bubbling over with glowing goo, embers underneath.
	MeshBuilder n;
	for (int k = 0; k < 3; ++k) {
		const float ang = k * Math_TAU / 3.0f;
		n.beam(Vector3(std::cos(ang) * 0.6f, 0.0f, std::sin(ang) * 0.6f), Vector3(std::cos(ang) * 0.7f, 0.45f, std::sin(ang) * 0.7f), 0.1f, 0.1f,
				mat_cell(pal::LAMP_BLACK, mat::METAL), Vector3(1, 0, 0));
	}
	n.blob(Vector3(0, 0.15f, 0), Vector3(0.3f, 0.08f, 0.3f), pal::GLOW_JACK_HOT, 0);
	n.cylinder(Vector3(0, 0.3f, 0), 0.72f, 0.75f, 12, mat_cell(pal::LAMP_BLACK, mat::METAL), 0.9f);
	n.cylinder(Vector3(0, 1.02f, 0), 0.98f, 0.1f, 12, mat_cell(pal::IRON, mat::METAL));
	n.cylinder(Vector3(0, 0.98f, 0), 0.86f, 0.12f, 12, pal::GLOW_MAGIC_TEAL);
	Rng rng(15);
	for (int k = 0; k < 5; ++k) {
		const float ang = rng.range(0.0f, Math_TAU), rr = rng.range(0.0f, 0.6f);
		n.blob(Vector3(std::cos(ang) * rr, 1.12f, std::sin(ang) * rr), Vector3(0.12f, 0.1f, 0.12f), pal::GLOW_MAGIC_TEAL, 0);
	}
	n.blob(Vector3(0.75f, 0.7f, 0.5f), Vector3(0.12f, 0.35f, 0.08f), pal::GLOW_MAGIC_TEAL, 0);
	a.nest = finish(n);
	a.nest_radius = 0.95f;
	a.nest_height = 1.2f;
}

void build_scarecrow(MonsterArt &a) {
	MeshBuilder b, l, r;
	b.cylinder(Vector3(0, 0.0f, 0), 0.05f, 0.58f, 6, mat_cell(pal::WOOD_DARK, mat::WOOD));
	b.rounded_box(Vector3(0, 0.82f, 0), Vector3(0.21f, 0.27f, 0.13f), 0.05f, mat_cell(pal::FLANNEL_RED, mat::FABRIC));
	b.box(Vector3(0, 0.6f, 0), Vector3(0.22f, 0.03f, 0.14f), mat_cell(pal::ROPE, mat::FABRIC));
	for (int k = 0; k < 7; ++k) {
		const float x = -0.18f + k * 0.06f;
		b.tri_out(Vector3(x - 0.03f, 0.56f, 0.12f), Vector3(x + 0.03f, 0.56f, 0.12f), Vector3(x, 0.44f, 0.13f), mat_cell(pal::STRAW, mat::STRAW),
				Vector3(x, 0.5f, 0.0f));
	}
	b.blob(Vector3(0, 1.24f, 0), Vector3(0.21f, 0.21f, 0.2f), mat_cell(pal::STRAW, mat::BURLAP), 1);
	for (float sx : { -1.0f, 1.0f }) {
		b.blob(Vector3(sx * 0.08f, 1.28f, 0.18f), Vector3(0.04f, 0.04f, 0.02f), pal::GLOW_JACK, 0);
	}
	for (int k = 0; k < 5; ++k) {
		const float x = -0.1f + k * 0.05f;
		b.box(Vector3(x, 1.15f + std::abs(x) * 0.4f, 0.19f), Vector3(0.022f, 0.006f, 0.006f), pal::INK);
		b.box(Vector3(x, 1.15f + std::abs(x) * 0.4f, 0.19f), Vector3(0.004f, 0.02f, 0.006f), pal::INK);
	}
	b.cylinder(Vector3(0, 1.38f, 0), 0.32f, 0.04f, 10, mat_cell(pal::WITCH_PURPLE, mat::FELT));
	b.set_transform(Transform3D(Basis(Vector3(0, 0, 1), 0.18f), Vector3(0, 1.4f, 0)));
	b.cylinder(Vector3(0, 0, 0), 0.17f, 0.4f, 8, mat_cell(pal::WITCH_PURPLE, mat::FELT), 0.02f);
	b.set_transform(Transform3D());
	for (int side = 0; side < 2; ++side) {
		MeshBuilder &m = side == 0 ? l : r;
		const float sx = side == 0 ? -1.0f : 1.0f;
		m.beam(Vector3(0, 0, 0), Vector3(sx * 0.45f, 0, 0), 0.1f, 0.1f, mat_cell(pal::FLANNEL_RED, mat::FABRIC));
		for (int k = 0; k < 3; ++k) {
			m.tri_out(Vector3(sx * 0.45f, 0.03f, 0), Vector3(sx * 0.45f, -0.03f, 0), Vector3(sx * 0.6f, (k - 1) * 0.08f, (k - 1) * 0.03f),
					mat_cell(pal::STRAW, mat::STRAW), Vector3(sx * 0.5f, 0, 1.0f));
		}
	}
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.2f, 0.98f, 0);
	a.pivot_b = Vector3(0.2f, 0.98f, 0);
	a.height = 1.6f;
	a.radius = 0.35f;
	// Nest: a tall post with a crossbar of glowing-eyed crows and a tattered flag.
	MeshBuilder n;
	n.blob(Vector3(0, 0.08f, 0), Vector3(0.55f, 0.18f, 0.5f), mat_cell(pal::STONE, mat::STONE), 1, 0.1f, 17);
	n.cylinder(Vector3(0, 0.1f, 0), 0.1f, 3.0f, 6, mat_cell(pal::WOOD_DARK, mat::WOOD));
	n.beam(Vector3(-1.0f, 2.6f, 0), Vector3(1.0f, 2.6f, 0), 0.12f, 0.12f, mat_cell(pal::WOOD_DARK, mat::WOOD));
	for (int k = 0; k < 3; ++k) {
		const float x = -0.75f + k * 0.75f;
		n.blob(Vector3(x, 2.84f, 0), Vector3(0.15f, 0.17f, 0.13f), mat_cell(pal::CAT_BLACK, mat::FELT), 1);
		n.blob(Vector3(x, 3.03f, 0.04f), Vector3(0.1f, 0.1f, 0.1f), mat_cell(pal::CAT_BLACK, mat::FELT), 1);
		n.tri_out(Vector3(x - 0.03f, 3.03f, 0.12f), Vector3(x + 0.03f, 3.03f, 0.12f), Vector3(x, 3.0f, 0.24f), pal::CANDY_CORN_YELLOW, Vector3(x, 2.9f, 0.1f));
		n.blob(Vector3(x + 0.05f, 3.06f, 0.1f), Vector3(0.02f, 0.02f, 0.01f), pal::GLOW_JACK_HOT, 0);
	}
	n.quad_out(Vector3(0.05f, 3.05f, 0), Vector3(0.65f, 2.95f, 0.05f), Vector3(0.6f, 3.35f, 0.02f), Vector3(0.05f, 3.38f, 0), mat_cell(pal::FLANNEL_RED, mat::FABRIC),
			Vector3(0.3f, 3.2f, -1.0f));
	n.quad_out(Vector3(0.05f, 3.05f, 0), Vector3(0.65f, 2.95f, 0.05f), Vector3(0.6f, 3.35f, 0.02f), Vector3(0.05f, 3.38f, 0), mat_cell(pal::FLANNEL_RED, mat::FABRIC),
			Vector3(0.3f, 3.2f, 1.0f));
	a.nest = finish(n);
	a.nest_radius = 0.5f;
	a.nest_height = 3.4f;
}

void build_spider(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int black = mat_cell(pal::CAT_BLACK, mat::FELT);
	b.blob(Vector3(0, 0.42f, -0.22f), Vector3(0.33f, 0.28f, 0.34f), black, 1);
	b.blob(Vector3(0, 0.62f, -0.26f), Vector3(0.12f, 0.06f, 0.14f), mat_cell(pal::WITCH_PURPLE, mat::FELT), 0);
	b.blob(Vector3(0, 0.36f, 0.14f), Vector3(0.2f, 0.17f, 0.18f), black, 1);
	const Vector3 eyes[4] = { Vector3(-0.07f, 0.42f, 0.29f), Vector3(0.07f, 0.42f, 0.29f), Vector3(-0.13f, 0.38f, 0.26f), Vector3(0.13f, 0.38f, 0.26f) };
	for (int k = 0; k < 4; ++k) {
		b.blob(eyes[k], Vector3(k < 2 ? 0.045f : 0.03f, k < 2 ? 0.045f : 0.03f, 0.02f), pal::GLOW_JACK_HOT, 0);
	}
	for (float sx : { -1.0f, 1.0f }) {
		b.tri_out(Vector3(sx * 0.05f, 0.27f, 0.28f), Vector3(sx * 0.02f, 0.27f, 0.29f), Vector3(sx * 0.035f, 0.18f, 0.27f), pal::BONE,
				Vector3(sx * 0.035f, 0.24f, 0.0f));
	}
	for (int side = 0; side < 2; ++side) {
		MeshBuilder &m = side == 0 ? l : r;
		const float sx = side == 0 ? -1.0f : 1.0f;
		for (float z : { 0.2f, 0.07f, -0.07f, -0.2f }) {
			const Vector3 knee(sx * 0.35f, 0.17f, z * 1.6f), foot(sx * 0.55f, -0.33f, z * 2.0f);
			m.beam(Vector3(0, 0, z * 0.5f), knee, 0.05f, 0.05f, black);
			m.beam(knee, foot, 0.04f, 0.04f, black);
		}
	}
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.12f, 0.35f, 0);
	a.pivot_b = Vector3(0.12f, 0.35f, 0);
	a.height = 0.7f;
	a.radius = 0.45f;
	// Nest: a dome of web over a dark hole, egg sacs tucked inside.
	MeshBuilder n;
	const int silk = mat_cell(pal::GHOST_WHITE, mat::FLAT);
	n.blob(Vector3(0, 0.05f, 0), Vector3(0.75f, 0.12f, 0.75f), pal::INK, 1);
	const Vector3 top(0, 1.45f, 0);
	for (int k = 0; k < 8; ++k) {
		const float ang = k * Math_TAU / 8.0f;
		const Vector3 base(std::cos(ang) * 1.15f, 0.0f, std::sin(ang) * 1.15f);
		const Vector3 mid(std::cos(ang) * 0.95f, 0.85f, std::sin(ang) * 0.95f);
		n.beam(base, mid, 0.03f, 0.03f, silk);
		n.beam(mid, top, 0.03f, 0.03f, silk);
		for (float h : { 0.35f, 0.85f, 1.2f }) {
			const float rr = h < 0.5f ? 1.1f : (h < 1.0f ? 0.95f : 0.5f);
			const float a2 = ang + Math_TAU / 8.0f;
			n.beam(Vector3(std::cos(ang) * rr, h, std::sin(ang) * rr), Vector3(std::cos(a2) * rr, h, std::sin(a2) * rr), 0.02f, 0.02f, silk);
		}
	}
	for (int k = 0; k < 3; ++k) {
		n.blob(Vector3((k - 1) * 0.3f, 0.25f, 0.1f * k), Vector3(0.14f, 0.18f, 0.14f), silk, 1);
	}
	a.nest = finish(n);
	a.nest_radius = 1.1f;
	a.nest_height = 1.5f;
}

void build_toadstool(MonsterArt &a) {
	MeshBuilder b, l, r;
	const int stem = mat_cell(pal::BONE, mat::FLAT);
	b.cylinder(Vector3(0, 0.08f, 0), 0.2f, 0.52f, 8, stem, 0.17f);
	for (float sx : { -1.0f, 1.0f }) {
		b.blob(Vector3(sx * 0.1f, 0.06f, 0.06f), Vector3(0.09f, 0.06f, 0.12f), stem, 0);
		b.blob(Vector3(sx * 0.07f, 0.42f, 0.17f), Vector3(0.03f, 0.04f, 0.02f), pal::INK, 1);
		b.blob(Vector3(sx * 0.12f, 0.35f, 0.16f), Vector3(0.035f, 0.02f, 0.01f), pal::CHEEK_PINK, 0);
	}
	b.box(Vector3(0, 0.31f, 0.185f), Vector3(0.04f, 0.012f, 0.01f), pal::INK);
	b.cylinder(Vector3(0, 0.58f, 0), 0.42f, 0.06f, 12, mat_cell(pal::CREAM, mat::FLAT));
	b.blob(Vector3(0, 0.72f, 0), Vector3(0.46f, 0.25f, 0.46f), mat_cell(pal::METAL_RED, mat::VINYL), 1);
	Rng rng(19);
	for (int k = 0; k < 7; ++k) {
		const float ang = k * Math_TAU / 7.0f + rng.range(-0.2f, 0.2f), rr = rng.range(0.18f, 0.34f);
		b.blob(Vector3(std::cos(ang) * rr, 0.84f + (0.34f - rr) * 0.4f, std::sin(ang) * rr), Vector3(0.07f, 0.035f, 0.07f), pal::LOLLY_WHITE, 0);
	}
	l.beam(Vector3(0, 0, 0), Vector3(-0.17f, -0.12f, 0.06f), 0.05f, 0.05f, stem);
	r.beam(Vector3(0, 0, 0), Vector3(0.17f, -0.12f, 0.06f), 0.05f, 0.05f, stem);
	a.body = finish(b);
	a.part_a = finish(l);
	a.part_b = finish(r);
	a.pivot_a = Vector3(-0.17f, 0.4f, 0);
	a.pivot_b = Vector3(0.17f, 0.4f, 0);
	a.height = 1.0f;
	a.radius = 0.36f;
	// Nest: a fairy ring of poisonous purple toadstools around a glowing hollow.
	MeshBuilder n;
	n.cylinder(Vector3(0, 0.0f, 0), 0.75f, 0.04f, 14, pal::GLOW_PURPLE);
	for (int k = 0; k < 8; ++k) {
		const float ang = k * Math_TAU / 8.0f;
		const float h = 0.35f + 0.25f * ((k * 5) % 3);
		const Vector3 p(std::cos(ang) * 1.15f, 0, std::sin(ang) * 1.15f);
		n.cylinder(p, 0.08f, h, 6, stem, 0.06f);
		n.blob(p + Vector3(0, h, 0), Vector3(0.26f, 0.14f, 0.26f), mat_cell(pal::WITCH_PURPLE, mat::VINYL), 1);
		n.blob(p + Vector3(0.08f, h + 0.1f, 0.05f), Vector3(0.05f, 0.03f, 0.05f), pal::GLOW_PURPLE, 0);
	}
	n.cylinder(Vector3(0, 0, 0), 0.16f, 0.9f, 8, stem, 0.13f);
	n.blob(Vector3(0, 0.95f, 0), Vector3(0.45f, 0.24f, 0.45f), mat_cell(pal::WITCH_PURPLE, mat::VINYL), 1);
	for (int k = 0; k < 5; ++k) {
		const float ang = k * Math_TAU / 5.0f;
		n.blob(Vector3(std::cos(ang) * 0.25f, 1.08f, std::sin(ang) * 0.25f), Vector3(0.06f, 0.03f, 0.06f), pal::GLOW_PURPLE, 0);
	}
	a.nest = finish(n);
	a.nest_radius = 0.6f;
	a.nest_height = 1.2f;
}

// Thorn vines: thick tendrils corkscrewing up out of the ground and flopping over, studded with
// pale thorns and a few glowing purple buds.
Ref<ArrayMesh> build_vine(uint32_t seed) {
	MeshBuilder m;
	Rng rng(seed);
	const int stems[2] = { mat_cell(pal::VINE_DARK, mat::BARK), mat_cell(pal::VINE, mat::BARK) };
	const int thorn_cell = mat_cell(pal::BONE, mat::FLAT);
	const int tendrils = rng.irange(6, 8);
	for (int t = 0; t < tendrils; ++t) {
		const float ang = t * Math_TAU / tendrils + rng.range(-0.3f, 0.3f);
		Vector3 p(std::cos(ang) * rng.range(0.3f, 1.1f), -0.1f, std::sin(ang) * rng.range(0.3f, 1.1f));
		float w = rng.range(0.2f, 0.28f);
		const float peak = rng.range(1.6f, 2.6f);
		float spin = ang;
		const int segs = 7;
		for (int s = 0; s < segs; ++s) {
			const float f = float(s + 1) / segs;
			spin += rng.range(0.5f, 1.0f);
			const float y = f < 0.65f ? peak * std::sin(f / 0.65f * Math_PI * 0.5f) : peak * (1.0f - (f - 0.65f) * 1.6f);
			const Vector3 q(p.x * 0.85f + std::cos(spin) * 0.35f, Math::max(y, 0.1f), p.z * 0.85f + std::sin(spin) * 0.35f);
			m.beam(p, q, w, w, stems[s & 1], Vector3(0, 1, 0));
			const Vector3 dir = (q - p).normalized();
			const Vector3 out = Vector3(std::cos(spin), 0.3f, std::sin(spin)).normalized();
			for (int k = 0; k < 2; ++k) {
				const Vector3 base = p.lerp(q, 0.3f + 0.4f * k);
				const Vector3 tdir = (out + dir * (k ? 0.3f : -0.2f) + Vector3(rng.range(-0.4f, 0.4f), rng.range(-0.2f, 0.4f), rng.range(-0.4f, 0.4f))).normalized();
				thorn(m, base + tdir * w * 0.4f, tdir, rng.range(0.6f, 0.9f), 0.15f, thorn_cell);
			}
			p = q;
			w *= 0.86f;
		}
		if (rng.chance(0.3f)) {
			m.blob(p, Vector3(0.16f, 0.14f, 0.16f), pal::GLOW_PURPLE, 1);
		}
	}
	// Roots knotting into the ground.
	for (int k = 0; k < 5; ++k) {
		const float ang = rng.range(0.0f, Math_TAU);
		m.beam(Vector3(0, 0.1f, 0), Vector3(std::cos(ang) * 1.8f, -0.05f, std::sin(ang) * 1.8f), 0.24f, 0.18f, stems[0], Vector3(0, 1, 0));
	}
	return m.build();
}

} // namespace

// A creeping thorn vine along a ground path (points relative to the first): two round strands
// twisting around each other and tapering toward the tip, bristling with bone-white spines, with
// pairs of small leaves, the odd curling side shoot and a few glowing purple buds.
Ref<ArrayMesh> build_tendril_mesh(const std::vector<Vector3> &pts, float width, uint32_t seed) {
	MeshBuilder m;
	Rng rng(seed);
	if (pts.size() < 2) {
		return Ref<ArrayMesh>();
	}
	const int stems[2] = { mat_cell(pal::VINE_DARK, mat::BARK), mat_cell(pal::VINE, mat::BARK) };
	const int thorn_cell = mat_cell(pal::BONE, mat::FLAT);
	const int leaf_cell = mat_cell(pal::VINE, mat::LEAVES);
	float total = 0.0f;
	for (size_t i = 1; i < pts.size(); ++i) {
		total += pts[i].distance_to(pts[i - 1]);
	}
	// Resample the path every 0.35 m so the twist reads as a smooth curve.
	std::vector<Vector3> path;
	std::vector<float> runs;
	{
		float run = 0.0f;
		for (size_t i = 0; i + 1 < pts.size(); ++i) {
			const float seg = pts[i].distance_to(pts[i + 1]);
			const int n = std::max(1, int(seg / 0.35f));
			for (int k = 0; k < n; ++k) {
				path.push_back(pts[i].lerp(pts[i + 1], float(k) / n));
				runs.push_back(run + seg * float(k) / n);
			}
			run += seg;
		}
		path.push_back(pts.back());
		runs.push_back(total);
	}
	const float phase = rng.range(0.0f, Math_TAU);
	auto width_at = [&](float r) { return width * (0.3f + 0.7f * (1.0f - r / total)); };
	auto side_at = [&](size_t i) {
		const Vector3 d = path[std::min(i + 1, path.size() - 1)] - path[i > 0 ? i - 1 : 0];
		return Vector3(-d.z, 0, d.x).normalized();
	};
	auto strand = [&](size_t i, int s) {
		const float w = width_at(runs[i]);
		const float ang = runs[i] * 2.0f + phase + (s ? Math_PI : 0.0f);
		return path[i] + side_at(i) * (std::cos(ang) * w * 0.32f) + Vector3(0, w * 0.5f + std::sin(ang) * w * 0.26f, 0);
	};
	for (size_t i = 0; i + 1 < path.size(); ++i) {
		for (int s = 0; s < 2; ++s) {
			const float w0 = width_at(runs[i]) * 0.3f, w1 = width_at(runs[i + 1]) * 0.3f;
			tube(m, strand(i, s), strand(i + 1, s), w0, w1, stems[s]);
		}
	}
	// Spines, leaves, shoots and buds along the run.
	float next_thorn = 0.15f, next_leaf = rng.range(0.6f, 1.2f), next_shoot = rng.range(2.0f, 4.0f);
	for (size_t i = 0; i + 1 < path.size(); ++i) {
		const float r = runs[i];
		const float w = width_at(r);
		const Vector3 dir = (path[i + 1] - path[i]).normalized();
		const Vector3 side = side_at(i);
		if (r >= next_thorn) {
			next_thorn += rng.range(0.28f, 0.42f);
			const float sd = rng.chance(0.5f) ? 1.0f : -1.0f;
			const Vector3 tdir = (side * sd * rng.range(0.6f, 1.0f) + Vector3(0, rng.range(0.5f, 1.1f), 0) + dir * rng.range(-0.3f, 0.3f)).normalized();
			const float len = (0.42f + 0.45f * w / width) * rng.range(0.8f, 1.15f);
			thorn(m, path[i] + Vector3(0, w * 0.55f, 0) + tdir * w * 0.25f, tdir, len, 0.06f + 0.08f * w / width, thorn_cell);
		}
		if (r >= next_leaf) {
			next_leaf += rng.range(0.9f, 1.6f);
			for (float sd : { -1.0f, 1.0f }) {
				const Vector3 base = path[i] + side * sd * w * 0.3f + Vector3(0, w * 0.45f, 0);
				leaf(m, base, (side * sd + dir * 0.4f).normalized(), dir, rng.range(0.28f, 0.4f), leaf_cell);
			}
		}
		if (r >= next_shoot && r < total - 1.0f) {
			// A curling side shoot ending in a bud.
			next_shoot += rng.range(2.5f, 4.5f);
			const float sd = rng.chance(0.5f) ? 1.0f : -1.0f;
			Vector3 p = path[i] + Vector3(0, w * 0.5f, 0);
			Vector3 d = (side * sd + Vector3(0, 0.5f, 0)).normalized();
			float sw = w * 0.22f;
			for (int k = 0; k < 4; ++k) {
				d = d.rotated(Vector3(0, 1, 0), sd * 0.5f).normalized();
				d.y -= 0.18f;
				const Vector3 q = p + d.normalized() * 0.32f;
				tube(m, p, q, sw, sw * 0.75f, stems[1]);
				if (k == 1) {
					thorn(m, q, (d + Vector3(0, 0.8f, 0)).normalized(), 0.3f, 0.05f, thorn_cell);
				}
				p = q;
				sw *= 0.75f;
			}
			m.blob(p, Vector3(0.11f, 0.1f, 0.11f), rng.chance(0.6f) ? pal::GLOW_PURPLE : leaf_cell, 1);
		}
		if (rng.chance(0.03f)) {
			m.blob(path[i] + side * w * 0.45f * (rng.chance(0.5f) ? 1.0f : -1.0f) + Vector3(0, w * 0.7f, 0), Vector3(0.13f, 0.11f, 0.13f), pal::GLOW_PURPLE, 1);
		}
	}
	return m.build();
}
void MonsterArtKit::build() {
	build_pumpkin(art[int(MonsterType::PumpkinHopper)]);
	build_bat(art[int(MonsterType::Bat)]);
	build_skeleton(art[int(MonsterType::Skeleton)]);
	build_ghost(art[int(MonsterType::Ghost)]);
	build_gremlin(art[int(MonsterType::Gremlin)]);
	build_mummy(art[int(MonsterType::Mummy)]);
	build_slime(art[int(MonsterType::Slime)]);
	build_scarecrow(art[int(MonsterType::Scarecrow)]);
	build_spider(art[int(MonsterType::Spider)]);
	build_toadstool(art[int(MonsterType::Toadstool)]);
	for (int v = 0; v < 3; ++v) {
		vine[v] = build_vine(101 + v * 7);
	}
	// Travel gate: a runed stone pad and a tall post carrying a big lantern.
	{
		MeshBuilder m;
		m.cylinder(Vector3(0, 0, 0), 1.35f, 0.22f, 12, mat_cell(pal::STONE_LIGHT, mat::STONE), -1.0f, mat_cell(pal::STONE_LIGHT, mat::PAVERS));
		m.cylinder(Vector3(0, 0, 0), 1.45f, 0.1f, 12, mat_cell(pal::STONE, mat::STONE));
		m.cylinder(Vector3(-1.1f, 0.2f, 0), 0.12f, 3.2f, 8, mat_cell(pal::WOOD_DARK, mat::WOOD));
		m.beam(Vector3(-1.1f, 3.25f, 0), Vector3(-0.15f, 3.25f, 0), 0.1f, 0.1f, mat_cell(pal::WOOD_DARK, mat::WOOD));
		m.beam(Vector3(-1.1f, 2.7f, 0), Vector3(-0.6f, 3.2f, 0), 0.07f, 0.07f, mat_cell(pal::WOOD_DARK, mat::WOOD));
		gate_base = m.build();
	}
	{
		MeshBuilder m;
		m.cylinder(Vector3(0, 0.225f, 0), 1.0f, 0.02f, 16, pal::GLOW_MAGIC_TEAL);
		m.cylinder(Vector3(0, 0.226f, 0), 0.82f, 0.022f, 16, mat_cell(pal::STONE_LIGHT, mat::PAVERS));
		for (int k = 0; k < 8; ++k) {
			const float ang = k * Math_TAU / 8.0f;
			m.box(Vector3(std::cos(ang) * 1.17f, 0.225f, std::sin(ang) * 1.17f), Vector3(0.07f, 0.012f, 0.07f), pal::GLOW_MAGIC_TEAL);
		}
		m.cylinder(Vector3(-0.15f, 2.85f, 0), 0.05f, 0.4f, 5, mat_cell(pal::IRON, mat::METAL));
		m.rounded_box(Vector3(-0.15f, 2.55f, 0), Vector3(0.28f, 0.34f, 0.28f), 0.03f, mat_cell(pal::IRON, mat::METAL));
		m.blob(Vector3(-0.15f, 2.55f, 0), Vector3(0.25f, 0.31f, 0.25f), pal::GLOW_MAGIC_TEAL, 1);
		m.cylinder(Vector3(-0.15f, 2.89f, 0), 0.34f, 0.25f, 4, mat_cell(pal::IRON, mat::METAL), 0.04f);
		gate_lantern = m.build();
	}
	// Candy shop cart: a wooden cart with a striped awning, candy jars, a hanging lantern and a
	// doormat in front (step on it to shop). Front faces +Z.
	{
		MeshBuilder m;
		const int wood = mat_cell(pal::WOOD, mat::PLANKS), dark = mat_cell(pal::WOOD_DARK, mat::WOOD);
		m.rounded_box(Vector3(0, 0.75f, 0), Vector3(0.95f, 0.38f, 0.55f), 0.05f, wood, mat_cell(pal::CREAM, mat::WOOD));
		m.rounded_box(Vector3(0, 1.16f, 0.05f), Vector3(1.02f, 0.05f, 0.62f), 0.02f, mat_cell(pal::CREAM, mat::WOOD));
		for (float sx : { -1.0f, 1.0f }) {
			m.set_transform(Transform3D(Basis(Vector3(0, 0, 1), Math_PI * 0.5f), Vector3(sx * 1.0f, 0.42f, 0)));
			m.cylinder(Vector3(0, -0.06f, 0), 0.42f, 0.12f, 12, mat_cell(pal::WOOD_DARK, mat::WOOD));
			m.cylinder(Vector3(0, -0.07f, 0), 0.12f, 0.14f, 8, mat_cell(pal::METAL_RED, mat::METAL));
			m.set_transform(Transform3D());
			for (float sz : { -1.0f, 1.0f }) {
				m.beam(Vector3(sx * 0.9f, 1.15f, sz * 0.5f), Vector3(sx * 0.9f, 2.25f, sz * 0.5f), 0.07f, 0.07f, dark);
			}
		}
		const int awning[2] = { mat_cell(pal::CANDY_PINK, mat::FABRIC), mat_cell(pal::TRIM_WHITE, mat::FABRIC) };
		m.set_transform(Transform3D(Basis(Vector3(1, 0, 0), 0.2f), Vector3(0, 2.3f, 0.1f)));
		for (int i = 0; i < 8; ++i) {
			const float a = -1.15f + i * 2.3f / 8.0f, b = a + 2.3f / 8.0f;
			m.box(Vector3((a + b) * 0.5f, 0, 0), Vector3((b - a) * 0.5f, 0.05f, 0.82f), awning[i & 1]);
			m.tri_out(Vector3(a, -0.05f, 0.83f), Vector3(b, -0.05f, 0.83f), Vector3((a + b) * 0.5f, -0.25f, 0.83f), awning[i & 1], Vector3((a + b) * 0.5f, 0, 0));
		}
		m.set_transform(Transform3D());
		m.rounded_box(Vector3(0, 2.62f, 0.62f), Vector3(0.8f, 0.2f, 0.05f), 0.03f, mat_cell(pal::WITCH_PURPLE, mat::WOOD));
		const int jar_candy[4] = { pal::CANDY_PINK, pal::CANDY_MINT, pal::CANDY_LEMON, pal::CANDY_GRAPE };
		for (int i = 0; i < 4; ++i) {
			const Vector3 jp(-0.66f + i * 0.44f, 1.21f, 0.15f);
			m.cylinder(jp, 0.15f, 0.36f, 8, mat_cell(pal::WATER_BLUE, mat::FLAT));
			for (int k = 0; k < 3; ++k) {
				m.blob(jp + Vector3((k - 1) * 0.05f, 0.1f + k * 0.08f, 0.1f), Vector3(0.06f, 0.05f, 0.05f), jar_candy[(i + k) % 4], 0);
			}
			m.cylinder(jp + Vector3(0, 0.36f, 0), 0.12f, 0.05f, 8, mat_cell(pal::METAL_RED, mat::METAL));
		}
		m.blob(Vector3(0.75f, 1.32f, -0.2f), Vector3(0.22f, 0.18f, 0.22f), mat_cell(pal::PUMPKIN, mat::PUMPKIN), 1);
		m.cylinder(Vector3(-0.85f, 2.05f, 0.55f), 0.02f, 0.18f, 4, mat_cell(pal::IRON, mat::METAL));
		m.blob(Vector3(-0.85f, 1.92f, 0.55f), Vector3(0.11f, 0.13f, 0.11f), pal::GLOW_LANTERN, 1);
		m.cylinder(Vector3(0, 0.0f, 1.25f), 0.75f, 0.04f, 14, mat_cell(pal::PUMPKIN_DARK, mat::BURLAP));
		m.cylinder(Vector3(0, 0.0f, 1.25f), 0.6f, 0.045f, 14, pal::GLOW_LANTERN);
		m.cylinder(Vector3(0, 0.0f, 1.25f), 0.5f, 0.05f, 14, mat_cell(pal::PUMPKIN_DARK, mat::BURLAP));
		shop_cart = m.build();
	}
}

void MonsterArtKit::apply_assets(AssetLibrary &lib) {
	static const char *kinds[] = { "pumpkin", "bat", "skeleton", "ghost", "gremlin", "mummy", "slime", "scarecrow", "spider", "toadstool" };
	for (int i = 0; i < int(MonsterType::Count); ++i) {
		MonsterArt &a = art[i];
		// The bouncing pumpkin keeps its original procedural look (the user preferred it).
		const std::string mid = i == int(MonsterType::PumpkinHopper) ? std::string() : std::string("monster_") + kinds[i];
		Vector3 origin;
		const Ref<ArrayMesh> body = mid.empty() ? Ref<Mesh>() : lib.part_mesh(mid, "body", &origin);
		if (body.is_valid()) {
			a.body = body;
			const Ref<ArrayMesh> pa = lib.part_mesh(mid, "part_a", &a.pivot_a);
			const Ref<ArrayMesh> pb = lib.part_mesh(mid, "part_b", &a.pivot_b);
			a.part_a = pa;
			a.part_b = pb;
		}
		const std::string nid = std::string("nest_") + kinds[i];
		if (lib.get(nid)->published) {
			const Ref<ArrayMesh> nest = lib.merged_mesh(nid);
			if (nest.is_valid()) {
				a.nest = nest;
			}
		}
	}
	// Thorn knots and the thorn nest's root mass with its heart.
	const char *knots[3] = { "thorn_knot_a", "thorn_knot_b", "thorn_knot_c" };
	for (int k = 0; k < 3; ++k) {
		if (lib.get(knots[k])->published) {
			const Ref<ArrayMesh> m = lib.merged_mesh(knots[k]);
			if (m.is_valid()) {
				vine[k] = m;
			}
		}
	}
	if (lib.get("thorn_root_mass")->published) {
		Vector3 o; // the roots' origin is the ground centre
		root_mass = lib.part_mesh("thorn_root_mass", "roots", &o);
		root_heart = lib.part_mesh("thorn_root_mass", "heart", &root_heart_origin);
	}
	// The travel gate (pad + post, and its lantern in the same space) and the candy shop cart.
	struct Swap {
		const char *id;
		Ref<ArrayMesh> *mesh;
	};
	for (const Swap &s : { Swap{ "travel_gate_base", &gate_base }, Swap{ "travel_gate_lantern", &gate_lantern }, Swap{ "shop_cart", &shop_cart } }) {
		if (lib.get(s.id)->published) {
			const Ref<ArrayMesh> m = lib.merged_mesh(s.id);
			if (m.is_valid()) {
				*s.mesh = m;
			}
		}
	}
}
} // namespace tot
