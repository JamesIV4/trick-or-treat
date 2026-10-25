// Stages 1-3: street skeleton, lots, households. Also the street surface and playground boundary.

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

#include "gen/context.h"

using namespace godot;

namespace tot {

const char *persona_name(Persona p) {
	static const char *names[] = { "Pumpkin fanatics", "Green thumbs", "Young family", "Cozy grandparents",
		"Crafty artists", "Minimalists", "Spooky-cute superfans", "Bakers", "Sporty family", "Pet lovers",
		"Stargazers", "Tinkerers" };
	return names[int(p)];
}

const char *landmark_name(Landmark l) {
	static const char *names[] = { "None", "Pumpkin Patch house", "Scarecrow house", "Sunflower house",
		"Tree House yard", "Hay Ride garage", "Cauldron cottage", "Inflatable Wonderland", "Lantern Porch house",
		"Carving Contest porch" };
	return names[int(l)];
}

// ---------------------------------------------------------------- context helpers
void GenContext::add_box(int lot, Vector3 center, Vector3 half, bool walkable, float yaw, Basis extra) {
	OBox b;
	Transform3D local(Basis(Vector3(0, 1, 0), yaw) * extra, center);
	b.xf = lot >= 0 ? lots[lot].xf * local : local;
	b.half = half;
	b.walkable = walkable;
	b.lot = lot;
	boxes.push_back(b);
}

void GenContext::add_prop(int lot, const std::string &id, Vector3 p, float yaw, float scale, float wind) {
	PropPlacement pp;
	pp.id = id;
	Transform3D local(Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(scale, scale, scale)), p);
	pp.xf = lot >= 0 ? lots[lot].xf * local : local;
	pp.lot = lot;
	pp.wind = wind;
	props.push_back(pp);
}

bool GenContext::occupied_at(int lot, float x, float z, float r) const {
	for (const Circle &c : occupied[lot]) {
		const float dx = c.x - x, dz = c.z - z;
		if (dx * dx + dz * dz < (c.r + r) * (c.r + r)) {
			return true;
		}
	}
	return false;
}

void GenContext::occupy(int lot, float x, float z, float r) {
	occupied[lot].push_back({ x, z, r });
}

float GenContext::route_distance(int lot, float x, float z) const {
	const std::vector<Vector3> &r = lots[lot].loop_route;
	float best = 1e9f;
	for (size_t i = 0; i + 1 < r.size(); ++i) {
		const Vector2 a(r[i].x, r[i].z), b(r[i + 1].x, r[i + 1].z), p(x, z);
		const Vector2 ab = b - a;
		const float t = Math::clamp((p - a).dot(ab) / Math::max(ab.length_squared(), 1e-6f), 0.0f, 1.0f);
		best = Math::min(best, (a + ab * t).distance_to(p));
	}
	return best;
}

// ---------------------------------------------------------------- street plan
static float z_of(float x, float amp, float k, float phase) {
	return amp * std::sin(k * x + phase);
}

Vector3 StreetPlan::at(float x) const {
	const float start = center.front().x;
	const float fi = Math::clamp(x - start, 0.0f, float(center.size() - 1) - 0.001f);
	const int i = int(fi);
	return center[i].lerp(center[i + 1], fi - i);
}

Vector3 StreetPlan::left_at(float x) const {
	const float start = center.front().x;
	const float fi = Math::clamp(x - start, 0.0f, float(left.size() - 1) - 0.001f);
	const int i = int(fi);
	return left[i].lerp(left[i + 1], fi - i).normalized();
}

