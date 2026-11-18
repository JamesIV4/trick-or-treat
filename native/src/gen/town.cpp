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

const char *town_template_name(TownTemplate t) {
	static const char *names[] = { "cross", "grid", "high_street", "fork" };
	return names[int(t) & 3];
}

// ---------------------------------------------------------------- lots
// Stage 4c: the square first (at the centre, a corner or an end, as the seed asks, else wherever it fits),
// then shop lots shoulder to shoulder by arc length along both sides of every road, each frame from the local
// tangent, set back and shallower on the inside of bends, never into a corridor, another lot or the square
// (the same tests as the neighbourhoods' placer and the sweep).
void plan_town_lots(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_LOTS);
	const RoadNet &net = ctx.roads;
	ctx.lots.clear();
	ctx.square = TownSquare();
	constexpr float MARGIN = 0.3f; // clearance from other corridors and fillets
	auto clear_of_roads = [&](const Lot &L) {
		float own, other;
		lot_corridor_reach(ctx, L, own, other);
		return own <= 0.02f && other <= -MARGIN;
	};
	auto fits = [&](const Lot &L) {
		if (!clear_of_roads(L)) {
			return false;
		}
		for (const Lot &o : ctx.lots) {
			if (lot_overlap(L, o) > 0.01f) {
				return false;
			}
		}
		return !ctx.square.valid || lot_overlap(L, ctx.square.area) <= 0.01f;
	};
	auto frontage_lot = [&](int road, int side, float s0, float w, float d) {
		const Road &R = net.roads[road];
		Lot L;
		L.road = road;
		L.side = side;
		L.width = w;
		L.depth = d;
		L.s = s0 + w * 0.5f;
		L.xf = R.frontage_frame(L.s, side);
		float back = 0.0f;
		for (float sx : { -0.5f, 0.5f }) {
			const RoadHit h = R.project(L.to_world(sx * w, 0, 0));
			if (h.beyond <= 0.0f) {
				back = Math::max(back, R.edge() - std::abs(h.lateral));
			}
		}
		if (back > 0.0f) {
			L.xf.origin -= L.xf.basis.get_column(2).normalized() * (back + 0.03f);
		}
		return L;
	};
	// ---- the square
	{
		const TownSize size = ctx.town_size;
		const float sw = size == TownSize::Small ? rng.range(20.0f, 23.0f) : (size == TownSize::Medium ? rng.range(21.0f, 25.0f) : rng.range(23.0f, 28.0f));
		const float sd = rng.range(16.5f, 18.5f);
		const SquarePlace want = ctx.town_template == TownTemplate::HighStreet ? SquarePlace::Centre : SquarePlace(rng.irange(0, 2));
		const int first_side = rng.chance(0.5f) ? 1 : -1;
		struct Cand {
			int road, side;
			float s; // centre station
			SquarePlace place;
		};
		std::vector<Cand> cands;
		const Road &M = net.trunk();
		const float mid = (M.lots_s0 + M.lots_s1) * 0.5f;
		// Centre: on the main street near its middle (a bowed high street: the outside of the bow's apex, the
		// point furthest from the chord between its ends).
		{
			float centre = mid;
			int sides[2] = { first_side, -first_side };
			if (ctx.town_template == TownTemplate::HighStreet) {
				const Vector3 a = M.at(M.lots_s0), b = M.at(M.lots_s1);
				const Vector3 ab = Vector3(b.x - a.x, 0.0f, b.z - a.z).normalized();
				float best = -1.0f;
				for (float st = M.lots_s0; st <= M.lots_s1; st += 1.0f) {
					const Vector3 p = M.at(st), v(p.x - a.x, 0.0f, p.z - a.z);
					const Vector3 off = v - ab * v.dot(ab);
					if (off.length() > best) {
						best = off.length();
						centre = st;
						sides[0] = M.left_at(st).dot(off) >= 0.0f ? 1 : -1;
						sides[1] = -sides[0];
					}
				}
			}
			for (float d = 0.0f; d <= 26.0f; d += 1.5f) {
				for (float sg : { 1.0f, -1.0f }) {
					for (int side : sides) {
						cands.push_back({ 0, side, centre + sg * d, SquarePlace::Centre });
					}
				}
			}
		}
		// Corner: on a street's frontage right beside a side street's mouth (or on the side street by its mouth).
		for (int r = 0; r < int(net.roads.size()); ++r) {
			for (const RoadJunction &j : net.junctions(r)) {
				const Road &C = net.roads[j.child];
				for (float slide = 0.0f; slide <= 10.0f; slide += 1.0f) {
					for (float sg : { 1.0f, -1.0f }) {
						cands.push_back({ r, j.side, j.s + sg * (C.edge() + 6.5f + sw * 0.5f + slide), SquarePlace::Corner });
					}
					for (int side : { 1, -1 }) {
						cands.push_back({ j.child, side, C.lots_s0 + sw * 0.5f + 0.5f + slide, SquarePlace::Corner });
					}
				}
			}
		}
		// End: where the main street leaves town, or at the far end of a dead-end side street.
		for (int r = 0; r < int(net.roads.size()); ++r) {
			const Road &R = net.roads[r];
			for (float slide = 0.0f; slide <= 12.0f; slide += 1.0f) {
				for (int side : { first_side, -first_side }) {
					if (r == 0) {
						cands.push_back({ 0, side, R.lots_s0 + sw * 0.5f + 1.0f + slide, SquarePlace::End });
						cands.push_back({ 0, side, R.lots_s1 - sw * 0.5f - 1.0f - slide, SquarePlace::End });
					} else if (R.end_kind == RoadEnd::DeadEnd) {
						cands.push_back({ r, side, R.lots_s1 - sw * 0.5f - 0.5f - slide, SquarePlace::End });
					}
				}
			}
		}
		// The asked-for place first, then the others (centre, corner, end), each in the order above.
		std::stable_sort(cands.begin(), cands.end(), [&](const Cand &x, const Cand &y) {
			const int kx = x.place == want ? -1 : int(x.place), ky = y.place == want ? -1 : int(y.place);
			return kx < ky;
		});
		for (const Cand &c : cands) {
			const Road &R = net.roads[c.road];
			if (c.s - sw * 0.5f < R.lots_s0 + 0.5f || c.s + sw * 0.5f > R.lots_s1 - 0.3f) {
				continue;
			}
			bool done = false;
			for (float dd : { sd, sd - 1.5f }) {
				Lot A = frontage_lot(c.road, c.side, c.s - sw * 0.5f, sw, dd);
				if (clear_of_roads(A)) {
					A.index = -1;
					A.shop = -2;
					ctx.square.valid = true;
					ctx.square.place = c.place;
					ctx.square.area = A;
					ctx.square.pad_z = -5.4f; // the front strip (2.4 m) and the ramp (3 m): terrain.cpp
					done = true;
					break;
				}
			}
			if (done) {
				break;
			}
		}
	}
	// ---- the shop rows
	auto add = [&](Lot L) {
		L.index = int(ctx.lots.size());
		ctx.lots.push_back(L);
	};
	auto row = [&](int road, int side) {
		const Road &R = net.roads[road];
		float x = R.lots_s0 + rng.range(0.2f, 1.0f);
		while (x < R.lots_s1 - 8.5f) {
			const float w = rng.range(9.5f, 13.0f);
			float d = rng.range(15.5f, 17.5f);
			const float mid = x + w * 0.5f;
			const float k = (R.at(mid - 6.0f) + R.at(mid + 6.0f) - R.at(mid) * 2.0f).dot(R.left_at(mid)) * float(side);
			if (k > 1e-3f) {
				// The inside of a bend: neighbours' side lines converge behind the frontage.
				d = Math::min(d, Math::max(13.0f, (36.0f / k - R.edge()) * 0.32f));
			}
			bool placed = false;
			for (float slide = 0.0f; slide <= 6.0f && !placed; slide += 0.5f) {
				const float room = R.lots_s1 - 0.3f - (x + slide);
				const float w0 = Math::min(w, room);
				if (w0 < 8.5f) {
					break;
				}
				for (float ww : { w0, Math::max(8.5f, w0 - 1.5f) }) {
					for (float dd : { d, Math::max(13.0f, d - 3.0f) }) {
						if (placed) {
							continue;
						}
						const Lot L = frontage_lot(road, side, x + slide, ww, dd);
						if (fits(L)) {
							add(L);
							x += slide + ww;
							placed = true;
						}
					}
				}
			}
			if (!placed) {
				x += 2.0f; // a corner, the square: try further on
			}
		}
	};
	for (int r = 0; r < int(net.roads.size()); ++r) {
		for (int side : { 1, -1 }) {
			row(r, side);
		}
	}
	link_lot_neighbours(ctx);
	ctx.occupied.assign(ctx.lots.size(), {});
	ctx.lot_mesh.assign(ctx.lots.size(), MeshBuilder());
	// Bounds: every lot corner, every corridor, the square.
	float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
	auto grow = [&](const Vector3 &p) {
		x0 = Math::min(x0, p.x);
		x1 = Math::max(x1, p.x);
		z0 = Math::min(z0, p.z);
		z1 = Math::max(z1, p.z);
	};
	auto grow_lot = [&](const Lot &lot) {
		for (float sx : { -0.5f, 0.5f }) {
			for (float lz : { 0.0f, -lot.depth }) {
				grow(lot.to_world(sx * lot.width, 0, lz));
			}
		}
	};
	for (const Lot &lot : ctx.lots) {
		grow_lot(lot);
	}
	if (ctx.square.valid) {
		grow_lot(ctx.square.area);
	}
	for (const Road &R : net.roads) {
		const float s0 = R.parent < 0 ? R.lots_s0 : R.s_begin(), s1 = R.lots_s1 > s0 ? R.lots_s1 : R.s_end();
		for (float st = s0; st <= s1 + 0.01f; st += 2.0f) {
			grow(R.point(st, R.edge()));
			grow(R.point(st, -R.edge()));
		}
	}
	ctx.bound_x0 = x0;
	ctx.bound_x1 = x1;
	ctx.bound_z0 = z0;
	ctx.bound_z1 = z1;
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
		if (lot.prev >= 0 && lot.prev < lot.index && ctx.lots[lot.prev].shop == pick) {
			pick = pool[++next % pool.size()];
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
	h.depth = Math::min(rng.range(10.0f, 12.0f), lot.depth - 2.2f); // (Stage 4c: shallower lots on bends and in tight blocks)
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
	if (lot.prev >= 0 && lot.prev < lot.index && ctx.lots[lot.prev].house.wall_cell == SHOP_WALLS[wall][0]) {
		wall = (wall + 4) % 10;
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
	if (!ctx.terrain && lot.index == 0) {
		plan_hood_terrain(ctx); // Stage 4c: every shop plan and the square are known; grade before anything is built
	}
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
	// Stage 4c: the lowest ground (lot-local, <= 0) under a footprint, sampled every ~0.4 m edges included.
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
	// Stage 4c: the shop's floor is level (its pad, at the sidewalk's height at the shop's higher end), so on a
	// sloping street a painted stone plinth carries it and the paved strip in front: it shows above the
	// sidewalk toward the lower end and down the side where the neighbour stands lower (stepped facades).
	lot.found_house = 0.0f;
	{
		const float g = ground_min(x0 - 0.04f, x1 + 0.04f, zb - 0.1f, 0.0f);
		if (g < -0.001f) {
			const float bottom = g - 0.3f, top = 0.06f;
			const Vector3 c(0, (top + bottom) * 0.5f, zb * 0.5f), half(W * 0.5f, (top - bottom) * 0.5f, -zb * 0.5f);
			m.box(c, half, mat_cell(pal::STONE_DARK, mat::STONE));
			m.box(Vector3(0, top - 0.04f, -0.02f), Vector3(W * 0.5f + 0.01f, 0.05f, 0.06f), mat_cell(pal::STONE_LIGHT, mat::STONE)); // a capping course along the front
			solid(c, half, true);
			lot.found_house = bottom;
		}
	}

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
	// Stage 4c: where the floor stands above the sidewalk, the doorstep sits on a stone block and a few treads
	// (risers <= 0.18 m) come down onto the sidewalk.
	lot.porch_steps = 3;
	lot.flight.clear();
	{
		const float ds_z1 = zf + 0.9f;
		const float gd = ground_min(h.door_x - 0.9f, h.door_x + 0.9f, 0.0f, ds_z1);
		lot.flight.push_back({ 0.15f, zf, ds_z1, 0.9f }); // the doorstep itself
		lot.flight_foot = ds_z1;
		lot.flight_riser = 0.15f;
		// Stage 3c: the treads come down to the sidewalk's own surface (7 cm over the lattice under it), not the lattice.
		const float gs = ctx.terrain ? street_surface(ctx, lot.to_world(h.door_x, 0.0f, ds_z1 + 0.3f)) - lot.xf.origin.y : gd;
		if (gd < -0.03f && gs < -0.03f) {
			const Vector3 c(h.door_x, (gd - 0.05f) * 0.5f, ds_z1 * 0.5f), half(0.9f, -(gd - 0.05f) * 0.5f, ds_z1 * 0.5f);
			m.box(c, half, stone);
			solid(c, half, true);
			// Stage 3c: as many treads as the drop needs (equal risers <= 0.18 m), never stopping short of the sidewalk.
			const int n = Math::clamp(int(std::ceil((0.15f - gs) / 0.18f)) - 1, 0, 12);
			const float rise = (0.15f - gs) / float(n + 1);
			lot.flight_riser = rise;
			lot.porch_steps = 3 + n; // (before the treads: in_structure keeps their solids off the yard_lift)
			for (int i = 1; i <= n; ++i) {
				const float top = 0.15f - rise * float(i);
				const float tz0 = ds_z1 + 0.32f * float(i - 1), tz1 = tz0 + 0.32f, hw = 0.9f + 0.1f * float(i);
				lot.flight.push_back({ top, tz0, tz1, hw });
				lot.flight_foot = tz1;
				const float bottom = ground_min(h.door_x - hw, h.door_x + hw, tz0, tz1) - 0.05f;
				if (top - bottom < 0.03f) {
					continue;
				}
				const Vector3 tc(h.door_x, (top + bottom) * 0.5f, (tz0 + tz1) * 0.5f), th(hw, (top - bottom) * 0.5f, (tz1 - tz0) * 0.5f);
				m.rounded_box(tc, th, 0.02f, stone, mat_cell(pal::STONE_LIGHT, mat::STONE));
				solid(tc, th, true);
			}
			lot.porch_steps = 3 + n;
		}
	}
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
		sign.width = sign_w - 0.3f;
		sign.height = 0.4f;
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
	// Stage 3c: every shop whose parapet stands more than a jump + flap over its awning gets the flower ledges too (as
	// many as the climb needs, 1.45 m apart, zigzagging), so its roof is reached without the awning's bounce (in 4c the
	// plinths had lifted the roofs out of reach of the next roof along the row).
	const float climb_top = eave + par + 0.11f;
	const int ledges = Math::max(h.roof_steps && tower_top.x < 1e8f ? 2 : 0, climb_top - AWNING_TOP > 2.6f ? int(std::ceil((climb_top - AWNING_TOP) / 1.45f)) - 1 : 0);
	if (ledges > 0) {
		const float lx = Math::clamp(h.porch_x + h.porch_w * 0.5f + 0.3f, x0 + 0.9f, x1 - 0.9f);
		for (int li = 0; li < ledges; ++li) {
			const float ly = AWNING_TOP + 1.45f * float(li + 1);
			const float ledge_x = li % 2 == 0 ? lx : lx - 1.6f;
			m.rounded_box(Vector3(ledge_x, ly - 0.12f, zf + 0.35f), Vector3(0.7f, 0.12f, 0.35f), 0.03f, mat_cell(pal::WOOD, mat::PLANKS), mat_cell(pal::HEDGE, mat::HEDGE));
			m.blob(Vector3(ledge_x - 0.3f, ly + 0.08f, zf + 0.35f), Vector3(0.18f, 0.13f, 0.16f), pal::MAPLE_ORANGE, 0);
			m.blob(Vector3(ledge_x + 0.3f, ly + 0.08f, zf + 0.35f), Vector3(0.18f, 0.13f, 0.16f), pal::CANDY_PINK, 0);
			solid(Vector3(ledge_x, ly - 0.12f, zf + 0.4f), Vector3(0.7f, 0.12f, 0.4f), true);
			lot.summit_route.push_back(Vector3(ledge_x, ly, zf + 0.4f));
		}
		if (h.roof_steps && tower_top.x < 1e8f) {
			lot.summit_route.push_back(Vector3(lx - 1.6f, eave + par + 0.11f, zf - 0.12f));
			lot.summit_route.push_back(Vector3(ac_x, eave + 0.9f, ac_z));
			lot.summit_route.push_back(tower_top);
		} else {
			lot.summit_route.clear(); // (a plain climb: no summit prize)
		}
	}
	m.set_ground_ao(false);

	// Paving in front of the shop, and a gravel service yard behind it.
	m.quad_out(Vector3(x0, 0.072f, 0), Vector3(x1, 0.072f, 0), Vector3(x1, 0.072f, zf), Vector3(x0, 0.072f, zf), mat_cell(pal::SIDEWALK, mat::CONCRETE),
			Vector3(0, -1, zf * 0.5f));
	if (ctx.terrain) {
		drape_lot_rect(ctx, m, lot, x0, x1, -lot.depth, zb, 0.004f, mat_cell(pal::STONE, mat::CONCRETE)); // Stage 4c: on the ground
	} else {
		m.quad_out(Vector3(x0, 0.004f, zb), Vector3(x1, 0.004f, zb), Vector3(x1, 0.004f, -lot.depth), Vector3(x0, 0.004f, -lot.depth),
				mat_cell(pal::STONE, mat::CONCRETE), Vector3(0, -1, zb - 1.0f));
	}
}

// ---------------------------------------------------------------- routes
void plan_shop_routes(GenContext &ctx, Lot &lot) {
	const HouseSpec &h = lot.house;
	const float zf = h.front_z;
	// Candy anchor (Stage 1e): straight in from the kerb side of the sidewalk to the doorstep's top.
	lot.front_walk = { Vector3(h.door_x, 0.0f, ctx.road_of(lot).walk - 0.4f), Vector3(h.door_x, 0.0f, zf + 0.95f), Vector3(h.door_x, h.deck_h, zf + 0.45f) };
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
	if (ctx.terrain) {
		// Stage 4c: the awning run's take-off out on the sidewalk sits on it (the sidewalk runs with the street, the
		// floor is level; candy's roof_run reads these as pad-relative heights, not heights over the ground).
		Vector3 &p = lot.roof_route.front();
		p.y = ctx.lot_ground(lot, p.x, p.z);
	}
}

// ---------------------------------------------------------------- sidewalk dressing
void dress_shopfront(GenContext &ctx, Lot &lot) {
	Rng rng = ctx.rng(STAGE_PROPS, lot.index);
	const HouseSpec &h = lot.house;
	const float zf = h.front_z;
	const float walk = ctx.road_of(lot).walk - ctx.road_of(lot).verge; // paved width in front of the shop
	const float tread_z = zf + 0.9f + 0.32f * float(lot.porch_steps - 3); // (Stage 4c: the doorstep's treads reach this far out)
	// Stage 3c: ground props (given at 0.07 on the paving, 0 in the service yard) take the surface under them (the
	// sidewalk running with the street, the level paving strip, the yard's ground): flat ones on its plane, tall ones
	// plumb sunk to its lowest point; what stands on one of them (a pumpkin on a barrel, a jack on a bale) rides it.
	struct Base {
		float x, z, y, given;
		Basis tilt;
	};
	std::vector<Base> bases;
	auto put = [&](const std::string &id, float x, float y, float z, float yaw, float scale = 1.0f, bool collide = true) {
		if (ctx.terrain && lot.porch_steps > 3 && z > 0.0f && z < tread_z + 0.4f && std::abs(x - h.door_x) < 1.9f) {
			x = Math::clamp(h.door_x + (x >= h.door_x ? 1.9f : -1.9f), -h.width * 0.5f + 0.3f, h.width * 0.5f - 0.3f);
		}
		if (ctx.terrain) {
			const float pad = lot.xf.origin.y;
			// Only samples on the same support as its centre count (the level paving strip in front of the shop, or the
			// sidewalk / yard): a footprint overhanging the plinth's edge stays on its centre's level.
			(void)pad;
			const int c_sup = shop_support(lot, x, z);
			auto surf = [&](float sx, float sz) { return shop_support(lot, sx, sz) == c_sup ? shop_surface(ctx, lot, sx, sz) : std::nanf(""); };
			Basis tilt;
			Vector3 p(x, y, z);
			const Base *on = nullptr;
			for (const Base &b : bases) {
				if (y > b.given + 0.05f && Vector2(b.x - x, b.z - z).length() < 0.45f) {
					on = &b;
				}
			}
			if (on) {
				p = Vector3(on->x, on->y, on->z) + on->tilt.xform(Vector3(0, y - on->given, 0)) + Vector3(x - on->x, 0.0f, z - on->z);
				tilt = on->tilt;
			} else if (y <= 0.08f) {
				const AssetInfo *a = ctx.assets->get(id);
				const AABB &bb = a->aabb;
				if (prop_settle_kind(id) == PropSettle::Upright) {
					const float r = Math::clamp(a->radius > 0.0f ? a->radius : Math::min(bb.size.x, bb.size.z) * 0.5f, 0.1f, 0.45f);
					p.y = footprint_low(surf, p, r * scale);
				} else {
					const GroundFit f = fit_footprint(surf, p, yaw, scale, bb.position.x, bb.get_end().x, bb.position.z, bb.get_end().z, 0.5f, 3);
					p.y = f.y;
					tilt = tilt_to(f.up);
				}
				bases.push_back({ x, z, p.y, y, tilt });
			} else {
				p.y += surf(x, z) - 0.07f;
			}
			PropPlacement pp;
			pp.id = id;
			pp.xf = lot.xf * Transform3D((tilt * Basis(Vector3(0, 1, 0), yaw)).scaled(Vector3(scale, scale, scale)), p);
			pp.lot = lot.index;
			pp.collide = collide;
			ctx.props.push_back(pp);
			return;
		}
		PropPlacement pp;
		pp.id = id;
		// Stage 4c: out on the sidewalk on its surface (it runs with the street; y is given over a level sidewalk at
		// the floor's height), clear of the doorstep's treads; in the service yard on the ground; the shop and its
		// plinth stay level.
		float lift = ctx.yard_lift(lot, x, z);
		if (ctx.terrain && z > 0.0f) {
			if (lot.porch_steps > 3 && z < tread_z + 0.4f && std::abs(x - h.door_x) < 1.9f) {
				x = Math::clamp(h.door_x + (x >= h.door_x ? 1.9f : -1.9f), -h.width * 0.5f + 0.3f, h.width * 0.5f - 0.3f);
			}
			const Vector3 w = lot.to_world(x, 0.0f, z);
			int which = -1;
			if (ctx.roads.zone(w, &which) != RoadZone::Off && which >= 0) { // (else a strip of ground: a set-back lot on a bend)
				lift = ctx.roads.roads[which].project(w).y - lot.xf.origin.y;
			} else {
				lift = ctx.yard_lift(lot, x, z) - 0.07f;
			}
		}
		pp.xf = lot.xf * Transform3D(Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(scale, scale, scale)), Vector3(x, y + lift, z));
		pp.lot = lot.index;
		pp.collide = collide;
		ctx.props.push_back(pp);
	};
	// Pumpkins and a jack-o'-lantern flanking the door, on the paved strip in front of the shop (Stage 4c: it
	// stays level with the floor when the sidewalk falls away below it).
	const float flank_z = ctx.terrain ? zf + 0.22f : zf + 0.45f;
	for (float sx : { -1.0f, 1.0f }) {
		const float x = h.door_x + sx * 1.15f;
		if (std::abs(x) > h.width * 0.5f - 0.5f) {
			continue;
		}
		if (rng.chance(0.6f)) {
			put(JACKS[rng.irange(0, 4)], x, 0.07f, flank_z, rng.range(-0.3f, 0.3f), 0.9f);
		} else {
			put("flower_pot", x, 0.07f, flank_z, 0.0f);
		}
		lot.features.push_back({ FeatureKind::Doorside, Vector3(x, 0.0f, flank_z), 0.35f, 0.0f }); // candy anchor (Stage 1e)
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
	lot.features.push_back({ FeatureKind::Shopfront, Vector3(fx, 0.0f, zf + 0.8f), 0.7f, 0.0f }); // candy anchor (Stage 1e)
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
				// Candy anchor (Stage 1e): a bale with a jack-o'-lantern on it, hopped over along the sidewalk.
				YardObstacle o;
				o.kind = ObstacleKind::Hay;
				o.c = Vector3(fx, 0.0f, zf + 0.7f);
				o.across = Vector3(1, 0, 0);
				o.half_across = ctx.assets->get("hay_bale")->aabb.size.x * 0.5f;
				o.top = 0.62f + 0.45f;
				lot.obstacles.push_back(o);
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
// Stage 4c: the square (planned in plan_town_lots, on its own level pad: terrain.cpp) gets the gazebo, benches,
// lamps strung to the gazebo roof, trees, hay and pumpkins, the travel gate and the candy cart; every other
// stretch of frontage no shop could use (a corner, the gore of a fork) a little plaza sized like a lot.
// Pavers lie on the ground (the square's front strip runs with the street, then a ramp to the pad).
void place_town_square(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_RHYTHM, 31);
	const RoadNet &net = ctx.roads;
	MeshBuilder &m = ctx.boundary_mesh;
	const Vector3 up(0, 1, 0);
	const int pavers = mat_cell(pal::BONE, mat::PAVERS);
	auto put_at = [&](const Transform3D &frame, const std::string &id, float lx, float lz, float yaw, float scale, float y) {
		PropPlacement pp;
		pp.id = id;
		pp.xf = frame * Transform3D(Basis(up, yaw).scaled(Vector3(scale, scale, scale)), Vector3(lx, y, lz));
		if (ctx.terrain) {
			pp.xf.origin.y += ctx.ground(pp.xf.origin.x, pp.xf.origin.z) - frame.origin.y; // on the ground
		}
		pp.lot = -1;
		ctx.props.push_back(pp);
	};
	auto pave = [&](const Lot &area, float w, float depth, float lift) {
		m.set_transform(area.xf); // (the boundary mesh is in settlement space; the paving is laid in the area's frame)
		if (ctx.terrain) {
			drape_lot_rect(ctx, m, area, -w * 0.5f, w * 0.5f, -depth, 0.0f, lift, pavers);
		} else {
			m.quad_out(Vector3(-w * 0.5f, lift, 0), Vector3(w * 0.5f, lift, 0), Vector3(w * 0.5f, lift, -depth), Vector3(-w * 0.5f, lift, -depth), pavers,
					Vector3(0, -1, -depth * 0.5f));
		}
		m.set_transform(Transform3D());
	};
	const TownSquare &sq = ctx.square;
	ctx.plazas.clear();
	// ---- little plazas in the frontage no shop could use
	for (int r = 0; r < int(net.roads.size()); ++r) {
		const Road &s = net.roads[r];
		for (int side : { 1, -1 }) {
			for (const auto &g : frontage_gaps(ctx, r, side, 5.0f)) {
				if (sq.valid && sq.area.road == r && sq.area.side == side && g.first < sq.area.s + sq.area.width * 0.5f + 1.0f &&
						g.second > sq.area.s - sq.area.width * 0.5f - 1.0f) {
					continue; // the square itself
				}
				const float cx = (g.first + g.second) * 0.5f;
				const float w = g.second - g.first;
				Lot probe;
				probe.index = -1;
				probe.shop = -2;
				probe.road = r;
				probe.side = side;
				probe.xf = s.frontage_frame(cx, side);
				probe.width = w;
				float depth = 0.0f;
				for (float d = 9.0f; d >= 3.0f; d -= 1.5f) {
					probe.depth = d;
					float own, other;
					lot_corridor_reach(ctx, probe, own, other);
					bool clear = own <= 0.05f && other <= -0.1f;
					for (size_t k = 0; k < ctx.lots.size() && clear; ++k) {
						clear = lot_overlap(probe, ctx.lots[k]) <= 0.0f;
					}
					clear = clear && (!sq.valid || lot_overlap(probe, sq.area) <= 0.0f);
					if (clear) {
						depth = d;
						break;
					}
				}
				if (depth <= 0.0f) {
					continue;
				}
				probe.depth = depth;
				pave(probe, w, depth, 0.05f);
				ctx.plazas.push_back(probe);
				const Transform3D &frame = probe.xf;
				const float hw = w * 0.5f - 1.2f;
				// A bench, a lamp, a tree in the paving and a pumpkin; a wider one a hay bale with a jack-o'-lantern too.
				put_at(frame, "bench", 0.0f, -Math::min(1.5f, depth - 1.0f), 0.0f, 1.0f, 0.0f);
				put_at(frame, "lamp_post", -hw, -0.8f, 0.0f, 1.0f, 0.0f);
				put_at(frame, TREES[rng.irange(0, 2)], hw * 0.6f, -Math::min(5.0f, depth - 1.5f), rng.range(0.0f, Math_TAU), 0.9f, 0.0f);
				put_at(frame, "pumpkin_round", -hw * 0.5f, -Math::min(3.0f, depth - 1.0f), rng.range(0.0f, Math_TAU), 1.0f, 0.0f);
				if (w >= 14.0f && depth >= 6.0f) {
					put_at(frame, "hay_bale", -hw * 0.6f, -depth + 1.6f, Math_PI * 0.5f, 1.0f, 0.0f);
					put_at(frame, JACKS[rng.irange(0, 4)], -hw * 0.6f, -depth + 1.6f, rng.range(-0.3f, 0.3f), 0.85f, 0.55f);
				}
			}
		}
	}
	// ---- the square
	if (sq.valid) {
		const Lot &A = sq.area;
		const Transform3D &frame = A.xf;
		const float w = A.width, depth = A.depth;
		pave(A, w, depth, 0.03f);
		ctx.plazas.push_back(A);
		const float hw = w * 0.5f - 1.2f;
		// Gazebo on the pad: octagonal deck, turned posts with a low rail, cone roof you can stand on.
		const float r = 3.0f, deck = 0.5f, post_h = 2.6f;
		const float gz = Math::max(sq.pad_z - r - 0.6f, -depth + r + 2.6f);
		const Vector3 gc = frame.xform(Vector3(0, 0, gz));
		const int trim = mat_cell(pal::TRIM_WHITE, mat::WOOD);
		m.cylinder(gc, r + 0.2f, deck, 8, mat_cell(pal::TRIM_WHITE, mat::LATTICE), -1.0f, mat_cell(pal::PLANK, mat::PLANKS));
		m.cylinder(gc + Vector3(0, deck, 0), r + 0.25f, 0.06f, 8, trim);
		const Vector3 street = frame.basis.xform(Vector3(0, 0, 1)); // toward the street
		const float a_open = std::atan2(street.z, street.x);
		for (int k = 0; k < 8; ++k) {
			const float a = a_open + Math_TAU / 16.0f + k * Math_TAU / 8.0f;
			const Vector3 p = gc + Vector3(std::cos(a) * r, deck, std::sin(a) * r);
			m.cylinder(p, 0.1f, post_h, 6, trim, 0.08f);
			const float a2 = a + Math_TAU / 8.0f;
			const Vector3 q = gc + Vector3(std::cos(a2) * r, deck, std::sin(a2) * r);
			if (k != 7) { // the opening faces the street
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
		auto put = [&](const std::string &id, float lx, float lz, float yaw, float scale = 1.0f, float y = 0.0f) {
			put_at(frame, id, lx, Math::max(lz, -depth + 0.8f), yaw, scale, y);
		};
		// Benches facing the gazebo, lamps with lights strung to its roof, trees, hay and pumpkins.
		for (int k = 0; k < 4; ++k) {
			const float a = Math_PI * 0.25f + k * Math_PI * 0.5f;
			const Vector3 lp(std::cos(a) * (r + 2.6f), 0, gz + std::sin(a) * (r + 2.6f));
			if (std::abs(lp.x) > hw) {
				continue;
			}
			put("bench", lp.x, lp.z, std::atan2(-lp.x, -(lp.z - gz)));
		}
		for (float sx : { -1.0f, 1.0f }) {
			const float lx = sx * Math::min(hw, 7.5f);
			put("lamp_post", lx, -1.0f, 0.0f);
			Vector3 la = frame.xform(Vector3(lx, 0.0f, -1.0f));
			la.y = (ctx.terrain ? ctx.ground(la.x, la.z) : la.y) + 3.0f;
			StringLight sl;
			sl.a = la;
			sl.b = gc + Vector3(0, deck + post_h + 1.0f, 0);
			sl.sag = 0.6f;
			ctx.strings.push_back(sl);
			put(TREES[rng.irange(0, 2)], sx * Math::min(hw, 8.5f), gz - 5.0f, rng.range(0.0f, Math_TAU), 0.95f);
			put("hay_bale", sx * (r + 3.2f), gz - 4.5f, Math_PI * 0.5f);
			put(JACKS[rng.irange(0, 4)], sx * (r + 3.2f), gz - 4.5f, 0.0f, 0.85f, 0.55f);
			put("pumpkin_round", sx * (r + 1.8f), gz + 4.8f, rng.range(0.0f, Math_TAU), 0.8f);
		}
		put("scarecrow", -Math::min(hw, 6.0f) * 0.6f, gz - 6.0f, 0.0f);
		// The travel gate on one side of the gazebo, the candy cart across from it facing it (plan_infestation).
		const float gs = rng.chance(0.5f) ? 1.0f : -1.0f;
		const float zz = sq.pad_z - 1.6f;
		Vector3 gate = frame.xform(Vector3(gs * (w * 0.5f - 2.6f), 0.0f, zz));
		Vector3 cart = frame.xform(Vector3(-gs * (w * 0.5f - 2.4f), 0.0f, zz));
		if (ctx.terrain) {
			gate.y = ctx.ground(gate.x, gate.z);
			cart.y = ctx.ground(cart.x, cart.z);
		}
		const Vector3 face = frame.basis.xform(Vector3(gs, 0.0f, 0.0f)).normalized();
		ctx.square.gate = gate;
		ctx.square.cart = Transform3D(Basis(up, std::atan2(face.x, face.z)), cart);
	}
	// Festival bunting strung across the streets every so often (not over a junction's corners).
	for (const Road &s : net.roads) {
		for (float x = s.lots_s0 + rng.range(8.0f, 14.0f); x < s.lots_s1 - 8.0f; x += rng.range(16.0f, 24.0f)) {
			if (s.cut(1, x) || s.cut(-1, x)) {
				continue;
			}
			StringLight sl;
			sl.style = STRING_BUNTING;
			sl.a = s.at(x) + s.left_at(x) * (s.edge() - 0.3f) + Vector3(0, 5.6f, 0);
			sl.b = s.at(x) - s.left_at(x) * (s.edge() - 0.3f) + Vector3(0, 5.6f, 0);
			sl.sag = 0.9f;
			sl.out = Vector3(s.left_at(x).z, 0, -s.left_at(x).x);
			ctx.strings.push_back(sl);
		}
	}
}

} // namespace tot
