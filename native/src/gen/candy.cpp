// Stages 7-10: prop collisions, traversal graph, candy trails, dressing scatter and validation.

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <deque>

#include "core/movement.h"
#include "gen/context.h"

using namespace godot;

namespace tot {

namespace {

constexpr float JUMP_REACH = 2.7f; // jump (1.69 m) + wing flap (1.17 m), minus a safety margin
constexpr float GAP_REACH = 2.4f; // horizontal gap a running jump clears at the same height

struct BoxInfo {
	float top, bottom;
};

BoxInfo world_extent(const OBox &b) {
	float lo = 1e9f, hi = -1e9f;
	for (int i = 0; i < 8; ++i) {
		const Vector3 c((i & 1) ? b.half.x : -b.half.x, (i & 2) ? b.half.y : -b.half.y, (i & 4) ? b.half.z : -b.half.z);
		const float y = b.xf.xform(c).y;
		lo = Math::min(lo, y);
		hi = Math::max(hi, y);
	}
	return { hi, lo };
}

bool over_box(const OBox &b, const Vector3 &p, float expand) {
	const Vector3 l = b.xf.affine_inverse().xform(Vector3(p.x, b.xf.origin.y, p.z));
	return std::abs(l.x) <= b.half.x + expand && std::abs(l.z) <= b.half.z + expand;
}

bool inside_box(const OBox &b, const Vector3 &p, float expand) {
	const Vector3 l = b.xf.affine_inverse().xform(p);
	return std::abs(l.x) <= b.half.x + expand && std::abs(l.y) <= b.half.y + expand && std::abs(l.z) <= b.half.z + expand;
}

// Approximate horizontal distance between two boxes' footprints.
float footprint_gap(const OBox &a, const OBox &b) {
	auto pts = [](const OBox &o) {
		std::vector<Vector3> v;
		for (float sx : { -1.0f, 0.0f, 1.0f }) {
			for (float sz : { -1.0f, 0.0f, 1.0f }) {
				v.push_back(o.xf.xform(Vector3(sx * o.half.x, 0, sz * o.half.z)));
			}
		}
		return v;
	};
	auto dist_to = [](const OBox &o, const Vector3 &p) {
		const Vector3 l = o.xf.affine_inverse().xform(Vector3(p.x, o.xf.origin.y, p.z));
		const float dx = Math::max(0.0f, std::abs(l.x) - o.half.x);
		const float dz = Math::max(0.0f, std::abs(l.z) - o.half.z);
		return std::sqrt(dx * dx + dz * dz);
	};
	float best = 1e9f;
	for (const Vector3 &p : pts(b)) {
		best = Math::min(best, dist_to(a, p));
	}
	for (const Vector3 &p : pts(a)) {
		best = Math::min(best, dist_to(b, p));
	}
	return best;
}

} // namespace

// ---------------------------------------------------------------- prop collisions
void register_prop_collisions(GenContext &ctx) {
	for (const PropPlacement &pp : ctx.props) {
		if (!pp.collide) {
			continue;
		}
		const AssetInfo *a = ctx.assets->get(pp.id);
		if (a->collision == "none") {
			continue;
		}
		const Vector3 sc = pp.xf.basis.get_scale();
		const Basis rot = pp.xf.basis.orthonormalized();
		OBox b;
		b.lot = pp.lot;
		b.walkable = a->walkable;
		b.bouncy = pp.bouncy;
		if (a->collision == "trunk") {
			b.cylinder = true;
			b.half = Vector3(a->radius * sc.x, a->height * 0.5f * sc.y, a->radius * sc.x);
			b.xf = Transform3D(rot, pp.xf.xform(Vector3(0, a->height * 0.5f, 0)));
			b.walkable = false;
		} else {
			const Vector3 center = a->aabb.get_center();
			Vector3 half = a->aabb.size * 0.5f;
			half = Vector3(half.x * sc.x, half.y * sc.y, half.z * sc.z);
			if (a->collision == "cylinder") {
				b.cylinder = true;
				const float r = Math::max(half.x, half.z) * 0.85f;
				half = Vector3(r, half.y, r);
			}
			b.half = half;
			b.xf = Transform3D(rot, pp.xf.xform(center));
		}
		ctx.boxes.push_back(b);
	}
}

// ---------------------------------------------------------------- candy (platforming only)
namespace {

Vector3 bezier(const Vector3 &a, const Vector3 &c, const Vector3 &b, float t) {
	const float s = 1.0f - t;
	return a * (s * s) + c * (2.0f * s * t) + b * (t * t);
}

constexpr float CARRY = 0.9f; // candy height above the feet where the bucket sweeps through it

// A short arc of candy from `a` to `b` that rises over `apex` (world height): reads as "jump here".
// Fallback for runs no real jump trajectory fits.
void arc(std::vector<CandySpec> &out, const Vector3 &a, const Vector3 &b, float apex, int count, int lot, int color, int group, int last_kind = -1) {
	const Vector3 c((a.x + b.x) * 0.5f, Math::max(apex, Math::max(a.y, b.y)) + 0.6f, (a.z + b.z) * 0.5f);
	for (int i = 0; i < count; ++i) {
		const float t = float(i + 1) / float(count);
		CandySpec cs;
		cs.pos = bezier(a, c, b, t);
		cs.kind = (i == count - 1 && last_kind >= 0) ? last_kind : color;
		cs.lot = lot;
		cs.group = group;
		out.push_back(cs);
	}
}

const std::vector<move::ArcPoint> &jump_path(bool flap) {
	static const std::vector<move::ArcPoint> single = move::simulate_jump(false), flapped = move::simulate_jump(true);
	return flap ? flapped : single;
}

// Flight time of the full-hold jump that comes down onto a landing `dh` above its takeoff. A
// landing within 0.3 m of the single jump's peak would be ambiguous ("can I make it?"), so it is
// planned as a flap jump instead ("Lessons from Suzy Cube: Measuring Up", Game Developer). Returns -1
// if even the flap falls short.
float flight_time(float dh, bool &flap) {
	const float t = move::landing_time(jump_path(false), dh + 0.3f);
	flap = t <= 0.0f;
	return flap ? move::landing_time(jump_path(true), dh + 0.3f) : move::landing_time(jump_path(false), dh);
}

// A rhythm group laid along the jump the kid really makes from takeoff `a` to landing `b` (both
// at the feet): one candy per beat, evenly spaced in time, the last one marking the landing. The
// player's hands feel the rhythm and the eye reads the exact arc (Smith et al., "Launchpad: A
// Rhythm-Based Level Generator for 2-D Platformers"); collecting the whole run is the proof of a
// well-timed jump. Returns false if no single or flap jump at running speed fits.
bool jump_arc(std::vector<CandySpec> &out, const Vector3 &a, const Vector3 &b, int count, int lot, int color, int group, int last_kind = -1) {
	const float dh = b.y - a.y;
	const Vector3 flat(b.x - a.x, 0.0f, b.z - a.z);
	bool flap = false;
	const float T = flight_time(dh, flap);
	if (T <= 0.0f || flat.length() / T > move::RUN_SPEED * 0.95f) {
		return false;
	}
	const std::vector<move::ArcPoint> &path = jump_path(flap);
	for (int i = 0; i < count; ++i) {
		const float t = T * float(i + 1) / float(count);
		CandySpec cs;
		cs.pos = a + flat * (t / T) + Vector3(0, move::height_at(path, t) + CARRY, 0);
		cs.kind = (i == count - 1 && last_kind >= 0) ? last_kind : color;
		cs.lot = lot;
		cs.group = group;
		out.push_back(cs);
	}
	return true;
}

bool blocked_at(const GenContext &ctx, const Vector3 &p) {
	for (const OBox &b : ctx.boxes) {
		if (inside_box(b, p, 0.25f)) {
			return true;
		}
	}
	return false;
}

} // namespace

void weave_candy(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_CANDY);
	const StreetPlan &s = ctx.street;
	std::vector<CandySpec> &out = ctx.candy;
	out.clear();
	const int colors[] = { CANDY_PINK, CANDY_MINT, CANDY_GRAPE, CANDY_LEMON };
	// Platforms worth jumping onto: bales, crates, barrels, rails, car roofs, benches, wagons...
	struct Plat {
		int box;
		Vector3 top;
		float order = 0.0f;
	};
	std::vector<std::vector<Plat>> by_lot(ctx.lots.size() + 1);
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		const OBox &b = ctx.boxes[i];
		const BoxInfo e = world_extent(b);
		if (!b.walkable || b.boundary || e.top < 0.75f || e.top > 3.2f || e.top - e.bottom > 3.3f) {
			continue;
		}
		if (b.half.x < 0.18f || b.half.z < 0.12f || (b.half.x > 2.4f && b.half.z > 1.2f)) {
			continue;
		}
		// The porch (rails, porch roof) or shop awning already has its own optional run; leave it out
		// so candy spreads around the yard instead of piling up over the front door.
		if (b.lot >= 0) {
			const Lot &lot = ctx.lots[b.lot];
			const HouseSpec &h = lot.house;
			const Vector3 l = lot.to_local(b.xf.origin);
			if (l.z > h.front_z - 0.6f && l.z < h.front_z + h.porch_d + 0.8f && std::abs(l.x - h.porch_x) < h.porch_w * 0.5f + 0.8f) {
				continue;
			}
		}
		Plat p{ int(i), Vector3(b.xf.origin.x, e.top, b.xf.origin.z) };
		by_lot[b.lot >= 0 ? b.lot : ctx.lots.size()].push_back(p);
	}
	int next_group = 0;
	// Each lot reads as a little four-beat course (Hayashida's kishōtenketsu for Super Mario 3D
	// Land: introduce, develop, twist, conclude): the lowest platform comes first with a plain
	// running jump from the ground, the next ones chain higher and may need the wing flap, a bonus
	// hangs above a platform top, and the porch roof run is the finale. Runs sit at least 4 m apart
	// so every rhythm group is followed by a rest.
	auto sequences = [&](std::vector<Plat> &plats, int lot, int want, Rng &r) {
		for (Plat &p : plats) {
			p.order = p.top.y + r.range(-0.3f, 0.3f);
		}
		std::sort(plats.begin(), plats.end(), [](const Plat &a, const Plat &b) { return a.order < b.order; });
		std::vector<Vector3> used;
		int made = 0;
		for (const Plat &p : plats) {
			if (made >= want) {
				break;
			}
			bool near = false;
			for (const Vector3 &u : used) {
				if (Vector2(u.x - p.top.x, u.z - p.top.z).length() < 4.0f) {
					near = true;
				}
			}
			if (near) {
				continue;
			}
			// Approach from open ground, preferring the street side, from where a running jump at
			// 80% speed comes down on the platform.
			bool flap = false;
			const float T = flight_time(p.top.y, flap);
			if (T <= 0.0f) {
				continue;
			}
			const float reach = Math::clamp(move::RUN_SPEED * 0.8f * T, 2.4f, 4.6f);
			Vector3 best;
			float best_score = -1e9f;
			const float a0 = r.range(0.0f, Math_TAU);
			for (int k = 0; k < 10; ++k) {
				const float a = a0 + k * Math_TAU / 10.0f;
				Vector3 g = p.top + Vector3(std::cos(a), 0, std::sin(a)) * reach;
				g.y = 0.0f;
				if (blocked_at(ctx, g + Vector3(0, 0.6f, 0))) {
					continue;
				}
				float score = r.range(0.0f, 0.5f);
				if (lot >= 0) {
					score += ctx.lots[lot].to_local(g).z * 0.15f; // street side
				}
				if (score > best_score) {
					best_score = score;
					best = g;
				}
			}
			if (best_score < -1e8f) {
				continue;
			}
			const int color = colors[r.irange(0, 3)];
			const int count = r.irange(3, 4);
			if (!jump_arc(out, best, p.top, count, lot, color, next_group)) {
				arc(out, best + Vector3(0, 1.2f, 0), p.top + Vector3(0, CARRY, 0), p.top.y + 1.5f, count, lot, color, next_group);
			}
			++next_group;
			// Bonus right above the platform: jump from the top to grab it.
			if (p.top.y < 2.2f && r.chance(0.6f)) {
				out.push_back({ p.top + Vector3(0, 2.35f, 0), CANDY_CHOCOLATE, lot });
			}
			// Chain to a higher neighbouring platform along the real jump between them.
			for (const Plat &q : plats) {
				const float dh = q.top.y - p.top.y;
				const float dist = Vector2(q.top.x - p.top.x, q.top.z - p.top.z).length();
				if (q.box != p.box && dh > 0.5f && dh < 2.3f && dist > 1.8f && dist < 4.2f &&
						jump_arc(out, p.top, q.top, 3, lot, color, next_group, CANDY_CHOCOLATE)) {
					++next_group;
					used.push_back(q.top);
					break;
				}
			}
			used.push_back(p.top);
			++made;
		}
	};
	for (Lot &lot : ctx.lots) {
		Rng lr = ctx.rng(STAGE_CANDY, lot.index);
		sequences(by_lot[lot.index], lot.index, lot.landmark != Landmark::None ? 4 : 3, lr);
		const HouseSpec &h = lot.house;
		// Porch roof (or shop awning): one short optional run up onto it, a lollipop for the bold.
		std::vector<Vector3> roof;
		for (const Vector3 &p : lot.roof_route) {
			roof.push_back(lot.to_world(p));
		}
		const int rc = colors[lr.irange(0, 3)];
		if (lr.chance(0.6f)) {
			const int roof_group = next_group++;
			if (!jump_arc(out, roof[0], roof[1], 3, lot.index, rc, roof_group)) {
				arc(out, roof[0] + Vector3(0, CARRY, 0), roof[1] + Vector3(0, CARRY, 0), roof[1].y + 1.2f, 3, lot.index, rc, roof_group);
			}
			if (lr.chance(0.4f)) {
				out.push_back({ roof.back() + Vector3(0, 1.2f, 0), CANDY_LOLLIPOP, lot.index });
			}
		}
		if (h.garage && lr.chance(0.6f)) {
			const float g = float(h.garage_side);
			const float gx = h.house_x + g * (h.width * 0.5f + 1.9f);
			out.push_back({ lot.to_world(gx, 3.04f + 1.2f, h.front_z - 0.4f - 3.0f), CANDY_LOLLIPOP, lot.index });
		}
		// Ground candy: little loose clusters at walking height around the yard (front lawn,
		// backyard, beside the path), so most candy is simply there to scoop up; the jumps are for
		// the chains and the valuable pieces.
		{
			const float hw = lot.width * 0.5f - 1.0f;
			const float front0 = lot.shop >= 0 ? 0.8f : h.front_z + h.porch_d + 1.4f, front1 = lot.shop >= 0 ? ctx.street.walk - ctx.street.verge - 0.5f : -0.8f;
			const float back0 = -lot.depth + 1.0f, back1 = h.front_z - h.depth - 1.0f;
			const int clusters = lr.irange(3, 4);
			int made = 0;
			for (int tries = 0; tries < 24 && made < clusters; ++tries) {
				const bool back = (tries % 3 == 2) && back1 - back0 > 0.6f;
				const float z0 = back ? back0 : front0, z1 = back ? back1 : front1;
				if (z1 - z0 < 0.3f) {
					continue;
				}
				const Vector3 c = lot.to_world(lr.range(-hw, hw), 0.0f, lr.range(z0, z1));
				const int n = lr.irange(3, 5);
				const float r = lr.range(0.6f, 1.0f), a0 = lr.range(0.0f, Math_TAU);
				std::vector<Vector3> pts;
				for (int k = 0; k < n; ++k) {
					const float a = a0 + k * Math_TAU / n;
					const Vector3 p = c + Vector3(std::cos(a) * r, 0.75f, std::sin(a) * r);
					if (!blocked_at(ctx, p) && !blocked_at(ctx, p - Vector3(0, 0.5f, 0))) {
						pts.push_back(p);
					}
				}
				if (pts.size() < 3) {
					continue;
				}
				const int color = colors[lr.irange(0, 3)];
				for (const Vector3 &p : pts) {
					out.push_back({ p, color, lot.index, next_group });
				}
				++next_group;
				++made;
			}
		}
		// Yard hops: easy runs across the lawn (or the shop's sidewalk) along a single ground-to-
		// ground jump.
		{
			const float hw = lot.width * 0.5f - 1.4f;
			struct Zone {
				float z0, z1;
			};
			std::vector<Zone> zones;
			if (lot.shop >= 0) {
				zones.push_back({ 0.9f, ctx.street.walk - ctx.street.verge - 0.6f });
				zones.push_back({ -lot.depth + 1.0f, h.front_z - h.depth - 1.0f });
			} else {
				zones.push_back({ h.front_z + h.porch_d + 1.6f, -1.2f });
				zones.push_back({ -lot.depth + 1.4f, h.front_z - h.depth - 1.4f });
				zones.push_back({ h.front_z + h.porch_d + 1.6f, -1.2f });
			}
			for (const Zone &zone : zones) {
				if (zone.z1 - zone.z0 < 0.4f) {
					continue;
				}
				for (int tries = 0; tries < 10; ++tries) {
					const float len = lr.range(3.4f, 4.6f);
					const float x0 = lr.range(-hw, hw - len), z = lr.range(zone.z0, zone.z1);
					const float dir = lr.chance(0.5f) ? 1.0f : -1.0f;
					const Vector3 a = lot.to_world(dir > 0 ? x0 : x0 + len, 0.0f, z), b = lot.to_world(dir > 0 ? x0 + len : x0, 0.0f, z);
					bool clear = true;
					for (float t : { 0.0f, 0.5f, 1.0f }) {
						const Vector3 q = a.lerp(b, t);
						if (blocked_at(ctx, q + Vector3(0, 0.6f, 0)) || blocked_at(ctx, q + Vector3(0, 1.6f, 0))) {
							clear = false;
						}
					}
					if (!clear) {
						continue;
					}
					if (jump_arc(out, a, b, 3, lot.index, colors[lr.irange(0, 3)], next_group)) {
						++next_group;
						break;
					}
				}
			}
		}
		if (!h.garage && lr.chance(0.35f)) {
			// Backyard secret high over the lawn (needs the wing flap).
			out.push_back({ lot.to_world(h.house_x, 2.9f, h.front_z - h.depth - 3.2f), CANDY_LOLLIPOP, lot.index });
		}
		// Landmark summit: candy above every other step, the golden bar on top.
		if (!lot.summit_route.empty()) {
			const int summit_group = next_group++;
			for (size_t i = 1; i + 1 < lot.summit_route.size(); i += 2) {
				out.push_back({ lot.to_world(lot.summit_route[i]) + Vector3(0, 1.2f, 0), colors[lr.irange(0, 3)], lot.index, summit_group });
			}
			out.push_back({ lot.to_world(lot.summit_route.back()) + Vector3(0, 0.95f, 0), CANDY_GOLDEN, lot.index });
		}
	}
	// Street: arcs onto parked cars, hay walls of the pocket parks, and hops over the hopscotch.
	Rng sr = ctx.rng(STAGE_CANDY, 999);
	sequences(by_lot.back(), -1, 6, sr);
	const float hx = s.lane_x + (s.lane_x < 0 ? 7.5f : -7.5f);
	const Vector3 hcen = s.at(hx), hl = s.left_at(hx);
	const int hop_group = next_group++;
	for (int i = -3; i <= 3; i += 2) {
		out.push_back({ hcen + hl * (i * 1.1f) + Vector3(0, 2.0f, 0), CANDY_LEMON, -1, hop_group });
	}
	// Ground candy along the sidewalks and verges: small loose clusters spread down the street.
	for (float x = ctx.bound_x0 + sr.range(4.0f, 8.0f); x < ctx.bound_x1 - 4.0f; x += sr.range(9.0f, 14.0f)) {
		if (std::abs(x - s.lane_x) < s.edge() + 1.5f) {
			continue;
		}
		const float side = sr.chance(0.5f) ? 1.0f : -1.0f;
		const Vector3 c = s.at(x) + s.left_at(x) * side * sr.range(s.half_road + 0.6f, s.edge() - 0.6f);
		const int color = colors[sr.irange(0, 3)];
		const int n = sr.irange(3, 4);
		for (int k = 0; k < n; ++k) {
			const Vector3 p = c + (s.at(x + 1.0f) - s.at(x - 1.0f)).normalized() * ((k - (n - 1) * 0.5f) * 0.8f) + Vector3(0, 0.8f, 0);
			if (!blocked_at(ctx, p) && !blocked_at(ctx, p - Vector3(0, 0.55f, 0))) {
				out.push_back({ p, color, -1 });
			}
		}
	}
	ctx.stats.candy = int(out.size());
}