void plan_street(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_STREET);
	StreetPlan &s = ctx.street;
	const bool town = ctx.kind == SettlementKind::Town;
	s.length = rng.range(90.0f, 104.0f);
	float amp = rng.range(2.5f, 5.0f);
	const float k = Math_TAU / rng.range(85.0f, 130.0f);
	const float phase = rng.range(0.0f, Math_TAU);
	if (town) {
		// Main Street: straighter, with wide paved sidewalks right up to the storefronts.
		amp = rng.range(0.4f, 1.4f);
		s.walk = 4.4f;
		s.verge = 1.1f;
	}
	// Alone, the street keeps going into the fog beyond the hay walls; in the open world it ends
	// where the country road takes over.
	const float half = s.length * 0.5f + (ctx.embedded ? 1.0f : 40.0f);
	s.center.clear();
	s.left.clear();
	for (float x = -half; x <= half + 0.01f; x += 1.0f) {
		const float z = z_of(x, amp, k, phase);
		const float dz = amp * k * std::cos(k * x + phase);
		const Vector3 t = Vector3(1, 0, dz).normalized();
		s.center.push_back(Vector3(x, 0, z));
		s.left.push_back(Vector3(-t.z, 0, t.x));
	}
	s.lane_x = rng.range(-14.0f, 14.0f);
	s.lane_dir = rng.chance(0.5f) ? 1 : -1;
	s.lane_len = rng.range(24.0f, 28.0f);
	s.lane_origin = s.at(s.lane_x);
	ctx.bound_x0 = -s.length * 0.5f;
	ctx.bound_x1 = s.length * 0.5f;
	// Spawn in the intersection, facing down the longer arm of the street.
	const bool face_pos = s.lane_x < 0.0f;
	ctx.spawn = s.at(s.lane_x + (face_pos ? -1.5f : 1.5f)) + Vector3(0, 0.1f, 0);
	ctx.spawn_yaw = face_pos ? Math_PI * 0.5f : -Math_PI * 0.5f; // model front is +Z: (sin yaw, cos yaw)
	// The big moon hangs low over the far end of the street the player starts facing.
	const Vector3 face(std::sin(ctx.spawn_yaw), 0, std::cos(ctx.spawn_yaw));
	// Rotated the same way as the camera's diagonal so it sits in the opening frame.
	const float elev = rng.range(0.15f, 0.2f);
	const Vector3 flat = face.rotated(Vector3(0, 1, 0), rng.range(0.22f, 0.42f));
	ctx.moon_dir = (flat * std::cos(elev) + Vector3(0, std::sin(elev), 0)).normalized();
}

