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
	settle_street_props(ctx); // Stage 3c: every prop placed; the street's ones onto their surface before their solids are made
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
		b.bouncy = pp.bouncy || a->bounce > 0.0f;
		if (a->bounce > 0.0f) {
			b.bounce = a->bounce;
		}
		if (a->collision == "trunk") {
			b.cylinder = true;
			b.half = Vector3(a->radius * sc.x, a->height * 0.5f * sc.y, a->radius * sc.x);
			b.xf = Transform3D(rot, pp.xf.xform(Vector3(0, a->height * 0.5f, 0)));
			b.walkable = false;
		} else {
			Vector3 center = a->aabb.get_center();
			Vector3 half = a->aabb.size * 0.5f;
			if (a->collision_top > 0.0f) {
				const float bottom = a->aabb.position.y;
				center.y = (bottom + a->collision_top) * 0.5f;
				half.y = (a->collision_top - bottom) * 0.5f;
			}
			half = Vector3(half.x * sc.x, half.y * sc.y, half.z * sc.z);
			if (a->collision == "cylinder") {
				b.cylinder = true;
				const float r = Math::max(half.x, half.z) * 0.85f;
				half = Vector3(r, half.y, r);
			}
			b.half = half;
			b.xf = Transform3D(rot, pp.xf.xform(center));
			if (a->has_hood && !b.cylinder) {
				// Cars: the cabin and boot stay solid; the hood ahead of the windscreen is a trampoline
				// (its own lower box, so the kid lands on the hood rather than at roof height).
				const AABB &bb = a->aabb;
				const float zh = a->hood.position.z, bottom = bb.position.y;
				OBox cabin = b;
				cabin.half.z = (zh - bb.position.z) * 0.5f * sc.z;
				cabin.xf.origin = pp.xf.xform(Vector3(center.x, center.y, (bb.position.z + zh) * 0.5f));
				ctx.boxes.push_back(cabin);
				const float top = a->hood.get_end().y;
				b.half = Vector3(half.x, (top - bottom) * 0.5f * sc.y, (bb.get_end().z - zh) * 0.5f * sc.z);
				b.xf.origin = pp.xf.xform(Vector3(center.x, (bottom + top) * 0.5f, (zh + bb.get_end().z) * 0.5f));
				b.bouncy = true;
				b.bounce = 14.0f;
			}
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


// ---------------------------------------------------------------- front-yard candy (Stage 1e)
// Every lot's candy is a handful of designed shapes, each anchored to something the builders
// published on the Lot (front walk, driveway, parked car, low obstacles, landmark centrepiece) or to
// a platform, each in one colour and one rhythm group: a line up the front walk, a line up the drive
// that hops onto the car's bouncy hood, platform sequences crowned on top, arcs over the things a kid
// jumps anyway, a ring round the landmark. Nothing is scattered at random.

constexpr int LOT_CAP = 30; // pieces per lot (landmark lots: LANDMARK_CAP); whole patterns are skipped past it
constexpr int LANDMARK_CAP = 34;
// Value of a lot's everyday and chocolate pieces before Stage 1e (50-seed sweep 1e_before: hood
// 33.3 + 5 x 1.92 = 42.9, town 26.6 + 5 x 0.80 = 30.6). Lollipops and golden bars keep their own
// rules; chocolates are promoted along the patterns until a lot comes within 2 of this.
constexpr int HOOD_VALUE = 43;
constexpr int TOWN_VALUE = 31;
constexpr float LINE_H = 0.75f; // ground pieces (clusters, the car run-up): height above the walking surface

// Platforms worth jumping onto: bales, crates, barrels, rails, car roofs, benches, wagons...
struct Plat {
	int box;
	Vector3 top;
	float order = 0.0f;
};

// The boxes around one lot with their inverse transforms, for the many clearance tests below (the
// same test as blocked_at / the validation's inside_box, with its 0.25 m margin).
struct NearBoxes {
	std::vector<const OBox *> box;
	std::vector<Transform3D> inv;
	void build(const GenContext &ctx, const Vector3 &centre, float radius) {
		for (const OBox &b : ctx.boxes) {
			const float ext = b.half.length();
			if (Vector2(b.xf.origin.x - centre.x, b.xf.origin.z - centre.z).length() < radius + ext) {
				box.push_back(&b);
				inv.push_back(b.xf.affine_inverse());
			}
		}
	}
	bool blocked(const Vector3 &p, float expand = 0.25f) const {
		for (size_t i = 0; i < box.size(); ++i) {
			const Vector3 l = inv[i].xform(p);
			const Vector3 &h = box[i]->half;
			if (std::abs(l.x) <= h.x + expand && std::abs(l.y) <= h.y + expand && std::abs(l.z) <= h.z + expand) {
				return true;
			}
		}
		return false;
	}
	// Open ground a kid can run across and jump from: nothing at knee or head height.
	bool open_ground(const Vector3 &g) const { return !blocked(g + Vector3(0, 0.6f, 0)) && !blocked(g + Vector3(0, 1.6f, 0)); }
};

// Positions along the real jump from feet `a` to feet `b` (as jump_arc): `count` beats evenly in
// time; `over` spaces them symmetrically (t = (i + 1) / (count + 1)) so the middle one tops an
// obstacle, otherwise the last one marks the landing. False if no single or flap jump fits.
bool jump_points(std::vector<Vector3> &pts, const Vector3 &a, const Vector3 &b, int count, bool over) {
	const float dh = b.y - a.y;
	const Vector3 flat(b.x - a.x, 0.0f, b.z - a.z);
	bool flap = false;
	const float T = flight_time(dh, flap);
	if (T <= 0.0f || flat.length() / T > move::RUN_SPEED * 0.95f) {
		return false;
	}
	const std::vector<move::ArcPoint> &path = jump_path(flap);
	for (int i = 0; i < count; ++i) {
		const float t = T * float(i + 1) / float(over ? count + 1 : count);
		pts.push_back(a + flat * (t / T) + Vector3(0, move::height_at(path, t) + CARRY, 0));
	}
	return true;
}

// One lot's candy: patterns in priority order under the cap, then the value top-up.
struct LotWeaver {
	GenContext &ctx;
	const Lot &lot;
	Rng &r;
	int &next_group;
	std::vector<CandySpec> &out;
	const NearBoxes &nb;
	size_t first = 0;
	int cap = LOT_CAP;
	// Pieces that may become chocolates to keep the lot's value, by priority (lower first).
	std::vector<std::pair<int, size_t>> promotable;
	// Last resort for the value: whole clusters (first piece, size), promoted together so they stay one kind.
	std::vector<std::pair<size_t, size_t>> promotable_groups;

	int count() const { return int(out.size() - first); }
	int room() const { return cap - count(); }
	// Stage 3b: lot-local anchors are heights above the ground under them (yard_lift: 0 on the buildings).
	Vector3 W(const Vector3 &local) const {
		Vector3 w = lot.to_world(local);
		if (ctx.terrain) {
			w.y += ctx.yard_lift(lot, local.x, local.z);
		}
		return w;
	}
	Vector3 Wdir(const Vector3 &local) const { return lot.xf.basis.xform(local); }

	// Every piece, and the midpoint between consecutive pieces, clear of solids (validation would lift
	// or drop single pieces and kink the shape).
	bool clear_run(const std::vector<Vector3> &pts) const {
		for (size_t i = 0; i < pts.size(); ++i) {
			if (nb.blocked(pts[i]) || (i > 0 && nb.blocked((pts[i - 1] + pts[i]) * 0.5f))) {
				return false;
			}
		}
		return true;
	}
	// Horizontally at least d from every piece this lot already has (no two shapes on top of each other).
	bool unclaimed(const Vector3 &p, float d) const {
		for (size_t i = first; i < out.size(); ++i) {
			if (Vector2(out[i].pos.x - p.x, out[i].pos.z - p.z).length() < d && std::abs(out[i].pos.y - p.y) < 1.6f) {
				return false;
			}
		}
		return true;
	}
	bool unclaimed_all(const std::vector<Vector3> &pts, float d) const {
		for (const Vector3 &p : pts) {
			if (!unclaimed(p, d)) {
				return false;
			}
		}
		return true;
	}
	// Emit a shape as one rhythm group; returns the index of its first piece.
	size_t emit(const std::vector<Vector3> &pts, int color, int pattern, int group = -2) {
		const size_t at = out.size();
		if (group == -2) {
			group = next_group++;
		}
		for (const Vector3 &p : pts) {
			CandySpec c;
			c.pos = p;
			c.kind = color;
			c.lot = lot.index;
			c.group = group;
			c.pattern = pattern;
			out.push_back(c);
		}
		return at;
	}

	// Never on the front walk: that is the magic trail's way to the door, and candy along it would read
	// as a breadcrumb trail (horizontal distance to the published walk polyline).
	bool off_path(const Vector3 &l) const {
		for (size_t i = 1; i < lot.front_walk.size(); ++i) {
			const Vector2 a(lot.front_walk[i - 1].x, lot.front_walk[i - 1].z), b(lot.front_walk[i].x, lot.front_walk[i].z), p(l.x, l.z);
			const Vector2 ab = b - a;
			const float t = ab.length_squared() > 1e-6f ? Math::clamp((p - a).dot(ab) / ab.length_squared(), 0.0f, 1.0f) : 0.0f;
			if ((a + ab * t).distance_to(p) < 0.95f) {
				return false;
			}
		}
		return true;
	}

	// Clusters: a tidy little ring or arc of 4-5 pieces, one kind, set round a real yard feature (both
	// sides of the porch steps, a front tree's trunk, a pumpkin cluster, beside a planter or a bale; on a
	// shop front, round the pieces flanking the door or the shop's own display). Placed on purpose, never
	// on the path, at carrying height so they are simply scooped up. At most `want`, one per feature.
	void clusters(int want, const int *colors, int c0) {
		const bool shop = lot.shop >= 0;
		const Road &road = ctx.road_of(lot);
		const float hw = lot.width * 0.5f + (shop ? 1.2f : -0.4f);
		const float z_max = shop ? road.walk - road.verge - 0.3f : -0.2f; // the picket (z -0.5) is tested as a box
		std::vector<int> order;
		for (int i = 0; i < int(lot.features.size()); ++i) {
			order.push_back(i);
		}
		for (int i = int(order.size()) - 1; i > 0; --i) {
			std::swap(order[i], order[r.irange(0, i)]);
		}
		int made = 0;
		for (int fi : order) {
			if (made >= want || room() < 4) {
				break;
			}
			const YardFeature &f = lot.features[fi];
			std::vector<std::vector<Vector3>> shapes; // lot-local, in pickup order
			auto ring = [&](float rad, int n, float a0) {
				std::vector<Vector3> s;
				for (int k = 0; k < n; ++k) {
					const float a = a0 + k * Math_TAU / n;
					s.push_back(f.c + Vector3(std::cos(a) * rad, LINE_H, std::sin(a) * rad));
				}
				shapes.push_back(s);
			};
			// An arc of n over `span` rad facing `dir` (lot-local, horizontal).
			auto arc_of = [&](float rad, int n, float span, const Vector3 &dir) {
				const float mid = std::atan2(dir.z, dir.x);
				std::vector<Vector3> s;
				for (int k = 0; k < n; ++k) {
					const float a = mid - span * 0.5f + span * float(k) / float(n - 1);
					s.push_back(f.c + Vector3(std::cos(a) * rad, LINE_H, std::sin(a) * rad));
				}
				shapes.push_back(s);
			};
			switch (f.kind) {
				case FeatureKind::Steps: {
					// A pair each side of the flight, in step with it (in front of the porch, beside the bottom step).
					for (float d : { 0.55f, 0.9f }) {
						std::vector<Vector3> s;
						for (float side : { -1.0f, 1.0f }) {
							for (float dz : { -0.2f, 0.55f }) { // clear of the porch deck and rails
								s.push_back(Vector3(f.c.x + side * (f.r + d), LINE_H, f.c.z + dz));
							}
						}
						shapes.push_back(s);
					}
					break;
				}
				case FeatureKind::Tree:
				case FeatureKind::Pumpkins: {
					const float a0 = r.range(0.0f, Math_TAU);
					const float base = f.r + (f.kind == FeatureKind::Tree ? 0.6f : 0.45f);
					ring(base, 5, a0);
					ring(base + 0.35f, 5, a0);
					arc_of(base, 4, 2.4f, Vector3(0, 0, 1));
					break;
				}
				case FeatureKind::Bale:
					// Against a side fence: an arc on its yard side.
					arc_of(f.r + 0.55f, 4, 2.2f, Vector3(f.c.x > 0.0f ? -1.0f : 1.0f, 0, 0));
					arc_of(f.r + 0.9f, 4, 2.0f, Vector3(f.c.x > 0.0f ? -1.0f : 1.0f, 0, 0));
					break;
				case FeatureKind::Doorside: {
					// Beside a shop door: an arc on the street side facing away from the door.
					const float door_x = lot.front_walk.empty() ? lot.house.door_x : lot.front_walk.back().x;
					const Vector3 away = Vector3(f.c.x >= door_x ? 1.0f : -1.0f, 0, 1.0f).normalized();
					arc_of(f.r + 0.5f, 4, 1.4f, away); // clear of the shop wall behind and the walk to the door
					arc_of(f.r + 0.8f, 4, 1.3f, away);
					break;
				}
				default:
					// Planter, a shop's display: an arc on the street side.
					arc_of(f.r + 0.5f, 4, 2.4f, Vector3(0, 0, 1));
					arc_of(f.r + 0.85f, 5, 2.2f, Vector3(0, 0, 1));
					break;
			}
			for (const std::vector<Vector3> &s : shapes) {
				bool ok = int(s.size()) <= room();
				std::vector<Vector3> pts;
				for (const Vector3 &l : s) {
					ok = ok && std::abs(l.x) <= hw && l.z <= z_max && l.z >= -lot.depth + 0.5f && off_path(l);
					pts.push_back(W(l));
				}
				if (!ok || !unclaimed_all(pts, 0.8f)) {
					continue;
				}
				for (size_t k = 0; k < pts.size() && ok; ++k) {
					ok = !nb.blocked(pts[k]);
				}
				if (!ok) {
					continue;
				}
				promotable_groups.push_back({ emit(pts, colors[(c0 + made) % 6], CPAT_CLUSTER), pts.size() });
				++made;
				break;
			}
		}
	}

	// Drive line: only as the run-up into the parked car's hood hop (a jump chain, not a trail): 4 from
	// the sidewalk's kerb side to a takeoff a running jump short of the bumper, the real jump onto the
	// bouncy hood, and a chocolate up where the bounce throws the kid. No car, no drive line.
	void drive_line(int color) {
		if (lot.drive_line.size() < 2 || !lot.car_in_drive || room() < 9) {
			return;
		}
		const Road &road = ctx.road_of(lot);
		const Vector3 a = lot.drive_line.front();
		const float zt = lot.car_hood.z + lot.car_hood_half.z + 1.5f;
		const float avail = (road.walk - road.verge - 0.3f) - zt;
		const float step = Math::clamp(avail / 3.0f, 0.6f, 1.1f);
		std::vector<Vector3> pts;
		for (int k = 3; k >= 0; --k) {
			pts.push_back(W(Vector3(a.x, LINE_H, zt + step * k)));
		}
		const Vector3 takeoff = W(Vector3(a.x, 0.0f, zt));
		const Vector3 hood = W(lot.car_hood);
		const Vector3 prize = hood + Vector3(0, 2.6f, 0);
		std::vector<Vector3> hop;
		if (avail < 2.4f || !jump_points(hop, takeoff, hood, 3, false)) {
			return;
		}
		std::vector<Vector3> all = pts;
		all.insert(all.end(), hop.begin(), hop.end());
		if (!clear_run(pts) || !clear_run(hop) || nb.blocked(prize) || !unclaimed_all(all, 0.8f)) {
			return;
		}
		const size_t at = emit(all, color, CPAT_DRIVE);
		promotable.push_back({ 4, at + all.size() - 1 }); // the landing on the hood
		promotable.push_back({ 8, at + 1 });
		CandySpec c;
		c.pos = prize;
		c.kind = CANDY_CHOCOLATE;
		c.lot = lot.index;
		c.pattern = CPAT_DRIVE;
		out.push_back(c);
	}

	// Porch roof (or shop awning): one short optional run up onto it, a lollipop for the bold
	// (the pre-Stage 1e run, unchanged).
	void roof_run(int rc) {
		std::vector<Vector3> roof;
		for (const Vector3 &p : lot.roof_route) {
			roof.push_back(lot.to_world(p));
		}
		if (r.chance(0.6f)) {
			const int roof_group = next_group++;
			const size_t at = out.size();
			if (!jump_arc(out, roof[0], roof[1], 3, lot.index, rc, roof_group)) {
				arc(out, roof[0] + Vector3(0, CARRY, 0), roof[1] + Vector3(0, CARRY, 0), roof[1].y + 1.2f, 3, lot.index, rc, roof_group);
			}
			if (r.chance(0.4f)) {
				out.push_back({ roof.back() + Vector3(0, 1.2f, 0), CANDY_LOLLIPOP, lot.index });
			}
			for (size_t i = at; i < out.size(); ++i) {
				out[i].pattern = CPAT_ROOF;
			}
			promotable.push_back({ 5, at + 1 });
		}
	}

	// Landmark summit: candy above every other step, the golden bar on top (unchanged).
	void summit(int color) {
		if (lot.summit_route.empty()) {
			return;
		}
		const int summit_group = next_group++;
		for (size_t i = 1; i + 1 < lot.summit_route.size(); i += 2) {
			out.push_back({ W(lot.summit_route[i]) + Vector3(0, 1.2f, 0), color, lot.index, summit_group, -1, CPAT_SUMMIT });
		}
		out.push_back({ W(lot.summit_route.back()) + Vector3(0, 0.95f, 0), CANDY_GOLDEN, lot.index, -1, -1, CPAT_SUMMIT });
	}

	// Stage 3c: now and then a little ring of five on the flat roof deck (the climb's reward), the street-facing one a
	// chocolate, kept clear of the chimney and the railing.
	void deck_ring(int color) {
		if (lot.deck_half.x <= 0.0f || room() < 5 || !r.chance(0.4f)) {
			return;
		}
		const float rad = Math::clamp(Math::min(lot.deck_half.x, lot.deck_half.z) - 0.45f, 0.6f, 1.1f);
		const float a0 = r.range(0.0f, Math_TAU);
		// Along the deck, off the chimney's end: try the middle, then a little either way.
		const Vector3 along = lot.deck_half.x >= lot.deck_half.z ? Vector3(1, 0, 0) : Vector3(0, 0, 1);
		for (float off : { 0.0f, -1.4f, 1.4f, -2.4f, 2.4f }) {
			std::vector<Vector3> pts;
			int front = 0;
			float best = -1e9f;
			for (int k = 0; k < 5; ++k) {
				const float a = a0 + k * Math_TAU / 5.0f;
				const Vector3 l = lot.deck_c + along * off + Vector3(std::cos(a) * rad, CARRY, std::sin(a) * rad);
				if (l.z > best) {
					best = l.z;
					front = k;
				}
				pts.push_back(lot.to_world(l)); // (on the house: no yard lift)
			}
			if (!unclaimed_all(pts, 0.7f) || !clear_run(pts) || nb.blocked((pts[0] + pts[4]) * 0.5f)) {
				continue;
			}
			const size_t at = emit(pts, color, CPAT_DECK);
			out[at + front].kind = CANDY_CHOCOLATE;
			return;
		}
	}

	// Landmark ring: 6-8 evenly round the centrepiece at carrying height, one of them (the one facing
	// the street) a chocolate. Tries a few radii and a higher ring before giving up.
	bool ring(int color) {
		if (!lot.has_centrepiece) {
			return false;
		}
		const float a0 = r.range(0.0f, Math_TAU);
		for (float h : { CARRY, 1.35f }) {
			for (float extra : { 0.7f, 1.1f, 1.5f, 1.9f }) {
				const float rad = lot.centre_r + extra;
				const int n = Math::clamp(int(std::round(Math_TAU * rad / 1.2f)), 6, 8);
				if (n > room()) {
					return false;
				}
				std::vector<Vector3> pts;
				int front = 0;
				float best = -1e9f;
				for (int k = 0; k < n; ++k) {
					const float a = a0 + k * Math_TAU / n;
					const Vector3 l = lot.centrepiece + Vector3(std::cos(a) * rad, h, std::sin(a) * rad);
					if (l.z > best) {
						best = l.z;
						front = k;
					}
					pts.push_back(W(l));
				}
				bool ok = unclaimed_all(pts, 0.7f);
				for (size_t k = 0; k < pts.size() && ok; ++k) {
					ok = !nb.blocked(pts[k]) && !nb.blocked((pts[k] + pts[(k + 1) % pts.size()]) * 0.5f);
				}
				if (ok) {
					const size_t at = emit(pts, color, CPAT_RING);
					out[at + front].kind = CANDY_CHOCOLATE;
					promotable.push_back({ 2, at + (front + n / 2) % n });
					return true;
				}
			}
		}
		return false;
	}

	// Platform sequences, at most `want`: a running jump from open ground onto the lowest free
	// platform, maybe a chain to a higher neighbour, and a crown of three over the top platform
	// (Hayashida's kishotenketsu: introduce, develop, conclude). Runs sit at least 4 m apart.
	void sequences(std::vector<Plat> &plats, int want, const int *colors, int c0) {
		for (Plat &p : plats) {
			p.order = p.top.y + r.range(-0.3f, 0.3f);
		}
		std::sort(plats.begin(), plats.end(), [](const Plat &a, const Plat &b) { return a.order < b.order; });
		std::vector<Vector3> used; // Stage 3b: takeoffs from the ground under them (ctx.ground), not the pad
		int made = 0;
		for (const Plat &p : plats) {
			if (made >= want || room() < 5) {
				break;
			}
			bool close = false;
			for (const Vector3 &u : used) {
				close = close || Vector2(u.x - p.top.x, u.z - p.top.z).length() < 4.0f;
			}
			if (close || !unclaimed(p.top + Vector3(0, CARRY, 0), 1.4f)) {
				continue;
			}
			bool flap = false;
			const float T = flight_time(p.top.y - ctx.ground(p.top.x, p.top.z), flap);
			if (T <= 0.0f) {
				continue;
			}
			const float reach = Math::clamp(move::RUN_SPEED * 0.8f * T, 2.4f, 4.6f);
			// Approach from open ground, preferring the street side.
			struct Cand {
				float score;
				Vector3 g;
			};
			std::vector<Cand> cands;
			const float a0 = r.range(0.0f, Math_TAU);
			for (int k = 0; k < 10; ++k) {
				const float a = a0 + k * Math_TAU / 10.0f;
				Vector3 g = p.top + Vector3(std::cos(a), 0, std::sin(a)) * reach;
				g.y = ctx.ground(g.x, g.z);
				if (!nb.open_ground(g)) {
					continue;
				}
				cands.push_back({ r.range(0.0f, 0.5f) + lot.to_local(g).z * 0.15f, g });
			}
			std::sort(cands.begin(), cands.end(), [](const Cand &x, const Cand &y) { return x.score > y.score; });
			std::vector<Vector3> approach;
			Vector3 from;
			for (const Cand &c : cands) {
				std::vector<Vector3> pts;
				if (!jump_points(pts, c.g, p.top, 3, false)) {
					continue;
				}
				std::vector<Vector3> flight(pts.begin(), pts.end() - 1); // the landing piece sits on the platform
				if (clear_run(flight) && unclaimed_all(pts, 0.75f)) {
					approach = pts;
					from = c.g;
					break;
				}
			}
			if (approach.empty()) {
				continue;
			}
			// Chain to a higher neighbouring platform along the real jump between them.
			std::vector<Vector3> chain;
			const Plat *top = &p;
			if (room() >= 8) {
				for (const Plat &q : plats) {
					const float dh = q.top.y - p.top.y;
					const float dist = Vector2(q.top.x - p.top.x, q.top.z - p.top.z).length();
					if (q.box == p.box || dh <= 0.5f || dh >= 2.3f || dist <= 1.8f || dist >= 4.2f || !unclaimed(q.top + Vector3(0, CARRY, 0), 1.4f)) {
						continue;
					}
					std::vector<Vector3> pts;
					if (jump_points(pts, p.top, q.top, 3, false) && clear_run(std::vector<Vector3>(pts.begin(), pts.end() - 1)) && unclaimed_all(pts, 0.75f)) {
						chain = pts;
						top = &q;
						break;
					}
				}
			}
			// Crown: a tight triangle over the top platform, one vertex toward where the jump came from.
			const Vector3 back = (chain.empty() ? from : p.top) - top->top;
			const float ab = std::atan2(back.z, back.x);
			std::vector<Vector3> crown;
			for (float lift : { CARRY, CARRY + 0.3f }) {
				crown.clear();
				for (int k = 0; k < 3; ++k) {
					const float a = ab + k * Math_TAU / 3.0f;
					crown.push_back(top->top + Vector3(std::cos(a) * 0.35f, lift, std::sin(a) * 0.35f));
				}
				bool ok = true;
				for (const Vector3 &c : crown) {
					ok = ok && !nb.blocked(c);
				}
				if (ok) {
					break;
				}
				crown.clear();
			}
			const int color = colors[(c0 + made) % 6];
			// The landing piece on the top platform gives way to the crown.
			std::vector<Vector3> &last = chain.empty() ? approach : chain;
			if (!crown.empty()) {
				last.pop_back();
			}
			const int total = int(approach.size() + chain.size() + crown.size());
			if (total > room()) {
				break;
			}
			// Approach (and its crown when there is no chain) is one rhythm group; the chain and its crown another.
			const int g0 = next_group++;
			promotable.push_back({ 7, emit(approach, color, CPAT_SEQUENCE, g0) + approach.size() / 2 });
			const int g1 = chain.empty() ? g0 : next_group++;
			if (!chain.empty()) {
				emit(chain, color, CPAT_SEQUENCE, g1);
			}
			if (!crown.empty()) {
				const size_t at = emit(crown, color, CPAT_CROWN, g1);
				promotable.push_back({ 1, at });
				promotable.push_back({ 7, at + 1 });
			}
			used.push_back(p.top);
			if (!chain.empty()) {
				used.push_back(top->top);
			}
			++made;
		}
	}

	// Jump-point arcs: three pieces over something the kid would jump anyway (the low front picket,
	// a pumpkin cluster, a single hay bale, a brick planter), takeoff and landing about 1.5 m either
	// side, approached from the street side where there is a choice. At most `want`.
	void obstacle_arcs(int want, const int *colors, int c0) {
		const Road &road = ctx.road_of(lot);
		const float hw = lot.width * 0.5f + (lot.shop >= 0 ? 1.2f : -0.4f); // shop fronts run on into the next one
		const float z_front = road.walk - road.verge - 0.3f; // takeoff may be on the sidewalk
		const float ground_y = 0.0f; // lot-local
		auto ground_ok = [&](const Vector3 &l) {
			return std::abs(l.x) <= hw && l.z <= z_front && l.z >= -lot.depth + 0.6f && nb.open_ground(W(Vector3(l.x, ground_y, l.z)));
		};
		std::vector<int> order;
		for (int i = 0; i < int(lot.obstacles.size()); ++i) {
			order.push_back(i);
		}
		for (int i = int(order.size()) - 1; i > 0; --i) {
			std::swap(order[i], order[r.irange(0, i)]);
		}
		int made = 0;
		for (int oi : order) {
			if (made >= want || room() < 3) {
				break;
			}
			const YardObstacle &o = lot.obstacles[oi];
			const float half = Math::max(1.5f, o.half_across + 0.9f);
			std::vector<Vector3> dirs;
			if (o.across.length() > 0.5f) {
				dirs = { o.across, -o.across };
			} else {
				const float a0 = r.range(0.0f, Math_TAU);
				for (int k = 0; k < 8; ++k) {
					const float a = a0 + k * Math_TAU / 8.0f;
					dirs.push_back(Vector3(std::cos(a), 0, std::sin(a)));
				}
			}
			std::vector<float> slides = { 0.0f };
			if (o.half_along > 0.6f) {
				slides.clear();
				const float span = o.half_along - 0.6f;
				const float aim = r.range(-span, span);
				for (float s = -span; s <= span + 1e-3f; s += 0.5f) {
					slides.push_back(s);
				}
				std::sort(slides.begin(), slides.end(), [&](float x, float y) { return std::abs(x - aim) < std::abs(y - aim); });
			}
			const Vector3 along = o.across.length() > 0.5f ? Vector3(0, 1, 0).cross(o.across).normalized() : Vector3();
			struct Best {
				float score = -1e9f;
				std::vector<Vector3> pts;
			} best;
			for (float sl : slides) {
				for (const Vector3 &d : dirs) {
					const Vector3 c = o.c + along * sl;
					const Vector3 la = c - d * half, lb = c + d * half;
					if (!ground_ok(la) || !ground_ok(lb)) {
						continue;
					}
					std::vector<Vector3> pts;
					if (!jump_points(pts, W(Vector3(la.x, ground_y, la.z)), W(Vector3(lb.x, ground_y, lb.z)), 3, true)) {
						continue;
					}
					if (!clear_run(pts) || !unclaimed_all(pts, 1.0f)) {
						continue;
					}
					// Into the yard from the street side first, then toward the house.
					const float score = -d.z + r.range(0.0f, 0.3f);
					if (score > best.score) {
						best.score = score;
						best.pts = pts;
					}
				}
				if (!best.pts.empty()) {
					break; // the first slide (nearest the aim) that works
				}
			}
			if (best.pts.empty()) {
				continue;
			}
			const size_t at = emit(best.pts, colors[(c0 + made) % 6], CPAT_OBSTACLE);
			promotable.push_back({ 3, at + 1 });
			++made;
		}
	}

	// Chocolates along the patterns until the lot's everyday + chocolate value is back near `target`.
	void top_up(int target) {
		int value = 0;
		for (size_t i = first; i < out.size(); ++i) {
			if (candy_tier(out[i].kind) <= 1) {
				value += candy_value(out[i].kind);
			}
		}
		std::stable_sort(promotable.begin(), promotable.end(), [](const std::pair<int, size_t> &a, const std::pair<int, size_t> &b) { return a.first < b.first; });
		for (const auto &pr : promotable) {
			if (value >= target - 2) {
				break;
			}
			CandySpec &c = out[pr.second];
			if (candy_tier(c.kind) == 0) {
				c.kind = CANDY_CHOCOLATE;
				value += candy_value(CANDY_CHOCOLATE) - 1;
			}
		}
		// Still short: a whole cluster turns chocolate (one kind still), if that does not overshoot by much.
		for (const auto &g : promotable_groups) {
			const int gain = int(g.second) * (candy_value(CANDY_CHOCOLATE) - 1);
			if (value >= target - 2 || value + gain > target + 6) {
				continue;
			}
			for (size_t i = g.first; i < g.first + g.second; ++i) {
				out[i].kind = CANDY_CHOCOLATE;
			}
			value += gain;
		}
	}
};
// Street candy on spline roads (Stage 2a): a row of hops across the parent road beside every side
// road's mouth (past the rounded corners, on the longer arm), small loose clusters along the sidewalks
// and verges of every road, and a ring round the middle of every turning circle.
void weave_street_candy(GenContext &ctx, Rng &sr, int &next_group) {
	const RoadNet &net = ctx.roads;
	std::vector<CandySpec> &out = ctx.candy;
	const int colors[] = { CANDY_PINK, CANDY_MINT, CANDY_GRAPE, CANDY_LEMON, CANDY_ORANGE, CANDY_CORN };
	for (int r = 0; r < int(net.roads.size()); ++r) {
		const Road &P = net.roads[r];
		for (const RoadJunction &j : net.junctions(r)) {
			const bool fwd = P.lots_s1 - j.s >= j.s - P.lots_s0;
			float off = 7.5f;
			for (const BandCut &c : P.cuts) {
				if (c.side == j.side && j.s >= c.s0 - 1.0f && j.s <= c.s1 + 1.0f) {
					off = Math::max(off, (fwd ? c.s1 - j.s : j.s - c.s0) + 3.0f);
				}
			}
			const float hx = j.s + (fwd ? off : -off);
			const Vector3 hcen = P.at(hx), hl = P.left_at(hx);
			const int hop_group = next_group++;
			for (int i = -3; i <= 3; i += 2) {
				out.push_back({ hcen + hl * (i * 1.1f) + Vector3(0, 2.0f, 0), CANDY_LEMON, -1, hop_group });
			}
		}
	}
	for (int r = 0; r < int(net.roads.size()); ++r) {
		const Road &s = net.roads[r];
		for (float x = s.lots_s0 + sr.range(4.0f, 8.0f); x < s.lots_s1 - 4.0f; x += sr.range(9.0f, 14.0f)) {
			const float side = sr.chance(0.5f) ? 1.0f : -1.0f;
			const Vector3 c = s.at(x) + s.left_at(x) * side * sr.range(s.half_road + 0.6f, s.edge() - 0.6f);
			const int color = colors[sr.irange(0, 5)];
			const int n = sr.irange(3, 4);
			const RoadZone z = net.zone(c);
			if (s.cut(int(side), x) || z == RoadZone::Asphalt || z == RoadZone::Off) {
				continue; // a mouth, a junction patch
			}
			for (int k = 0; k < n; ++k) {
				const Vector3 p = c + s.tangent_at(x) * ((k - (n - 1) * 0.5f) * 0.8f) + Vector3(0, 0.8f, 0);
				if (!blocked_at(ctx, p) && !blocked_at(ctx, p - Vector3(0, 0.55f, 0))) {
					out.push_back({ p, color, -1 });
				}
			}
		}
		if (s.bulb()) {
			const int color = colors[sr.irange(0, 5)];
			const int group = next_group++;
			const float a0 = sr.range(0.0f, Math_TAU);
			for (int k = 0; k < 8; ++k) {
				const float a = a0 + k * Math_TAU / 8.0f;
				const Vector3 p = s.bulb_c + Vector3(std::cos(a), 0.0f, std::sin(a)) * 4.6f + Vector3(0, 0.8f, 0);
				if (!blocked_at(ctx, p)) {
					out.push_back({ p, color, -1, group });
				}
			}
		}
	}
}
// Stage 3c: candy columns over a share (~40%) of the bounce pads: car hoods (drives and kerbs), trampolines, bouncy
// inflatables, shop awnings. Straight up the pad's axis: three everyday pieces a plain bounce reaches, three
// chocolates up to the full-jump bounce's ceiling (jump held on contact), and a lollipop at the very top, halfway
// between that ceiling and the full jump + flap one, so only bounce + full jump + flap gets it. A column goes where
// the whole axis is clear (no roof, eave or canopy over it) and no other platform within a running jump's reach
// stands high enough to take the top piece another way.
void bounce_columns(GenContext &ctx, int &next_group) {
	Rng br = ctx.rng(STAGE_CANDY, 0xB0B5);
	std::vector<CandySpec> &out = ctx.candy;
	const int colors[] = { CANDY_PINK, CANDY_MINT, CANDY_GRAPE, CANDY_LEMON, CANDY_ORANGE, CANDY_CORN };
	std::vector<int> pads;
	for (int i = 0; i < int(ctx.boxes.size()); ++i) {
		if (ctx.boxes[i].bouncy) {
			pads.push_back(i);
		}
	}
	if (pads.empty()) {
		return;
	}
	br.shuffle(pads);
	struct Top {
		const OBox *b;
		Transform3D inv;
		float top;
	};
	std::vector<Top> tops; // everything else a kid can stand on
	for (const OBox &b : ctx.boxes) {
		if (b.walkable && !b.bouncy) {
			tops.push_back({ &b, b.xf.affine_inverse(), world_extent(b).top });
		}
	}
	struct Canopy {
		Vector3 c;
		float r;
	};
	std::vector<Canopy> canopies; // trees and lamps: no collision up there, but candy in a canopy reads badly
	for (const PropPlacement &pp : ctx.props) {
		if (pp.id.rfind("tree_", 0) == 0 || pp.id == "lamp_post") {
			canopies.push_back({ pp.xf.origin, pp.id == "lamp_post" ? 1.2f : 2.4f });
		}
	}
	const int want = int(std::ceil(float(pads.size()) * 0.4f));
	int made = 0;
	// Two passes: first the pads whose top piece no deck or roof could reach even by a full running leap + flap (8.5 m
	// out), then the rest to the share with a short run-up's reach (6.65 m).
	std::vector<char> done(pads.size(), 0);
	for (int pass = 0; pass < 2; ++pass) {
	const float leap = pass == 0 ? 8.5f : 6.65f;
	for (size_t pk = 0; pk < pads.size(); ++pk) {
		if (made >= want) {
			break;
		}
		if (done[pk]) {
			continue;
		}
		const int pi = pads[pk];
		const OBox &b = ctx.boxes[pi];
		const float top = world_extent(b).top;
		const Vector3 axis(b.xf.origin.x, top, b.xf.origin.z);
		const float s = b.bounce;
		const float a1 = move::bounce_apex(s, false, false, false, true); // a plain bounce, jump not held (cut short)
		const float a2 = move::bounce_apex(s, true, false, true); // jump held on contact, the air spin at the top
		const float a3 = move::bounce_apex(s, true, true, false); // ... and the wing flap at the apex
		const float h1 = a1 + move::COLLECT_HIGH - 0.35f, h2 = a2 + move::COLLECT_HIGH - 0.35f;
		const float h_top = (a2 + a3) * 0.5f + move::COLLECT_HIGH;
		std::vector<float> hs;
		for (int k = 0; k < 3; ++k) {
			hs.push_back(Math::lerp(1.3f, h1, float(k) / 2.0f));
		}
		for (int k = 0; k < 3; ++k) {
			hs.push_back(Math::lerp(h1 + 0.6f, h2, float(k) / 2.0f));
		}
		hs.push_back(h_top);
		// A column straight up, or (when a roof deck or a chimney stands within a running jump of the axis) a vertical
		// arc leaning away from it: its offset grows with the square of the height, at most 2.4 m at the top, well inside
		// the bucket's 2.65 m reach of a kid bouncing straight up. Tried straight first, then leaning 1.2 / 2.4 m 8 ways.
		auto offset_at = [&](float h, const Vector3 &lean) { return lean * ((h / h_top) * (h / h_top)); };
		const float top_y = axis.y + h_top;
		int why = 0; // the first test a lean failed (for the stats)
		Vector3 lean;
		bool ok = false;
		const float a0 = br.range(0.0f, Math_TAU);
		std::vector<const OBox *> near; // what could stand in the column's way (a 2.4 m lean, a kid's width)
		for (const OBox &o : ctx.boxes) {
			if (&o != &b && Vector2(o.xf.origin.x - axis.x, o.xf.origin.z - axis.z).length() < 3.4f + o.half.length() * o.xf.basis.get_scale().length()) {
				near.push_back(&o);
			}
		}
		for (int t = 0; t < 17 && !ok; ++t) {
			const float L = t == 0 ? 0.0f : (t <= 8 ? 1.2f : 2.4f);
			const float a = a0 + float((t - 1) % 8) * Math_TAU / 8.0f;
			lean = Vector3(std::cos(a), 0.0f, std::sin(a)) * L;
			bool clear = true;
			// The whole path clear, a kid's width round it, from just over the pad to above the top piece.
			for (float y = 0.6f; y <= h_top + 0.6f && clear; y += 0.5f) {
				const Vector3 p = axis + Vector3(0, y, 0) + offset_at(Math::min(y, h_top), lean);
				for (const OBox *o : near) {
					if (inside_box(*o, p, 0.45f)) {
						clear = false;
						break;
					}
				}
			}
			if (!clear) {
				why = Math::max(why, 0);
				continue;
			}
			for (const Canopy &cn : canopies) {
				for (float h : hs) {
					const Vector3 p = axis + offset_at(h, lean);
					clear = clear && (h < 1.8f || Vector2(cn.c.x - p.x, cn.c.z - p.z).length() > cn.r);
				}
			}
			if (!clear) {
				why = Math::max(why, 1);
				continue;
			}
			// Only that way: nothing else to stand on within a jump + flap with a short run-up of the top piece (~4 m out
			// from its edge, plus the bucket's 2.65 m reach) high enough that its own jump + flap (3.4 m, spin included)
			// brings the top piece into reach, nor a higher one it could be glided down to from (3.4 m out per metre of drop).
			const Vector3 tp = axis + offset_at(h_top, lean);
			for (const Top &tt : tops) {
				if (tt.top + 3.4f + move::COLLECT_HIGH < top_y - 0.2f) {
					continue;
				}
				const Vector3 l = tt.inv.xform(Vector3(tp.x, tt.b->xf.origin.y, tp.z));
				const Vector3 sc = tt.b->xf.basis.get_scale();
				const float dx = Math::max(0.0f, std::abs(l.x) - tt.b->half.x) * sc.x, dz = Math::max(0.0f, std::abs(l.z) - tt.b->half.z) * sc.z;
				const float drop = Math::max(0.0f, tt.top - (top_y - move::COLLECT_HIGH));
				if (std::sqrt(dx * dx + dz * dz) < leap + 3.4f * drop) {
					clear = false;
					break;
				}
			}
			if (!clear) {
				why = 2;
				continue;
			}
			ok = true;
		}
		if (!ok) {
			ctx.stats.bounce_reject[why] += pass == 1 ? 1 : 0;
			continue;
		}
		done[pk] = 1;		// The car-hood hop's chocolate (drive_line: 2.6 m over the hood) gives way to the column.
		for (size_t i = 0; i < out.size();) {
			const CandySpec &c = out[i];
			if (c.pattern == CPAT_DRIVE && c.kind == CANDY_CHOCOLATE && c.group < 0 && Vector2(c.pos.x - axis.x, c.pos.z - axis.z).length() < 0.8f && c.pos.y > axis.y + 1.0f) {
				out.erase(out.begin() + long(i));
			} else {
				++i;
			}
		}
		const int color = colors[br.irange(0, 5)];
		const int group = next_group++;
		for (size_t k = 0; k < hs.size(); ++k) {
			CandySpec c;
			c.pos = axis + Vector3(0, hs[k], 0) + offset_at(hs[k], lean);
			c.kind = k < 3 ? color : (k + 1 < hs.size() ? CANDY_CHOCOLATE : CANDY_LOLLIPOP);
			c.lot = b.lot;
			c.group = k + 1 < hs.size() ? group : -1;
			c.pattern = CPAT_BOUNCE;
			out.push_back(c);
		}
		++made;
	}
	}
}
} // namespace