// ---------------------------------------------------------------- magic guide trail
void plan_magic_paths(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_CANDY, 4242);
	const StreetPlan &s = ctx.street;
	ctx.magic.clear();
	// Spine: meanders down the street and up the side lane.
	MagicPath spine;
	spine.lot = -1;
	const float phase = rng.range(0.0f, Math_TAU);
	for (float x = ctx.bound_x0 + 3.0f; x <= ctx.bound_x1 - 3.0f; x += 2.0f) {
		Vector3 p = s.at(x) + s.left_at(x) * (1.6f * std::sin(x * 0.11f + phase));
		p.y = 1.0f + 0.25f * std::sin(x * 0.23f + phase * 2.0f);
		spine.points.push_back(p);
	}
	ctx.magic.push_back(spine);
	MagicPath lane;
	lane.lot = -1;
	for (float d = 0.0f; d <= s.lane_len - 1.0f; d += 2.0f) {
		Vector3 p = s.lane_origin + Vector3(1.2f * std::sin(d * 0.2f + phase), 1.0f, float(s.lane_dir) * d);
		lane.points.push_back(p);
	}
	ctx.magic.push_back(lane);
	// Branches: from the street spine to each doorstep, ending in a little swirl at the door.
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		MagicPath b;
		b.lot = lot.index;
		const Vector3 entrance = lot.to_world(h.door_x, 0, 0.6f);
		const Vector3 on_street = s.at(entrance.x) + (entrance - s.at(entrance.x)).normalized() * 1.2f;
		const float steps_front = h.front_z + h.porch_d + 3 * 0.36f;
		auto P = [&](const Vector3 &w, float y) { return Vector3(w.x, y, w.z); };
		b.points.push_back(P(on_street, 1.0f));
		b.points.push_back(P(lot.to_world(h.door_x, 0, ctx.street.walk + 0.4f), 0.95f));
		b.points.push_back(P(entrance, 0.9f));
		b.points.push_back(lot.to_world(h.door_x + 0.3f, 0.85f, (steps_front - 0.6f) * 0.5f));
		b.points.push_back(lot.to_world(h.door_x, 0.9f, steps_front + 0.8f));
		b.points.push_back(lot.to_world(h.door_x, h.deck_h + 0.9f, h.front_z + h.porch_d - 0.3f));
		for (int k = 0; k <= 8; ++k) {
			const float a = k * Math_TAU / 8.0f;
			b.points.push_back(lot.to_world(h.door_x + std::sin(a) * 0.55f, h.deck_h + 0.9f + k * 0.08f, h.front_z + 0.8f + std::cos(a) * 0.35f));
		}
		ctx.magic.push_back(b);
	}
}

