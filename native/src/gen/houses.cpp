// Stage 4: house assembly. Parametric textured low-poly houses: lap siding, stucco or brick walls,
// shingle or fish-scale roofs, porches with lattice skirts and turned posts, shutters, flower boxes,
// glowing curtained windows, gutters, dormers, chimneys with pots, garages and swinging doors.
// Also plans each lot's guided-not-linear routes.

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

#include "gen/context.h"

using namespace godot;

namespace tot {

namespace {

const int WALLS[] = { pal::HOUSE_CREAM, pal::HOUSE_SAGE, pal::HOUSE_TEAL, pal::HOUSE_BUTTER, pal::HOUSE_PLUM,
	pal::HOUSE_BARNRED, pal::HOUSE_LAVENDER, pal::HOUSE_MINT, pal::HOUSE_PEACH, pal::HOUSE_SKY };
// Roofs that sit well on each wall colour (same order as WALLS).
const int ROOF_FOR[][3] = {
	{ pal::ROOF_RED, pal::ROOF_SLATE, pal::ROOF_MOSS }, { pal::ROOF_PLUM, pal::ROOF_BROWN, pal::ROOF_SLATE },
	{ pal::ROOF_RED, pal::ROOF_BROWN, pal::ROOF_SLATE }, { pal::ROOF_TEAL, pal::ROOF_PLUM, pal::ROOF_SLATE },
	{ pal::ROOF_SLATE, pal::ROOF_MOSS, pal::ROOF_TEAL }, { pal::ROOF_SLATE, pal::ROOF_BROWN, pal::ROOF_MOSS },
	{ pal::ROOF_PLUM, pal::ROOF_SLATE, pal::ROOF_TEAL }, { pal::ROOF_RED, pal::ROOF_PLUM, pal::ROOF_SLATE },
	{ pal::ROOF_TEAL, pal::ROOF_SLATE, pal::ROOF_PLUM }, { pal::ROOF_RED, pal::ROOF_SLATE, pal::ROOF_BROWN },
};
const int DOORS[] = { pal::METAL_RED, pal::ROOF_TEAL, pal::ROOF_PLUM, pal::WOOD, pal::FLANNEL_RED, pal::METAL_GREEN,
	pal::WITCH_PURPLE, pal::WOOD_DARK };
const int SHUTTERS[] = { pal::ROOF_TEAL, pal::ROOF_PLUM, pal::METAL_GREEN, pal::ROOF_SLATE, pal::FLANNEL_RED, pal::WOOD_DARK, pal::QUILT_BLUE };
const int CURTAINS[] = { pal::GLOW_WINDOW_WARM, pal::GLOW_LANTERN_PINK, pal::GLOW_LANTERN_RED, pal::GLOW_PURPLE, pal::GLOW_WINDOW };
const int FLOWERS[] = { pal::MAPLE_ORANGE, pal::CANDY_PINK, pal::GOLDEN_LEAF, pal::CANDY_GRAPE, pal::GUMBALL_RED, pal::LOLLY_WHITE };

constexpr float GARAGE_W = 3.8f;
constexpr float GARAGE_D = 6.0f;
constexpr float GARAGE_H = 2.8f;
constexpr float PORCH_ROOF_GAP = 2.1f;

float eave_h(const HouseSpec &h) {
	return h.storeys * h.storey_h;
}
float rise(const HouseSpec &h) {
	const float span = (h.ridge_along_x ? h.depth : h.width) * 0.5f;
	return std::tan(Math::deg_to_rad(h.pitch_deg)) * span;
}
float porch_roof_top(const HouseSpec &h) {
	return h.deck_h + PORCH_ROOF_GAP + 0.2f;
}

} // namespace

// ---------------------------------------------------------------- plan
void plan_house(GenContext &ctx, Lot &lot) {
	Rng rng = ctx.rng(STAGE_HOUSE, lot.index);
	HouseSpec &h = lot.house;
	const float fence = lot.width * 0.5f - 0.3f;
	h.storeys = rng.chance(0.45f) ? 2 : 1;
	if (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::Minimalists) {
		h.storeys = rng.chance(0.7f) ? 1 : 2;
	}
	// Bungalows are taller per storey so the porch roof tucks under the eave.
	h.storey_h = h.storeys == 1 ? 3.4f : 3.0f;
	h.depth = rng.range(7.0f, 9.0f);
	h.pitch_deg = rng.range(48.0f, 58.0f);
	h.ridge_along_x = rng.chance(0.65f);
	h.front_z = -rng.range(7.0f, 8.6f);
	h.roof_steps = lot.landmark != Landmark::None && lot.landmark != Landmark::TreeHouse && lot.landmark != Landmark::HayRide;
	if (h.roof_steps) {
		h.ridge_along_x = true;
	}
	h.garage = lot.width >= 16.5f && rng.chance(0.55f) && lot.landmark != Landmark::PumpkinPatch;
	h.garage_side = rng.chance(0.5f) ? 1 : -1;
	if (h.garage) {
		const float g = float(h.garage_side);
		h.width = Math::min(rng.range(7.5f, 10.0f), 2.0f * fence - 3.6f - GARAGE_W);
		// house_x range so the passage side keeps 2.0 m and the garage side 1.6 m.
		float lo, hi;
		if (g > 0) {
			lo = -fence + 2.0f + h.width * 0.5f;
			hi = fence - 1.6f - GARAGE_W - h.width * 0.5f;
		} else {
			lo = -fence + 1.6f + GARAGE_W + h.width * 0.5f;
			hi = fence - 2.0f - h.width * 0.5f;
		}
		h.house_x = hi > lo ? rng.range(lo, hi) : (lo + hi) * 0.5f;
		lot.drive_x = h.house_x + g * (h.width * 0.5f + GARAGE_W * 0.5f);
		lot.passage_side = -h.garage_side;
	} else {
		const int d = rng.chance(0.5f) ? 1 : -1;
		h.width = Math::min(rng.range(7.5f, 10.5f), 2.0f * fence - 5.6f);
		const float slack = Math::max(0.0f, 2.0f * fence - 5.6f - h.width);
		h.house_x = -float(d) * 0.8f + rng.range(-slack * 0.5f, slack * 0.5f);
		lot.drive_x = float(d) * (fence - 1.8f);
		lot.passage_side = -d;
	}
	h.porch_w = rng.range(3.6f, Math::min(h.width - 0.6f, 6.0f));
	const int porch_pos = rng.irange(-1, 1);
	h.porch_x = h.house_x + float(porch_pos) * (h.width * 0.5f - h.porch_w * 0.5f - 0.2f);
	h.porch_d = rng.range(2.3f, 2.8f);
	h.door_x = h.porch_x + (h.porch_w > 4.5f ? rng.range(-0.4f, 0.4f) : 0.0f);
	h.chimney = h.roof_steps || rng.chance(0.7f);
	h.chimney_side = rng.chance(0.5f) ? 1 : -1;

	// Colours: avoid matching the neighbour on the same side.
	int wall = rng.irange(0, 9);
	for (const Lot &o : ctx.lots) {
		if (o.index < lot.index && o.side == lot.side && lot.index - o.index == 1 && o.house.wall_cell == WALLS[wall]) {
			wall = (wall + 3) % 10;
		}
	}
	h.wall_cell = WALLS[wall];
	h.roof_cell = ROOF_FOR[wall][rng.irange(0, 2)];
	h.trim_cell = rng.chance(0.85f) ? pal::TRIM_WHITE : pal::CREAM;
	h.door_cell = DOORS[rng.irange(0, 7)];
	// Style: lap siding most often, some stucco cottages and brick houses; Victorian two-tone uppers.
	const float style = rng.f();
	if (style < 0.14f) {
		const int bricks[] = { pal::BRICK, pal::BRICK_DARK, pal::TERRACOTTA };
		h.wall_cell = bricks[rng.irange(0, 2)];
		h.wall_mat = mat::BRICK;
		h.roof_cell = rng.chance(0.5f) ? pal::ROOF_SLATE : pal::ROOF_BROWN;
	} else if (style < 0.28f) {
		h.wall_mat = mat::STUCCO;
	} else {
		h.wall_mat = mat::SIDING;
	}
	h.upper_mat = h.wall_mat;
	h.upper_cell = h.wall_cell;
	if (h.storeys == 2 && h.wall_mat == mat::SIDING && rng.chance(0.4f)) {
		h.upper_mat = mat::SCALLOP;
		h.upper_cell = WALLS[(wall + 5) % 10];
		h.gingerbread = true;
	}
	h.roof_mat = rng.chance(0.25f) ? mat::SCALLOP : mat::SHINGLES;
	h.garage_cell = h.wall_cell;
	h.shutters = rng.chance(0.55f);
	h.shutter_cell = SHUTTERS[rng.irange(0, 6)];
	h.flower_boxes = (lot.persona == Persona::GreenThumbs || lot.persona == Persona::CozyGrandparents || lot.persona == Persona::CraftyArtists ||
							 lot.persona == Persona::Bakers)
			? rng.chance(0.85f)
			: rng.chance(0.2f);
	h.dormer = h.storeys == 2 && h.ridge_along_x && !h.roof_steps && rng.chance(0.55f);
	h.gingerbread = h.gingerbread || rng.chance(0.3f);
	h.pediment = rng.chance(0.5f);
	h.curtain_cell = CURTAINS[rng.irange(0, 4)];
	h.drive_mat = rng.chance(0.5f) ? mat::PAVERS : mat::CONCRETE;

	// Fences: tidy personas like pickets; neighbours share a fence or clearly contrast.
	int fence_type = rng.irange(0, 2);
	if (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs) {
		fence_type = 1;
	} else if (lot.persona == Persona::Minimalists) {
		fence_type = 0;
	}
	int fence_cell = rng.chance(0.7f) ? h.trim_cell : (rng.chance(0.5f) ? pal::WOOD_LIGHT : h.wall_cell);
	if (lot.persona == Persona::CraftyArtists) {
		fence_cell = rng.chance(0.5f) ? pal::CANDY_MINT : pal::CANDY_PINK;
	}
	for (const Lot &o : ctx.lots) {
		if (o.side == lot.side && lot.index - o.index == 1 && o.house.fence_type != 0) {
			if (rng.chance(0.5f)) {
				fence_type = o.house.fence_type;
				fence_cell = o.house.fence_cell;
			} else if (fence_type == o.house.fence_type) {
				fence_type = fence_type == 1 ? 2 : 1;
			}
		}
	}
	h.fence_type = fence_type;
	h.fence_cell = fence_cell;
}

// ---------------------------------------------------------------- build helpers
namespace {

struct HouseCtx {
	GenContext &ctx;
	Lot &lot;
	MeshBuilder &m;
	HouseSpec &h;
	Rng &rng;
	int trim() const { return mat_cell(h.trim_cell, mat::WOOD); }
	void box(Vector3 c, Vector3 half, int cell, int top = -1) { m.box(c, half, cell, top); }
	void rbox(Vector3 c, Vector3 half, float bevel, int cell, int top = -1) { m.rounded_box(c, half, bevel, cell, top); }
	void solid(Vector3 c, Vector3 half, bool walkable) { ctx.add_box(lot.index, c, half, walkable); }
	void cover(float x0, float x1, float z0, float z1) {
		for (float x = x0 + 0.6f; x <= x1 - 0.2f; x += 1.3f) {
			for (float z = z0 + 0.6f; z <= z1 - 0.2f; z += 1.3f) {
				ctx.occupy(lot.index, x, z, 0.95f);
			}
		}
	}
};

// Glowing window with trim, mullions and curtains, on a wall facing +z*facing at depth wall_z.
void window_front(HouseCtx &hc, float x, float y, float wall_z, float w, float hgt, bool shutters, float facing = 1.0f) {
	const HouseSpec &h = hc.h;
	const float z = wall_z + facing * 0.03f;
	const int glow = hc.rng.chance(0.7f) ? pal::GLOW_WINDOW : pal::GLOW_WINDOW_WARM;
	hc.box(Vector3(x, y, z), Vector3(w * 0.5f, hgt * 0.5f, 0.03f), glow);
	// Curtains: tied-back panels glowing with the room light behind them.
	const int curtain = mat_cell(h.curtain_cell, mat::FABRIC);
	for (float s : { -1.0f, 1.0f }) {
		hc.box(Vector3(x + s * w * 0.36f, y + hgt * 0.06f, wall_z + facing * 0.055f), Vector3(w * 0.13f, hgt * 0.44f, 0.012f), curtain);
	}
	hc.box(Vector3(x, y + hgt * 0.44f, wall_z + facing * 0.056f), Vector3(w * 0.5f, hgt * 0.06f, 0.012f), curtain);
	const float t = 0.09f;
	const float zf = wall_z + facing * 0.07f;
	const int trim = hc.trim();
	hc.rbox(Vector3(x, y + hgt * 0.5f + t * 0.7f, zf), Vector3(w * 0.5f + t * 1.6f, t * 0.7f, 0.08f), 0.02f, trim);
	hc.rbox(Vector3(x, y - hgt * 0.5f - t * 0.6f, zf + facing * 0.04f), Vector3(w * 0.5f + t * 1.4f, t * 0.6f, 0.12f), 0.02f, trim);
	hc.box(Vector3(x - w * 0.5f - t * 0.5f, y, zf), Vector3(t * 0.5f, hgt * 0.5f, 0.06f), trim);
	hc.box(Vector3(x + w * 0.5f + t * 0.5f, y, zf), Vector3(t * 0.5f, hgt * 0.5f, 0.06f), trim);
	hc.box(Vector3(x, y, zf), Vector3(0.03f, hgt * 0.5f, 0.04f), trim);
	hc.box(Vector3(x, y + hgt * 0.1f, zf), Vector3(w * 0.5f, 0.03f, 0.04f), trim);
	if (shutters) {
		const int sh = mat_cell(h.shutter_cell, mat::PLANKS);
		for (float s : { -1.0f, 1.0f }) {
			hc.rbox(Vector3(x + s * (w * 0.5f + t + 0.24f), y, wall_z + facing * 0.06f), Vector3(0.22f, hgt * 0.52f, 0.035f), 0.015f, sh);
		}
	}
}

void window_side(HouseCtx &hc, float z, float y, float wall_x, float w, float hgt, float facing) {
	const int glow = hc.rng.chance(0.6f) ? pal::GLOW_WINDOW : pal::GLOW_WINDOW_WARM;
	const float x = wall_x + facing * 0.03f;
	hc.box(Vector3(x, y, z), Vector3(0.03f, hgt * 0.5f, w * 0.5f), glow);
	const int curtain = mat_cell(hc.h.curtain_cell, mat::FABRIC);
	for (float s : { -1.0f, 1.0f }) {
		hc.box(Vector3(wall_x + facing * 0.055f, y + hgt * 0.06f, z + s * w * 0.36f), Vector3(0.012f, hgt * 0.44f, w * 0.13f), curtain);
	}
	const float t = 0.09f, xf = wall_x + facing * 0.07f;
	const int trim = hc.trim();
	hc.rbox(Vector3(xf, y + hgt * 0.5f + t * 0.7f, z), Vector3(0.08f, t * 0.7f, w * 0.5f + t * 1.6f), 0.02f, trim);
	hc.rbox(Vector3(xf, y - hgt * 0.5f - t * 0.6f, z), Vector3(0.12f, t * 0.6f, w * 0.5f + t * 1.4f), 0.02f, trim);
	hc.box(Vector3(xf, y, z), Vector3(0.04f, hgt * 0.5f, 0.03f), trim);
}

void flower_box(HouseCtx &hc, float x, float y, float wall_z, float w) {
	hc.rbox(Vector3(x, y, wall_z + 0.2f), Vector3(w * 0.5f, 0.12f, 0.14f), 0.03f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::CINNAMON, mat::SOIL));
	const int n = std::max(3, int(w / 0.22f));
	for (int i = 0; i < n; ++i) {
		const float fx = x - w * 0.42f + w * 0.84f * float(i) / (n - 1);
		hc.m.blob(Vector3(fx, y + 0.17f, wall_z + 0.2f + hc.rng.range(-0.04f, 0.04f)), Vector3(0.1f, 0.08f, 0.09f),
				mat_cell(i % 2 ? FLOWERS[hc.rng.irange(0, 5)] : pal::HEDGE, i % 2 ? mat::FLAT : mat::HEDGE), 0);
	}
}

void gable_roof(HouseCtx &hc, float x0, float x1, float z0, float z1, float base_y, bool ridge_x, float pitch, int roof_mat, int roof_cell,
		int wall_cell, int trim_cell, bool collide, bool gingerbread) {
	const float o = 0.45f; // overhang
	const float thick = 0.22f;
	MeshBuilder &m = hc.m;
	const int roof = mat_cell(roof_cell, roof_mat);
	const int trim = mat_cell(trim_cell, mat::WOOD);
	const int gutter = mat_cell(pal::SILVER, mat::METAL);
	const float rr = std::tan(pitch) * ((ridge_x ? z1 - z0 : x1 - x0) * 0.5f);
	const float top = base_y + rr;
	const float ye = base_y - o * std::tan(pitch);
	if (ridge_x) {
		const float zm = (z0 + z1) * 0.5f;
		for (float xe : { x0, x1 }) {
			m.tri_out(Vector3(xe, base_y, z0), Vector3(xe, base_y, z1), Vector3(xe, top, zm), wall_cell, Vector3((x0 + x1) * 0.5f, base_y, zm));
		}
		for (float s : { 1.0f, -1.0f }) {
			const float ze = s > 0 ? z1 + o : z0 - o;
			Vector3 e0(x0 - o, ye, ze), e1(x1 + o, ye, ze), r0(x0 - o, top, zm), r1(x1 + o, top, zm);
			const Vector3 nrm = (Vector3(0, std::cos(pitch), s * std::sin(pitch))).normalized();
			const Vector3 up = nrm * thick;
			const Vector3 inside = Vector3((x0 + x1) * 0.5f, base_y, zm);
			m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, -std::sin(pitch), s * std::cos(pitch)));
			m.quad_out(e0 + up, e1 + up, r1 + up, r0 + up, roof, inside);
			m.quad_out(e0, e1, r1, r0, mat_cell(pal::WOOD_DARK, mat::PLANKS), inside + Vector3(0, 100, 0));
			m.clear_uv_axes();
			m.quad_out(e0, e1, e1 + up, e0 + up, trim, inside); // eave fascia
			m.quad_out(e0, r0, r0 + up, e0 + up, roof, Vector3(x1, top, zm));
			m.quad_out(e1, r1, r1 + up, e1 + up, roof, Vector3(x0, top, zm));
			// Rake boards along both gable edges and a gutter along the eave.
			for (float xe : { x0 - o, x1 + o }) {
				const float side = xe < x0 ? -1.0f : 1.0f;
				m.beam(Vector3(xe + side * 0.03f, ye - 0.06f, ze), Vector3(xe + side * 0.03f, top + 0.05f, zm), 0.08f, 0.26f, trim, nrm);
			}
			m.beam(Vector3(x0 - o, ye - 0.08f, ze + s * 0.07f), Vector3(x1 + o, ye - 0.08f, ze + s * 0.07f), 0.14f, 0.12f, gutter);
			if (s > 0 && collide) {
				const float pa = hc.h.porch_x - hc.h.porch_w * 0.5f - 0.4f, pb = hc.h.porch_x + hc.h.porch_w * 0.5f + 0.4f;
				for (float xe : { x0 + 0.15f, x1 - 0.15f }) {
					if (xe > pa && xe < pb) {
						continue; // no downspout through the porch
					}
					m.beam(Vector3(xe, ye - 0.1f, ze + 0.07f), Vector3(xe, ye - 0.1f, z1 + 0.12f), 0.08f, 0.08f, gutter);
					m.beam(Vector3(xe, ye - 0.08f, z1 + 0.12f), Vector3(xe, 0.15f, z1 + 0.12f), 0.08f, 0.08f, gutter);
				}
			}
			if (collide) {
				const Vector3 c = (e0 + e1 + r0 + r1) * 0.25f + up * 0.5f;
				const float len = (r0 - e0).length();
				const Basis tilt(Vector3(1, 0, 0), s * pitch);
				hc.ctx.add_box(hc.lot.index, c, Vector3((x1 - x0) * 0.5f + o, thick * 0.5f, len * 0.5f), false, 0.0f, tilt);
			}
		}
		if (gingerbread) {
			for (float xe : { x0 - o - 0.04f, x1 + o + 0.04f }) {
				m.beam(Vector3(xe, top - 0.1f, zm), Vector3(xe, top - 1.1f, zm), 0.06f, 0.12f, trim, Vector3(0, 0, 1));
				m.beam(Vector3(xe, top - 0.55f, zm - 0.55f), Vector3(xe, top - 0.55f, zm + 0.55f), 0.06f, 0.1f, trim);
				m.blob(Vector3(xe, top - 1.15f, zm), Vector3(0.07f, 0.1f, 0.07f), trim, 0);
			}
		}
		m.rounded_box(Vector3((x0 + x1) * 0.5f, top + 0.12f, zm), Vector3((x1 - x0) * 0.5f + o + 0.05f, 0.12f, 0.28f), 0.05f, trim, roof);
		if (collide) {
			hc.solid(Vector3((x0 + x1) * 0.5f, top + 0.12f, zm), Vector3((x1 - x0) * 0.5f + o, 0.14f, 0.32f), true);
		}
	} else {
		const float xm = (x0 + x1) * 0.5f;
		for (float ze : { z0, z1 }) {
			m.tri_out(Vector3(x0, base_y, ze), Vector3(x1, base_y, ze), Vector3(xm, top, ze), wall_cell, Vector3(xm, base_y, (z0 + z1) * 0.5f));
		}
		for (float s : { 1.0f, -1.0f }) {
			const float xe = s > 0 ? x1 + o : x0 - o;
			Vector3 e0(xe, ye, z0 - o), e1(xe, ye, z1 + o), r0(xm, top, z0 - o), r1(xm, top, z1 + o);
			const Vector3 nrm = Vector3(s * std::sin(pitch), std::cos(pitch), 0).normalized();
			const Vector3 up = nrm * thick;
			const Vector3 inside(xm, base_y, (z0 + z1) * 0.5f);
			m.set_uv_axes(Vector3(0, 0, 1), Vector3(s * std::cos(pitch), -std::sin(pitch), 0));
			m.quad_out(e0 + up, e1 + up, r1 + up, r0 + up, roof, inside);
			m.quad_out(e0, e1, r1, r0, mat_cell(pal::WOOD_DARK, mat::PLANKS), inside + Vector3(0, 100, 0));
			m.clear_uv_axes();
			m.quad_out(e0, e1, e1 + up, e0 + up, trim, inside);
			m.quad_out(e0, r0, r0 + up, e0 + up, roof, Vector3(xm, top, z1));
			m.quad_out(e1, r1, r1 + up, e1 + up, roof, Vector3(xm, top, z0));
			for (float ze : { z0 - o, z1 + o }) {
				const float side = ze < z0 ? -1.0f : 1.0f;
				m.beam(Vector3(xe, ye - 0.06f, ze + side * 0.03f), Vector3(xm, top + 0.05f, ze + side * 0.03f), 0.08f, 0.26f, trim, nrm);
			}
			m.beam(Vector3(xe + s * 0.07f, ye - 0.08f, z0 - o), Vector3(xe + s * 0.07f, ye - 0.08f, z1 + o), 0.14f, 0.12f, gutter);
			if (collide) {
				const Vector3 c = (e0 + e1 + r0 + r1) * 0.25f + up * 0.5f;
				const float len = (r0 - e0).length();
				const Basis tilt(Vector3(0, 0, 1), -s * pitch);
				hc.ctx.add_box(hc.lot.index, c, Vector3(len * 0.5f, thick * 0.5f, (z1 - z0) * 0.5f + o), false, 0.0f, tilt);
			}
		}
		if (gingerbread) {
			const float ze = z1 + o + 0.04f;
			m.beam(Vector3(xm, top - 0.1f, ze), Vector3(xm, top - 1.1f, ze), 0.12f, 0.06f, trim, Vector3(1, 0, 0));
			m.beam(Vector3(xm - 0.55f, top - 0.55f, ze), Vector3(xm + 0.55f, top - 0.55f, ze), 0.1f, 0.06f, trim);
			m.blob(Vector3(xm, top - 1.15f, ze), Vector3(0.07f, 0.1f, 0.07f), trim, 0);
		}
		m.rounded_box(Vector3(xm, top + 0.12f, (z0 + z1) * 0.5f), Vector3(0.28f, 0.12f, (z1 - z0) * 0.5f + o + 0.05f), 0.05f, trim, roof);
		if (collide) {
			hc.solid(Vector3(xm, top + 0.12f, (z0 + z1) * 0.5f), Vector3(0.32f, 0.14f, (z1 - z0) * 0.5f + o), true);
		}
	}
}