void weave_candy(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_CANDY);
	const Road &s = ctx.roads.trunk();
	const std::vector<RoadJunction> mouths = ctx.roads.junctions(0);
	std::vector<CandySpec> &out = ctx.candy;
	out.clear();
	const int colors[] = { CANDY_PINK, CANDY_MINT, CANDY_GRAPE, CANDY_LEMON, CANDY_ORANGE, CANDY_CORN };
	// Platforms worth jumping onto: bales, crates, barrels, rails, car roofs, benches, wagons...
	std::vector<std::vector<Plat>> by_lot(ctx.lots.size() + 1);
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		const OBox &b = ctx.boxes[i];
		const BoxInfo e = world_extent(b);
		const float gb = ctx.ground(b.xf.origin.x, b.xf.origin.z); // Stage 3b: heights above the ground under it
		if (!b.walkable || b.boundary || e.top - gb < 0.75f || e.top - gb > 3.2f || e.top - e.bottom > 3.3f) {
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
			const float gp = ctx.ground(p.top.x, p.top.z); // Stage 3b: heights above the ground under it
			const float T = flight_time(p.top.y - gp, flap);
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
				g.y = ctx.ground(g.x, g.z);
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
			const int color = colors[r.irange(0, 5)];
			const int count = r.irange(3, 4);
			if (!jump_arc(out, best, p.top, count, lot, color, next_group)) {
				arc(out, best + Vector3(0, 1.2f, 0), p.top + Vector3(0, CARRY, 0), p.top.y + 1.5f, count, lot, color, next_group);
			}
			++next_group;
			// Bonus right above the platform: jump from the top to grab it.
			if (p.top.y - gp < 2.2f && r.chance(0.6f)) {
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
	// Front yards (and shop fronts): designed shapes in priority order under the lot's cap, each one
	// colour and one rhythm group, then chocolates along them to keep the lot's value (Stage 1e).
	for (Lot &lot : ctx.lots) {
		Rng lr = ctx.rng(STAGE_CANDY, lot.index);
		const HouseSpec &h = lot.house;
		const bool landmark = lot.landmark != Landmark::None;
		NearBoxes nb;
		nb.build(ctx, lot.to_world(0.0f, 0.0f, -lot.depth * 0.5f), Vector2(lot.width * 0.5f, lot.depth * 0.5f).length() + 6.0f);
		LotWeaver w{ ctx, lot, lr, next_group, out, nb };
		w.first = out.size();
		// Bonus pieces (unchanged odds): the garage roof's lollipop (the bales by the garage door are
		// the way up), and a backyard secret high over the lawn (needs the wing flap). Drawn first so
		// they count against the cap.
		const bool garage_prize = h.garage && lr.chance(0.6f);
		const bool secret = !h.garage && lr.chance(0.35f);
		w.cap = (landmark ? LANDMARK_CAP : LOT_CAP) - (garage_prize ? 1 : 0) - (secret ? 1 : 0);
		const int c0 = lr.irange(0, 5);
		auto color = [&](int k) { return colors[(c0 + k) % 6]; };
		w.drive_line(color(1));
		w.roof_run(color(2));
		w.summit(color(5));
		w.deck_ring(color(4));
		w.ring(color(3));
		w.clusters(2, colors, c0);
		w.sequences(by_lot[lot.index], landmark ? 3 : 2, colors, c0 + 4);
		w.obstacle_arcs(w.count() < 20 ? 3 : 2, colors, c0 + 2); // the flex: a third arc on a sparse lot
		w.cap += (garage_prize ? 1 : 0) + (secret ? 1 : 0);
		if (garage_prize) {
			const float g = float(h.garage_side);
			const float gx = h.house_x + g * (h.width * 0.5f + 1.9f);
			out.push_back({ lot.to_world(gx, 3.04f + 1.2f, h.front_z - 0.4f - 3.0f), CANDY_LOLLIPOP, lot.index });
		}
		if (secret) {
			out.push_back({ lot.to_world(h.house_x, 2.9f + ctx.yard_lift(lot, h.house_x, h.front_z - h.depth - 3.2f), h.front_z - h.depth - 3.2f), CANDY_LOLLIPOP, lot.index });
		}
		w.top_up(lot.shop >= 0 ? TOWN_VALUE : HOOD_VALUE);
	}
	// Street: arcs onto parked cars, hay walls of the pocket parks, and a row of hops across the road.
	Rng sr = ctx.rng(STAGE_CANDY, 999);
	sequences(by_lot.back(), -1, 6, sr);
	bounce_columns(ctx, next_group); // Stage 3c
	if (!s.x_station) { // (Stage 4c: towns too; the legacy branch below is the pre-4c town street)
		weave_street_candy(ctx, sr, next_group);
		ctx.stats.candy = int(out.size());
		return;
	}
	// A row of hops across the road beside each side road's mouth (on the long-street side).
	for (const RoadJunction &j : mouths) {
		const float hx = j.s + (j.s < 0 ? 7.5f : -7.5f);
		const Vector3 hcen = s.at(hx), hl = s.left_at(hx);
		const int hop_group = next_group++;
		for (int i = -3; i <= 3; i += 2) {
			out.push_back({ hcen + hl * (i * 1.1f) + Vector3(0, 2.0f, 0), CANDY_LEMON, -1, hop_group });
		}
	}
	// Ground candy along the sidewalks and verges: small loose clusters spread down the street.
	for (float x = s.lots_s0 + sr.range(4.0f, 8.0f); x < s.lots_s1 - 4.0f; x += sr.range(9.0f, 14.0f)) {
		bool mouth = false;
		for (const RoadJunction &j : mouths) {
			mouth = mouth || std::abs(x - j.s) < ctx.roads.roads[j.child].edge() + 1.5f;
		}
		if (mouth) {
			continue;
		}
		const float side = sr.chance(0.5f) ? 1.0f : -1.0f;
		const Vector3 c = s.at(x) + s.left_at(x) * side * sr.range(s.half_road + 0.6f, s.edge() - 0.6f);
		const int color = colors[sr.irange(0, 5)];
		const int n = sr.irange(3, 4);
		for (int k = 0; k < n; ++k) {
			const Vector3 p = c + s.tangent_at(x) * ((k - (n - 1) * 0.5f) * 0.8f) + Vector3(0, 0.8f, 0);
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
	ctx.magic.clear();
	// Spine: meanders down the street and up each side road.
	const float phase = rng.range(0.0f, Math_TAU);
	for (int r = 0; r < int(ctx.roads.roads.size()); ++r) {
		const Road &s = ctx.roads.roads[r];
		MagicPath spine;
		spine.lot = -1;
		if (s.parent < 0) {
			for (float x = s.lots_s0 + 3.0f; x <= s.lots_s1 - 3.0f; x += 2.0f) {
				Vector3 p = s.at(x) + s.left_at(x) * (1.6f * std::sin(x * 0.11f + phase));
				p.y = s.at(x).y + 1.0f + 0.25f * std::sin(x * 0.23f + phase * 2.0f); // Stage 3b: over the graded road
				spine.points.push_back(p);
			}
		} else {
			// From the junction up the side road; the sway is measured the way the parent runs.
			const float sway = -float(s.parent_side);
			const float end = s.bulb() ? s.bulb_s - s.s_begin() : s.length() - 1.0f;
			for (float d = 0.0f; d <= end; d += 2.0f) {
				spine.points.push_back(s.at(s.s_begin() + d) + s.left_at(s.s_begin() + d) * (sway * (1.2f * std::sin(d * 0.2f + phase))) + Vector3(0, 1.0f, 0));
			}
			if (s.bulb()) {
				// Once round the turning circle (clear of the gate island in its middle) and back to its mouth.
				const Vector3 te = s.tangent_at(s.s_end()), le = s.left_at(s.s_end());
				const float rr = s.bulb_r - 3.5f;
				for (int k = 1; k <= 20; ++k) {
					const float a = float(Math_PI) - k * float(Math_TAU) / 20.0f;
					spine.points.push_back(s.bulb_c + (te * std::cos(a) + le * std::sin(a)) * rr + Vector3(0, 1.0f + 0.2f * std::sin(k * 0.9f + phase), 0));
				}
			}
		}
		ctx.magic.push_back(spine);
	}
	// Branches: from the street spine to each doorstep, ending in a little swirl at the door.
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		MagicPath b;
		b.lot = lot.index;
		const Vector3 entrance = lot.to_world(h.door_x, 0, 0.6f);
		Vector3 street = ctx.roads.project(lot.road, entrance).point;
		if (lot.bulb) {
			// From the spine's loop round the turning circle.
			const Road &B = ctx.road_of(lot);
			const Vector3 d = Vector3(entrance.x - B.bulb_c.x, 0.0f, entrance.z - B.bulb_c.z).normalized();
			street = B.bulb_c + d * (B.bulb_r - 3.5f);
		}
		const Vector3 on_street = street + (entrance - street).normalized() * 1.2f;
		const float steps_front = lot.flight.empty() ? h.front_z + h.porch_d + float(lot.porch_steps) * 0.36f : lot.flight_foot; // Stage 3c
		auto P = [&](const Vector3 &w, float y) { return Vector3(w.x, ctx.ground(w.x, w.z) + y, w.z); }; // Stage 3b: over the ground
		b.points.push_back(P(on_street, 1.0f));
		b.points.push_back(P(lot.to_world(h.door_x, 0, ctx.road_of(lot).walk + 0.4f), 0.95f));
		b.points.push_back(P(entrance, 0.9f));
		b.points.push_back(lot.to_world(h.door_x + 0.3f, 0.85f + ctx.yard_lift(lot, h.door_x + 0.3f, (steps_front - 0.6f) * 0.5f), (steps_front - 0.6f) * 0.5f));
		b.points.push_back(lot.to_world(h.door_x, 0.9f + ctx.yard_lift(lot, h.door_x, steps_front + 0.8f), steps_front + 0.8f));
		b.points.push_back(lot.to_world(h.door_x, h.deck_h + 0.9f, h.front_z + h.porch_d - 0.3f));
		for (int k = 0; k <= 8; ++k) {
			const float a = k * Math_TAU / 8.0f;
			b.points.push_back(lot.to_world(h.door_x + std::sin(a) * 0.55f, h.deck_h + 0.9f + k * 0.08f, h.front_z + 0.8f + std::cos(a) * 0.35f));
		}
		ctx.magic.push_back(b);
	}
}

// ---------------------------------------------------------------- traversal + validation
// Stage 3c: the bounce pads' ceilings (feet above the pad's top) with a safety margin: a full jump (jump held on
// contact) and the wing flap at its apex; and what a pad lets the kid reach (a piece's height above its top).
float bounce_reach(float speed) {
	return move::bounce_apex(speed, true, true) - 0.35f;
}

std::vector<char> platform_reach(const GenContext &ctx, bool bounce) {
	std::vector<int> plat;
	std::vector<BoxInfo> ext(ctx.boxes.size());
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		ext[i] = world_extent(ctx.boxes[i]);
		if (ctx.boxes[i].walkable || ctx.boxes[i].bouncy) { // (Stage 3c: a bounce pad is somewhere to land too)
			plat.push_back(int(i));
		}
	}
	std::vector<char> reach(ctx.boxes.size(), 0);
	std::vector<float> rad(ctx.boxes.size(), 0.0f); // horizontal radius bounding each footprint
	for (int i : plat) {
		const OBox &b = ctx.boxes[i];
		rad[i] = (b.half * b.xf.basis.get_scale()).length() * 1.001f + 1e-3f;
	}
	std::deque<int> q;
	for (int i : plat) {
		const float g = ctx.ground(ctx.boxes[i].xf.origin.x, ctx.boxes[i].xf.origin.z); // Stage 3b: from the ground under it
		if (ext[i].top - g <= JUMP_REACH || ext[i].bottom - g <= 0.35f) {
			reach[i] = 1;
			q.push_back(i);
		}
	}
	while (!q.empty()) {
		const int i = q.front();
		q.pop_front();
		// Stage 3c: a reachable bounce pad throws the kid up to its full jump + flap ceiling (air control steers).
		const float lift = bounce && ctx.boxes[i].bouncy ? bounce_reach(ctx.boxes[i].bounce) : JUMP_REACH;
		for (int j : plat) {
			if (reach[j]) {
				continue;
			}
			const float dh = ext[j].top - ext[i].top;
			if (dh > lift) {
				continue;
			}
			// Falling from higher up buys extra horizontal reach (glide).
			const float gap_ok = bounce && ctx.boxes[i].bouncy && dh > JUMP_REACH ? 2.0f : GAP_REACH + Math::clamp(-dh, 0.0f, 3.0f) * 0.8f - Math::max(0.0f, dh) * 0.3f;
			// (an exact pre-test: two footprints are never closer than their centres less both radii)
			const Vector3 &ci = ctx.boxes[i].xf.origin, &cj = ctx.boxes[j].xf.origin;
			if (Vector2(ci.x - cj.x, ci.z - cj.z).length() - rad[i] - rad[j] > gap_ok + 0.01f) {
				continue;
			}
			if (footprint_gap(ctx.boxes[i], ctx.boxes[j]) <= gap_ok) {
				reach[j] = 1;
				q.push_back(j);
			}
		}
	}
	return reach;
}

void validate_traversal(GenContext &ctx) {
	std::vector<int> plat;
	std::vector<BoxInfo> ext(ctx.boxes.size());
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		ext[i] = world_extent(ctx.boxes[i]);
		if (ctx.boxes[i].walkable || ctx.boxes[i].bouncy) { // (Stage 3c: a bounce pad is somewhere to land too)
			plat.push_back(int(i));
		}
	}
	const std::vector<char> reach = platform_reach(ctx);
	std::vector<float> hold_top(ctx.boxes.size(), JUMP_REACH); // how far over a box's top a piece can hang (Stage 3c: bounce pads)
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		if (ctx.boxes[i].bouncy) {
			hold_top[i] = Math::max(JUMP_REACH, bounce_reach(ctx.boxes[i].bounce) + move::COLLECT_HIGH);
		}
	}
	ctx.stats.platforms = int(plat.size());
	ctx.stats.reachable_platforms = 0;
	for (int i : plat) {
		ctx.stats.reachable_platforms += reach[i];
	}
	// Candy: lift out of solids, check support, drop what cannot be reached. (A box can only hold or carry
	// a piece within its own reach of its centre: an exact pre-test that skips the rest.)
	std::vector<float> reach3(ctx.boxes.size()), reach2(ctx.boxes.size());
	for (size_t i = 0; i < ctx.boxes.size(); ++i) {
		const OBox &b = ctx.boxes[i];
		const float norm = b.xf.basis.get_scale().length() * 1.001f + 1e-4f; // >= the basis' largest stretch
		reach3[i] = (b.half + Vector3(0.3f, 0.3f, 0.3f)).length() * norm + 0.01f;
		// over_box ignores the box's local y: only an upright box bounds it horizontally.
		const Vector3 yc = b.xf.basis.get_column(1);
		const float side = b.bouncy ? 2.65f : 1.05f; // (Stage 3c: over a bounce pad the bucket's magnet reach counts)
		reach2[i] = std::abs(yc.x) + std::abs(yc.z) < 1e-5f ? Vector2(b.half.x + side, b.half.z + side).length() * norm + 0.01f : 1e9f;
	}
	auto near3 = [&](size_t i, const Vector3 &p) { return (p - ctx.boxes[i].xf.origin).length_squared() <= reach3[i] * reach3[i]; };
	auto near2 = [&](size_t i, const Vector3 &p) {
		const Vector3 &o = ctx.boxes[i].xf.origin;
		return (p.x - o.x) * (p.x - o.x) + (p.z - o.z) * (p.z - o.z) <= reach2[i] * reach2[i];
	};
	std::vector<CandySpec> kept;
	for (CandySpec c : ctx.candy) {
		bool moved = false;
		for (size_t i = 0; i < ctx.boxes.size(); ++i) {
			const OBox &b = ctx.boxes[i];
			if (near3(i, c.pos) && inside_box(b, c.pos, 0.25f)) {
				const float lift_to = ext[i].top + 0.8f;
				if (lift_to - c.pos.y < 1.8f) {
					c.pos.y = lift_to;
					moved = true;
				}
			}
		}
		bool inside = false;
		for (size_t i = 0; i < ctx.boxes.size(); ++i) {
			if (near3(i, c.pos) && inside_box(ctx.boxes[i], c.pos, 0.15f)) {
				inside = true;
				break;
			}
		}
		if (inside) {
			++ctx.stats.candy_dropped;
			++ctx.stats.pattern_dropped[c.pattern];
			continue;
		}
		// Reachable with a jump from the ground: on a slope, from the highest ground within a stride (Stage 3b; an
		// arc over a fence on a hillside takes off uphill of the piece above the fence).
		float g_hi = ctx.ground(c.pos.x, c.pos.z);
		if (ctx.terrain) {
			for (const Vector3 &d : { Vector3(1.6f, 0, 0), Vector3(-1.6f, 0, 0), Vector3(0, 0, 1.6f), Vector3(0, 0, -1.6f) }) {
				g_hi = Math::max(g_hi, ctx.ground(c.pos.x + d.x, c.pos.z + d.z));
			}
		}
		bool supported = c.pos.y - g_hi <= JUMP_REACH + 0.2f;
		for (size_t i = 0; i < ctx.boxes.size() && !supported; ++i) {
			if ((!ctx.boxes[i].walkable && !ctx.boxes[i].bouncy) || !reach[i]) {
				continue;
			}
			const float dy = c.pos.y - ext[i].top;
			// Stage 3c: over a bounce pad, as high as its full jump + flap throws the bucket.
			if (dy >= -0.3f && dy <= hold_top[i] && near2(i, c.pos) && over_box(ctx.boxes[i], c.pos, ctx.boxes[i].bouncy && dy > JUMP_REACH ? 2.6f : 1.0f)) {
				supported = true;
			}
		}
		if (!supported) {
			++ctx.stats.candy_dropped;
			++ctx.stats.pattern_dropped[c.pattern];
			if (c.kind == CANDY_GOLDEN) {
				ctx.stats.openness_failures++;
				ctx.stats.notes.push_back(std::string("summit prize unreachable on ") + landmark_name(ctx.lots[c.lot].landmark));
			}
			continue;
		}
		if (moved) {
			++ctx.stats.candy_raised;
			++ctx.stats.pattern_raised[c.pattern];
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
namespace {
// A patch of fallen leaves: any heading, a little size variation.
Basis leaf_basis(Rng &rng) {
	const float s = rng.range(0.55f, 1.0f);
	return Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(s, 1.0f, s));
}
// Grass outside the lots, over the settlement's whole footprint: the street verges, pocket-park
// lawns and the open ground behind and between the rows. Never on asphalt, curbs, sidewalks,
// the side lane or the country roads running on from the street ends, never inside the lots
// (they grow their own) or under hedges and other set pieces, and not on town squares' paving.
// Spline roads (Stage 2a): the same, with RoadNet::zone() deciding the street bands (bulbs, junction
// patches and rounded corners included), the country roads running on past every Port end, and lots
// and blockers looked up through a coarse grid (the footprint is ~4x today's).
void scatter_open_grass_net(GenContext &ctx, Rng &rng) {
	const RoadNet &net = ctx.roads;
	constexpr float ROAD = 3.4f; // half width of the country roads (WorldPlan::ROAD_HALF)
	const unsigned c = pal::RGB[pal::LAWN];
	const Color lawn = Color(float((c >> 16) & 255) / 255.0f, float((c >> 8) & 255) / 255.0f, float(c & 255) / 255.0f).srgb_to_linear();
	const float gx0 = ctx.bound_x0 - 3.0f, gz0 = ctx.bound_z0 - 4.0f;
	constexpr float CELL = 8.0f;
	const int gw = int((ctx.bound_x1 + 3.0f - gx0) / CELL) + 1, gh = int((ctx.bound_z1 + 4.0f - gz0) / CELL) + 1;
	struct Shape {
		Transform3D inv;
		float hx, hz; // lots: half width / depth band; boxes: half extents
		bool lot, cyl;
		float zmin, zmax;
	};
	std::vector<Shape> shapes;
	std::vector<std::vector<int>> grid(size_t(gw) * gh);
	auto bucket = [&](int idx, float x0, float x1, float z0, float z1) {
		for (int gz = Math::max(0, int((z0 - gz0) / CELL)); gz <= Math::min(gh - 1, int((z1 - gz0) / CELL)); ++gz) {
			for (int gxx = Math::max(0, int((x0 - gx0) / CELL)); gxx <= Math::min(gw - 1, int((x1 - gx0) / CELL)); ++gxx) {
				grid[size_t(gz) * gw + gxx].push_back(idx);
			}
		}
	};
	auto add_rect = [&](const Lot &lot) {
		Shape s;
		s.inv = lot.xf.affine_inverse();
		s.hx = lot.width * 0.5f + 0.1f;
		s.zmin = -lot.depth - 0.1f;
		s.zmax = 0.1f;
		s.lot = true;
		s.cyl = false;
		const float r = Vector2(lot.width * 0.5f, lot.depth).length() + 0.2f;
		bucket(int(shapes.size()), lot.xf.origin.x - r, lot.xf.origin.x + r, lot.xf.origin.z - r, lot.xf.origin.z + r);
		shapes.push_back(s);
	};
	for (const Lot &lot : ctx.lots) {
		add_rect(lot);
	}
	for (const Lot &p : ctx.plazas) {
		add_rect(p); // Stage 4c: a town's square and plazas are paved
	}
	const bool town = ctx.kind == SettlementKind::Town;
	for (const OBox &b : ctx.boxes) {
		if (b.lot >= 0 || b.xf.origin.y - b.half.y - ctx.ground(b.xf.origin.x, b.xf.origin.z) >= 0.4f) {
			continue;
		}
		Shape s;
		s.inv = b.xf.affine_inverse();
		s.hx = b.half.x;
		s.hz = b.half.z;
		s.lot = false;
		s.cyl = b.cylinder;
		const float r = Vector2(b.half.x, b.half.z).length() * b.xf.basis.get_scale().x + 0.2f;
		bucket(int(shapes.size()), b.xf.origin.x - r, b.xf.origin.x + r, b.xf.origin.z - r, b.xf.origin.z + r);
		shapes.push_back(s);
	}
	// The country road past each Port end.
	struct Run {
		Vector3 a, d;
	};
	std::vector<Run> runs;
	for (const Road &R : net.roads) {
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) == RoadEnd::Port) {
				const Vector3 e = at_end ? R.pts.back() : R.pts.front();
				Vector3 d = R.end_dir(at_end);
				d.y = 0.0f;
				runs.push_back({ e, d.normalized() });
			}
		}
	}
	const float GAP = 0.9f;
	for (float z = ctx.bound_z0 - 4.0f; z < ctx.bound_z1 + 4.0f; z += GAP) {
		for (float x = ctx.bound_x0 - 3.0f; x < ctx.bound_x1 + 3.0f; x += GAP) {
			const Vector3 p(x + rng.range(-0.4f, 0.4f), 0.0f, z + rng.range(-0.4f, 0.4f));
			bool skip = false;
			for (const Run &run : runs) {
				const Vector3 v = p - run.a;
				const float along = v.x * run.d.x + v.z * run.d.z;
				skip = skip || (along > 0.0f && std::abs(v.x * run.d.z - v.z * run.d.x) < ROAD + 2.4f);
			}
			if (skip) {
				continue;
			}
			const RoadZone zone = net.zone(p);
			if (zone == RoadZone::Asphalt || zone == RoadZone::Curb || zone == RoadZone::Walk || (town && zone == RoadZone::Verge)) {
				continue; // (a town's verge is brick pavers)
			}
			// Grass verge, or open ground 4 cm below the settlement level (Stage 3b: over the graded ground).
			const float y = ctx.ground(p.x, p.z) + (zone == RoadZone::Verge ? 0.05f : -0.03f);
			const int gxx = Math::clamp(int((p.x - gx0) / CELL), 0, gw - 1), gzz = Math::clamp(int((p.z - gz0) / CELL), 0, gh - 1);
			bool blocked = false;
			for (int idx : grid[size_t(gzz) * gw + gxx]) {
				const Shape &s = shapes[idx];
				const Vector3 l = s.inv.xform(p);
				if (s.lot) {
					blocked = std::abs(l.x) < s.hx && l.z < s.zmax && l.z > s.zmin;
				} else {
					blocked = s.cyl ? Vector2(l.x, l.z).length() < s.hx : (std::abs(l.x) < s.hx && std::abs(l.z) < s.hz);
				}
				if (blocked) {
					break;
				}
			}
			if (blocked) {
				continue;
			}
			ScatterItem s;
			s.kind = rng.chance(0.95f) ? 0 : 2;
			s.lot = -1;
			s.color = lawn * rng.range(0.8f, 1.1f);
			const float sc = rng.range(0.75f, 1.3f);
			s.xf = Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(sc, sc * rng.range(0.8f, 1.3f), sc)), Vector3(p.x, y, p.z));
			ctx.scatter.push_back(s);
		}
	}
}

