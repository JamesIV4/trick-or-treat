// Towns: a Main Street of continuous two- and three-storey storefronts (bakery, candy shop, toy
// store...) with striped awnings, glowing display windows and sign boards, wide paved sidewalks
// dressed for Halloween, flat roofs to climb, and a town square with a gazebo. Shops fill in the
// same lot and house data as homes (door, awning as the "porch", routes), so doorbells, candy and
// the magic trail work unchanged.

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

#include "gen/context.h"

using namespace godot;

namespace tot {

const char *shop_sign(ShopKind k) {
	static const char *names[] = { "BAKERY", "CANDY", "TOYS", "BOOKS", "DINER", "HARDWARE", "FLOWERS", "COSTUMES", "BARBER", "PETS",
		"CINEMA", "PHARMACY" };
	return names[int(k)];
}

namespace {

constexpr float GROUND_STOREY = 3.6f;
constexpr float UPPER_STOREY = 3.1f;
constexpr float AWNING_TOP = 2.95f;

const int SHOP_WALLS[][2] = { { pal::BRICK, mat::BRICK }, { pal::BRICK_DARK, mat::BRICK }, { pal::TERRACOTTA, mat::BRICK },
	{ pal::HOUSE_CREAM, mat::STUCCO }, { pal::HOUSE_SAGE, mat::SIDING }, { pal::HOUSE_TEAL, mat::STUCCO }, { pal::HOUSE_PLUM, mat::SIDING },
	{ pal::HOUSE_BUTTER, mat::STUCCO }, { pal::HOUSE_SKY, mat::SIDING }, { pal::STONE_LIGHT, mat::STONE } };
const int AWNINGS[][2] = { { pal::BANNER_ORANGE, pal::CREAM }, { pal::BANNER_PURPLE, pal::CREAM }, { pal::BANNER_GREEN, pal::CREAM },
	{ pal::METAL_RED, pal::TRIM_WHITE }, { pal::ROOF_TEAL, pal::TRIM_WHITE }, { pal::CANDY_PINK, pal::TRIM_WHITE }, { pal::QUILT_BLUE, pal::CREAM },
	{ pal::WITCH_PURPLE, pal::CANDY_CORN_YELLOW } };
const int SIGN_BOARDS[] = { pal::CHARCOAL, pal::WOOD_DARK, pal::SHADOW_NAVY, pal::METAL_GREEN, pal::ROOF_PLUM };
const Color SIGN_TEXT[] = { Color(1.0f, 0.86f, 0.45f), Color(1.0f, 0.62f, 0.85f), Color(0.6f, 1.0f, 0.85f), Color(1.0f, 0.95f, 0.85f),
	Color(1.0f, 0.62f, 0.3f) };
const char *TREES[] = { "tree_maple_red", "tree_maple_orange", "tree_maple_gold" };
const char *JACKS[] = { "jackolantern_happy", "jackolantern_wink", "jackolantern_surprised", "jackolantern_toothy", "jackolantern_sleepy" };

float eave_of(const HouseSpec &h) {
	return GROUND_STOREY + UPPER_STOREY * float(h.storeys - 1);
}

Persona persona_for(ShopKind k) {
	switch (k) {
		case ShopKind::Bakery:
		case ShopKind::Diner:
			return Persona::Bakers;
		case ShopKind::Florist:
			return Persona::GreenThumbs;
		case ShopKind::Costumes:
		case ShopKind::Candy:
			return Persona::SpookyCute;
		case ShopKind::Toys:
		case ShopKind::Cinema:
			return Persona::YoungFamily;
		case ShopKind::Pets:
			return Persona::PetLovers;
		case ShopKind::Hardware:
			return Persona::Tinkerers;
		default:
			return Persona::CraftyArtists;
	}
}

} // namespace

// ---------------------------------------------------------------- lots
void plan_town_lots(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_LOTS);
	const StreetPlan &s = ctx.street;
	ctx.lots.clear();
	const float corridor = s.edge() + 0.8f;
	// The square takes a stretch of frontage across from the side street, near the crossing.
	const int square_side = -s.lane_dir;
	const float square_w = rng.range(20.0f, 24.0f);
	const float square_x0 = Math::clamp(s.lane_x + rng.range(-6.0f, 6.0f) - square_w * 0.5f, ctx.bound_x0 + 10.0f, ctx.bound_x1 - 10.0f - square_w);
	const float square_x1 = square_x0 + square_w;
	for (int side : { 1, -1 }) {
		float x = ctx.bound_x0 + rng.range(0.5f, 1.5f);
		while (true) {
			float w = rng.range(9.5f, 13.0f);
			if (side == s.lane_dir && x < s.lane_x + corridor && x + w > s.lane_x - corridor) {
				const float room = (s.lane_x - corridor) - x;
				if (room >= 8.5f) {
					w = room;
				} else {
					x = s.lane_x + corridor;
					continue;
				}
			}
			if (side == square_side && x < square_x1 && x + w > square_x0) {
				const float room = square_x0 - x;
				if (room >= 8.5f) {
					w = room;
				} else {
					x = square_x1;
					continue;
				}
			}
			if (x + w > ctx.bound_x1 - 1.0f) {
				const float room = ctx.bound_x1 - 1.0f - x;
				if (room < 8.5f) {
					break;
				}
				w = room;
			}
			Lot lot;
			lot.index = int(ctx.lots.size());
			lot.side = side;
			lot.width = w;
			lot.depth = rng.range(15.5f, 17.5f);
			lot.center_x = x + w * 0.5f;
			const Vector3 c = s.at(lot.center_x);
			const Vector3 l = s.left_at(lot.center_x);
			const Vector3 toward = -l * float(side);
			const Vector3 up(0, 1, 0);
			lot.xf = Transform3D(Basis(up.cross(toward), up, toward), c + l * float(side) * s.edge());
			ctx.lots.push_back(lot);
			x += w;
		}
	}
	ctx.occupied.assign(ctx.lots.size(), {});
	ctx.lot_mesh.assign(ctx.lots.size(), MeshBuilder());
	float zmin = 1e9f, zmax = -1e9f;
	for (const Lot &lot : ctx.lots) {
		for (float sx : { -0.5f, 0.5f }) {
			const Vector3 p = lot.to_world(sx * lot.width, 0, -lot.depth);
			zmin = Math::min(zmin, p.z);
			zmax = Math::max(zmax, p.z);
		}
	}
	ctx.bound_z0 = Math::min(zmin, -s.edge() - 16.0f);
	ctx.bound_z1 = Math::max(zmax, s.edge() + 16.0f);
}