// ---------------------------------------------------------------- traversal + validation
void validate_traversal(GenContext &ctx) {
	std::vector<int> plat;
	std::vector<BoxInfo> ext(ctx.boxes.size());
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		ext[i] = world_extent(ctx.boxes[i]);
		if (ctx.boxes[i].walkable) {
			plat.push_back(int(i));
		}
	}
	std::vector<char> reach(ctx.boxes.size(), 0);
	std::deque<int> q;
	for (int i : plat) {
		if (ext[i].top <= JUMP_REACH || ext[i].bottom <= 0.35f) {
			reach[i] = 1;
			q.push_back(i);
		}
	}
	while (!q.empty()) {
		const int i = q.front();
		q.pop_front();
		for (int j : plat) {
			if (reach[j]) {
				continue;
			}
			const float dh = ext[j].top - ext[i].top;
			if (dh > JUMP_REACH) {
				continue;
			}
			// Falling from higher up buys extra horizontal reach (glide).
			const float gap_ok = GAP_REACH + Math::clamp(-dh, 0.0f, 3.0f) * 0.8f - Math::max(0.0f, dh) * 0.3f;
			if (footprint_gap(ctx.boxes[i], ctx.boxes[j]) <= gap_ok) {
				reach[j] = 1;
				q.push_back(j);
			}
		}
	}
	ctx.stats.platforms = int(plat.size());
	ctx.stats.reachable_platforms = 0;
	for (int i : plat) {
		ctx.stats.reachable_platforms += reach[i];
	}
	// Candy: lift out of solids, check support, drop what cannot be reached.
	std::vector<CandySpec> kept;
	for (CandySpec c : ctx.candy) {
		bool moved = false;
		for (size_t i = 0; i < ctx.boxes.size(); ++i) {
			const OBox &b = ctx.boxes[i];
			if (inside_box(b, c.pos, 0.25f)) {
				const float lift_to = ext[i].top + 0.8f;
				if (lift_to - c.pos.y < 1.8f) {
					c.pos.y = lift_to;
					moved = true;
				}
			}
		}
		bool inside = false;
		for (const OBox &b : ctx.boxes) {
			if (inside_box(b, c.pos, 0.15f)) {
				inside = true;
				break;
			}
		}
		if (inside) {
			++ctx.stats.candy_dropped;
			continue;
		}
		bool supported = c.pos.y <= JUMP_REACH + 0.2f;
		for (size_t i = 0; i < ctx.boxes.size() && !supported; ++i) {
			if (!ctx.boxes[i].walkable || !reach[i]) {
				continue;
			}
			const float dy = c.pos.y - ext[i].top;
			if (dy >= -0.3f && dy <= JUMP_REACH && over_box(ctx.boxes[i], c.pos, 1.0f)) {
				supported = true;
			}
		}
		if (!supported) {
			++ctx.stats.candy_dropped;
			if (c.kind == CANDY_GOLDEN) {
				ctx.stats.openness_failures++;
				ctx.stats.notes.push_back(std::string("summit prize unreachable on ") + landmark_name(ctx.lots[c.lot].landmark));
			}
			continue;
		}
		if (moved) {
			++ctx.stats.candy_raised;
		}
		kept.push_back(c);
	}
	ctx.candy.swap(kept);
	ctx.stats.candy = int(ctx.candy.size());

	// Openness checks: several trail starts and first stops near the spawn, landmarks in view.
	int near_entrances = 0, near_lots = 0;
	for (const Lot &lot : ctx.lots) {
		int e = 0;
		for (const Vector3 &p : { lot.loop_route.front(), lot.loop_route.back() }) {
			if (lot.to_world(p).distance_to(ctx.spawn) < 30.0f) {
				++e;
			}
		}
		near_entrances += e;
		near_lots += e > 0 ? 1 : 0;
	}
	int landmarks_near = 0;
	for (const Lot &lot : ctx.lots) {
		if (lot.landmark != Landmark::None && lot.xf.origin.distance_to(ctx.spawn) < 50.0f) {
			++landmarks_near;
		}
	}
	if (near_entrances < 3) {
		ctx.stats.openness_failures++;
		ctx.stats.notes.push_back("fewer than 3 trail starts near spawn");
	}
	if (near_lots < 3) {
		ctx.stats.openness_failures++;
		ctx.stats.notes.push_back("fewer than 3 houses near spawn");
	}
	if (landmarks_near < 2) {
		ctx.stats.notes.push_back("fewer than 2 landmarks within 50 m of spawn");
	}
}