// ---------------------------------------------------------------- lots
void plan_lots(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_LOTS);
	const StreetPlan &s = ctx.street;
	ctx.lots.clear();
	const float corridor = s.edge() + 0.8f;
	for (int side : { 1, -1 }) {
		float x = ctx.bound_x0 + rng.range(1.0f, 3.0f);
		while (true) {
			float w = rng.range(14.5f, 19.0f);
			if (side == s.lane_dir && x < s.lane_x + corridor && x + w > s.lane_x - corridor) {
				// Corner: end this run at the lane, or jump over it.
				const float room = (s.lane_x - corridor) - x;
				if (room >= 13.5f) {
					w = room;
				} else {
					x = s.lane_x + corridor;
					continue;
				}
			}
			if (x + w > ctx.bound_x1 - 1.0f) {
				const float room = ctx.bound_x1 - 1.0f - x;
				if (room < 13.0f) {
					break;
				}
				w = room;
			}
			Lot lot;
			lot.index = int(ctx.lots.size());
			lot.side = side;
			lot.width = w;
			lot.depth = rng.range(21.0f, 24.0f);
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
	ctx.bound_z0 = zmin;
	ctx.bound_z1 = zmax;
}

// ---------------------------------------------------------------- households
static Persona typical_persona(Landmark l) {
	switch (l) {
		case Landmark::PumpkinPatch:
		case Landmark::CarvingContest:
			return Persona::PumpkinFanatics;
		case Landmark::Scarecrow:
		case Landmark::Sunflower:
			return Persona::GreenThumbs;
		case Landmark::TreeHouse:
		case Landmark::HayRide:
			return Persona::YoungFamily;
		case Landmark::Cauldron:
		case Landmark::Inflatables:
			return Persona::SpookyCute;
		case Landmark::LanternPorch:
			return Persona::CozyGrandparents;
		default:
			return Persona::CozyGrandparents;
	}
}

static bool climb_heavy(Landmark l) {
	return l == Landmark::PumpkinPatch || l == Landmark::TreeHouse || l == Landmark::HayRide;
}

void assign_households(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_HOUSEHOLDS);
	const StreetPlan &s = ctx.street;
	std::vector<Landmark> pool;
	for (int i = 1; i < int(Landmark::Count); ++i) {
		pool.push_back(Landmark(i));
	}
	rng.shuffle(pool);
	const int want = rng.chance(0.5f) ? 3 : 2;
	std::vector<Landmark> chosen;
	std::vector<Persona> used_personas;
	for (Landmark l : pool) {
		if (int(chosen.size()) >= want) {
			break;
		}
		Persona p = typical_persona(l);
		if (std::find(used_personas.begin(), used_personas.end(), p) != used_personas.end()) {
			continue;
		}
		chosen.push_back(l);
		used_personas.push_back(p);
	}
	if (std::none_of(chosen.begin(), chosen.end(), climb_heavy)) {
		for (Landmark l : pool) {
			if (climb_heavy(l) && std::find(used_personas.begin(), used_personas.end(), typical_persona(l)) == used_personas.end()) {
				chosen.back() = l;
				break;
			}
		}
	}
	// Visibility score: intersection corners, the outside of the curve, near the spawn.
	std::vector<std::pair<float, int>> scored;
	for (const Lot &lot : ctx.lots) {
		float score = 0.0f;
		const float dx = std::abs(lot.center_x - s.lane_x);
		score += Math::max(0.0f, 3.0f - dx / 10.0f);
		const Vector3 c0 = s.at(lot.center_x - 5), c1 = s.at(lot.center_x), c2 = s.at(lot.center_x + 5);
		const float curvature = (c0.z + c2.z - 2.0f * c1.z);
		if (curvature * lot.side < 0.0f) {
			score += 1.0f;
		}
		score += rng.range(0.0f, 0.8f);
		scored.push_back({ -score, lot.index });
	}
	std::sort(scored.begin(), scored.end());
	std::vector<int> landmark_lots;
	for (auto &sc : scored) {
		if (landmark_lots.size() >= chosen.size()) {
			break;
		}
		const Lot &cand = ctx.lots[sc.second];
		bool adjacent = false;
		for (int li : landmark_lots) {
			const Lot &o = ctx.lots[li];
			if (o.side == cand.side && std::abs(o.index - cand.index) == 1) {
				adjacent = true;
			}
		}
		if (cand.width < 15.0f || adjacent) {
			continue;
		}
		landmark_lots.push_back(cand.index);
	}
	for (size_t i = 0; i < landmark_lots.size(); ++i) {
		Lot &lot = ctx.lots[landmark_lots[i]];
		lot.landmark = chosen[i];
		lot.persona = typical_persona(chosen[i]);
	}
	// Everyone else: favour personas not seen yet tonight (at most two of each), never the same
	// as a street neighbour.
	std::vector<int> used(int(Persona::Count), 0);
	for (const Lot &lot : ctx.lots) {
		if (lot.landmark != Landmark::None) {
			used[int(lot.persona)]++;
		}
	}
	for (Lot &lot : ctx.lots) {
		if (lot.landmark != Landmark::None) {
			continue;
		}
		std::vector<float> w(int(Persona::Count), 0.0f);
		for (int p = 0; p < int(Persona::Count); ++p) {
			w[p] = used[p] == 0 ? 3.0f : (used[p] == 1 ? 1.0f : 0.0f);
			for (const Lot &o : ctx.lots) {
				if (o.index != lot.index && o.side == lot.side && std::abs(o.index - lot.index) == 1 && int(o.persona) == p &&
						(o.landmark != Landmark::None || o.index < lot.index)) {
					w[p] = 0.0f;
				}
			}
		}
		const int pick = rng.weighted(w);
		lot.persona = Persona(pick);
		used[pick]++;
	}
}