void assign_shops(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_HOUSEHOLDS);
	std::vector<int> pool;
	for (int i = 0; i < int(ShopKind::Count); ++i) {
		pool.push_back(i);
	}
	rng.shuffle(pool);
	size_t next = 0;
	for (Lot &lot : ctx.lots) {
		int pick = pool[next % pool.size()];
		// Never the same shop as the neighbour on the same side.
		for (const Lot &o : ctx.lots) {
			if (o.index < lot.index && o.side == lot.side && lot.index - o.index == 1 && o.shop == pick) {
				pick = pool[++next % pool.size()];
			}
		}
		++next;
		lot.shop = pick;
		lot.persona = persona_for(ShopKind(pick));
	}
}

// ---------------------------------------------------------------- plan
void plan_shop(GenContext &ctx, Lot &lot) {
	Rng rng = ctx.rng(STAGE_HOUSE, lot.index);
	HouseSpec &h = lot.house;
	h.width = lot.width - 0.08f; // shoulder to shoulder: one continuous Main Street facade
	h.house_x = 0.0f;
	h.depth = rng.range(10.0f, 12.0f);
	h.front_z = -0.4f;
	h.roof_steps = lot.index % 3 == 1;
	h.storeys = (!h.roof_steps && rng.chance(0.3f)) ? 3 : 2;
	h.storey_h = UPPER_STOREY;
	h.porch_w = Math::min(h.width - 1.2f, rng.range(6.0f, 9.0f));
	h.porch_x = 0.0f;
	h.porch_d = rng.range(1.6f, 1.9f);
	h.deck_h = 0.15f;
	h.door_x = rng.chance(0.5f) ? 0.0f : Math::clamp(rng.sign() * (h.porch_w * 0.5f - 1.2f), -h.width * 0.5f + 1.4f, h.width * 0.5f - 1.4f);
	h.garage = false;
	h.chimney = rng.chance(0.5f);
	h.chimney_side = rng.chance(0.5f) ? 1 : -1;
	h.fence_type = 0;
	int wall = rng.irange(0, 9);
	for (const Lot &o : ctx.lots) {
		if (o.index < lot.index && o.side == lot.side && lot.index - o.index == 1 && o.house.wall_cell == SHOP_WALLS[wall][0]) {
			wall = (wall + 4) % 10;
		}
	}
	h.wall_cell = SHOP_WALLS[wall][0];
	h.wall_mat = SHOP_WALLS[wall][1];
	const int aw = rng.irange(0, 7);
	h.roof_cell = AWNINGS[aw][0]; // awning stripes
	h.upper_cell = AWNINGS[aw][1];
	h.trim_cell = rng.chance(0.7f) ? pal::TRIM_WHITE : pal::CREAM;
	h.door_cell = rng.chance(0.5f) ? pal::WOOD_DARK : pal::METAL_GREEN;
	h.shutter_cell = SIGN_BOARDS[rng.irange(0, 4)]; // sign board
	h.curtain_cell = rng.irange(0, 4); // sign text colour index
	h.flower_boxes = rng.chance(0.5f);
	h.dormer = rng.chance(0.4f); // water tower on the roof
	lot.drive_x = 0.0f;
	lot.passage_side = 1;
}