void rail_run(HouseCtx &hc, Vector3 a, Vector3 b, float deck) {
	const Vector3 d = b - a;
	const float len = d.length();
	if (len < 0.3f) {
		return;
	}
	const bool along_x = std::abs(d.x) > std::abs(d.z);
	const Vector3 mid = (a + b) * 0.5f;
	const float top = deck + 0.9f;
	const int trim = hc.trim();
	const Vector3 half_top = along_x ? Vector3(len * 0.5f + 0.1f, 0.06f, 0.14f) : Vector3(0.14f, 0.06f, len * 0.5f + 0.1f);
	hc.rbox(Vector3(mid.x, top, mid.z), half_top, 0.03f, trim);
	hc.box(Vector3(mid.x, deck + 0.2f, mid.z), along_x ? Vector3(len * 0.5f, 0.04f, 0.06f) : Vector3(0.06f, 0.04f, len * 0.5f), trim);
	const int n = std::max(2, int(len / 0.3f));
	for (int i = 1; i < n; ++i) {
		const Vector3 p = a + d * (float(i) / n);
		hc.m.cylinder(Vector3(p.x, deck + 0.22f, p.z), 0.035f, 0.62f, 6, trim, 0.03f);
	}
	// Chunky rail tops are platforms (a stepping stone up to the porch roof).
	hc.solid(Vector3(mid.x, deck + 0.5f, mid.z), along_x ? Vector3(len * 0.5f + 0.1f, 0.46f, 0.15f) : Vector3(0.15f, 0.46f, len * 0.5f + 0.1f), true);
}