// ---------------------------------------------------------------- street surface
void build_street(GenContext &ctx) {
	const StreetPlan &s = ctx.street;
	MeshBuilder &m = ctx.street_mesh;
	const float hr = s.half_road, hc = s.half_road + s.curb, he = s.edge();
	const float curb_h = 0.12f, walk_y = 0.07f, verge_y = 0.05f;
	const int verge_cell = ctx.kind == SettlementKind::Town ? mat_cell(pal::BRICK, mat::PAVERS) : mat_cell(pal::LAWN, mat::GRASS);
	// Road.
	m.ribbon(s.center, s.left, -hr - 0.1f, hr + 0.1f, 0.01f, mat_cell(pal::ASPHALT, mat::ASPHALT));
	// Curbs and sidewalks, with a gap where the side lane joins.
	const size_t n = s.center.size();
	for (int side : { 1, -1 }) {
		for (size_t i = 0; i + 1 < n; ++i) {
			const Vector3 &c0 = s.center[i], &c1 = s.center[i + 1];
			const Vector3 &l0 = s.left[i], &l1 = s.left[i + 1];
			const float x = (c0.x + c1.x) * 0.5f;
			const bool mouth = side == s.lane_dir && std::abs(x - s.lane_x) < hc + 0.2f;
			if (mouth) {
				continue;
			}
			const float sd = float(side);
			auto P = [&](const Vector3 &c, const Vector3 &l, float off, float y) {
				Vector3 p = c + l * (off * sd);
				p.y = y;
				return p;
			};
			// Curb top + road-facing face.
			m.quad_out(P(c0, l0, hr, curb_h), P(c1, l1, hr, curb_h), P(c1, l1, hc, curb_h), P(c0, l0, hc, curb_h), mat_cell(pal::STONE_LIGHT, mat::CONCRETE), P(c0, l0, (hr + hc) * 0.5f, -1));
			m.quad_out(P(c0, l0, hr, 0.0f), P(c1, l1, hr, 0.0f), P(c1, l1, hr, curb_h), P(c0, l0, hr, curb_h), mat_cell(pal::STONE, mat::CONCRETE), P(c0, l0, hr + 1.0f, curb_h * 0.5f));
			// Sidewalk, scored every 2 m with alternating tone.
			const int tone = mat_cell((int(std::floor(x / 2.0f)) & 1) ? pal::SIDEWALK : pal::STONE_LIGHT, mat::CONCRETE);
			// Grass verge between curb and sidewalk (brick pavers in town), then the sidewalk.
			const float hv = hc + s.verge;
			m.quad_out(P(c0, l0, hc, verge_y), P(c1, l1, hc, verge_y), P(c1, l1, hv, verge_y), P(c0, l0, hv, verge_y), verge_cell, P(c0, l0, (hc + hv) * 0.5f, -1));
			m.quad_out(P(c0, l0, hv, walk_y), P(c1, l1, hv, walk_y), P(c1, l1, he, walk_y), P(c0, l0, he, walk_y), tone, P(c0, l0, (hv + he) * 0.5f, -1));
		}
	}
	// Dashed centre line, skipping the intersection.
	for (float x = s.center.front().x; x < s.center.back().x - 2.0f; x += 4.0f) {
		if (std::abs(x - s.lane_x) < hr + 1.0f) {
			continue;
		}
		const Vector3 a = s.at(x), b = s.at(x + 2.0f);
		const Vector3 la = s.left_at(x), lb = s.left_at(x + 2.0f);
		Vector3 q[4] = { a - la * 0.09f, b - lb * 0.09f, b + lb * 0.09f, a + la * 0.09f };
		for (Vector3 &p : q) {
			p.y = 0.02f;
		}
		m.quad_out(q[0], q[1], q[2], q[3], pal::ROAD_LINE, (a + b) * 0.5f - Vector3(0, 1, 0));
	}
	// Gutters, storm drains and manhole covers.
	Rng drng = ctx.rng(STAGE_STREET, 7);
	for (int side : { 1, -1 }) {
		for (size_t i = 0; i + 1 < n; ++i) {
			const float x = (s.center[i].x + s.center[i + 1].x) * 0.5f;
			if (side == s.lane_dir && std::abs(x - s.lane_x) < hc + 0.2f) {
				continue;
			}
			const float sd = float(side);
			Vector3 a0 = s.center[i] + s.left[i] * (sd * (hr - 0.4f)), a1 = s.center[i + 1] + s.left[i + 1] * (sd * (hr - 0.4f));
			Vector3 b0 = s.center[i] + s.left[i] * (sd * hr), b1 = s.center[i + 1] + s.left[i + 1] * (sd * hr);
			a0.y = a1.y = b0.y = b1.y = 0.014f;
			m.quad_out(a0, a1, b1, b0, mat_cell(pal::STONE, mat::CONCRETE), (a0 + b1) * 0.5f - Vector3(0, 1, 0));
		}
		for (float x = s.center.front().x + drng.range(6.0f, 12.0f); x < s.center.back().x; x += drng.range(16.0f, 22.0f)) {
			if (side == s.lane_dir && std::abs(x - s.lane_x) < hc + 1.0f) {
				continue;
			}
			const Vector3 p = s.at(x) + s.left_at(x) * (float(side) * (hr - 0.25f));
			const Vector3 t = (s.at(x + 1) - s.at(x - 1)).normalized();
			const Vector3 lf = s.left_at(x);
			for (int k = -2; k <= 2; ++k) {
				m.beam(p + t * (k * 0.16f) - lf * 0.2f + Vector3(0, 0.02f, 0), p + t * (k * 0.16f) + lf * 0.2f + Vector3(0, 0.02f, 0), 0.06f, 0.02f,
						mat_cell(pal::IRON, mat::METAL));
			}
			m.beam(p - t * 0.42f + Vector3(0, 0.016f, 0), p + t * 0.42f + Vector3(0, 0.016f, 0), 0.48f, 0.01f, pal::CHARCOAL);
		}
	}
	for (float x = s.center.front().x + 9.0f; x < s.center.back().x; x += drng.range(24.0f, 34.0f)) {
		if (std::abs(x - s.lane_x) < hc + 2.0f) {
			continue;
		}
		const Vector3 p = s.at(x) + s.left_at(x) * drng.range(-1.5f, 1.5f);
		m.cylinder(p + Vector3(0, 0.0f, 0), 0.38f, 0.03f, 14, mat_cell(pal::IRON, mat::METAL));
		m.cylinder(p + Vector3(0, 0.0f, 0), 0.44f, 0.022f, 14, mat_cell(pal::STONE, mat::CONCRETE));
	}
	// Zebra crosswalk across the side lane's mouth.
	{
		const Vector3 o = s.lane_origin + Vector3(0, 0, float(s.lane_dir) * (hc + 1.4f));
		for (int k = -3; k <= 3; ++k) {
			const Vector3 c = o + Vector3(k * 1.1f, 0.024f, 0);
			m.quad_out(c + Vector3(-0.3f, 0, -0.9f), c + Vector3(0.3f, 0, -0.9f), c + Vector3(0.3f, 0, 0.9f), c + Vector3(-0.3f, 0, 0.9f),
					mat_cell(pal::TRIM_WHITE, mat::CONCRETE), c - Vector3(0, 1, 0));
		}
	}
	// Painted pumpkin hopscotch across the road beside the intersection.
	const float hx = s.lane_x + (s.lane_x < 0 ? 7.5f : -7.5f);
	const Vector3 hcen = s.at(hx), hl = s.left_at(hx);
	for (int i = -3; i <= 3; ++i) {
		const Vector3 c = hcen + hl * (i * 1.1f);
		const int cell = (i & 1) ? pal::PUMPKIN : pal::CANDY_CORN_YELLOW;
		Vector3 t = Vector3(1, 0, 0);
		Vector3 q[4] = { c - t * 0.45f - hl * 0.45f, c + t * 0.45f - hl * 0.45f, c + t * 0.45f + hl * 0.45f, c - t * 0.45f + hl * 0.45f };
		for (Vector3 &p : q) {
			p.y = 0.022f;
		}
		m.quad_out(q[0], q[1], q[2], q[3], cell, c - Vector3(0, 1, 0));
	}
	// Side lane: straight along z from the main street centre.
	const Vector3 o = s.lane_origin;
	const Vector3 dir(0, 0, float(s.lane_dir));
	const Vector3 lat(1, 0, 0);
	std::vector<Vector3> lc, ll;
	for (float d = 0.0f; d <= s.lane_len + 0.01f; d += 1.0f) {
		lc.push_back(o + dir * d);
		ll.push_back(lat);
	}
	m.ribbon(lc, ll, -hr, hr, 0.012f, mat_cell(pal::ASPHALT, mat::ASPHALT));
	for (float sd : { -1.0f, 1.0f }) {
		for (size_t i = 0; i + 1 < lc.size(); ++i) {
			const float d = float(i);
			if (d < hr - 0.5f) {
				continue;
			}
			auto P = [&](size_t k, float off, float y) {
				Vector3 p = lc[k] + lat * (off * sd);
				p.y = y;
				return p;
			};
			m.quad_out(P(i, hr, curb_h), P(i + 1, hr, curb_h), P(i + 1, hc, curb_h), P(i, hc, curb_h), mat_cell(pal::STONE_LIGHT, mat::CONCRETE), P(i, (hr + hc) * 0.5f, -1));
			m.quad_out(P(i, hr, 0.0f), P(i + 1, hr, 0.0f), P(i + 1, hr, curb_h), P(i, hr, curb_h), mat_cell(pal::STONE, mat::CONCRETE), P(i, hr + 1.0f, curb_h * 0.5f));
			const int tone = mat_cell((int(std::floor(d / 2.0f)) & 1) ? pal::SIDEWALK : pal::STONE_LIGHT, mat::CONCRETE);
			const float hv = hc + s.verge;
			m.quad_out(P(i, hc, verge_y), P(i + 1, hc, verge_y), P(i + 1, hv, verge_y), P(i, hv, verge_y), verge_cell, P(i, (hc + hv) * 0.5f, -1));
			m.quad_out(P(i, hv, walk_y), P(i + 1, hv, walk_y), P(i + 1, he, walk_y), P(i, he, walk_y), tone, P(i, (hv + he) * 0.5f, -1));
		}
	}
	// Lane centre dashes.
	for (float d = he + 1.0f; d < s.lane_len - 2.0f; d += 4.0f) {
		const Vector3 a = o + dir * d, b = o + dir * (d + 2.0f);
		Vector3 q[4] = { a - lat * 0.09f, b - lat * 0.09f, b + lat * 0.09f, a + lat * 0.09f };
		for (Vector3 &p : q) {
			p.y = 0.022f;
		}
		m.quad_out(q[0], q[1], q[2], q[3], pal::ROAD_LINE, (a + b) * 0.5f - Vector3(0, 1, 0));
	}
}