// ---------------------------------------------------------------- build
void build_shop(GenContext &ctx, Lot &lot) {
	HouseSpec &h = lot.house;
	MeshBuilder &m = ctx.lot_mesh[lot.index];
	Rng rng = ctx.rng(STAGE_HOUSE, lot.index, 1);
	const float W = h.width, D = h.depth;
	const float x0 = -W * 0.5f, x1 = W * 0.5f;
	const float zf = h.front_z, zb = zf - D;
	const float eave = eave_of(h);
	const int wall = mat_cell(h.wall_cell, h.wall_mat);
	const int trim = mat_cell(h.trim_cell, mat::WOOD);
	const int stone = mat_cell(pal::STONE, mat::STONE);
	auto solid = [&](Vector3 c, Vector3 half, bool walkable) { ctx.add_box(lot.index, c, half, walkable); };
	m.set_ground_ao(true);

	// Body, with a stone base course and a belt course between storeys.
	m.box(Vector3(0, eave * 0.5f, (zf + zb) * 0.5f), Vector3(W * 0.5f, eave * 0.5f, D * 0.5f), wall);
	solid(Vector3(0, eave * 0.5f - 0.1f, (zf + zb) * 0.5f), Vector3(W * 0.5f, eave * 0.5f - 0.1f, D * 0.5f), false);
	for (float x = x0 + 0.6f; x <= x1 - 0.2f; x += 1.3f) {
		for (float z = zb + 0.6f; z <= zf + 0.6f; z += 1.3f) {
			ctx.occupy(lot.index, x, z, 0.95f);
		}
	}
	for (int s = 1; s < h.storeys; ++s) {
		const float y = GROUND_STOREY + UPPER_STOREY * float(s - 1);
		m.rounded_box(Vector3(0, y, zf + 0.02f), Vector3(W * 0.5f, 0.09f, 0.1f), 0.02f, trim);
	}

	// Storefront: pilasters, kick plate, display windows either side of the door, transom bar.
	for (float sx : { -1.0f, 1.0f }) {
		m.rounded_box(Vector3(sx * (W * 0.5f - 0.28f), GROUND_STOREY * 0.5f, zf + 0.1f), Vector3(0.26f, GROUND_STOREY * 0.5f, 0.12f), 0.03f, stone);
	}
	const float wx0 = x0 + 0.6f, wx1 = x1 - 0.6f;
	const float dx0 = h.door_x - 0.75f, dx1 = h.door_x + 0.75f;
	m.box(Vector3(0, 0.27f, zf + 0.06f), Vector3((wx1 - wx0) * 0.5f, 0.27f, 0.06f), mat_cell(h.shutter_cell, mat::WOOD));
	const int glow = rng.chance(0.6f) ? pal::GLOW_WINDOW_WARM : pal::GLOW_WINDOW;
	for (int side = 0; side < 2; ++side) {
		const float a = side == 0 ? wx0 : dx1 + 0.15f;
		const float b = side == 0 ? dx0 - 0.15f : wx1;
		if (b - a < 0.6f) {
			continue;
		}
		const float cx = (a + b) * 0.5f;
		m.box(Vector3(cx, 1.6f, zf + 0.03f), Vector3((b - a) * 0.5f, 1.02f, 0.03f), glow);
		m.box(Vector3(cx, 2.22f, zf + 0.07f), Vector3((b - a) * 0.5f, 0.04f, 0.04f), trim);
		m.box(Vector3(cx, 0.6f, zf + 0.09f), Vector3((b - a) * 0.5f + 0.05f, 0.05f, 0.09f), trim);
		const int panes = std::max(1, int((b - a) / 1.3f));
		for (int k = 0; k <= panes; ++k) {
			const float x = Math::lerp(a, b, float(k) / panes);
			m.box(Vector3(x, 1.6f, zf + 0.07f), Vector3(0.04f, 1.02f, 0.04f), trim);
		}
		// Window display: a few chunky goods in the shop's colours.
		for (int k = 0; k < panes * 2; ++k) {
			const float x = Math::lerp(a + 0.3f, b - 0.3f, (k + 0.5f) / (panes * 2));
			const int goods[] = { pal::PUMPKIN, pal::CANDY_PINK, pal::CANDY_MINT, pal::CANDY_LEMON, pal::GUMBALL_RED, pal::WRAPPER_PURPLE, pal::CARAMEL };
			m.blob(Vector3(x, 0.82f + rng.range(0.0f, 0.12f), zf + 0.12f), Vector3(0.14f, 0.16f, 0.08f), goods[rng.irange(0, 6)], 0);
		}
	}
	// Door: frame, glow behind it, a doorstep, and a swinging glass-panelled door node.
	m.box(Vector3(h.door_x, 1.12f, zf + 0.02f), Vector3(0.6f, 1.1f, 0.03f), pal::GLOW_WINDOW_WARM);
	m.rounded_box(Vector3(h.door_x, 2.32f, zf + 0.09f), Vector3(0.78f, 0.1f, 0.1f), 0.02f, trim);
	for (float sx : { -1.0f, 1.0f }) {
		m.rounded_box(Vector3(h.door_x + sx * 0.68f, 1.15f, zf + 0.08f), Vector3(0.09f, 1.15f, 0.09f), 0.02f, trim);
	}
	m.rounded_box(Vector3(h.door_x, 0.075f, zf + 0.45f), Vector3(0.9f, 0.075f, 0.45f), 0.02f, stone, mat_cell(pal::STONE_LIGHT, mat::STONE));
	solid(Vector3(h.door_x, 0.075f, zf + 0.45f), Vector3(0.9f, 0.075f, 0.45f), true);
	m.box(Vector3(h.door_x, 0.16f, zf + 0.5f), Vector3(0.45f, 0.012f, 0.28f), mat_cell(pal::PUMPKIN_DARK, mat::BURLAP));
	DoorSpec door;
	door.lot = lot.index;
	door.hinge = lot.xf * Transform3D(Basis(), Vector3(h.door_x - 0.55f, h.deck_h, zf + 0.06f));
	door.width = 1.1f;
	door.height = 2.15f;
	door.cell = mat_cell(h.door_cell, mat::WOOD);
	door.trim_cell = trim;
	ctx.doors.push_back(door);
	lot.doorbell = lot.to_world(h.door_x, h.deck_h, zf + 0.7f);
	lot.porch_light = lot.to_world(h.door_x, AWNING_TOP - 0.45f, zf + 0.7f);

	// Sign board above the awning, lettered with the shop's name.
	const float sign_w = Math::min(h.porch_w + 0.4f, W - 1.2f);
	m.rounded_box(Vector3(0, 3.28f, zf + 0.08f), Vector3(sign_w * 0.5f, 0.27f, 0.07f), 0.03f, mat_cell(h.shutter_cell, mat::WOOD));
	m.box(Vector3(0, 3.28f, zf + 0.15f), Vector3(sign_w * 0.5f + 0.06f, 0.3f, 0.01f), trim);
	m.box(Vector3(0, 3.28f, zf + 0.155f), Vector3(sign_w * 0.5f - 0.02f, 0.25f, 0.012f), mat_cell(h.shutter_cell, mat::WOOD));
	if (lot.shop >= 0) {
		SignSpec sign;
		sign.text = shop_sign(ShopKind(lot.shop));
		sign.xf = lot.xf * Transform3D(Basis(), Vector3(0, 3.28f, zf + 0.18f));
		sign.color = SIGN_TEXT[h.curtain_cell % 5];
		sign.size = Math::min(1.3f, sign_w / (0.26f * float(sign.text.size()) + 0.4f));
		ctx.signs.push_back(sign);
	}

	// Striped awning: a gently sloped walkable slab with a scalloped valance and iron brackets.
	{
		const float aw = h.porch_w, ad = h.porch_d;
		const int stripes = std::max(4, int(aw / 0.55f));
		const float slope = 0.16f;
		// It's a trampoline: the fabric is its own node, hinged at the wall, so it can dip and spring
		// back when the kid bounces on it.
		const Transform3D tilt(Basis(Vector3(1, 0, 0), slope), Vector3(h.porch_x, AWNING_TOP - 0.1f, zf + ad * 0.5f));
		const Vector3 hinge(h.porch_x, AWNING_TOP - 0.1f + std::sin(slope) * ad * 0.5f, zf);
		SpringSpec &spring = ctx.springs.emplace_back();
		spring.xf = lot.xf * Transform3D(Basis(), hinge);
		spring.anchor = lot.xf.xform(tilt.origin);
		spring.reach = Math::max(aw, ad) * 0.5f + 0.6f;
		MeshBuilder &fabric = spring.mesh;
		fabric.set_transform(Transform3D(Basis(), -hinge) * tilt);
		for (int i = 0; i < stripes; ++i) {
			const float a = -aw * 0.5f + aw * float(i) / stripes, b = -aw * 0.5f + aw * float(i + 1) / stripes;
			const int cell = mat_cell((i & 1) ? h.upper_cell : h.roof_cell, mat::FABRIC);
			fabric.box(Vector3((a + b) * 0.5f, 0.0f, 0.0f), Vector3((b - a) * 0.5f, 0.07f, ad * 0.5f), cell);
			const Vector3 v0(a, -0.07f, ad * 0.5f + 0.01f), v1(b, -0.07f, ad * 0.5f + 0.01f), v2((a + b) * 0.5f, -0.3f, ad * 0.5f + 0.01f);
			fabric.tri_out(v0, v1, v2, cell, Vector3((a + b) * 0.5f, -0.1f, 0.0f));
			fabric.tri_out(v0, v1, v2, cell, Vector3((a + b) * 0.5f, -0.1f, ad));
		}
		fabric.set_transform(Transform3D());
		for (float sx : { -1.0f, 1.0f }) {
			const float bx = h.porch_x + sx * (aw * 0.5f - 0.2f);
			m.beam(Vector3(bx, AWNING_TOP - 0.75f, zf + 0.05f), Vector3(bx, AWNING_TOP - 0.18f, zf + ad - 0.15f), 0.05f, 0.05f, mat_cell(pal::IRON, mat::METAL),
					Vector3(1, 0, 0));
		}
		solid(Vector3(h.porch_x, AWNING_TOP - 0.1f, zf + ad * 0.5f), Vector3(aw * 0.5f, 0.1f, ad * 0.5f), true);
		ctx.boxes.back().bouncy = true;
		ctx.boxes.back().bounce = 14.0f; // like a mini trampoline
		// Lamp under the awning over the door.
		m.cylinder(Vector3(h.door_x, AWNING_TOP - 0.62f, zf + 0.7f), 0.13f, 0.26f, 8, pal::GLOW_PORCH, 0.09f);
	}

	// Upper storeys: tall sash windows with lintels and sills, some with flower boxes.
	const int count = std::max(2, int(W / 2.6f));
	for (int s = 1; s < h.storeys; ++s) {
		const float wy = GROUND_STOREY + UPPER_STOREY * float(s - 1) + 1.55f;
		for (int i = 0; i < count; ++i) {
			const float wx = Math::lerp(x0 + 1.3f, x1 - 1.3f, count == 1 ? 0.5f : float(i) / (count - 1));
			const int wg = rng.chance(0.25f) ? pal::GLOW_LANTERN_PINK : (rng.chance(0.5f) ? pal::GLOW_WINDOW : pal::GLOW_WINDOW_WARM);
			const bool dark = rng.chance(0.2f);
			m.box(Vector3(wx, wy, zf + 0.03f), Vector3(0.48f, 0.72f, 0.03f), dark ? pal::SHADOW_NAVY : wg);
			m.rounded_box(Vector3(wx, wy + 0.82f, zf + 0.08f), Vector3(0.62f, 0.1f, 0.09f), 0.02f, trim);
			m.rounded_box(Vector3(wx, wy - 0.8f, zf + 0.1f), Vector3(0.6f, 0.06f, 0.12f), 0.02f, trim);
			m.box(Vector3(wx, wy, zf + 0.07f), Vector3(0.5f, 0.03f, 0.03f), trim);
			m.box(Vector3(wx, wy, zf + 0.07f), Vector3(0.03f, 0.72f, 0.03f), trim);
			if (h.flower_boxes && s == 1) {
				m.rounded_box(Vector3(wx, wy - 0.95f, zf + 0.22f), Vector3(0.55f, 0.11f, 0.14f), 0.03f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::CINNAMON, mat::SOIL));
				for (int k = 0; k < 5; ++k) {
					const int fl[] = { pal::MAPLE_ORANGE, pal::CANDY_PINK, pal::GOLDEN_LEAF, pal::HEDGE, pal::CANDY_GRAPE };
					m.blob(Vector3(wx - 0.4f + k * 0.2f, wy - 0.78f, zf + 0.22f), Vector3(0.1f, 0.08f, 0.09f), fl[(k + i) % 5], 0);
				}
			}
		}
	}

	// Cornice, parapet with a walkable cap, and the flat roof.
	m.rounded_box(Vector3(0, eave - 0.05f, zf + 0.12f), Vector3(W * 0.5f + 0.04f, 0.17f, 0.22f), 0.04f, trim);
	for (float x = x0 + 0.5f; x < x1 - 0.3f; x += 0.9f) {
		m.box(Vector3(x, eave - 0.32f, zf + 0.12f), Vector3(0.07f, 0.12f, 0.1f), trim);
	}
	const float par = 0.6f;
	m.box(Vector3(0, eave + par * 0.5f, zf - 0.12f), Vector3(W * 0.5f, par * 0.5f, 0.12f), wall);
	m.rounded_box(Vector3(0, eave + par + 0.05f, zf - 0.12f), Vector3(W * 0.5f + 0.03f, 0.06f, 0.16f), 0.02f, mat_cell(pal::STONE_LIGHT, mat::STONE));
	solid(Vector3(0, eave + par * 0.5f + 0.05f, zf - 0.12f), Vector3(W * 0.5f, par * 0.5f + 0.06f, 0.16f), true);
	m.box(Vector3(0, eave + par * 0.5f, zb + 0.12f), Vector3(W * 0.5f, par * 0.5f, 0.12f), wall);
	solid(Vector3(0, eave + par * 0.5f, zb + 0.12f), Vector3(W * 0.5f, par * 0.5f, 0.12f), false);
	m.quad_out(Vector3(x0, eave + 0.005f, zf), Vector3(x1, eave + 0.005f, zf), Vector3(x1, eave + 0.005f, zb), Vector3(x0, eave + 0.005f, zb),
			mat_cell(pal::STONE_DARK, mat::CONCRETE), Vector3(0, eave - 1.0f, (zf + zb) * 0.5f));
	solid(Vector3(0, eave - 0.1f, (zf + zb) * 0.5f), Vector3(W * 0.5f - 0.1f, 0.1f, D * 0.5f - 0.1f), true);
	// Roof clutter: an air-conditioning box, a chimney stack, maybe a water tower. The AC box sits
	// beside the tower so it is the step up onto it.
	const float tx = Math::clamp(rng.range(-W * 0.25f, W * 0.25f), x0 + 1.4f, x1 - 1.4f), tz = zf - D * 0.62f;
	const float ac_x = Math::clamp(tx + (tx > 0.0f ? -1.7f : 1.7f), x0 + 1.0f, x1 - 1.0f);
	const float ac_z = tz + 1.1f;
	m.rounded_box(Vector3(ac_x, eave + 0.45f, ac_z), Vector3(0.6f, 0.45f, 0.5f), 0.05f, mat_cell(pal::SILVER, mat::METAL), mat_cell(pal::STONE_LIGHT, mat::METAL));
	m.cylinder(Vector3(ac_x, eave + 0.9f, ac_z), 0.32f, 0.04f, 10, mat_cell(pal::IRON, mat::METAL));
	solid(Vector3(ac_x, eave + 0.45f, ac_z), Vector3(0.6f, 0.45f, 0.5f), true);
	if (h.chimney) {
		const float cx = float(h.chimney_side) * (W * 0.5f - 0.8f);
		m.rounded_box(Vector3(cx, eave + 0.9f, zb + 1.2f), Vector3(0.45f, 0.9f, 0.45f), 0.04f, mat_cell(pal::BRICK, mat::BRICK), mat_cell(pal::BRICK_DARK, mat::BRICK));
		solid(Vector3(cx, eave + 0.9f, zb + 1.2f), Vector3(0.45f, 0.9f, 0.45f), true);
	}
	Vector3 tower_top(1e9f, 0, 0);
	if (h.dormer || h.roof_steps) {
		// Water tower: barrel on legs with a cone hat; its top is the summit of the climb.
		const float leg_h = 1.5f;
		for (int k = 0; k < 4; ++k) {
			const float a = Math_PI * 0.25f + k * Math_PI * 0.5f;
			m.beam(Vector3(tx + std::cos(a) * 0.75f, eave, tz + std::sin(a) * 0.75f), Vector3(tx + std::cos(a) * 0.62f, eave + leg_h, tz + std::sin(a) * 0.62f), 0.1f, 0.1f,
					mat_cell(pal::WOOD_DARK, mat::WOOD), Vector3(1, 0, 0));
		}
		m.cylinder(Vector3(tx, eave + leg_h, tz), 0.95f, 1.5f, 12, mat_cell(pal::WOOD, mat::PLANKS), -1.0f, mat_cell(pal::WOOD_DARK, mat::PLANKS));
		for (float ry : { 0.3f, 1.1f }) {
			m.cylinder(Vector3(tx, eave + leg_h + ry, tz), 0.98f, 0.08f, 12, mat_cell(pal::IRON, mat::METAL));
		}
		m.cylinder(Vector3(tx, eave + leg_h + 1.5f, tz), 1.05f, 0.7f, 12, mat_cell(pal::ROOF_SLATE, mat::SHINGLES), 0.08f);
		solid(Vector3(tx, eave + leg_h * 0.5f + 0.75f, tz), Vector3(0.95f, leg_h * 0.5f + 0.75f, 0.95f), true);
		tower_top = Vector3(tx, eave + leg_h + 1.5f, tz);
	}
	// Summit climb on every third shop: two flower ledges up the front, onto the parapet, the roof,
	// the AC box and the water tower.
	lot.summit_route.clear();
	if (h.roof_steps && tower_top.x < 1e8f) {
		const float lx = Math::clamp(h.porch_x + h.porch_w * 0.5f + 0.3f, x0 + 0.9f, x1 - 0.9f);
		const float ly1 = AWNING_TOP + 1.45f, ly2 = ly1 + 1.45f;
		for (float ly : { ly1, ly2 }) {
			const float ledge_x = ly == ly1 ? lx : lx - 1.6f;
			m.rounded_box(Vector3(ledge_x, ly - 0.12f, zf + 0.35f), Vector3(0.7f, 0.12f, 0.35f), 0.03f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::HEDGE, mat::HEDGE));
			m.blob(Vector3(ledge_x - 0.3f, ly + 0.08f, zf + 0.35f), Vector3(0.18f, 0.13f, 0.16f), pal::MAPLE_ORANGE, 0);
			m.blob(Vector3(ledge_x + 0.3f, ly + 0.08f, zf + 0.35f), Vector3(0.18f, 0.13f, 0.16f), pal::CANDY_PINK, 0);
			solid(Vector3(ledge_x, ly - 0.12f, zf + 0.4f), Vector3(0.7f, 0.12f, 0.4f), true);
			lot.summit_route.push_back(Vector3(ledge_x, ly, zf + 0.4f));
		}
		lot.summit_route.push_back(Vector3(lx - 1.6f, eave + par + 0.11f, zf - 0.12f));
		lot.summit_route.push_back(Vector3(ac_x, eave + 0.9f, ac_z));
		lot.summit_route.push_back(tower_top);
	}
	m.set_ground_ao(false);

	// Paving in front of the shop, and a gravel service yard behind it.
	m.quad_out(Vector3(x0, 0.072f, 0), Vector3(x1, 0.072f, 0), Vector3(x1, 0.072f, zf), Vector3(x0, 0.072f, zf), mat_cell(pal::SIDEWALK, mat::CONCRETE),
			Vector3(0, -1, zf * 0.5f));
	m.quad_out(Vector3(x0, 0.004f, zb), Vector3(x1, 0.004f, zb), Vector3(x1, 0.004f, -lot.depth), Vector3(x0, 0.004f, -lot.depth),
			mat_cell(pal::STONE, mat::CONCRETE), Vector3(0, -1, zb - 1.0f));
}