// Turned porch post: base block, tapered shaft with rings, capital.
void porch_post(HouseCtx &hc, float x, float z, float y0, float height) {
	const int trim = hc.trim();
	hc.rbox(Vector3(x, y0 + 0.2f, z), Vector3(0.14f, 0.2f, 0.14f), 0.03f, trim);
	hc.m.cylinder(Vector3(x, y0 + 0.4f, z), 0.1f, height - 0.65f, 8, trim, 0.08f);
	hc.m.cylinder(Vector3(x, y0 + 0.62f, z), 0.12f, 0.06f, 8, trim);
	hc.m.cylinder(Vector3(x, y0 + height - 0.42f, z), 0.11f, 0.06f, 8, trim);
	hc.rbox(Vector3(x, y0 + height - 0.12f, z), Vector3(0.16f, 0.12f, 0.16f), 0.03f, trim);
	if (hc.h.gingerbread) {
		for (float s : { -1.0f, 1.0f }) {
			hc.m.beam(Vector3(x, y0 + height - 0.55f, z), Vector3(x + s * 0.45f, y0 + height - 0.08f, z), 0.06f, 0.06f, trim, Vector3(0, 0, 1));
		}
	}
}

} // namespace

// ---------------------------------------------------------------- build
void build_house(GenContext &ctx, Lot &lot) {
	HouseSpec &h = lot.house;
	MeshBuilder &m = ctx.lot_mesh[lot.index];
	Rng rng = ctx.rng(STAGE_HOUSE, lot.index, 1);
	HouseCtx hc{ ctx, lot, m, h, rng };
	const float x0 = h.house_x - h.width * 0.5f, x1 = h.house_x + h.width * 0.5f;
	const float zf = h.front_z, zb = h.front_z - h.depth;
	const float eave = eave_h(h);
	const float pitch = Math::deg_to_rad(h.pitch_deg);
	const int trim = hc.trim();
	const int wall = mat_cell(h.wall_cell, h.wall_mat);
	const int upper = mat_cell(h.upper_cell, h.upper_mat);
	m.set_ground_ao(true);

	// Foundation + body (two-tone when the upper storey differs).
	hc.rbox(Vector3(h.house_x, 0.2f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f + 0.12f, 0.2f, h.depth * 0.5f + 0.12f), 0.04f, mat_cell(pal::STONE, mat::STONE));
	if (h.storeys == 2) {
		hc.box(Vector3(h.house_x, h.storey_h * 0.5f + 0.2f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f, h.storey_h * 0.5f - 0.2f, h.depth * 0.5f), wall);
		hc.box(Vector3(h.house_x, h.storey_h * 1.5f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f, h.storey_h * 0.5f, h.depth * 0.5f), upper);
		hc.rbox(Vector3(h.house_x, h.storey_h, (zf + zb) * 0.5f), Vector3(h.width * 0.5f + 0.07f, 0.09f, h.depth * 0.5f + 0.07f), 0.02f, trim);
	} else {
		hc.box(Vector3(h.house_x, eave * 0.5f + 0.2f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f, eave * 0.5f - 0.2f, h.depth * 0.5f), wall);
	}
	hc.solid(Vector3(h.house_x, eave * 0.5f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f, eave * 0.5f, h.depth * 0.5f), false);
	hc.cover(x0, x1, zb, zf + h.porch_d + 1.2f);
	for (float x : { x0, x1 }) {
		for (float z : { zf, zb }) {
			hc.rbox(Vector3(x, eave * 0.5f + 0.2f, z), Vector3(0.11f, eave * 0.5f - 0.2f, 0.11f), 0.02f, trim);
		}
	}

	// Windows per storey (skipping the door), with shutters and flower boxes on the ground floor.
	for (int s = 0; s < h.storeys; ++s) {
		const float wy = s * h.storey_h + (s == 0 ? 1.65f : 1.7f);
		const int count = h.width > 9.0f ? 3 : 2;
		for (int i = 0; i < count; ++i) {
			const float wx = Math::lerp(x0 + 1.4f, x1 - 1.4f, count == 1 ? 0.5f : float(i) / (count - 1));
			if (s == 0 && std::abs(wx - h.door_x) < 1.5f) {
				continue;
			}
			window_front(hc, wx, wy, zf, 0.95f, 1.2f, h.shutters);
			if (s == 0 && h.flower_boxes && (wx < h.porch_x - h.porch_w * 0.5f - 0.5f || wx > h.porch_x + h.porch_w * 0.5f + 0.5f)) {
				flower_box(hc, wx, wy - 0.85f, zf, 1.1f);
			}
		}
		for (float sx : { -1.0f, 1.0f }) {
			window_side(hc, (zf + zb) * 0.5f, wy, sx > 0 ? x1 : x0, 1.0f, 1.1f, sx);
		}
		window_front(hc, h.house_x + rng.range(-1.5f, 1.5f), wy, zb, 0.9f, 1.0f, false, -1.0f);
	}
	// Round attic window in the front gable.
	if (!h.ridge_along_x) {
		const float r = std::tan(pitch) * h.width * 0.5f;
		Transform3D save = m.transform();
		m.set_transform(Transform3D(Basis(Vector3(1, 0, 0), Math_PI * 0.5f), Vector3(h.house_x, eave + r * 0.35f, zf + 0.05f)));
		m.cylinder(Vector3(0, -0.04f, 0), 0.48f, 0.1f, 12, trim);
		m.cylinder(Vector3(0, -0.06f, 0), 0.37f, 0.14f, 12, pal::GLOW_WINDOW);
		m.set_transform(save);
		m.beam(Vector3(h.house_x - 0.37f, eave + r * 0.35f, zf + 0.12f), Vector3(h.house_x + 0.37f, eave + r * 0.35f, zf + 0.12f), 0.05f, 0.05f, trim);
		m.beam(Vector3(h.house_x, eave + r * 0.35f - 0.37f, zf + 0.12f), Vector3(h.house_x, eave + r * 0.35f + 0.37f, zf + 0.12f), 0.05f, 0.05f, trim);
	}

	// Door: frame, optional pediment, warm interior glow behind it, and a separate swinging door node.
	const float deck = h.deck_h;
	hc.box(Vector3(h.door_x, deck + 1.05f, zf + 0.02f), Vector3(0.62f, 1.1f, 0.03f), pal::GLOW_WINDOW_WARM);
	hc.rbox(Vector3(h.door_x, deck + 2.27f, zf + 0.09f), Vector3(0.82f, 0.12f, 0.1f), 0.03f, trim);
	for (float sx : { -1.0f, 1.0f }) {
		hc.rbox(Vector3(h.door_x + sx * 0.7f, deck + 1.1f, zf + 0.08f), Vector3(0.1f, 1.1f, 0.09f), 0.02f, trim);
	}
	if (h.pediment) {
		m.set_transform(Transform3D(Basis(), Vector3(h.door_x, deck + 2.39f, zf + 0.1f)));
		m.prism_xy({ { -0.95f, 0 }, { 0.95f, 0 }, { 0, 0.45f } }, 0.0f, 0.16f, trim);
		m.set_transform(Transform3D());
	}
	DoorSpec door;
	door.lot = lot.index;
	door.hinge = lot.xf * Transform3D(Basis(), Vector3(h.door_x - 0.56f, deck, zf + 0.06f));
	door.width = 1.12f;
	door.height = 2.12f;
	door.cell = mat_cell(h.door_cell, mat::WOOD);
	door.trim_cell = trim;
	ctx.doors.push_back(door);

	// Porch: lattice skirt, plank deck, steps with stringers, turned posts, rails, roof, lamp.
	const float px0 = h.porch_x - h.porch_w * 0.5f, px1 = h.porch_x + h.porch_w * 0.5f;
	const float pz0 = zf, pz1 = zf + h.porch_d;
	hc.box(Vector3(h.porch_x, deck * 0.5f - 0.04f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f - 0.05f, deck * 0.5f - 0.04f, h.porch_d * 0.5f - 0.05f),
			mat_cell(h.trim_cell, mat::LATTICE));
	m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, 0, 1));
	hc.rbox(Vector3(h.porch_x, deck - 0.07f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f, 0.07f, h.porch_d * 0.5f), 0.03f, mat_cell(pal::PLANK, mat::PLANKS));
	m.clear_uv_axes();
	hc.solid(Vector3(h.porch_x, deck * 0.5f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f, deck * 0.5f, h.porch_d * 0.5f), true);
	// Wraparound steps: each lower step also runs round both sides of the one above, so the kid can
	// walk up from the front or from either side (no getting stuck against the side of the flight).
	const int steps = 3;
	const float step_h = deck / steps, step_d = 0.36f, step_w = 1.7f;
	for (int i = 0; i < steps; ++i) {
		const float top = deck - step_h * i;
		const float hw = step_w * 0.5f + step_d * i, depth = step_d * (i + 1);
		const Vector3 c(h.door_x, top * 0.5f, pz1 + depth * 0.5f);
		hc.rbox(c, Vector3(hw, top * 0.5f, depth * 0.5f), 0.025f, mat_cell(pal::PLANK, mat::PLANKS), mat_cell(pal::WOOD_LIGHT, mat::PLANKS));
		hc.solid(c, Vector3(hw, top * 0.5f, depth * 0.5f), true);
		// A trim nosing along the front and side edges of each tread.
		m.box(Vector3(h.door_x, top - 0.02f, pz1 + depth + 0.01f), Vector3(hw + 0.01f, 0.03f, 0.02f), trim);
		for (float sx : { -1.0f, 1.0f }) {
			m.box(Vector3(h.door_x + sx * (hw + 0.01f), top - 0.02f, pz1 + depth * 0.5f), Vector3(0.02f, 0.03f, depth * 0.5f + 0.01f), trim);
		}
	}
	hc.box(Vector3(h.door_x, deck + 0.012f, zf + 0.55f), Vector3(0.5f, 0.012f, 0.32f), mat_cell(pal::PUMPKIN_DARK, mat::BURLAP));
	lot.doorbell = lot.to_world(h.door_x, deck, zf + 0.7f);
	lot.porch_light = lot.to_world(h.door_x, deck + 1.95f, zf + 0.9f);
	const float roof_y = deck + PORCH_ROOF_GAP;
	std::vector<float> post_x = { px0 + 0.15f, px1 - 0.15f };
	if (h.porch_w > 4.8f) {
		post_x.push_back(h.porch_x + (h.door_x > h.porch_x ? -1.0f : 1.0f) * h.porch_w * 0.25f);
	}
	for (float x : post_x) {
		porch_post(hc, x, pz1 - 0.15f, deck, PORCH_ROOF_GAP);
		hc.solid(Vector3(x, deck + PORCH_ROOF_GAP * 0.5f, pz1 - 0.15f), Vector3(0.13f, PORCH_ROOF_GAP * 0.5f, 0.13f), false);
	}
	m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, 0, 1));
	hc.rbox(Vector3(h.porch_x, roof_y + 0.1f, (pz0 + pz1) * 0.5f + 0.1f), Vector3(h.porch_w * 0.5f + 0.25f, 0.1f, h.porch_d * 0.5f + 0.25f), 0.05f,
			mat_cell(pal::WOOD_LIGHT, mat::PLANKS), mat_cell(h.roof_cell, h.roof_mat));
	m.clear_uv_axes();
	hc.box(Vector3(h.porch_x, roof_y - 0.03f, pz1 + 0.33f), Vector3(h.porch_w * 0.5f + 0.25f, 0.09f, 0.035f), trim);
	hc.solid(Vector3(h.porch_x, roof_y + 0.1f, (pz0 + pz1) * 0.5f + 0.1f), Vector3(h.porch_w * 0.5f + 0.25f, 0.1f, h.porch_d * 0.5f + 0.25f), true);
	// Ceiling lamp over the door.
	m.cylinder(Vector3(h.door_x, roof_y - 0.32f, zf + 0.9f), 0.16f, 0.3f, 8, pal::GLOW_PORCH, 0.11f);
	m.cylinder(Vector3(h.door_x, roof_y - 0.06f, zf + 0.9f), 0.2f, 0.06f, 8, mat_cell(pal::IRON, mat::METAL));
	// Rails: front on both sides of the steps, starting past the widest (bottom) step so the whole
	// wraparound flight opens onto the deck; one end rail on the driveway side (passage side open).
	const float mouth = step_w * 0.5f + step_d * (steps - 1) + 0.05f;
	const float sx0 = h.door_x - mouth, sx1 = h.door_x + mouth;
	if (sx0 - px0 > 0.5f) {
		rail_run(hc, Vector3(px0 + 0.15f, 0, pz1 - 0.15f), Vector3(sx0, 0, pz1 - 0.15f), deck);
	}
	if (px1 - sx1 > 0.5f) {
		rail_run(hc, Vector3(sx1, 0, pz1 - 0.15f), Vector3(px1 - 0.15f, 0, pz1 - 0.15f), deck);
	}
	const float drive_side = float(-lot.passage_side);
	const float end_x = drive_side > 0 ? px1 - 0.15f : px0 + 0.15f;
	rail_run(hc, Vector3(end_x, 0, pz0 + 0.2f), Vector3(end_x, 0, pz1 - 0.15f), deck);

	// Garage with a walkable flat roof, plank door and a glowing window row.
	if (h.garage) {
		const float g = float(h.garage_side);
		const float gx = h.house_x + g * (h.width * 0.5f + GARAGE_W * 0.5f);
		const float gz1 = zf - 0.4f, gz0 = gz1 - GARAGE_D;
		hc.rbox(Vector3(gx, 0.15f, (gz0 + gz1) * 0.5f), Vector3(GARAGE_W * 0.5f + 0.08f, 0.15f, GARAGE_D * 0.5f + 0.08f), 0.03f, mat_cell(pal::STONE, mat::STONE));
		hc.box(Vector3(gx, GARAGE_H * 0.5f + 0.15f, (gz0 + gz1) * 0.5f), Vector3(GARAGE_W * 0.5f, GARAGE_H * 0.5f - 0.15f, GARAGE_D * 0.5f), mat_cell(h.garage_cell, h.wall_mat));
		hc.rbox(Vector3(gx, GARAGE_H + 0.12f, (gz0 + gz1) * 0.5f), Vector3(GARAGE_W * 0.5f + 0.2f, 0.12f, GARAGE_D * 0.5f + 0.2f), 0.05f, trim,
				mat_cell(h.roof_cell, mat::SHINGLES));
		hc.solid(Vector3(gx, GARAGE_H * 0.5f + 0.12f, (gz0 + gz1) * 0.5f), Vector3(GARAGE_W * 0.5f + 0.2f, GARAGE_H * 0.5f + 0.12f, GARAGE_D * 0.5f + 0.2f), true);
		const int gdoor = mat_cell(h.trim_cell, mat::PLANKS);
		m.set_uv_axes(Vector3(0, -1, 0), Vector3(1, 0, 0));
		for (int i = 0; i < 4; ++i) {
			if (i == 3) {
				for (int k = 0; k < 4; ++k) {
					hc.box(Vector3(gx - 1.1f + k * 0.73f, 0.3f + i * 0.5f, gz1 + 0.035f), Vector3(0.3f, 0.18f, 0.03f), pal::GLOW_WINDOW_WARM);
				}
			}
			hc.rbox(Vector3(gx, 0.3f + i * 0.5f, gz1 + 0.025f), Vector3(1.48f, 0.23f, 0.03f), 0.02f, gdoor);
		}
		m.clear_uv_axes();
		hc.rbox(Vector3(gx, 2.32f, gz1 + 0.06f), Vector3(1.62f, 0.1f, 0.06f), 0.02f, trim);
		for (float sx : { -1.0f, 1.0f }) {
			hc.rbox(Vector3(gx + sx * 1.6f, 1.15f, gz1 + 0.06f), Vector3(0.09f, 1.15f, 0.06f), 0.02f, trim);
		}
		hc.cover(gx - GARAGE_W * 0.5f, gx + GARAGE_W * 0.5f, gz0, gz1 + 0.6f);
	}
	m.set_ground_ao(false);
	// Driveway slab and stepping-stone path to the street.
	{
		const float dz1 = h.garage ? zf - 0.4f : zf + 1.0f;
		const float hw = 1.45f;
		m.quad_out(Vector3(lot.drive_x - hw, 0.025f, 0), Vector3(lot.drive_x + hw, 0.025f, 0), Vector3(lot.drive_x + hw, 0.025f, dz1),
				Vector3(lot.drive_x - hw, 0.025f, dz1), mat_cell(h.drive_mat == mat::PAVERS ? pal::STONE_LIGHT : pal::SIDEWALK, h.drive_mat),
				Vector3(lot.drive_x, -1, dz1 * 0.5f));
		for (float sx : { -1.0f, 1.0f }) {
			m.beam(Vector3(lot.drive_x + sx * (hw + 0.06f), 0.03f, 0), Vector3(lot.drive_x + sx * (hw + 0.06f), 0.03f, dz1), 0.12f, 0.06f,
					mat_cell(pal::STONE, mat::STONE));
		}
		// Apron across the sidewalk and verge down to the curb.
		const float apron = ctx.street.walk;
		m.quad_out(Vector3(lot.drive_x - hw, 0.08f, apron), Vector3(lot.drive_x + hw, 0.08f, apron), Vector3(lot.drive_x + hw, 0.08f, 0),
				Vector3(lot.drive_x - hw, 0.08f, 0), mat_cell(pal::SIDEWALK, mat::CONCRETE), Vector3(lot.drive_x, -1, apron * 0.5f));
	}
	// Luminaria: glowing paper bags lining the front path for the cosiest households.
	if (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::CraftyArtists || lot.persona == Persona::Stargazers ||
			lot.landmark == Landmark::LanternPorch || rng.chance(0.2f)) {
		const int bag = mat_cell(rng.chance(0.5f) ? pal::GLOW_LANTERN : pal::GLOW_CANDLE, mat::PAPER);
		for (float z = -1.0f; z > pz1 + 1.4f; z -= 1.1f) {
			for (float sx : { -1.0f, 1.0f }) {
				const Vector3 c(h.door_x + sx * 0.75f, 0.13f, z);
				m.rounded_box(c, Vector3(0.09f, 0.13f, 0.09f), 0.02f, bag, mat_cell(pal::CHARCOAL, mat::FLAT));
			}
		}
	}
	for (float z = -0.6f; z > pz1 + 1.2f; z -= 0.75f) {
		hc.rbox(Vector3(h.door_x + rng.range(-0.08f, 0.08f), 0.035f, z), Vector3(0.42f, 0.035f, 0.27f), 0.03f, mat_cell(pal::STONE_LIGHT, mat::STONE),
				mat_cell(pal::BONE, mat::STONE));
	}

	// Main roof, dormer and chimney.
	gable_roof(hc, x0, x1, zb, zf, eave, h.ridge_along_x, pitch, h.roof_mat, h.roof_cell, mat_cell(h.upper_cell, h.upper_mat), h.trim_cell, true, h.gingerbread);
	const float r = rise(h);
	if (h.dormer) {
		// A little gable dormer poking out of the front slope.
		const float dx = h.house_x + (h.door_x > h.house_x ? -1.0f : 1.0f) * h.width * 0.18f;
		const float dz = zf - 0.6f;
		const float base = eave + 0.55f;
		hc.box(Vector3(dx, base + 0.75f, dz - 0.6f), Vector3(0.75f, 0.75f, 0.8f), upper);
		window_front(hc, dx, base + 0.75f, dz + 0.2f, 0.7f, 0.85f, false);
		gable_roof(hc, dx - 0.75f, dx + 0.75f, dz - 1.4f, dz + 0.2f, base + 1.5f, false, Math::deg_to_rad(50.0f), h.roof_mat, h.roof_cell, upper, h.trim_cell, false,
				false);
	}
	if (h.chimney) {
		float cx, cz;
		if (h.ridge_along_x) {
			cx = h.house_x + float(h.chimney_side) * (h.width * 0.5f - 0.9f);
			cz = (zf + zb) * 0.5f;
		} else {
			cx = h.house_x;
			cz = zb + 1.0f;
		}
		const float ctop = eave + r + 1.25f;
		hc.rbox(Vector3(cx, (eave + ctop) * 0.5f, cz), Vector3(0.42f, (ctop - eave) * 0.5f, 0.42f), 0.04f, mat_cell(pal::BRICK, mat::BRICK),
				mat_cell(pal::BRICK_DARK, mat::BRICK));
		hc.rbox(Vector3(cx, ctop + 0.06f, cz), Vector3(0.52f, 0.08f, 0.52f), 0.03f, mat_cell(pal::STONE_LIGHT, mat::STONE));
		m.cylinder(Vector3(cx + 0.15f, ctop + 0.12f, cz), 0.12f, 0.3f, 8, mat_cell(pal::TERRACOTTA, mat::STUCCO), 0.1f);
		hc.solid(Vector3(cx, (eave + ctop) * 0.5f + 0.06f, cz), Vector3(0.5f, (ctop - eave) * 0.5f + 0.07f, 0.5f), true);
	}
	// Summit route for landmarks: window-box ledge (2 storeys) and steps up the front slope.
	if (h.roof_steps && h.ridge_along_x) {
		// Steps rise from above the porch roof (where the climb starts), toward the chimney side.
		const float want = h.house_x + float(h.chimney_side) * (h.width * 0.5f - 0.9f) - float(h.chimney_side) * 1.5f;
		const float sx = Math::clamp(want, px0 + 0.7f, px1 - 0.7f);
		lot.summit_route.clear();
		if (h.storeys == 2) {
			const float ly = h.storey_h + 1.35f;
			const float lx = Math::clamp(sx, px0 + 0.6f, px1 - 0.6f);
			hc.rbox(Vector3(lx, ly, zf + 0.35f), Vector3(0.8f, 0.14f, 0.35f), 0.04f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::HEDGE, mat::HEDGE));
			m.blob(Vector3(lx - 0.35f, ly + 0.2f, zf + 0.35f), Vector3(0.22f, 0.16f, 0.2f), pal::MAPLE_ORANGE, 0);
			m.blob(Vector3(lx + 0.35f, ly + 0.2f, zf + 0.35f), Vector3(0.22f, 0.16f, 0.2f), pal::CANDY_PINK, 0);
			hc.solid(Vector3(lx, ly, zf + 0.4f), Vector3(0.8f, 0.14f, 0.4f), true);
			lot.summit_route.push_back(Vector3(lx, ly + 0.14f, zf + 0.4f));
		}
		const int n = std::max(2, int(std::ceil(r / 1.35f)));
		for (int k = 0; k < n; ++k) {
			const float t = (k + 0.35f) / n;
			const float z = Math::lerp(zf + 0.2f, (zf + zb) * 0.5f, t);
			const float y = eave + r * t + 0.45f;
			m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, 0, 1));
			hc.rbox(Vector3(sx, y, z), Vector3(0.6f, 0.18f, 0.42f), 0.05f, mat_cell(pal::WOOD_LIGHT, mat::PLANKS), mat_cell(pal::PLANK, mat::PLANKS));
			m.clear_uv_axes();
			hc.solid(Vector3(sx, y, z), Vector3(0.6f, 0.18f, 0.42f), true);
			lot.summit_route.push_back(Vector3(sx, y + 0.18f, z));
		}
		lot.summit_route.push_back(Vector3(sx, eave + r + 0.24f, (zf + zb) * 0.5f));
		const float cx = h.house_x + float(h.chimney_side) * (h.width * 0.5f - 0.9f);
		lot.summit_route.push_back(Vector3(cx, eave + r + 1.38f, (zf + zb) * 0.5f));
	}

	// Fences: right side always, left side only on the first lot of a row (no doubled fences);
	// low front picket for tidy personas, with gaps for the path and driveway.
	if (h.fence_type != 0) {
		const float fx = lot.width * 0.5f - 0.3f;
		const int fence_wood = mat_cell(h.fence_cell, mat::WOOD);
		const int fence_plank = mat_cell(h.fence_cell, mat::PLANKS);
		m.set_ground_ao(true);
		auto run = [&](Vector3 a, Vector3 b, float height, bool gap_path) {
			const Vector3 d = b - a;
			const float len = d.length();
			const Vector3 dir = d / len;
			const float post_step = 1.8f;
			auto blocked = [&](const Vector3 &p) {
				if (!gap_path) {
					return false;
				}
				return std::abs(p.x - h.door_x) < 0.9f || std::abs(p.x - lot.drive_x) < 1.7f;
			};
			for (float s = 0.0f; s <= len + 0.01f; s += post_step) {
				const Vector3 p = a + dir * s;
				if (blocked(p)) {
					continue;
				}
				hc.rbox(Vector3(p.x, height * 0.55f, p.z), Vector3(0.075f, height * 0.55f, 0.075f), 0.02f, fence_wood);
				m.blob(Vector3(p.x, height * 1.12f, p.z), Vector3(0.08f, 0.06f, 0.08f), fence_wood, 0);
			}
			if (h.fence_type == 1) {
				for (float s = 0.12f; s < len; s += 0.24f) {
					const Vector3 p = a + dir * s;
					if (blocked(p)) {
						continue;
					}
					m.set_uv_axes(dir, Vector3(0, -1, 0));
					hc.box(Vector3(p.x, height * 0.45f, p.z), Vector3(0.05f, height * 0.45f, 0.05f), fence_plank);
					m.clear_uv_axes();
					const Vector3 side = Vector3(dir.z, 0, -dir.x);
					m.tri_out(Vector3(p.x - dir.x * 0.05f, height * 0.9f, p.z - dir.z * 0.05f), Vector3(p.x + dir.x * 0.05f, height * 0.9f, p.z + dir.z * 0.05f),
							Vector3(p.x, height * 0.99f, p.z), fence_plank, Vector3(p.x, 0, p.z) + side);
					m.tri_out(Vector3(p.x - dir.x * 0.05f, height * 0.9f, p.z - dir.z * 0.05f), Vector3(p.x + dir.x * 0.05f, height * 0.9f, p.z + dir.z * 0.05f),
							Vector3(p.x, height * 0.99f, p.z), fence_plank, Vector3(p.x, 0, p.z) - side);
				}
			}
			for (float rail_y : { height * 0.3f, height * 0.72f }) {
				const bool add_collision = rail_y < height * 0.5f;
				float seg_start = -1.0f;
				for (float s = 0.0f; s <= len + 0.2f; s += 0.2f) {
					const bool open = s > len || blocked(a + dir * s);
					if (!open && seg_start < 0.0f) {
						seg_start = s;
					}
					if (open && seg_start >= 0.0f) {
						const Vector3 p0 = a + dir * seg_start, p1 = a + dir * Math::min(s, len);
						const Vector3 mid = (p0 + p1) * 0.5f;
						const float hl = (p1 - p0).length() * 0.5f;
						m.beam(Vector3(p0.x, rail_y, p0.z), Vector3(p1.x, rail_y, p1.z), 0.07f, 0.09f, fence_wood);
						if (add_collision) {
							ctx.add_box(lot.index, Vector3(mid.x, height * 0.5f, mid.z), Vector3(std::abs(dir.x) * hl + 0.06f, height * 0.5f, std::abs(dir.z) * hl + 0.06f), false);
							ctx.boxes.back().shot_through = true;
						}
						seg_start = -1.0f;
					}
				}
			}
		};
		const float fh = h.fence_type == 1 ? 1.0f : 1.1f;
		// Each run is a choice of its own (a separate stream, so the rest of the layout is
		// unchanged): a side between houses is left open, fences just the backyard (from behind the
		// house), or runs the whole depth of the lot.
		Rng frng = ctx.rng(STAGE_HOUSE, uint64_t(lot.index), 0xFE7CEu);
		const float behind = Math::min(-0.8f, h.front_z - h.depth - 0.3f);
		auto side_run = [&](float x) {
			const float r = frng.range(0.0f, 1.0f);
			if (r < 0.35f) {
				return;
			}
			const float z0 = r < 0.65f ? behind : -0.8f;
			if (z0 - (-lot.depth + 0.4f) > 1.0f) {
				run(Vector3(x, 0, z0), Vector3(x, 0, -lot.depth + 0.4f), fh, false);
			}
		};
		side_run(fx);
		bool first_in_row = true;
		for (const Lot &o : ctx.lots) {
			if (o.side == lot.side && o.index == lot.index - 1) {
				first_in_row = false;
			}
		}
		if (first_in_row) {
			side_run(-fx);
		}
		// A low front picket: most tidy households, now and then anyone else.
		const bool tidy = lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs || lot.persona == Persona::Bakers;
		if (frng.chance(tidy ? 0.75f : 0.15f)) {
			run(Vector3(-fx, 0, -0.5f), Vector3(fx, 0, -0.5f), 0.75f, true);
		}
		m.set_ground_ao(false);
	}
	// Lawn: painted grass with a slightly different tone per lot so yards read as separate.
	const int lawn = (lot.index % 3 == 0) ? pal::LAWN_DARK : (lot.index % 3 == 1 ? pal::LAWN : pal::MOSS);
	const float hw = lot.width * 0.5f;
	m.quad_out(Vector3(-hw, 0.004f, 0), Vector3(hw, 0.004f, 0), Vector3(hw, 0.004f, -lot.depth), Vector3(-hw, 0.004f, -lot.depth),
			mat_cell(lawn, mat::GRASS), Vector3(0, -1, -lot.depth * 0.5f));
}