// ---------------------------------------------------------------- boundary
static void hay_wall(GenContext &ctx, Vector3 center, Vector3 along, float width, Rng &rng) {
	MeshBuilder &m = ctx.boundary_mesh;
	const Vector3 up(0, 1, 0);
	const Vector3 across = up.cross(along).normalized();
	const int cols = int(width / 1.15f);
	for (int row = 0; row < 6; ++row) {
		for (int c = 0; c < cols; ++c) {
			if (row >= 4 && rng.chance(0.3f)) {
				continue;
			}
			const float off = (c - (cols - 1) * 0.5f) * 1.15f + ((row & 1) ? 0.4f : 0.0f);
			const Vector3 p = center + along * off + across * rng.range(-0.1f, 0.1f) + up * (0.28f + row * 0.56f);
			Transform3D t(Basis(along, up, across), p);
			m.set_transform(t * Transform3D(Basis(Vector3(0, 1, 0), rng.range(-0.08f, 0.08f)), Vector3()));
			m.rounded_box(Vector3(), Vector3(0.56f, 0.28f, 0.32f), 0.07f, mat_cell(row % 3 == 2 ? pal::STRAW_DARK : pal::HAY, mat::STRAW), mat_cell(pal::STRAW, mat::STRAW));
			if (row == 3 && rng.chance(0.35f)) {
				m.blob(Vector3(0, 0.5f, 0), Vector3(0.28f, 0.22f, 0.28f), mat_cell(pal::PUMPKIN, mat::PUMPKIN), 1);
			}
		}
	}
	m.set_transform(Transform3D());
	OBox b;
	b.xf = Transform3D(Basis(along, up, across), center + up * 10.0f);
	b.half = Vector3(width * 0.5f + 0.5f, 10.0f, 0.5f);
	b.boundary = true;
	ctx.boxes.push_back(b);
}