void scatter_open_grass(GenContext &ctx, Rng &rng) {
	if (!ctx.roads.trunk().x_station) { // (Stage 4c: towns too)
		scatter_open_grass_net(ctx, rng);
		return;
	}
	const Road &st = ctx.roads.trunk();
	const bool town = ctx.kind == SettlementKind::Town;
	const float hr = st.half_road, hc = hr + st.curb, hv = hc + st.verge, he = st.edge();
	constexpr float ROAD = 3.4f; // half width of the country roads (WorldPlan::ROAD_HALF)
	std::vector<Transform3D> lot_inv;
	for (const Lot &lot : ctx.lots) {
		lot_inv.push_back(lot.xf.affine_inverse());
	}
	const unsigned c = pal::RGB[pal::LAWN];
	const Color lawn = Color(float((c >> 16) & 255) / 255.0f, float((c >> 8) & 255) / 255.0f, float(c & 255) / 255.0f).srgb_to_linear();
	std::vector<const OBox *> blockers;
	for (const OBox &b : ctx.boxes) {
		if (b.lot < 0 && b.xf.origin.y - b.half.y < 0.4f) {
			blockers.push_back(&b);
		}
	}
	const float GAP = 0.9f;
	for (float z = ctx.bound_z0 - 4.0f; z < ctx.bound_z1 + 4.0f; z += GAP) {
		for (float x = ctx.bound_x0 - 3.0f; x < ctx.bound_x1 + 3.0f; x += GAP) {
			const Vector3 p(x + rng.range(-0.4f, 0.4f), 0.0f, z + rng.range(-0.4f, 0.4f));
			const RoadHit sh = ctx.roads.project(0, p);
			const float lat = std::abs(sh.lateral);
			const bool past_ends = sh.beyond > 0.0f;
			float y = -0.03f; // open ground (its visible surface sits 4 cm below the settlement level)
			if (past_ends) {
				if (lat < ROAD + 2.4f) {
					continue; // the country road carrying on from the street
				}
			} else if (lat < hv) {
				if (lat < hc || town) {
					continue; // asphalt, curb, or a town's brick verge
				}
				y = 0.05f; // grass verge
			} else if (lat < he) {
				continue; // sidewalk
			} else if (town && lat < he + 16.0f) {
				continue; // town squares and shop forecourts are paved
			}
			// The side roads and the road beyond their ends.
			bool skip = false;
			for (const RoadJunction &j : ctx.roads.junctions(0)) {
				const Road &lane = ctx.roads.roads[j.child];
				const RoadHit lh = ctx.roads.project(j.child, p);
				const float along = lh.s_ext - lane.s_begin(), llat = std::abs(lh.lateral);
				const float lhc = lane.half_road + lane.curb, lhv = lhc + lane.verge;
				if (along > 0.0f && llat < lane.edge() && (along < lane.length() + 0.5f || (ctx.lane_open && llat < ROAD + 2.4f))) {
					if (llat < lhc || llat > lhv || town || along > lane.length()) {
						skip = true;
						break;
					}
					y = 0.05f;
				}
			}
			if (skip) {
				continue;
			}
			bool blocked = false;
			for (const Lot &lot : ctx.lots) {
				const Vector3 l = lot_inv[lot.index].xform(p);
				if (std::abs(l.x) < lot.width * 0.5f + 0.1f && l.z < 0.1f && l.z > -lot.depth - 0.1f) {
					blocked = true;
					break;
				}
			}
			for (size_t k = 0; k < blockers.size() && !blocked; ++k) {
				const Vector3 l = blockers[k]->xf.affine_inverse().xform(p);
				const Vector3 &h = blockers[k]->half;
				blocked = blockers[k]->cylinder ? Vector2(l.x, l.z).length() < h.x : (std::abs(l.x) < h.x && std::abs(l.z) < h.z);
			}
			if (blocked) {
				continue;
			}
			ScatterItem s;
			s.kind = rng.chance(0.95f) ? 0 : 2;
			s.lot = -1;
			s.color = lawn * rng.range(0.8f, 1.1f);
			const float sc = rng.range(0.75f, 1.3f);
			s.xf = Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(sc, sc * rng.range(0.8f, 1.3f), sc)), Vector3(p.x, y, p.z));
			ctx.scatter.push_back(s);
		}
	}
}
} // namespace

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
		const int n = int(lot.width * lot.depth * 3.4f);
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
			if (ctx.terrain) {
				s.xf.origin.y += ctx.yard_lift(lot, x, z); // Stage 3b: on the lawn's ground
			}
			ctx.scatter.push_back(s);
		}
	}
	// Leaf litter under trees (biased downwind) and in the street gutters.
	for (const PropPlacement &pp : ctx.props) {
		if (pp.id.rfind("tree_", 0) != 0) {
			continue;
		}
		for (int i = 0; i < 12; ++i) {
			const float a = rng.range(0.0f, Math_TAU), r = 0.4f + 3.0f * std::sqrt(rng.f());
			Vector3 p = pp.xf.origin + Vector3(std::cos(a) * r + 0.8f, 0.0f, std::sin(a) * r);
			p.y = (ctx.terrain ? ctx.ground(p.x, p.z) + 0.01f : 0.01f) + rng.range(0.0f, 0.01f);
			ScatterItem s;
			s.kind = 1;
			s.xf = Transform3D(leaf_basis(rng), p);
			ctx.scatter.push_back(s);
		}
	}
	// Leaves drifted into the gutters, and a looser sprinkle across the road and sidewalks.
	// Spline roads: every road gets its share by length (180 per ~100 m of built road, as today's street).
	std::vector<float> share;
	float built = 0.0f;
	const bool legacy = ctx.roads.trunk().x_station; // the pre-Stage-4c town street
	for (const Road &R : ctx.roads.roads) {
		built += legacy && share.size() > 0 ? 0.0f : Math::max(0.0f, R.lots_s1 - R.lots_s0);
		share.push_back(built);
	}
	const int leaves = legacy ? 180 : int(built * 1.75f);
	for (int i = 0; i < leaves; ++i) {
		int ri = 0;
		if (!legacy) {
			const float pick = rng.range(0.0f, built);
			while (ri + 1 < int(share.size()) && pick >= share[ri]) {
				++ri;
			}
		}
		const Road &st = ctx.roads.roads[ri];
		const float x = rng.range(st.lots_s0, st.lots_s1);
		const float side = rng.sign();
		const float off = i < (legacy ? 110 : leaves * 61 / 100) ? st.half_road - 0.4f - 0.5f * rng.f() * rng.f() : rng.range(0.0f, st.edge());
		Vector3 p = st.at(x) + st.left_at(x) * side * off;
		// Road, curb top, verge or sidewalk surface.
		const float hc = st.half_road + st.curb;
		const float road_y = p.y; // Stage 3b: the graded road's height (0 on a town's street)
		p.y = (off < st.half_road - 0.4f ? 0.015f : (off < st.half_road ? 0.035f : (off < hc ? 0.125f : (off < hc + st.verge ? 0.055f : 0.075f)))) + rng.range(0.0f, 0.01f);
		if (ctx.terrain) {
			p.y += road_y;
		}
		if (!legacy && off >= st.half_road - 0.4f && (st.cut(side > 0.0f ? 1 : -1, x) || (off >= st.half_road && ctx.roads.zone(p) == RoadZone::Asphalt))) {
			continue; // a mouth or a rounded corner: no straight gutter or sidewalk there
		}
		ScatterItem s;
		s.kind = 1;
		s.xf = Transform3D(leaf_basis(rng), p);
		ctx.scatter.push_back(s);
	}
	scatter_open_grass(ctx, rng);
}
} // namespace tot