// ---------------------------------------------------------------- routes
void plan_shop_routes(GenContext &ctx, Lot &lot) {
	(void)ctx;
	const HouseSpec &h = lot.house;
	const float zf = h.front_z;
	lot.loop_route = {
		Vector3(h.door_x - 2.0f, 0, 1.4f),
		Vector3(h.door_x, 0, zf + 1.0f),
		Vector3(h.door_x, h.deck_h, zf + 0.5f),
		Vector3(h.door_x + 2.0f, 0, 1.4f),
	};
	// Awning branch: from the sidewalk at the awning's end (a crate makes the first step) along the top.
	const float ds = h.door_x > 0.0f ? -1.0f : 1.0f;
	const float end_x = h.porch_x + ds * (h.porch_w * 0.5f - 0.5f);
	lot.roof_route = {
		Vector3(end_x, 0.0f, zf + h.porch_d + 1.7f),
		Vector3(end_x, AWNING_TOP, zf + h.porch_d * 0.5f),
		Vector3(h.porch_x, AWNING_TOP, zf + h.porch_d * 0.5f),
		Vector3(h.porch_x - ds * (h.porch_w * 0.5f - 0.6f), AWNING_TOP, zf + h.porch_d * 0.5f),
	};
	if (!lot.summit_route.empty()) {
		lot.summit_route.insert(lot.summit_route.begin(), Vector3(h.porch_x + h.porch_w * 0.5f - 0.4f, AWNING_TOP, zf + h.porch_d * 0.5f));
	}
}

