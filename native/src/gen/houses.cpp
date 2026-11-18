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
	if (lot.prev >= 0 && lot.prev < lot.index && ctx.lots[lot.prev].house.wall_cell == WALLS[wall]) {
		wall = (wall + 3) % 10;
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
	if (lot.prev >= 0) {
		const Lot &o = ctx.lots[lot.prev];
		if (o.house.fence_type != 0) {
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
	// The building's own solids stay with the pad (Stage 3c: pushed as is; add_box would lift a piece that pokes out
	// past the footprint, a deck railing over the gable overhang, by the ground under it).
	void solid(Vector3 c, Vector3 half, bool walkable) {
		OBox b;
		b.xf = lot.xf * Transform3D(Basis(), c);
		b.half = half;
		b.walkable = walkable;
		b.lot = lot.index;
		ctx.boxes.push_back(b);
	}
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

// Stage 3c: a flat walkable deck on a truncated ridge (cut_f > 0: the slopes stop at that fraction of the rise and
// a plank deck with a trimmed edge spans between them; the gable ends become trapezoids). Its top, centre and half
// sizes go to the lot (Lot::deck_c / deck_half).
struct DeckStyle {
	float cut_f = 0.0f;
	bool rail = false; // a widow's-walk railing round it (an opening where the climb arrives)
	float open_x = 0.0f; // lot-local x of that opening (the climb's last step; a ridge along x)
	float open_z = 0.0f; // ... or its z on the side edge open_side (a ridge along z)
	int open_side = 1;
};

void gable_roof(HouseCtx &hc, float x0, float x1, float z0, float z1, float base_y, bool ridge_x, float pitch, int roof_mat, int roof_cell,
		int wall_cell, int trim_cell, bool collide, bool gingerbread, const DeckStyle &deck = DeckStyle()) {
	const float o = 0.45f; // overhang
	const float thick = 0.22f;
	MeshBuilder &m = hc.m;
	const int roof = mat_cell(roof_cell, roof_mat);
	const int trim = mat_cell(trim_cell, mat::WOOD);
	const int gutter = mat_cell(pal::SILVER, mat::METAL);
	const float rr_full = std::tan(pitch) * ((ridge_x ? z1 - z0 : x1 - x0) * 0.5f);
	const float f = deck.cut_f > 0.0f ? deck.cut_f : 1.0f;
	const float rr = rr_full * f;
	const float top = base_y + rr; // the ridge, or the deck's line on the slopes
	const float ye = base_y - o * std::tan(pitch);
	if (deck.cut_f > 0.0f && collide) {
		// The deck: plank boards over the cut, its edge trimmed, overhanging the shingles' top edge a little.
		const float span = (ridge_x ? z1 - z0 : x1 - x0) * 0.5f;
		const float half_in = span * (1.0f - f) + thick * std::sin(pitch) + 0.06f; // half width across the ridge line
		const float dtop = top + thick * std::cos(pitch) + 0.07f;
		const float half_along = (ridge_x ? x1 - x0 : z1 - z0) * 0.5f + o + 0.04f;
		const Vector3 dc((x0 + x1) * 0.5f, dtop - 0.11f, (z0 + z1) * 0.5f);
		const Vector3 dh = ridge_x ? Vector3(half_along, 0.11f, half_in) : Vector3(half_in, 0.11f, half_along);
		m.set_uv_axes(ridge_x ? Vector3(1, 0, 0) : Vector3(0, 0, 1), ridge_x ? Vector3(0, 0, 1) : Vector3(1, 0, 0));
		hc.rbox(dc, dh, 0.04f, trim, mat_cell(pal::PLANK, mat::PLANKS));
		m.clear_uv_axes();
		// A darker board border just inside the trim, so the deck reads as built, not sliced off.
		for (float sgn : { -1.0f, 1.0f }) {
			const Vector3 bc = ridge_x ? Vector3(dc.x, dtop + 0.005f, dc.z + sgn * (dh.z - 0.12f)) : Vector3(dc.x + sgn * (dh.x - 0.12f), dtop + 0.005f, dc.z);
			hc.box(bc, ridge_x ? Vector3(dh.x - 0.05f, 0.012f, 0.07f) : Vector3(0.07f, 0.012f, dh.z - 0.05f), mat_cell(pal::WOOD_DARK, mat::PLANKS));
		}
		hc.solid(dc, dh, true);
		hc.lot.deck_c = Vector3(dc.x, dtop, dc.z);
		hc.lot.deck_half = Vector3(dh.x, 0.0f, dh.z);
		if (deck.rail) {
			// A low widow's-walk railing: turned balusters under a chunky cap, open where the climb arrives.
			const float ry = dtop, rh = 0.62f;
			auto run = [&](Vector3 a, Vector3 b) {
				const Vector3 d = b - a;
				const float len = d.length();
				if (len < 0.4f) {
					return;
				}
				const Vector3 mid = (a + b) * 0.5f;
				const bool ax = std::abs(d.x) > std::abs(d.z);
				hc.rbox(Vector3(mid.x, ry + rh, mid.z), ax ? Vector3(len * 0.5f + 0.06f, 0.05f, 0.08f) : Vector3(0.08f, 0.05f, len * 0.5f + 0.06f), 0.02f, trim);
				const int nb = Math::max(2, int(len / 0.32f));
				for (int i = 0; i <= nb; ++i) {
					const Vector3 p = a + d * (float(i) / float(nb));
					m.cylinder(Vector3(p.x, ry, p.z), i == 0 || i == nb ? 0.06f : 0.03f, rh, 6, trim, 0.02f);
				}
				hc.solid(Vector3(mid.x, ry + rh * 0.5f, mid.z), ax ? Vector3(len * 0.5f, rh * 0.5f, 0.06f) : Vector3(0.06f, rh * 0.5f, len * 0.5f), false);
			};
			const float ex = dh.x - 0.1f, ez = dh.z - 0.1f;
			const Vector3 c00(dc.x - ex, 0, dc.z - ez), c10(dc.x + ex, 0, dc.z - ez), c11(dc.x + ex, 0, dc.z + ez), c01(dc.x - ex, 0, dc.z + ez);
			// An opening where the climb arrives: on the front edge (+z, toward the street) at open_x when the ridge
			// runs along x; on the side edge open_side (+-x) at open_z when it runs along z (the climb comes up a side slope).
			const float gap = 0.75f;
			if (ridge_x) {
				const float ox = Math::clamp(deck.open_x, c01.x + gap + 0.3f, c11.x - gap - 0.3f);
				run(c01, Vector3(ox - gap, 0, c01.z));
				run(Vector3(ox + gap, 0, c11.z), c11);
				run(c00, c01);
				run(c10, c11);
			} else {
				const float oz = Math::clamp(deck.open_z, c00.z + gap + 0.3f, c01.z - gap - 0.3f);
				const Vector3 a = deck.open_side > 0 ? c10 : c00, b = deck.open_side > 0 ? c11 : c01;
				run(a, Vector3(a.x, 0, oz - gap));
				run(Vector3(b.x, 0, oz + gap), b);
				run(deck.open_side > 0 ? c00 : c10, deck.open_side > 0 ? c01 : c11);
				run(c01, c11);
			}
			run(c00, c10);
		}
	}
	if (ridge_x) {
		const float zm = (z0 + z1) * 0.5f;
		for (float xe : { x0, x1 }) {
			if (deck.cut_f > 0.0f) {
				const float zt0 = z0 + (zm - z0) * f, zt1 = z1 + (zm - z1) * f;
				m.quad_out(Vector3(xe, base_y, z0), Vector3(xe, base_y, z1), Vector3(xe, top, zt1), Vector3(xe, top, zt0), wall_cell, Vector3((x0 + x1) * 0.5f, base_y, zm));
			} else {
				m.tri_out(Vector3(xe, base_y, z0), Vector3(xe, base_y, z1), Vector3(xe, top, zm), wall_cell, Vector3((x0 + x1) * 0.5f, base_y, zm));
			}
		}
		for (float s : { 1.0f, -1.0f }) {
			const float ze = s > 0 ? z1 + o : z0 - o;
			const float zr = deck.cut_f > 0.0f ? (s > 0 ? z1 + (zm - z1) * f : z0 + (zm - z0) * f) : zm;
			Vector3 e0(x0 - o, ye, ze), e1(x1 + o, ye, ze), r0(x0 - o, top, zr), r1(x1 + o, top, zr);
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
				m.beam(Vector3(xe + side * 0.03f, ye - 0.06f, ze), Vector3(xe + side * 0.03f, top + 0.05f, zr), 0.08f, 0.26f, trim, nrm);
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
		if (deck.cut_f <= 0.0f) {
			m.rounded_box(Vector3((x0 + x1) * 0.5f, top + 0.12f, zm), Vector3((x1 - x0) * 0.5f + o + 0.05f, 0.12f, 0.28f), 0.05f, trim, roof);
		}
		if (collide && deck.cut_f <= 0.0f) {
			hc.solid(Vector3((x0 + x1) * 0.5f, top + 0.12f, zm), Vector3((x1 - x0) * 0.5f + o, 0.14f, 0.32f), true);
		}
	} else {
		const float xm = (x0 + x1) * 0.5f;
		for (float ze : { z0, z1 }) {
			if (deck.cut_f > 0.0f) {
				const float xt0 = x0 + (xm - x0) * f, xt1 = x1 + (xm - x1) * f;
				m.quad_out(Vector3(x0, base_y, ze), Vector3(x1, base_y, ze), Vector3(xt1, top, ze), Vector3(xt0, top, ze), wall_cell, Vector3(xm, base_y, (z0 + z1) * 0.5f));
			} else {
				m.tri_out(Vector3(x0, base_y, ze), Vector3(x1, base_y, ze), Vector3(xm, top, ze), wall_cell, Vector3(xm, base_y, (z0 + z1) * 0.5f));
			}
		}
		for (float s : { 1.0f, -1.0f }) {
			const float xe = s > 0 ? x1 + o : x0 - o;
			const float xr = deck.cut_f > 0.0f ? (s > 0 ? x1 + (xm - x1) * f : x0 + (xm - x0) * f) : xm;
			Vector3 e0(xe, ye, z0 - o), e1(xe, ye, z1 + o), r0(xr, top, z0 - o), r1(xr, top, z1 + o);
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
				m.beam(Vector3(xe, ye - 0.06f, ze + side * 0.03f), Vector3(xr, top + 0.05f, ze + side * 0.03f), 0.08f, 0.26f, trim, nrm);
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
		if (deck.cut_f <= 0.0f) {
			m.rounded_box(Vector3(xm, top + 0.12f, (z0 + z1) * 0.5f), Vector3(0.28f, 0.12f, (z1 - z0) * 0.5f + o + 0.05f), 0.05f, trim, roof);
		}
		if (collide && deck.cut_f <= 0.0f) {
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
	if (ctx.kind != SettlementKind::Town && !ctx.terrain && lot.index == 0) {
		plan_hood_terrain(ctx); // Stage 3b: every house plan is known now; grade before anything is built
	}
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
	// Stage 3b: the lowest ground under a footprint (lot-local, <= 0), sampled every ~0.4 m edges included.
	auto ground_min = [&](float gx0, float gx1, float gz0, float gz1) {
		float g = 0.0f;
		const int nx = Math::max(1, int(std::ceil((gx1 - gx0) / 0.4f))), nz = Math::max(1, int(std::ceil((gz1 - gz0) / 0.4f)));
		for (int i = 0; i <= nx; ++i) {
			for (int k = 0; k <= nz; ++k) {
				g = Math::min(g, ctx.lot_ground(lot, Math::lerp(gx0, gx1, float(i) / nx), Math::lerp(gz0, gz1, float(k) / nz)));
			}
		}
		return g;
	};
	// A painted stone foundation from the floor down past the lowest ground under it (no gap ever shows under
	// a building on a slope); solid, so nothing walks under the floor.
	auto foundation = [&](float fx0, float fx1, float fz0, float fz1, int cell) {
		const float g = ground_min(fx0 - 0.1f, fx1 + 0.1f, fz0 - 0.1f, fz1 + 0.1f);
		if (g > -0.001f) {
			return 0.0f; // level ground all round: the slab sits on it
		}
		const float bottom = g - 0.3f, top = 0.06f;
		const Vector3 c((fx0 + fx1) * 0.5f, (top + bottom) * 0.5f, (fz0 + fz1) * 0.5f), half((fx1 - fx0) * 0.5f, (top - bottom) * 0.5f, (fz1 - fz0) * 0.5f);
		hc.rbox(c, half, 0.03f, cell);
		hc.solid(c, half, false);
		return bottom;
	};

	// Foundation + body (two-tone when the upper storey differs).
	hc.rbox(Vector3(h.house_x, 0.2f, (zf + zb) * 0.5f), Vector3(h.width * 0.5f + 0.12f, 0.2f, h.depth * 0.5f + 0.12f), 0.04f, mat_cell(pal::STONE, mat::STONE));
	lot.found_house = foundation(h.house_x - h.width * 0.5f - 0.06f, h.house_x + h.width * 0.5f + 0.06f, zb - 0.06f, zf + 0.06f, mat_cell(pal::STONE_DARK, mat::STONE));
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
	// The lattice skirt reaches down to the ground (Stage 3b: below the pad where the yard falls away).
	const float porch_g = ground_min(px0, px1, pz0, pz1) - (ctx.terrain ? 0.2f : 0.0f);
	lot.found_porch = porch_g;
	hc.box(Vector3(h.porch_x, (deck - 0.08f + porch_g) * 0.5f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f - 0.05f, (deck - 0.08f - porch_g) * 0.5f, h.porch_d * 0.5f - 0.05f),
			mat_cell(h.trim_cell, mat::LATTICE));
	m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, 0, 1));
	hc.rbox(Vector3(h.porch_x, deck - 0.07f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f, 0.07f, h.porch_d * 0.5f), 0.03f, mat_cell(pal::PLANK, mat::PLANKS));
	m.clear_uv_axes();
	hc.solid(Vector3(h.porch_x, (deck + porch_g) * 0.5f, (pz0 + pz1) * 0.5f), Vector3(h.porch_w * 0.5f, (deck - porch_g) * 0.5f, h.porch_d * 0.5f), true);
	// Wraparound steps: each lower step also runs round both sides of the one above, so the kid can
	// walk up from the front or from either side (no getting stuck against the side of the flight).
	// Stage 3b: where the path drops away below the pad, the flight carries on down to the ground.
	// Stage 3c: the flight never gives up: it keeps adding treads (straight on at the bottom step's width) until it
	// meets the ground, with every riser equal (the deck's 3 risers stretched or shrunk to the drop, at most
	// RISER_MAX) and a deeper landing every 8 treads, so no ledge is ever left above the ground.
	const float step_d = 0.36f, step_w = 1.7f, hw_bottom = step_w * 0.5f + step_d * 2.0f;
	constexpr float RISER_MAX = 0.21f, LANDING = 1.1f;
	auto tread_depth = [&](int i) { return i >= 8 && i % 8 == 0 ? LANDING : step_d; };
	// The ground where you step off the flight's foot (lot-local, at the door axis just past the last tread).
	auto foot_ground = [&](float z) { return ctx.lot_ground(lot, h.door_x, z + 0.15f); };
	int steps = 3;
	float foot = pz1 + step_d * 3.0f;
	float riser = deck / 3.0f;
	if (ctx.terrain) {
		for (int n = 3; n <= 60; ++n) {
			if (n > 3) {
				foot += tread_depth(n - 1);
			}
			const float drop = deck - foot_ground(foot);
			steps = n;
			riser = drop / float(n);
			if (riser <= RISER_MAX || foot > -0.3f) { // (at the sidewalk the flight ends whatever is left)
				break;
			}
		}
		riser = Math::max(riser, 0.05f);
	}
	lot.porch_steps = steps;
	lot.flight_riser = riser;
	lot.flight_foot = foot;
	lot.flight.clear();
	{
		float z = pz1;
		for (int i = 0; i < steps; ++i) {
			Lot::Tread t;
			t.top = deck - riser * float(i);
			t.hw = step_w * 0.5f + step_d * Math::min(i, 2);
			// The deck's own three wrap round from the porch front (each lower one a block out to its edge).
			t.z0 = i < 3 ? pz1 : z;
			t.z1 = i < 3 ? pz1 + step_d * float(i + 1) : z + tread_depth(i);
			z = t.z1;
			lot.flight.push_back(t);
		}
	}
	for (int i = 0; i < steps; ++i) {
		const Lot::Tread &t = lot.flight[i];
		const float top = t.top, hw = t.hw, depth = t.z1 - t.z0;
		const float bottom = ctx.terrain ? Math::min(0.0f, ground_min(h.door_x - hw, h.door_x + hw, t.z0, t.z1) - 0.15f) : 0.0f;
		if (top - bottom < 0.03f) {
			continue;
		}
		const Vector3 c(h.door_x, (top + bottom) * 0.5f, (t.z0 + t.z1) * 0.5f);
		hc.rbox(c, Vector3(hw, (top - bottom) * 0.5f, depth * 0.5f), 0.025f, mat_cell(pal::PLANK, mat::PLANKS), mat_cell(pal::WOOD_LIGHT, mat::PLANKS));
		if (i < 3) {
			hc.solid(c, Vector3(hw, (top - bottom) * 0.5f, depth * 0.5f), true);
		} else {
			// (pushed as is: add_box would lift a box out on the yard by the ground under it)
			OBox b;
			b.xf = lot.xf * Transform3D(Basis(), c);
			b.half = Vector3(hw, (top - bottom) * 0.5f, depth * 0.5f);
			b.walkable = true;
			b.lot = lot.index;
			ctx.boxes.push_back(b);
		}
		// A trim nosing along the front and side edges of each tread.
		m.box(Vector3(h.door_x, top - 0.02f, t.z1 + 0.01f), Vector3(hw + 0.01f, 0.03f, 0.02f), trim);
		for (float sx : { -1.0f, 1.0f }) {
			m.box(Vector3(h.door_x + sx * (hw + 0.01f), top - 0.02f, (t.z0 + t.z1) * 0.5f), Vector3(0.02f, 0.03f, depth * 0.5f + 0.01f), trim);
		}
		if (i >= 8 && i % 8 == 0) {
			// A landing: newel posts with ball caps at its corners, so the long flight reads as built that way.
			for (float sx : { -1.0f, 1.0f }) {
				const Vector3 np(h.door_x + sx * (hw - 0.08f), top, t.z1 - 0.1f);
				hc.rbox(np + Vector3(0, 0.3f, 0), Vector3(0.07f, 0.3f, 0.07f), 0.02f, trim);
				m.blob(np + Vector3(0, 0.66f, 0), Vector3(0.08f, 0.07f, 0.08f), trim, 0);
			}
		}
	}
	// Candy anchor: the front walk from the sidewalk edge up the walk to the foot of the steps, then over each
	// tread's centre to the top step.
	lot.front_walk = { Vector3(h.door_x, 0.0f, 0.0f), Vector3(h.door_x, 0.0f, foot) };
	for (int i = steps - 1; i >= 0; --i) {
		const Lot::Tread &t = lot.flight[i];
		lot.front_walk.push_back(Vector3(h.door_x, t.top, i < 3 ? pz1 + step_d * (i + 0.5f) : (t.z0 + t.z1) * 0.5f));
	}
	// Candy anchor: the wraparound flight itself, for a small cluster flanking it.
	{
		YardFeature f;
		f.kind = FeatureKind::Steps;
		f.c = Vector3(h.door_x, 0.0f, (pz1 + foot) * 0.5f);
		f.r = hw_bottom;
		f.r_z = (foot - pz1) * 0.5f;
		lot.features.push_back(f);
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
	const float mouth = step_w * 0.5f + step_d * 2.0f + 0.05f;
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
		lot.found_garage = foundation(gx - GARAGE_W * 0.5f - 0.03f, gx + GARAGE_W * 0.5f + 0.03f, gz0 - 0.03f, gz1 + 0.03f, mat_cell(pal::STONE_DARK, mat::STONE));
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
		// Stage 3b: laid on the ground (the terrain lattice clipped to the slab), edging beams in short pieces.
		drape_lot_rect(ctx, m, lot, lot.drive_x - hw, lot.drive_x + hw, dz1, 0.0f, 0.025f, mat_cell(h.drive_mat == mat::PAVERS ? pal::STONE_LIGHT : pal::SIDEWALK, h.drive_mat));
		for (float sx : { -1.0f, 1.0f }) {
			const float ex = lot.drive_x + sx * (hw + 0.06f);
			const int pieces = Math::max(1, int(std::ceil(-dz1 / 1.0f)));
			for (int k = 0; k < pieces; ++k) {
				const float za = dz1 * float(k) / pieces, zb2 = dz1 * float(k + 1) / pieces;
				m.beam(Vector3(ex, 0.03f + ctx.lot_ground(lot, ex, za), za), Vector3(ex, 0.03f + ctx.lot_ground(lot, ex, zb2), zb2), 0.12f, 0.06f, mat_cell(pal::STONE, mat::STONE));
			}
		}
		// Candy anchor: the driveway's centreline from the sidewalk edge to the garage door or the slab's
		// end (yards.cpp shortens it when it parks a car or stacks bales on it).
		lot.drive_line = { Vector3(lot.drive_x, 0.0f, 0.0f), Vector3(lot.drive_x, 0.0f, dz1) };
		// Apron across the sidewalk and verge down to the curb (further on a bulb, the inside of a bend, or a
		// lot set back from the sidewalk).
		const float apron = ctx.kind == SettlementKind::Town ? ctx.road_of(lot).walk : frontage_reach(ctx, lot, lot.drive_x);
		drape_lot_rect(ctx, m, lot, lot.drive_x - hw, lot.drive_x + hw, 0.0f, apron, 0.08f, mat_cell(pal::SIDEWALK, mat::CONCRETE));
	}
	// Luminaria: glowing paper bags lining the front path for the cosiest households.
	if (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::CraftyArtists || lot.persona == Persona::Stargazers ||
			lot.landmark == Landmark::LanternPorch || rng.chance(0.2f)) {
		const int bag = mat_cell(rng.chance(0.5f) ? pal::GLOW_LANTERN : pal::GLOW_CANDLE, mat::PAPER);
		for (float z = -1.0f; z > lot.flight_foot + 0.3f; z -= 1.1f) {
			for (float sx : { -1.0f, 1.0f }) {
				// Stage 3c: the bag's foot on the ground (its lowest corner), so none hovers on a slope.
				const float x = h.door_x + sx * 0.75f;
				const float g = Math::min(Math::min(ctx.lot_ground(lot, x - 0.09f, z - 0.09f), ctx.lot_ground(lot, x + 0.09f, z - 0.09f)),
						Math::min(ctx.lot_ground(lot, x - 0.09f, z + 0.09f), ctx.lot_ground(lot, x + 0.09f, z + 0.09f)));
				m.rounded_box(Vector3(x, 0.13f + g, z), Vector3(0.09f, 0.13f, 0.09f), 0.02f, bag, mat_cell(pal::CHARCOAL, mat::FLAT));
			}
		}
	}
	// Stage 3c: the front walk, flagstones laid unbroken (6 cm joints) from the foot of the flight to the sidewalk's
	// edge, each on the ground's plane under it (the walk follows the hill, flush, and always reaches the street).
	lot.walk_flags.clear();
	{
		// Out to the sidewalk itself: a lot set back from it (on a bend, round a bulb) has a strip of ground between.
		float z_end = 0.0f;
		if (ctx.kind != SettlementKind::Town) {
			auto on_walk = [&](float z) {
				const RoadZone zn = ctx.roads.zone(lot.to_world(h.door_x, 0.0f, z));
				return zn == RoadZone::Walk || zn == RoadZone::Verge || zn == RoadZone::Curb || zn == RoadZone::Asphalt;
			};
			while (z_end < 6.0f && !on_walk(z_end + 0.05f)) {
				z_end += 0.1f;
			}
		}
		lot.walk_end = z_end;
		const float len = z_end - lot.flight_foot; // the foot is lot-local z < 0; the sidewalk's edge is z = 0 (or z_end)
		if (len > 0.1f) {
			const int n = Math::max(1, int(std::ceil((len + 0.06f) / 0.62f)));
			const float pitch = len / float(n), hl = pitch * 0.5f - 0.03f;
			auto surf = [&](float x, float z) { return ctx.lot_ground(lot, x, z); };
			for (int i = 0; i < n; ++i) {
				const float z = lot.flight_foot + pitch * (float(i) + 0.5f);
				const float sxj = h.door_x + rng.range(-0.08f, 0.08f);
				const float yaw = rng.range(-0.05f, 0.05f);
				const float hw = 0.42f;
				const GroundFit f = fit_footprint(surf, Vector3(sxj, 0.0f, z), yaw, 1.0f, -hw, hw, -hl, hl, 0.45f, 3);
				Transform3D save = m.transform();
				m.set_transform(Transform3D(tilt_to(f.up) * Basis(Vector3(0, 1, 0), yaw), Vector3(sxj, f.y, z)));
				hc.rbox(Vector3(0, 0.03f, 0), Vector3(hw, 0.04f, hl), 0.03f, mat_cell(pal::STONE_LIGHT, mat::STONE), mat_cell(pal::BONE, mat::STONE));
				m.set_transform(save);
				lot.walk_flags.push_back({ Vector3(sxj, f.y, z), hl, hw, yaw });
			}
		}
	}

	// Main roof, dormer and chimney. Stage 3c: the ridge is cut off into a flat walkable deck (a widow's walk with a
	// railing on some), its line varied per house, kept behind a dormer, never narrower than ~2.3 m; a climb of plank
	// steps up the slope above the porch roof (after a window-box ledge on a two-storey front) reaches it.
	Rng drng = ctx.rng(STAGE_HOUSE, lot.index, 0xDEC);
	const float span = (h.ridge_along_x ? h.depth : h.width) * 0.5f;
	float cut = drng.range(0.46f, 0.62f);
	if (h.dormer) {
		cut = Math::max(cut, 2.3f / span);
	}
	cut = Math::min(cut, 1.0f - 1.15f / span);
	DeckStyle dstyle;
	dstyle.cut_f = cut;
	dstyle.rail = drng.chance(h.gingerbread || h.storeys == 2 ? 0.6f : 0.3f);
	// The climb: over the porch, toward the chimney side (a ridge along x: up the front slope); a ridge along z: up the
	// side slope on the porch's side, just behind the front gable.
	const float climb_x = Math::clamp(h.house_x + float(h.chimney_side) * (h.width * 0.5f - 2.4f), px0 + 0.7f, px1 - 0.7f);
	const int climb_side = h.porch_x >= h.house_x ? 1 : -1;
	dstyle.open_x = climb_x;
	dstyle.open_side = climb_side;
	dstyle.open_z = zf - 0.7f;
	gable_roof(hc, x0, x1, zb, zf, eave, h.ridge_along_x, pitch, h.roof_mat, h.roof_cell, mat_cell(h.upper_cell, h.upper_mat), h.trim_cell, true, h.gingerbread, dstyle);
	const float r = rise(h) * cut; // the deck's line over the eave (Stage 3c: was the ridge)
	const float deck_top = lot.deck_c.y;
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
		const float ctop = deck_top + 1.05f; // (Stage 3c: a stack standing up off the deck)
		hc.rbox(Vector3(cx, (eave + ctop) * 0.5f, cz), Vector3(0.42f, (ctop - eave) * 0.5f, 0.42f), 0.04f, mat_cell(pal::BRICK, mat::BRICK),
				mat_cell(pal::BRICK_DARK, mat::BRICK));
		hc.rbox(Vector3(cx, ctop + 0.06f, cz), Vector3(0.52f, 0.08f, 0.52f), 0.03f, mat_cell(pal::STONE_LIGHT, mat::STONE));
		m.cylinder(Vector3(cx + 0.15f, ctop + 0.12f, cz), 0.12f, 0.3f, 8, mat_cell(pal::TERRACOTTA, mat::STUCCO), 0.1f);
		hc.solid(Vector3(cx, (eave + ctop) * 0.5f + 0.06f, cz), Vector3(0.5f, (ctop - eave) * 0.5f + 0.07f, 0.5f), true);
	}
	// The climb to the deck (Stage 3c: every house; landmarks' summit route runs up it to the chimney top). A
	// window-box ledge on a two-storey front first, then plank steps up the slope, the last a jump below the deck.
	{
		std::vector<Vector3> climb;
		const float lz_ledge = zf + 0.35f;
		const float sx = h.ridge_along_x ? climb_x : Math::clamp(h.house_x + float(climb_side) * (h.width * 0.5f - 0.8f), px0 + 0.6f, px1 - 0.6f);
		if (h.storeys == 2) {
			const float ly = h.storey_h + 1.35f;
			const float lx = Math::clamp(sx, px0 + 0.6f, px1 - 0.6f);
			hc.rbox(Vector3(lx, ly, lz_ledge), Vector3(0.8f, 0.14f, 0.35f), 0.04f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::HEDGE, mat::HEDGE));
			m.blob(Vector3(lx - 0.35f, ly + 0.2f, lz_ledge), Vector3(0.22f, 0.16f, 0.2f), pal::MAPLE_ORANGE, 0);
			m.blob(Vector3(lx + 0.35f, ly + 0.2f, lz_ledge), Vector3(0.22f, 0.16f, 0.2f), pal::CANDY_PINK, 0);
			hc.solid(Vector3(lx, ly, zf + 0.4f), Vector3(0.8f, 0.14f, 0.4f), true);
			climb.push_back(Vector3(lx, ly + 0.14f, zf + 0.4f));
		}
		const int n = std::max(1, int(std::ceil((deck_top - eave) / 1.3f)));
		for (int k = 0; k < n; ++k) {
			const float t = (k + 0.35f) / (float(n) + 0.35f); // (the last one a short hop below the deck's edge, clear of it)
			const float y = eave + r * t + 0.45f;
			Vector3 c;
			Vector3 half;
			if (h.ridge_along_x) {
				c = Vector3(sx, y, Math::lerp(zf + 0.2f, zf - span * cut, t));
				half = Vector3(0.6f, 0.18f, 0.42f);
				m.set_uv_axes(Vector3(1, 0, 0), Vector3(0, 0, 1));
			} else {
				const float xw = climb_side > 0 ? x1 : x0;
				c = Vector3(Math::lerp(xw - float(climb_side) * 0.2f, xw - float(climb_side) * span * cut, t), y, zf - 0.7f);
				half = Vector3(0.42f, 0.18f, 0.6f);
				m.set_uv_axes(Vector3(0, 0, 1), Vector3(1, 0, 0));
			}
			hc.rbox(c, half, 0.05f, mat_cell(pal::WOOD_LIGHT, mat::PLANKS), mat_cell(pal::PLANK, mat::PLANKS));
			m.clear_uv_axes();
			hc.solid(c, half, true);
			climb.push_back(c + Vector3(0, 0.18f, 0));
		}
		const Vector3 on_deck = h.ridge_along_x ? Vector3(sx, deck_top, (zf + zb) * 0.5f) : Vector3(h.house_x, deck_top, zf - 0.7f);
		climb.push_back(on_deck);
		if (h.roof_steps) {
			lot.summit_route = climb;
			if (h.chimney) {
				const Vector3 cc = h.ridge_along_x ? Vector3(h.house_x + float(h.chimney_side) * (h.width * 0.5f - 0.9f), 0, (zf + zb) * 0.5f) : Vector3(h.house_x, 0, zb + 1.0f);
				lot.summit_route.push_back(Vector3(cc.x, deck_top + 1.05f + 0.13f, cc.z));
			}
		}
	}

	// Fences: right side always, left side only on the first lot of a row (no doubled fences);
	// low front picket for tidy personas, with gaps for the path and driveway.
	if (h.fence_type != 0) {
		const float fx = lot.width * 0.5f - 0.3f;
		const float gate_w = lot.flight_foot > -0.75f ? 0.85f + 0.72f + 0.2f : 0.9f; // (Stage 3c: wide enough for a flight that comes down to it)
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
				return std::abs(p.x - h.door_x) < gate_w || std::abs(p.x - lot.drive_x) < 1.7f;
			};
			for (float s = 0.0f; s <= len + 0.01f; s += post_step) {
				const Vector3 p = a + dir * s;
				if (blocked(p)) {
					continue;
				}
				const float gp = ctx.lot_ground(lot, p.x, p.z); // Stage 3b: each post on the ground
				hc.rbox(Vector3(p.x, height * 0.55f + gp, p.z), Vector3(0.075f, height * 0.55f, 0.075f), 0.02f, fence_wood);
				m.blob(Vector3(p.x, height * 1.12f + gp, p.z), Vector3(0.08f, 0.06f, 0.08f), fence_wood, 0);
			}
			if (h.fence_type == 1) {
				for (float s = 0.12f; s < len; s += 0.24f) {
					const Vector3 p = a + dir * s;
					if (blocked(p)) {
						continue;
					}
					const float gk = ctx.lot_ground(lot, p.x, p.z);
					const Vector3 up(0, gk, 0);
					m.set_uv_axes(dir, Vector3(0, -1, 0));
					hc.box(Vector3(p.x, height * 0.45f, p.z) + up, Vector3(0.05f, height * 0.45f, 0.05f), fence_plank);
					m.clear_uv_axes();
					const Vector3 side = Vector3(dir.z, 0, -dir.x);
					m.tri_out(Vector3(p.x - dir.x * 0.05f, height * 0.9f, p.z - dir.z * 0.05f) + up, Vector3(p.x + dir.x * 0.05f, height * 0.9f, p.z + dir.z * 0.05f) + up,
							Vector3(p.x, height * 0.99f, p.z) + up, fence_plank, Vector3(p.x, 0, p.z) + side + up);
					m.tri_out(Vector3(p.x - dir.x * 0.05f, height * 0.9f, p.z - dir.z * 0.05f) + up, Vector3(p.x + dir.x * 0.05f, height * 0.9f, p.z + dir.z * 0.05f) + up,
							Vector3(p.x, height * 0.99f, p.z) + up, fence_plank, Vector3(p.x, 0, p.z) - side + up);
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
						// Stage 3b: rails follow the ground in short pieces; collision in pieces of up to 2 m.
						const float seg_len = (p1 - p0).length();
						const int rails = ctx.terrain ? Math::max(1, int(std::ceil(seg_len / 0.9f))) : 1;
						for (int k = 0; k < rails; ++k) {
							const Vector3 q0 = p0.lerp(p1, float(k) / rails), q1 = p0.lerp(p1, float(k + 1) / rails);
							m.beam(Vector3(q0.x, rail_y + ctx.lot_ground(lot, q0.x, q0.z), q0.z), Vector3(q1.x, rail_y + ctx.lot_ground(lot, q1.x, q1.z), q1.z), 0.07f, 0.09f, fence_wood);
						}
						if (add_collision) {
							// Split only where the ground under the run changes (each piece is a collision shape in the game).
							const float rise = std::abs(ctx.lot_ground(lot, p1.x, p1.z) - ctx.lot_ground(lot, p0.x, p0.z));
							const int parts = rise > 0.2f ? Math::max(1, int(std::ceil(seg_len / 2.0f))) : 1;
							for (int k = 0; k < parts; ++k) {
								const Vector3 q0 = p0.lerp(p1, float(k) / parts), q1 = p0.lerp(p1, float(k + 1) / parts);
								const Vector3 mid = (q0 + q1) * 0.5f;
								const float hl = (q1 - q0).length() * 0.5f;
								ctx.add_box(lot.index, Vector3(mid.x, height * 0.5f, mid.z), Vector3(std::abs(dir.x) * hl + 0.06f, height * 0.5f, std::abs(dir.z) * hl + 0.06f), false);
								ctx.boxes.back().shot_through = true;
							}
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
		const bool first_in_row = lot.prev < 0;
		if (first_in_row) {
			side_run(-fx);
		}
		// A low front picket: most tidy households, now and then anyone else.
		const bool tidy = lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs || lot.persona == Persona::Bakers;
		if (frng.chance(tidy ? 0.75f : 0.15f)) {
			run(Vector3(-fx, 0, -0.5f), Vector3(fx, 0, -0.5f), 0.75f, true);
			// Candy anchor: each stretch of the low picket between the path and driveway gaps is
			// something to hop over from the sidewalk.
			std::vector<std::pair<float, float>> gaps = { { h.door_x - gate_w, h.door_x + gate_w }, { lot.drive_x - 1.7f, lot.drive_x + 1.7f } };
			std::sort(gaps.begin(), gaps.end());
			float x = -fx;
			for (size_t g = 0; g <= gaps.size(); ++g) {
				const float end = g < gaps.size() ? Math::max(-fx, gaps[g].first) : fx;
				if (end - x > 1.4f) {
					YardObstacle o;
					o.kind = ObstacleKind::Fence;
					o.c = Vector3((x + end) * 0.5f, 0.0f, -0.5f);
					o.across = Vector3(0, 0, 1);
					o.half_along = (end - x) * 0.5f;
					o.half_across = 0.1f;
					o.top = 0.75f * 1.12f + 0.06f; // post caps
					lot.obstacles.push_back(o);
				}
				if (g < gaps.size()) {
					x = Math::min(fx, Math::max(x, gaps[g].second));
				}
			}
		}
		m.set_ground_ao(false);
	}
	// Lawn: painted grass with a slightly different tone per lot so yards read as separate.
	const int lawn = (lot.index % 3 == 0) ? pal::LAWN_DARK : (lot.index % 3 == 1 ? pal::LAWN : pal::MOSS);
	const float hw = lot.width * 0.5f;
	drape_lot_rect(ctx, m, lot, -hw, hw, -lot.depth, 0.0f, 0.004f, mat_cell(lawn, mat::GRASS)); // Stage 3b: on the ground
	// Stage 3b: a painted stone retaining wall down a side whose neighbour's yard sits much lower (plan_hood_terrain
	// keeps both yards level to the line there), in 1.5 m courses that follow the ground, capped a little above
	// the higher yard; solid (and walkable on top) in 3 m pieces.
	for (int k = 0; k < 2; ++k) {
		if (lot.retain[k] <= 0.0f) {
			continue;
		}
		const float sx = k == 0 ? -1.0f : 1.0f;
		const float wx = sx * hw;
		const float z_front = -0.6f, z_back = -lot.depth;
		auto course = [&](float za, float zb2, float &top, float &bottom) {
			const float zc = (za + zb2) * 0.5f;
			top = Math::max(ctx.lot_ground(lot, wx - sx * 0.6f, za), ctx.lot_ground(lot, wx - sx * 0.6f, zb2)) + 0.3f;
			bottom = Math::min(Math::min(ctx.lot_ground(lot, wx, za), ctx.lot_ground(lot, wx, zb2)), ctx.lot_ground(lot, wx + sx * 0.6f, zc)) - 0.3f;
		};
		const int n = Math::max(1, int(std::ceil((z_front - z_back) / 1.5f)));
		for (int i = 0; i < n; ++i) {
			const float za = Math::lerp(z_back, z_front, float(i) / n), zb2 = Math::lerp(z_back, z_front, float(i + 1) / n);
			float top, bottom;
			course(za, zb2, top, bottom);
			hc.rbox(Vector3(wx, (top + bottom) * 0.5f, (za + zb2) * 0.5f), Vector3(0.17f, (top - bottom) * 0.5f, (zb2 - za) * 0.5f + 0.02f), 0.03f,
					mat_cell(pal::STONE, mat::STONE), mat_cell(pal::STONE_LIGHT, mat::STONE));
		}
		const int parts = Math::max(1, int(std::ceil((z_front - z_back) / 3.0f)));
		for (int i = 0; i < parts; ++i) {
			const float za = Math::lerp(z_back, z_front, float(i) / parts), zb2 = Math::lerp(z_back, z_front, float(i + 1) / parts);
			float top, bottom;
			course(za, zb2, top, bottom);
			OBox b; // pushed as is: add_box would lift it by the ground under its centre
			b.xf = lot.xf * Transform3D(Basis(), Vector3(wx, (top + bottom) * 0.5f, (za + zb2) * 0.5f));
			b.half = Vector3(0.17f, (top - bottom) * 0.5f, (zb2 - za) * 0.5f);
			b.walkable = true;
			b.lot = lot.index;
			ctx.boxes.push_back(b);
		}
	}
	// Ship: the same painted stone wall along a back or side line where the yard meets a much higher or lower yard or
	// sidewalk of another street (plan_hood_terrain: lattice edges too steep to drape, Lot::walls). Courses of up to
	// 1.5 m follow the ground, from 0.3 m under the lowest ground within 1.5 m either side to 0.3 m over the highest;
	// solid and walkable on top in pieces of up to 3 m.
	for (const Lot::Wall &w : lot.walls) {
		if (w.side) {
			continue;
		}
		const Vector3 along = w.b - w.a;
		const float len = along.length();
		if (len < 0.2f) {
			continue;
		}
		const Vector3 dir = along / len;
		const Vector3 across(dir.z, 0.0f, -dir.x);
		const bool on_x = std::abs(dir.x) > 0.5f; // a back line runs along lot-local x, a side line along z
		auto course = [&](float s0, float s1, float &top, float &bottom) {
			top = -1e9f;
			bottom = 1e9f;
			for (float u : { 0.0f, 0.5f, 1.0f }) {
				const Vector3 c = w.a + dir * Math::lerp(s0, s1, u);
				for (float o : { -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f }) { // (the drop spans a 2 m lattice square)
					const Vector3 q = c + across * o;
					const float g = ctx.lot_ground(lot, q.x, q.z);
					top = Math::max(top, g);
					bottom = Math::min(bottom, g);
				}
			}
			top += 0.3f;
			bottom -= 0.3f;
		};
		auto half_of = [&](float hl, float hh) { return on_x ? Vector3(hl, hh, 0.17f) : Vector3(0.17f, hh, hl); };
		const int n = Math::max(1, int(std::ceil(len / 1.5f)));
		for (int i = 0; i < n; ++i) {
			const float s0 = len * float(i) / n, s1 = len * float(i + 1) / n;
			float top, bottom;
			course(s0, s1, top, bottom);
			const Vector3 c = w.a + dir * ((s0 + s1) * 0.5f);
			hc.rbox(Vector3(c.x, (top + bottom) * 0.5f, c.z), half_of((s1 - s0) * 0.5f + 0.02f, (top - bottom) * 0.5f), 0.03f, mat_cell(pal::STONE, mat::STONE),
					mat_cell(pal::STONE_LIGHT, mat::STONE));
		}
		const int parts = Math::max(1, int(std::ceil(len / 3.0f)));
		for (int i = 0; i < parts; ++i) {
			const float s0 = len * float(i) / parts, s1 = len * float(i + 1) / parts;
			float top, bottom;
			course(s0, s1, top, bottom);
			const Vector3 c = w.a + dir * ((s0 + s1) * 0.5f);
			OBox b; // pushed as is: add_box would lift it by the ground under its centre
			b.xf = lot.xf * Transform3D(Basis(), Vector3(c.x, (top + bottom) * 0.5f, c.z));
			b.half = half_of((s1 - s0) * 0.5f, (top - bottom) * 0.5f);
			b.walkable = true;
			b.lot = lot.index;
			ctx.boxes.push_back(b);
			for (float u : { 0.0f, 0.5f, 1.0f }) {
				const Vector3 q = w.a + dir * Math::lerp(s0, s1, u);
				ctx.occupy(lot.index, q.x, q.z, 0.6f); // (no yard prop stands in it)
			}
		}
	}
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
	const float steps_front = lot.flight.empty() ? zf + h.porch_d + float(lot.porch_steps) * 0.36f : lot.flight_foot; // Stage 3c
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