// ---------------------------------------------------------------- dressing
void scatter_dressing(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_DRESSING);
	ctx.scatter.clear();
	auto srgb = [](int cell) {
		const unsigned c = pal::RGB[cell];
		return Color(float((c >> 16) & 255) / 255.0f, float((c >> 8) & 255) / 255.0f, float(c & 255) / 255.0f).srgb_to_linear();
	};
	for (const Lot &lot : ctx.lots) {
		// Dense lawn grass in clumps, thicker along fences and foundations.
		const int lawn = (lot.index % 3 == 0) ? pal::LAWN_DARK : (lot.index % 3 == 1 ? pal::LAWN : pal::MOSS);
		const Color lawn_col = srgb(lawn);
		const int n = int(lot.width * lot.depth * 2.2f);
		for (int i = 0; i < n; ++i) {
			float x = rng.range(-lot.width * 0.5f + 0.3f, lot.width * 0.5f - 0.3f);
			float z = rng.range(-lot.depth + 0.4f, -0.3f);
			if (rng.chance(0.15f)) {
				x = (rng.chance(0.5f) ? 1.0f : -1.0f) * (lot.width * 0.5f - rng.range(0.3f, 0.7f)); // fence lines
			}
			if (ctx.occupied_at(lot.index, x, z, 0.1f)) {
				continue;
			}
			if (std::abs(x - lot.drive_x) < 1.55f && z > lot.house.front_z - 1.0f) {
				continue;
			}
			if (std::abs(x - lot.house.door_x) < 0.55f && z > lot.house.front_z) {
				continue;
			}
			ScatterItem s;
			s.kind = rng.chance(0.93f) ? 0 : 2;
			s.lot = lot.index;
			s.color = lawn_col * rng.range(0.85f, 1.12f);
			const float sc = rng.range(0.75f, 1.35f);
			s.xf = lot.xf * Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(sc, sc * rng.range(0.8f, 1.3f), sc)), Vector3(x, 0, z));
			ctx.scatter.push_back(s);
		}
	}
	// Leaf litter under trees (biased downwind) and in the street gutters.
	for (const PropPlacement &pp : ctx.props) {
		if (pp.id.rfind("tree_", 0) != 0) {
			continue;
		}
		for (int i = 0; i < 22; ++i) {
			const float a = rng.range(0.0f, Math_TAU), r = rng.range(0.4f, 3.6f);
			Vector3 p = pp.xf.origin + Vector3(std::cos(a) * r + 0.8f, 0.0f, std::sin(a) * r);
			p.y = 0.01f;
			ScatterItem s;
			s.kind = 1;
			s.xf = Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)), p);
			ctx.scatter.push_back(s);
		}
	}
	const StreetPlan &st = ctx.street;
	for (int i = 0; i < 140; ++i) {
		const float x = rng.range(ctx.bound_x0, ctx.bound_x1);
		const float side = rng.sign();
		Vector3 p = st.at(x) + st.left_at(x) * side * (st.half_road - rng.range(0.1f, 0.6f));
		p.y = 0.015f;
		ScatterItem s;
		s.kind = 1;
		s.xf = Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)), p);
		ctx.scatter.push_back(s);
	}
}

} // namespace tot