// ---------------------------------------------------------------- sidewalk dressing
void dress_shopfront(GenContext &ctx, Lot &lot) {
	Rng rng = ctx.rng(STAGE_PROPS, lot.index);
	const HouseSpec &h = lot.house;
	const float zf = h.front_z;
	const float walk = ctx.street.walk - ctx.street.verge; // paved width in front of the shop
	auto put = [&](const std::string &id, float x, float y, float z, float yaw, float scale = 1.0f, bool collide = true) {
		PropPlacement pp;
		pp.id = id;
		pp.xf = lot.xf * Transform3D(Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(scale, scale, scale)), Vector3(x, y, z));
		pp.lot = lot.index;
		pp.collide = collide;
		ctx.props.push_back(pp);
	};
	// Pumpkins and a jack-o'-lantern flanking the door.
	for (float sx : { -1.0f, 1.0f }) {
		const float x = h.door_x + sx * 1.15f;
		if (std::abs(x) > h.width * 0.5f - 0.5f) {
			continue;
		}
		if (rng.chance(0.6f)) {
			put(JACKS[rng.irange(0, 4)], x, 0.07f, zf + 0.45f, rng.range(-0.3f, 0.3f), 0.9f);
		} else {
			put("flower_pot", x, 0.07f, zf + 0.45f, 0.0f);
		}
	}
	// A crate (and maybe a barrel) under the awning's end: the first step up.
	const HouseSpec &hs = h;
	const float ds = hs.door_x > 0.0f ? -1.0f : 1.0f;
	const float end_x = hs.porch_x + ds * (hs.porch_w * 0.5f - 0.5f);
	put("crate", end_x, 0.07f, zf + hs.porch_d + 0.5f, rng.range(-0.2f, 0.2f));
	if (rng.chance(0.5f)) {
		put("barrel", end_x - ds * 0.9f, 0.07f, zf + hs.porch_d + 0.4f, 0.0f);
	}
	// A bench facing the street, away from the door.
	const float bx = Math::clamp(-hs.door_x * 0.5f - ds * 1.4f, -h.width * 0.5f + 1.3f, h.width * 0.5f - 1.3f);
	if (std::abs(bx - end_x) > 1.6f && std::abs(bx - h.door_x) > 1.6f && rng.chance(0.7f)) {
		put("bench", bx, 0.07f, walk - 0.9f, Math_PI);
	}
	// Shop-specific dressing.
	const float fx = -ds * (h.width * 0.5f - 0.8f);
	switch (ShopKind(lot.shop)) {
		case ShopKind::Florist:
			put("sunflower", fx, 0.07f, zf + 0.6f, 0.0f);
			put("flower_pot", fx - ds * 0.7f, 0.07f, zf + 0.5f, 0.0f);
			put("flower_pot", fx, 0.07f, zf + 1.3f, 0.0f, 0.8f);
			break;
		case ShopKind::Costumes:
		case ShopKind::Toys:
			put(rng.chance(0.5f) ? "inflatable_ghost" : "inflatable_cat", fx, 0.07f, zf + 1.1f, Math_PI + rng.range(-0.3f, 0.3f), 0.75f);
			break;
		case ShopKind::Hardware:
			put("wheelbarrow", fx, 0.07f, zf + 1.0f, rng.range(0.0f, Math_TAU));
			put("hay_bale", fx - ds * 1.1f, 0.07f, zf + 0.6f, 0.2f);
			break;
		case ShopKind::Candy:
		case ShopKind::Bakery:
			put("giant_pumpkin", fx, 0.07f, zf + 1.0f, rng.range(0.0f, Math_TAU), 0.5f);
			put("pumpkin_small", fx - ds * 0.9f, 0.07f, zf + 0.6f, rng.range(0.0f, Math_TAU));
			break;
		case ShopKind::Diner:
			put("barrel", fx, 0.07f, zf + 0.7f, 0.0f);
			put("pumpkin_round", fx, 0.97f, zf + 0.7f, 0.0f, 0.7f, false);
			break;
		default:
			if (rng.chance(0.6f)) {
				put("hay_bale", fx, 0.07f, zf + 0.7f, 0.1f);
				put(JACKS[rng.irange(0, 4)], fx, 0.62f, zf + 0.7f, rng.range(-0.3f, 0.3f), 0.8f);
			} else {
				put("scarecrow", fx, 0.07f, zf + 0.8f, Math_PI + rng.range(-0.3f, 0.3f));
			}
	}
	// String lights (bulbs or mini jack-o'-lanterns) or bunting along the shop front, under the
	// cornice of the ground storey.
	const float pick = rng.f();
	if (pick < 0.85f) {
		StringLight sl;
		sl.lot = lot.index;
		sl.a = lot.to_world(-h.width * 0.5f + 0.4f, 3.75f, zf + 0.25f);
		sl.b = lot.to_world(h.width * 0.5f - 0.4f, 3.75f, zf + 0.25f);
		sl.sag = 0.3f;
		sl.style = pick < 0.3f ? STRING_BULBS : (pick < 0.6f ? STRING_JACKS : STRING_BUNTING);
		sl.out = lot.xf.basis.xform(Vector3(0, 0, 1));
		ctx.strings.push_back(sl);
	}
	// Service yard behind: crates and barrels to climb, and a pumpkin pile.
	const float yz = zf - h.depth - 1.6f;
	if (yz > -lot.depth + 0.8f) {
		put("crate", rng.range(-h.width * 0.3f, h.width * 0.3f), 0.0f, yz, rng.range(0.0f, Math_TAU));
		put("barrel", rng.range(-h.width * 0.35f, h.width * 0.35f), 0.0f, yz - 0.9f, 0.0f);
	}
}