static void hedge_run(GenContext &ctx, Vector3 a, Vector3 b, Rng &rng, float height = 3.2f) {
	MeshBuilder &m = ctx.boundary_mesh;
	const Vector3 d = b - a;
	const float len = d.length();
	if (len < 0.1f) {
		return;
	}
	const Vector3 along = d / len, up(0, 1, 0), across = up.cross(along);
	Transform3D t(Basis(along, up, across), (a + b) * 0.5f);
	m.set_transform(t);
	m.rounded_box(Vector3(0, height * 0.45f, 0), Vector3(len * 0.5f + 0.3f, height * 0.45f, 0.6f), 0.25f, mat_cell(pal::HEDGE_DARK, mat::HEDGE), mat_cell(pal::HEDGE, mat::HEDGE));
	for (float s = -len * 0.5f; s <= len * 0.5f; s += 1.6f) {
		m.blob(Vector3(s + rng.range(-0.3f, 0.3f), height * 0.92f, rng.range(-0.15f, 0.15f)), Vector3(1.0f, 0.55f, 0.75f), mat_cell(pal::HEDGE, mat::HEDGE), 1, 0.15f, uint32_t(rng.next()));
	}
	m.set_transform(Transform3D());
	OBox box;
	box.xf = Transform3D(Basis(along, up, across), (a + b) * 0.5f + up * 10.0f);
	box.half = Vector3(len * 0.5f + 0.3f, 10.0f, 0.7f);
	box.boundary = true;
	ctx.boxes.push_back(box);
}