// ---------------------------------------------------------------- routes (guided, not linear)
void plan_routes(GenContext &ctx, Lot &lot) {
	const HouseSpec &h = lot.house;
	const float deck = h.deck_h;
	const float zf = h.front_z, zb = h.front_z - h.depth;
	const float ps = float(lot.passage_side);
	const float fence = lot.width * 0.5f - 0.3f;
	const float house_edge_ps = h.house_x + ps * h.width * 0.5f;
	const float passage_x = (house_edge_ps + ps * fence) * 0.5f;
	float other_x;
	if (h.garage) {
		const float gedge = h.house_x - ps * (h.width * 0.5f + 3.8f);
		other_x = (gedge - ps * fence) * 0.5f;
	} else {
		other_x = lot.drive_x;
	}
	const float steps_front = zf + h.porch_d + 3 * 0.36f;
	lot.loop_route = {
		Vector3(h.door_x, 0, -0.4f),
		Vector3(h.door_x, 0, steps_front + 1.0f),
		Vector3(h.door_x, deck, zf + h.porch_d - 0.6f),
		Vector3(h.porch_x + ps * (h.porch_w * 0.5f - 0.6f), deck, zf + 0.9f),
		Vector3(passage_x, 0, zf - 0.3f),
		Vector3(passage_x, 0, zb + 0.6f),
		Vector3(h.house_x, 0, zb - 3.2f),
		Vector3(other_x, 0, zb + 0.6f),
		Vector3(other_x, 0, zf + (h.garage ? 0.4f : 1.2f)),
		Vector3(lot.drive_x, 0, -0.4f),
	};
	// Porch-roof branch, starting from the rail at the driveway end.
	const float ds = -ps;
	const float roof_top = porch_roof_top(h);
	lot.roof_route = {
		Vector3(h.porch_x + ds * (h.porch_w * 0.5f - 0.15f), deck + 0.96f, zf + h.porch_d * 0.6f),
		Vector3(h.porch_x + ds * (h.porch_w * 0.5f - 0.7f), roof_top, zf + h.porch_d * 0.5f + 0.1f),
		Vector3(h.porch_x, roof_top, zf + h.porch_d * 0.5f + 0.2f),
		Vector3(h.porch_x - ds * (h.porch_w * 0.5f - 0.7f), roof_top, zf + h.porch_d * 0.5f + 0.1f),
	};
	if (!lot.summit_route.empty()) {
		lot.summit_route.insert(lot.summit_route.begin(), Vector3(h.porch_x, roof_top, zf + 0.5f));
	}
}

} // namespace tot