// ---------------------------------------------------------------- town square
void place_town_square(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_RHYTHM, 31);
	const StreetPlan &s = ctx.street;
	MeshBuilder &m = ctx.boundary_mesh;
	for (int side : { 1, -1 }) {
		std::vector<std::pair<float, float>> covered;
		for (const Lot &lot : ctx.lots) {
			if (lot.side == side) {
				covered.push_back({ lot.center_x - lot.width * 0.5f, lot.center_x + lot.width * 0.5f });
			}
		}
		if (side == s.lane_dir) {
			covered.push_back({ s.lane_x - s.edge() - 0.6f, s.lane_x + s.edge() + 0.6f });
		}
		std::sort(covered.begin(), covered.end());
		std::vector<std::pair<float, float>> gaps;
		float x = ctx.bound_x0 + 1.0f;
		for (const auto &c : covered) {
			if (c.first - x > 5.0f) {
				gaps.push_back({ x, c.first });
			}
			x = Math::max(x, c.second);
		}
		if (ctx.bound_x1 - 1.0f - x > 5.0f) {
			gaps.push_back({ x, ctx.bound_x1 - 1.0f });
		}
		for (const auto &g : gaps) {
			const float cx = (g.first + g.second) * 0.5f;
			const float w = g.second - g.first;
			const Vector3 c = s.at(cx), l = s.left_at(cx);
			const Vector3 toward = -l * float(side);
			const Vector3 up(0, 1, 0);
			const Transform3D frame(Basis(up.cross(toward), up, toward), c + l * float(side) * s.edge());
			auto put = [&](const std::string &id, float lx, float lz, float yaw, float scale = 1.0f, float y = 0.0f) {
				PropPlacement pp;
				pp.id = id;
				pp.xf = frame * Transform3D(Basis(up, yaw).scaled(Vector3(scale, scale, scale)), Vector3(lx, y, lz));
				pp.lot = -1;
				ctx.props.push_back(pp);
			};
			const float depth = 15.0f;
			m.set_transform(frame);
			m.quad_out(Vector3(-w * 0.5f, 0.06f, 0), Vector3(w * 0.5f, 0.06f, 0), Vector3(w * 0.5f, 0.06f, -depth), Vector3(-w * 0.5f, 0.06f, -depth),
					mat_cell(pal::BONE, mat::PAVERS), Vector3(0, -1, -depth * 0.5f));
			m.set_transform(Transform3D());
			const float hw = w * 0.5f - 1.2f;
			if (w < 14.0f) {
				// A little plaza: bench, lamp, tree in a planter.
				put("bench", 0.0f, -1.5f, 0.0f);
				put("lamp_post", -hw, -0.8f, 0.0f);
				put(TREES[rng.irange(0, 2)], hw * 0.6f, -5.0f, rng.range(0.0f, Math_TAU), 0.9f);
				put("pumpkin_round", -hw * 0.5f, -3.0f, rng.range(0.0f, Math_TAU));
				continue;
			}
			// Gazebo: octagonal deck, turned posts with a low rail, cone roof you can stand on.
			const Vector3 gc = frame.xform(Vector3(0, 0, -7.0f));
			const float r = 3.0f, deck = 0.5f, post_h = 2.6f;
			const int trim = mat_cell(pal::TRIM_WHITE, mat::WOOD);
			m.cylinder(gc, r + 0.2f, deck, 8, mat_cell(pal::TRIM_WHITE, mat::LATTICE), -1.0f, mat_cell(pal::PLANK, mat::PLANKS));
			m.cylinder(gc + Vector3(0, deck, 0), r + 0.25f, 0.06f, 8, trim);
			for (int k = 0; k < 8; ++k) {
				const float a = k * Math_TAU / 8.0f + Math_TAU / 16.0f;
				const Vector3 p = gc + Vector3(std::cos(a) * r, deck, std::sin(a) * r);
				m.cylinder(p, 0.1f, post_h, 6, trim, 0.08f);
				const float a2 = a + Math_TAU / 8.0f;
				const Vector3 q = gc + Vector3(std::cos(a2) * r, deck, std::sin(a2) * r);
				if (k != 2) { // the opening faces the street
					m.beam(p + Vector3(0, 0.85f, 0), q + Vector3(0, 0.85f, 0), 0.1f, 0.08f, trim);
				}
			}
			m.cylinder(gc + Vector3(0, deck + post_h, 0), r + 0.6f, 0.2f, 8, trim);
			m.cylinder(gc + Vector3(0, deck + post_h + 0.2f, 0), r + 0.6f, 1.5f, 8, mat_cell(pal::ROOF_PLUM, mat::SCALLOP), 0.12f);
			m.blob(gc + Vector3(0, deck + post_h + 1.85f, 0), Vector3(0.25f, 0.3f, 0.25f), pal::GLOW_LANTERN, 0);
			OBox base;
			base.xf = Transform3D(Basis(), gc + Vector3(0, deck * 0.5f, 0));
			base.half = Vector3(r + 0.2f, deck * 0.5f, r + 0.2f);
			base.cylinder = true;
			base.walkable = true;
			ctx.boxes.push_back(base);
			OBox roof;
			roof.xf = Transform3D(Basis(), gc + Vector3(0, deck + post_h + 0.3f, 0));
			roof.half = Vector3(r * 0.75f, 0.3f, r * 0.75f);
			roof.cylinder = true;
			roof.walkable = true;
			ctx.boxes.push_back(roof);
			LightSpec ls;
			ls.pos = gc + Vector3(0, deck + post_h - 0.3f, 0);
			ls.color = Color(1.0f, 0.62f, 0.3f);
			ls.energy = 1.6f;
			ls.range = 8.0f;
			ls.kind = LIGHT_LAMP;
			ctx.lights.push_back(ls);
			// Benches facing the gazebo, lamps with lights strung to its roof, trees and pumpkins.
			for (int k = 0; k < 4; ++k) {
				const float a = Math_PI * 0.25f + k * Math_PI * 0.5f;
				const Vector3 lp(std::cos(a) * (r + 2.6f), 0, -7.0f + std::sin(a) * (r + 2.6f));
				if (std::abs(lp.x) > hw) {
					continue;
				}
				put("bench", lp.x, lp.z, std::atan2(-lp.x, -(lp.z + 7.0f)));
			}
			for (float sx : { -1.0f, 1.0f }) {
				const float lx = sx * Math::min(hw, 7.5f);
				put("lamp_post", lx, -1.0f, 0.0f);
				StringLight sl;
				sl.a = frame.xform(Vector3(lx, 3.0f, -1.0f));
				sl.b = gc + Vector3(0, deck + post_h + 1.0f, 0);
				sl.sag = 0.6f;
				ctx.strings.push_back(sl);
				put(TREES[rng.irange(0, 2)], sx * Math::min(hw, 8.5f), -12.0f, rng.range(0.0f, Math_TAU), 0.95f);
				put("hay_bale", sx * (r + 3.2f), -11.5f, Math_PI * 0.5f);
				put(JACKS[rng.irange(0, 4)], sx * (r + 3.2f), -11.5f, 0.0f, 0.85f, 0.55f);
				put("pumpkin_round", sx * (r + 1.8f), -2.2f, rng.range(0.0f, Math_TAU), 0.8f);
			}
			put("scarecrow", -Math::min(hw, 6.0f) * 0.6f, -13.0f, 0.0f);
		}
	}
	// Festival bunting strung across the high street every so often.
	for (float x = ctx.bound_x0 + rng.range(8.0f, 14.0f); x < ctx.bound_x1 - 8.0f; x += rng.range(16.0f, 24.0f)) {
		StringLight sl;
		sl.style = STRING_BUNTING;
		sl.a = s.at(x) + s.left_at(x) * (s.edge() - 0.3f) + Vector3(0, 5.6f, 0);
		sl.b = s.at(x) - s.left_at(x) * (s.edge() - 0.3f) + Vector3(0, 5.6f, 0);
		sl.sag = 0.9f;
		sl.out = Vector3(s.left_at(x).z, 0, -s.left_at(x).x);
		ctx.strings.push_back(sl);
	}
}

} // namespace tot