void build_boundary(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_STREET, 99);
	const StreetPlan &s = ctx.street;
	// Back hedges behind every lot, plus side hedges at the outer ends of each row.
	for (const Lot &lot : ctx.lots) {
		const float hw = lot.width * 0.5f;
		hedge_run(ctx, lot.to_world(-hw - 0.6f, 0, -lot.depth - 0.6f), lot.to_world(hw + 0.6f, 0, -lot.depth - 0.6f), rng);
		for (float sx : { -1.0f, 1.0f }) {
			const Vector3 edge = lot.to_world(sx * hw, 0, 0);
			const bool outer = edge.x < ctx.bound_x0 + 4.0f || edge.x > ctx.bound_x1 - 4.0f;
			if (outer) {
				hedge_run(ctx, lot.to_world(sx * (hw + 0.6f), 0, -0.4f), lot.to_world(sx * (hw + 0.6f), 0, -lot.depth - 0.6f), rng);
			}
		}
	}
	// Hay-bale walls across both street ends (in the open world the street runs on as a road).
	if (!ctx.embedded) {
		for (float ex : { ctx.bound_x0 + 0.5f, ctx.bound_x1 - 0.5f }) {
			const Vector3 c = s.at(ex);
			Vector3 l = s.left_at(ex);
			hay_wall(ctx, c, l, (s.edge() + 1.2f) * 2.0f, rng);
		}
	}
	// Lane end: a lantern arch, closed by a hedge unless a road carries on beyond it.
	const Vector3 lane_end = s.lane_origin + Vector3(0, 0, float(s.lane_dir) * (s.lane_len + 0.8f));
	if (!ctx.lane_open) {
		hedge_run(ctx, lane_end + Vector3(-s.edge() - 1.0f, 0, 0), lane_end + Vector3(s.edge() + 1.0f, 0, 0), rng, 3.4f);
	}
	MeshBuilder &m = ctx.boundary_mesh;
	for (float sx : { -1.0f, 1.0f }) {
		m.cylinder(lane_end + Vector3(sx * 3.2f, 0, -float(s.lane_dir) * 0.9f), 0.22f, 4.4f, 8, mat_cell(pal::WOOD_DARK, mat::WOOD));
	}
	for (int i = 0; i <= 10; ++i) {
		const float t = float(i) / 10.0f;
		const float ax = Math::lerp(-3.2f, 3.2f, t);
		const float ay = 4.4f + std::sin(t * Math_PI) * 1.2f;
		m.blob(lane_end + Vector3(ax, ay, -float(s.lane_dir) * 0.9f), Vector3(0.2f, 0.24f, 0.2f), (i & 1) ? pal::GLOW_LANTERN : pal::GLOW_LANTERN_RED, 0);
	}
	// Distant house silhouettes with glowing windows beyond the hedges, for depth (the open world
	// has real scenery there instead).
	for (int side : { 1, -1 }) {
		if (ctx.embedded) {
			break;
		}
		for (float x = ctx.bound_x0 - 30.0f; x < ctx.bound_x1 + 30.0f; x += rng.range(9.0f, 14.0f)) {
			const float zback = side > 0 ? ctx.bound_z1 + rng.range(9.0f, 18.0f) : ctx.bound_z0 - rng.range(9.0f, 18.0f);
			const float w = rng.range(6.0f, 9.0f), h = rng.range(3.5f, 6.5f);
			const Vector3 base(x, 0, zback);
			m.box(base + Vector3(0, h * 0.5f, 0), Vector3(w * 0.5f, h * 0.5f, 3.0f), mat_cell(pal::ROOF_SLATE, mat::SIDING));
			std::vector<Vector2> gable = { { -w * 0.5f - 0.3f, h }, { w * 0.5f + 0.3f, h }, { 0, h + w * 0.55f } };
			m.set_transform(Transform3D(Basis(), base));
			m.prism_xy(gable, 0.0f, 6.4f, mat_cell(pal::SHADOW_NAVY, mat::SHINGLES));
			m.set_transform(Transform3D());
			const float face = side > 0 ? -3.02f : 3.02f;
			for (int wi = 0; wi < 3; ++wi) {
				if (rng.chance(0.6f)) {
					const Vector3 wpos = base + Vector3(-w * 0.3f + wi * w * 0.3f, h * 0.55f, face);
					m.box(wpos, Vector3(0.45f, 0.55f, 0.04f), rng.chance(0.5f) ? pal::GLOW_WINDOW : pal::GLOW_WINDOW_WARM);
				}
			}
		}
	}
}

} // namespace tot
