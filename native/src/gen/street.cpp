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
	if (lot >= 0 && terrain) {
		center.y += yard_lift(lots[lot], center.x, center.z); // Stage 3b: yard things stand on the ground
	}
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
	if (lot >= 0 && terrain) {
		p.y += yard_lift(lots[lot], p.x, p.z);
	}
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

// ---------------------------------------------------------------- roads
namespace {
// Station st on road r: segment i (pts[i] .. pts[i + 1]) and the fraction t along it.
void locate(const Road &r, float st, int &i, float &t) {
	const int n = int(r.pts.size());
	if (r.step > 0.0f) {
		const float fi = Math::clamp((st - r.s.front()) / r.step, 0.0f, float(r.pts.size() - 1) - 0.001f);
		i = int(fi);
		t = fi - i;
		return;
	}
	if (st <= r.s.front()) {
		i = 0;
		t = 0.0f;
		return;
	}
	if (st >= r.s.back()) {
		i = n - 2;
		t = 1.0f;
		return;
	}
	i = Math::clamp(int(std::upper_bound(r.s.begin(), r.s.end(), st) - r.s.begin()) - 1, 0, n - 2);
	t = (st - r.s[i]) / (r.s[i + 1] - r.s[i]);
}
} // namespace

Vector3 Road::at(float st) const {
	if (straight) {
		return pts.front() + dir * (st - s.front()); // a straight road carries on past its ends
	}
	int i;
	float t;
	locate(*this, st, i, t);
	return pts[i].lerp(pts[i + 1], t);
}

Vector3 Road::left_at(float st) const {
	if (straight) {
		return left.front();
	}
	int i;
	float t;
	locate(*this, st, i, t);
	return left[i].lerp(left[i + 1], t).normalized();
}

Vector3 Road::tangent_at(float st) const {
	if (straight) {
		return dir;
	}
	return (at(st + 1.0f) - at(st - 1.0f)).normalized();
}

Transform3D Road::frontage_frame(float st, int side) const {
	const Vector3 c = at(st);
	const Vector3 l = left_at(st);
	const Vector3 toward = -l * float(side);
	const Vector3 up(0, 1, 0);
	return Transform3D(Basis(up.cross(toward), up, toward), c + l * float(side) * edge());
}

Vector3 Road::end_dir(bool at_end) const {
	const size_t n = pts.size();
	return at_end ? pts[n - 1] - pts[n - 3] : pts[0] - pts[2];
}

RoadHit Road::project(const Vector3 &p) const {
	RoadHit h;
	if (x_station) {
		h.s_ext = p.x;
		h.s = Math::clamp(p.x, s.front(), s.back());
		h.point = at(p.x);
		h.left = left_at(p.x);
		h.tangent = tangent_at(p.x);
	} else if (straight) {
		h.s_ext = s.front() + (p - pts.front()).dot(dir);
		h.s = Math::clamp(h.s_ext, s.front(), s.back());
		h.point = at(h.s);
		h.left = left.front();
		h.tangent = dir;
	} else {
		float best = 1e30f, bt = 0.0f, braw = 0.0f;
		int bi = 0;
		const int nseg = int(pts.size()) - 1;
		// The first segment at the smallest distance (ties keep the lower index, as a plain scan would).
		auto seg = [&](int i) {
			const Vector3 &a = pts[i], &b = pts[i + 1];
			const float abx = b.x - a.x, abz = b.z - a.z;
			const float l2 = abx * abx + abz * abz;
			const float raw = l2 > 1e-9f ? ((p.x - a.x) * abx + (p.z - a.z) * abz) / l2 : 0.0f;
			const float t = Math::clamp(raw, 0.0f, 1.0f);
			const float dx = a.x + abx * t - p.x, dz = a.z + abz * t - p.z;
			const float d2 = dx * dx + dz * dz;
			if (d2 < best || (d2 == best && i < bi)) {
				best = d2;
				bi = i;
				bt = t;
				braw = raw;
			}
		};
		const int nc = int(chunk_box.size() / 4);
		if (nc > 0) {
			// Chunks of 16 segments: scan the nearest box first, then every box that could still hold a
			// closer (or equally close, lower-index) segment.
			float lb[64];
			int first = 0;
			for (int c = 0; c < nc && c < 64; ++c) {
				const float *bx = &chunk_box[c * 4];
				const float dx = Math::max(0.0f, Math::max(bx[0] - p.x, p.x - bx[2]));
				const float dz = Math::max(0.0f, Math::max(bx[1] - p.z, p.z - bx[3]));
				lb[c] = dx * dx + dz * dz - 1e-3f;
				if (lb[c] < lb[first]) {
					first = c;
				}
			}
			for (int i = first * 16; i < Math::min(nseg, first * 16 + 16); ++i) {
				seg(i);
			}
			for (int c = 0; c < nc; ++c) {
				if (c < 64 && (c == first || lb[c] > best)) {
					continue;
				}
				for (int i = c * 16; i < Math::min(nseg, c * 16 + 16); ++i) {
					seg(i);
				}
			}
		} else {
			for (int i = 0; i < nseg; ++i) {
				seg(i);
			}
		}
		const float len = s[bi + 1] - s[bi];
		h.s = s[bi] + len * bt;
		const bool off_start = bi == 0 && braw < 0.0f, off_end = bi + 2 == int(pts.size()) && braw > 1.0f;
		h.s_ext = (off_start || off_end) ? s[bi] + len * braw : h.s;
		h.point = pts[bi].lerp(pts[bi + 1], bt);
		h.left = left[bi].lerp(left[bi + 1], bt).normalized();
		h.tangent = tangent_at(h.s);
	}
	h.beyond = h.s_ext < s.front() ? s.front() - h.s_ext : (h.s_ext > s.back() ? h.s_ext - s.back() : 0.0f);
	h.lateral = (p - h.point).dot(h.left);
	h.dist = Vector2(p.x - h.point.x, p.z - h.point.z).length();
	h.y = h.point.y;
	return h;
}

std::vector<RoadJunction> RoadNet::junctions(int road) const {
	std::vector<RoadJunction> out;
	for (int i = 0; i < int(roads.size()); ++i) {
		if (roads[i].parent == road) {
			out.push_back({ road, i, roads[i].parent_s, roads[i].parent_side });
		}
	}
	std::stable_sort(out.begin(), out.end(), [](const RoadJunction &a, const RoadJunction &b) { return a.s < b.s; });
	return out;
}

RoadHit RoadNet::project(int road, const Vector3 &p) const {
	RoadHit h = roads[road].project(p);
	h.road = road;
	return h;
}

RoadHit RoadNet::nearest(const Vector3 &p) const {
	RoadHit closest, claim;
	for (int r = 0; r < int(roads.size()); ++r) {
		const Road &R = roads[r];
		const RoadHit h = project(r, p);
		const bool past_far_end = h.s_ext > R.s_end();
		const bool in_bulb = R.bulb() && Vector2(p.x - R.bulb_c.x, p.z - R.bulb_c.z).length() < R.bulb_r + R.curb + R.walk;
		if (R.parent >= 0 && (in_bulb || (std::abs(h.lateral) < R.edge() && h.s_ext - R.s_begin() > roads[R.parent].edge() && (!past_far_end || R.end_kind == RoadEnd::Port)))) {
			if (claim.road < 0 || std::abs(h.lateral) < std::abs(claim.lateral)) {
				claim = h;
			}
		} else if (R.parent < 0 && h.dist < closest.dist) {
			closest = h; // outside every branch's corridor a point belongs to the trunk
		}
	}
	return claim.road >= 0 ? claim : closest;
}

void Road::build_index() {
	chunk_box.clear();
	if (x_station || straight) {
		return;
	}
	const int n = int(pts.size());
	for (int c0 = 0; c0 + 1 < n; c0 += 16) {
		const int c1 = Math::min(c0 + 16, n - 1);
		float x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
		for (int i = c0; i <= c1; ++i) {
			x0 = Math::min(x0, pts[i].x);
			z0 = Math::min(z0, pts[i].z);
			x1 = Math::max(x1, pts[i].x);
			z1 = Math::max(z1, pts[i].z);
		}
		chunk_box.insert(chunk_box.end(), { x0, z0, x1, z1 });
	}
}

namespace {
RoadZone band_of(float off, float hr, float curb, float verge, float walk) {
	if (off < hr) {
		return RoadZone::Asphalt;
	}
	if (off < hr + curb) {
		return RoadZone::Curb;
	}
	if (off < hr + curb + verge) {
		return RoadZone::Verge;
	}
	return off < hr + curb + walk ? RoadZone::Walk : RoadZone::Off;
}
} // namespace

RoadZone RoadNet::zone(const Vector3 &p, int *which) const {
	RoadZone best = RoadZone::Off;
	int br = -1;
	auto take = [&](RoadZone z, int r) {
		if (z < best) {
			best = z;
			br = r;
		}
	};
	for (int r = 0; r < int(roads.size()); ++r) {
		const Road &R = roads[r];
		const RoadHit h = R.project(p);
		if (h.beyond <= 0.0f) {
			RoadZone z = band_of(std::abs(h.lateral), R.half_road, R.curb, R.verge, R.walk);
			if (z != RoadZone::Asphalt && z != RoadZone::Off && R.cut(h.lateral >= 0.0f ? 1 : -1, h.s)) {
				z = RoadZone::Off; // a mouth or a bulb shoulder: the fillets decide there
			}
			take(z, r);
		}
		if (R.bulb()) {
			const float dx = p.x - R.bulb_c.x, dz = p.z - R.bulb_c.z;
			const float d = std::sqrt(dx * dx + dz * dz);
			RoadZone z = band_of(d, R.bulb_r, R.curb, R.verge, R.walk);
			if (z != RoadZone::Asphalt && z != RoadZone::Off && d > 1e-3f) {
				const Vector3 back = -R.tangent_at(R.s_end());
				if ((dx * back.x + dz * back.z) / d > std::cos(R.bulb_entry)) {
					z = RoadZone::Off; // the ring is open where the road comes in
				}
			}
			take(z, r);
		}
	}
	for (const RoadFillet &f : fillets) {
		if (!f.in_sector(p)) {
			continue;
		}
		const float d = Vector2(p.x - f.c.x, p.z - f.c.z).length();
		const float delta = f.r - d; // outward from the asphalt edge
		if (delta > -f.half_road) {
			take(band_of(delta + f.half_road, f.half_road, f.curb, f.verge, f.walk), f.road);
		}
	}
	if (which) {
		*which = br;
	}
	return best;
}

void link_lot_neighbours(GenContext &ctx) {
	// Along each side of each road, by station (index order breaks ties).
	std::vector<int> order(ctx.lots.size());
	for (size_t i = 0; i < order.size(); ++i) {
		order[i] = int(i);
	}
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		const Lot &la = ctx.lots[a], &lb = ctx.lots[b];
		if (la.road != lb.road) {
			return la.road < lb.road;
		}
		if (la.side != lb.side) {
			return la.side > lb.side;
		}
		return la.s < lb.s;
	});
	for (Lot &lot : ctx.lots) {
		lot.prev = lot.next = -1;
	}
	for (size_t k = 0; k + 1 < order.size(); ++k) {
		Lot &a = ctx.lots[order[k]], &b = ctx.lots[order[k + 1]];
		if (a.road == b.road && a.side == b.side) {
			a.next = b.index;
			b.prev = a.index;
		}
	}
}

std::vector<std::pair<float, float>> frontage_gaps(const GenContext &ctx, int road, int side, float min_len) {
	const Road &R = ctx.roads.roads[road];
	std::vector<std::pair<float, float>> covered;
	for (const Lot &lot : ctx.lots) {
		if (lot.road == road && lot.side == side) {
			covered.push_back({ lot.s - lot.width * 0.5f, lot.s + lot.width * 0.5f });
		}
	}
	for (const RoadJunction &j : ctx.roads.junctions(road)) {
		if (j.side == side) {
			const float e = ctx.roads.roads[j.child].edge();
			covered.push_back({ j.s - e - 0.6f, j.s + e + 0.6f });
		}
	}
	for (const BandCut &c : R.cuts) {
		if (c.side == side) {
			covered.push_back({ c.s0 - 0.6f, c.s1 + 0.6f }); // a rounded corner (spline roads)
		}
	}
	std::sort(covered.begin(), covered.end());
	std::vector<std::pair<float, float>> gaps;
	float x = R.lots_s0 + 1.0f;
	for (const auto &c : covered) {
		if (c.first - x > min_len) {
			gaps.push_back({ x, c.first });
		}
		x = Math::max(x, c.second);
	}
	if (R.lots_s1 - 1.0f - x > min_len) {
		gaps.push_back({ x, R.lots_s1 - 1.0f });
	}
	return gaps;
}
// ---------------------------------------------------------------- street plan
static float z_of(float x, float amp, float k, float phase) {
	return amp * std::sin(k * x + phase);
}

// Towns (until Stage 4c): road 0 the gently curving main street (x stations, sampled every 1 m along x),
// road 1 the straight side lane leaving it at a T junction.
static void plan_street_legacy(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_STREET);
	const bool town = ctx.kind == SettlementKind::Town;
	Road main;
	main.x_station = true;
	main.step = 1.0f;
	const float length = rng.range(90.0f, 104.0f);
	float amp = rng.range(2.5f, 5.0f);
	const float k = Math_TAU / rng.range(85.0f, 130.0f);
	const float phase = rng.range(0.0f, Math_TAU);
	if (town) {
		// Main Street: straighter, with wide paved sidewalks right up to the storefronts.
		amp = rng.range(0.4f, 1.4f);
		main.walk = 4.4f;
		main.verge = 1.1f;
	}
	// Alone, the street keeps going into the fog beyond the hay walls; in the open world it ends
	// where the country road takes over.
	const float half = length * 0.5f + (ctx.embedded ? 1.0f : 40.0f);
	for (float x = -half; x <= half + 0.01f; x += 1.0f) {
		const float z = z_of(x, amp, k, phase);
		const float dz = amp * k * std::cos(k * x + phase);
		const Vector3 t = Vector3(1, 0, dz).normalized();
		main.pts.push_back(Vector3(x, 0, z));
		main.left.push_back(Vector3(-t.z, 0, t.x));
		main.s.push_back(x);
	}
	main.lots_s0 = -length * 0.5f;
	main.lots_s1 = length * 0.5f;
	const float lane_x = rng.range(-14.0f, 14.0f);
	const int lane_dir = rng.chance(0.5f) ? 1 : -1; // the lane runs toward +z (1) or -z (-1)
	const float lane_len = rng.range(24.0f, 28.0f);
	Road lane;
	lane.straight = true;
	lane.dir = Vector3(0, 0, float(lane_dir));
	lane.half_road = main.half_road;
	lane.curb = main.curb;
	lane.walk = main.walk;
	lane.verge = main.verge;
	const Vector3 o = main.at(lane_x);
	const Vector3 lane_left(-lane.dir.z, 0, lane.dir.x);
	for (float d = 0.0f; d < lane_len; d += 1.0f) {
		lane.pts.push_back(o + lane.dir * d);
		lane.left.push_back(lane_left);
		lane.s.push_back(d);
	}
	lane.pts.push_back(o + lane.dir * lane_len);
	lane.left.push_back(lane_left);
	lane.s.push_back(lane_len);
	lane.parent = 0;
	lane.parent_s = lane_x;
	lane.parent_side = lane_dir;
	lane.start_kind = RoadEnd::Junction;
	lane.end_kind = RoadEnd::Port; // a country road may carry on (ctx.lane_open), else a hedge closes it
	lane.lots_s0 = 0.0f;
	lane.lots_s1 = lane_len;
	ctx.roads.roads.clear();
	ctx.roads.roads.push_back(main);
	ctx.roads.roads.push_back(lane);
	ctx.bound_x0 = -length * 0.5f;
	ctx.bound_x1 = length * 0.5f;
	// Spawn in the intersection, facing down the longer arm of the street.
	const Road &R = ctx.roads.trunk();
	const bool face_pos = lane_x < 0.0f;
	ctx.spawn = R.at(lane_x + (face_pos ? -1.5f : 1.5f)) + Vector3(0, 0.1f, 0);
	ctx.spawn_yaw = face_pos ? Math_PI * 0.5f : -Math_PI * 0.5f; // model front is +Z: (sin yaw, cos yaw)
	// The big moon hangs low over the far end of the street the player starts facing.
	const Vector3 face(std::sin(ctx.spawn_yaw), 0, std::cos(ctx.spawn_yaw));
	// Rotated the same way as the camera's diagonal so it sits in the opening frame.
	const float elev = rng.range(0.15f, 0.2f);
	const Vector3 flat = face.rotated(Vector3(0, 1, 0), rng.range(0.22f, 0.42f));
	ctx.moon_dir = (flat * std::cos(elev) + Vector3(0, std::sin(elev), 0)).normalized();
}

// ---------------------------------------------------------------- neighbourhood roads (Stage 2a)
// A winding main road from the W port to the E port (Catmull-Rom through 4-6 control points, resampled
// evenly by arc length), and 1-3 side roads leaving it at T junctions (60-90 degrees), each 30-70 m from
// the main road's sidewalk to its end, ending in a cul-de-sac bulb or (at most one) running out as the
// N/S port. Layouts whose corridors come too close, bend too tightly or don't fit the region are
// rerolled (each attempt its own stream); a plain layout that always fits is the last resort.
namespace {
// Stage 2b: 2-3x Stage 2a's houses (18.7 lots / hood) on curvier roads: the envelope grew from 152 m.
// WorldPlan::REGION is derived from it (world_plan.h).
constexpr float ROW_REACH = 23.0f; // planning allowance for a row of lots behind a sidewalk
constexpr float BULB_ROW_REACH = 25.0f;
constexpr float JUNCTION_FILLET = 6.0f; // asphalt-edge radius of a T junction's corners
constexpr float BULB_FILLET = 5.0f; // ... of a bulb's shoulders
constexpr float PORT_RUN = 35.0f; // straight run-on past a Port end kept clear of lots and other roads
constexpr int HOOD_ATTEMPTS = 64;
constexpr float MAIN_RADIUS_MIN = 24.0f, BRANCH_RADIUS_MIN = 21.0f; // tightest centreline bends (Stage 2a: 40 / 28)
constexpr float JUNCTION_RADIUS_MIN = 36.0f; // a side road leaves where its parent bends no tighter (Stage 2a: 50)
constexpr int HOOD_LOTS_MIN = 38, HOOD_LOTS_MAX = 56; // every hood's house count lands in this band

Vector3 flat(Vector3 v) {
	v.y = 0.0f;
	return v.length_squared() > 1e-12f ? v.normalized() : Vector3(1, 0, 0);
}

Vector3 catmull(const Vector3 &a, const Vector3 &b, const Vector3 &c, const Vector3 &d, float t) {
	const float t2 = t * t, t3 = t2 * t;
	return ((b * 2.0f) + (-a + c) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (-a + b * 3.0f - c * 3.0f + d) * t3) * 0.5f;
}

// Uniform Catmull-Rom through cp, densely sampled (pre / post: phantom end points).
std::vector<Vector3> spline_points(const std::vector<Vector3> &cp, const Vector3 &pre, const Vector3 &post) {
	std::vector<Vector3> P;
	P.push_back(pre);
	P.insert(P.end(), cp.begin(), cp.end());
	P.push_back(post);
	std::vector<Vector3> out;
	for (size_t i = 1; i + 2 < P.size(); ++i) {
		const int steps = Math::max(4, int(P[i].distance_to(P[i + 1]) * 2.0f));
		for (int k = 0; k < steps; ++k) {
			out.push_back(catmull(P[i - 1], P[i], P[i + 1], P[i + 2], float(k) / steps));
		}
	}
	out.push_back(P[P.size() - 2]);
	return out;
}

// Resamples a polyline evenly by arc length into R (spacing just under 1 m, the last segment too, so
// Road::step lookups hold). Stations start at 0.
void set_road_points(Road &R, const std::vector<Vector3> &dense) {
	std::vector<float> cum(dense.size(), 0.0f);
	for (size_t i = 1; i < dense.size(); ++i) {
		cum[i] = cum[i - 1] + Vector2(dense[i].x - dense[i - 1].x, dense[i].z - dense[i - 1].z).length();
	}
	const float L = cum.back();
	const int n = Math::max(2, int(std::ceil(L - 1e-3f)));
	const float step = L / float(n);
	R.pts.clear();
	R.s.clear();
	R.left.clear();
	size_t j = 0;
	for (int i = 0; i <= n; ++i) {
		const float st = i == n ? L : float(i) * step;
		while (j + 2 < dense.size() && cum[j + 1] < st) {
			++j;
		}
		const float seg = Math::max(cum[j + 1] - cum[j], 1e-6f);
		Vector3 p = dense[j].lerp(dense[j + 1], Math::clamp((st - cum[j]) / seg, 0.0f, 1.0f));
		p.y = 0.0f; // flat ground until Stage 3b
		R.pts.push_back(p);
		R.s.push_back(st);
	}
	R.step = step;
	for (int i = 0; i <= n; ++i) {
		const Vector3 t = flat(R.pts[Math::min(i + 1, n)] - R.pts[Math::max(i - 1, 0)]);
		R.left.push_back(Vector3(-t.z, 0, t.x));
	}
	R.straight = false;
	R.x_station = false;
}

// Tightest centreline radius over [s0, s1] (turning of the tangent over 4 m).
float min_radius(const Road &R, float s0, float s1) {
	float worst = 1e9f;
	s0 = Math::max(s0, R.s_begin() + 3.0f);
	s1 = Math::min(s1, R.s_end() - 3.0f);
	for (float st = s0; st <= s1; st += 1.0f) {
		const float ang = std::acos(Math::clamp(R.tangent_at(st - 2.0f).dot(R.tangent_at(st + 2.0f)), -1.0f, 1.0f));
		if (ang > 1e-4f) {
			worst = Math::min(worst, 4.0f / ang);
		}
	}
	return worst;
}

// Distance from p to the segment a-b (xz).
float seg_dist(const Vector3 &p, const Vector3 &a, const Vector3 &b) {
	const Vector2 ab(b.x - a.x, b.z - a.z), ap(p.x - a.x, p.z - a.z);
	const float t = Math::clamp(ap.dot(ab) / Math::max(ab.length_squared(), 1e-9f), 0.0f, 1.0f);
	return (ap - ab * t).length();
}

float hdist(const Vector3 &a, const Vector3 &b) {
	return Vector2(a.x - b.x, a.z - b.z).length();
}

// The stretch of a road the settlement is built along (for checks and the envelope).
void core_of(const Road &R, float &s0, float &s1) {
	s0 = R.parent < 0 ? R.lots_s0 : R.s_begin();
	s1 = R.lots_s1 > s0 ? R.lots_s1 : R.s_end();
}

struct BranchSpec {
	int side = 1;
	float st = 0.0f;
	bool port = false;
};

// Stage 2b: roads drawn as a turtle path of straight runs and circular arcs (sampled every 0.5 m), so
// the tightest centreline radius is exactly the smallest arc radius. Heading h is measured from +x toward
// +z (a positive turn bends toward the road's left).
struct Turtle {
	std::vector<Vector3> pts;
	Vector3 p;
	float h = 0.0f;
	Vector3 dir() const { return Vector3(std::cos(h), 0.0f, std::sin(h)); }
	void start(const Vector3 &at, float heading) {
		pts.assign(1, at);
		p = at;
		h = heading;
	}
	void straight(float len) {
		const int n = Math::max(1, int(std::ceil(len / 0.5f)));
		const Vector3 d = dir() * (len / float(n));
		for (int k = 0; k < n; ++k) {
			p += d;
			pts.push_back(p);
		}
	}
	void arc(float radius, float angle) {
		const float len = radius * std::abs(angle);
		const int n = Math::max(1, int(std::ceil(len / 0.5f)));
		const float dh = angle / float(n);
		for (int k = 0; k < n; ++k) {
			// The chord of each 0.5 m step, so the samples lie on the circle.
			const float chord = 2.0f * radius * std::sin(std::abs(dh) * 0.5f);
			h += dh * 0.5f;
			p += dir() * chord;
			h += dh * 0.5f;
			pts.push_back(p);
		}
	}
};

float wrap_angle(float a) {
	while (a > Math_PI) {
		a -= Math_TAU;
	}
	while (a < -Math_PI) {
		a += Math_TAU;
	}
	return a;
}

// A side road's bends (drawn once per branch so the shorter retries keep its character).
struct BranchBends {
	int n = 1;
	float radius[3] = { 30.0f, 30.0f, 30.0f };
	float turn[3] = { 0.0f, 0.0f, 0.0f }; // radians, signed; scaled down when the road is short
	float gap[3] = { 6.0f, 6.0f, 6.0f }; // straight run after each bend
};

Road build_branch(const Road &M, int parent, const BranchSpec &sp, float len, float theta, float lean, const BranchBends &bends, float run) {
	const Vector3 J = M.at(sp.st);
	const Vector3 t = M.tangent_at(sp.st), l = M.left_at(sp.st);
	const Vector3 d0 = flat(t * (std::cos(theta) * lean) + l * (float(sp.side) * std::sin(theta)));
	// Straight for 14 m off the junction and into the end (the fillets and the bulb sit on straight road);
	// curved in between: S-bends (a cul-de-sac) or bends that bring a port road round to due north / south.
	const float total = M.edge() + len;
	const float middle = Math::max(0.0f, total - 28.0f);
	Turtle T;
	T.start(J, std::atan2(d0.z, d0.x));
	T.straight(14.0f);
	float used = 0.0f;
	if (sp.port) {
		// One or two bends ending due north or south; whatever length is left goes into the end straight.
		const float want = float(sp.side) * float(Math_PI) * 0.5f;
		if (bends.n > 1 && middle > 30.0f) {
			const float a = bends.turn[0] * 0.6f;
			T.arc(bends.radius[0], a);
			T.straight(bends.gap[0]);
			used += bends.radius[0] * std::abs(a) + bends.gap[0];
		}
		const float a = wrap_angle(want - T.h);
		T.arc(bends.radius[1], a);
		used += bends.radius[1] * std::abs(a);
		T.straight(Math::max(0.0f, middle - used) + 14.0f);
	} else {
		float need = 0.0f;
		for (int k = 0; k < bends.n; ++k) {
			need += bends.radius[k] * std::abs(bends.turn[k]) + bends.gap[k];
		}
		const float squeeze = need > middle && need > 1e-3f ? middle / need : 1.0f;
		for (int k = 0; k < bends.n; ++k) {
			T.arc(bends.radius[k], bends.turn[k] * squeeze);
			T.straight(Math::max(0.5f, bends.gap[k] * squeeze));
			used += (bends.radius[k] * std::abs(bends.turn[k]) + bends.gap[k]) * squeeze;
		}
		T.straight(Math::max(0.0f, middle - used) + 14.0f);
	}
	std::vector<Vector3> dense = T.pts;
	if (run > 0.0f) {
		dense.push_back(dense.back() + T.dir() * run);
	}
	Road B;
	set_road_points(B, dense);
	B.parent = parent;
	B.parent_s = sp.st;
	B.parent_side = sp.side;
	B.start_kind = RoadEnd::Junction;
	B.end_kind = sp.port ? RoadEnd::Port : RoadEnd::Bulb;
	B.lots_s0 = M.edge();
	B.lots_s1 = B.s_end() - run;
	if (!sp.port) {
		B.bulb_c = B.pts.back();
	}
	return B;
}

// Corridors far enough apart, bends gentle enough, ends heading the right way, and the whole thing in
// the envelope; then the layout is moved so the envelope is centred on the origin.
// Why attempts were rerolled (reported when a layout falls back): a counter array per planning run (no
// shared state, so areas can be planned on worker threads).
#define HOOD_REJECT(k) do { ++rej[k]; return false; } while (0)
// (Stage 4c: towns pass their own allowances: how far from a junction a side road and its parent may still
// run close (a Y fork's arms part slowly), and the depth of a row of shops for the envelope.)
bool hood_checks(RoadNet &net, int *rej, bool centre, float related_skip = 45.0f, float row_reach = ROW_REACH) {
	const std::vector<Road> &roads = net.roads;
	const Road &M = roads[0];
	if (min_radius(M, M.lots_s0, M.lots_s1) < MAIN_RADIUS_MIN) {
		HOOD_REJECT(1);
	}
	if (flat(M.end_dir(false)).x > -0.87f || flat(M.end_dir(true)).x < 0.87f) {
		HOOD_REJECT(2); // the W / E ports must read as W and E (WorldPlan sorts ports by their direction)
	}
	for (size_t r = 1; r < roads.size(); ++r) {
		const Road &B = roads[r];
		if (min_radius(B, 2.0f, B.lots_s1 - 2.0f) < BRANCH_RADIUS_MIN) {
			HOOD_REJECT(3);
		}
		if (B.end_kind == RoadEnd::Port && flat(B.end_dir(true)).z * float(B.parent_side) < 0.9f) {
			HOOD_REJECT(4);
		}
	}
	struct Sample {
		Vector3 p;
		float s;
	};
	std::vector<std::vector<Sample>> smp(roads.size());
	for (size_t r = 0; r < roads.size(); ++r) {
		float s0, s1;
		core_of(roads[r], s0, s1);
		for (float st = s0; st <= s1 + 0.01f; st += 2.0f) {
			smp[r].push_back({ roads[r].at(st), st });
		}
	}
	// Corridors: a side road and its parent may meet near their junction; elsewhere, room for lots.
	for (size_t a = 0; a < roads.size(); ++a) {
		for (size_t b = a + 1; b < roads.size(); ++b) {
			const Road &Ra = roads[a], &Rb = roads[b];
			const bool related = Rb.parent == int(a);
			const float need = Ra.edge() + Rb.edge() + (related ? 15.0f : 24.0f);
			// Stage 4c: the two arms of a + crossing (siblings leaving one station on opposite sides) part straight
			// away from each other; near the crossing they are the same street.
			const bool crossing = Ra.parent >= 0 && Ra.parent == Rb.parent && std::abs(Ra.parent_s - Rb.parent_s) < 1.0f && Ra.parent_side != Rb.parent_side;
			for (const Sample &pa : smp[a]) {
				for (const Sample &pb : smp[b]) {
					if (related && pb.s < related_skip && std::abs(pa.s - Rb.parent_s) < related_skip) {
						continue;
					}
					if (crossing && (pa.s < 30.0f || pb.s < 30.0f)) {
						continue;
					}
					if (!related && (pa.s < roads[0].edge() + 8.0f || pb.s < roads[0].edge() + 8.0f)) {
						continue; // two side roads' mouths on the main road (their own corridors start past it)
					}
					if (hdist(pa.p, pb.p) < need) {
						HOOD_REJECT(5);
					}
				}
			}
		}
		// A road never doubles back close to itself.
		for (size_t i = 0; i < smp[a].size(); ++i) {
			for (size_t k = i + 1; k < smp[a].size(); ++k) {
				if (smp[a][k].s - smp[a][i].s > 60.0f && hdist(smp[a][i].p, smp[a][k].p) < roads[a].edge() * 2.0f + 24.0f) {
					HOOD_REJECT(6);
				}
			}
		}
	}
	// Bulbs: room for their ring of lots.
	for (size_t b = 1; b < roads.size(); ++b) {
		const Road &B = roads[b];
		if (!B.bulb()) {
			continue;
		}
		const float re = B.bulb_r + B.curb + B.walk;
		for (size_t a = 0; a < roads.size(); ++a) {
			if (a == b) {
				continue;
			}
			for (const Sample &pa : smp[a]) {
				if (hdist(pa.p, B.bulb_c) < re + roads[a].edge() + 16.0f) { // a ring lot at its shallowest (15 m)
					HOOD_REJECT(7);
				}
			}
			if (roads[a].bulb() && a > b && hdist(roads[a].bulb_c, B.bulb_c) < re + roads[a].bulb_r + roads[a].curb + roads[a].walk + 26.0f) {
				HOOD_REJECT(8);
			}
		}
	}
	// The straight run-on past each Port end (the country road) clear of other corridors and bulbs.
	for (size_t r = 0; r < roads.size(); ++r) {
		const Road &R = roads[r];
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) != RoadEnd::Port) {
				continue;
			}
			const Vector3 E = at_end ? R.pts.back() : R.pts.front();
			const Vector3 F = E + flat(R.end_dir(at_end)) * PORT_RUN;
			for (size_t a = 0; a < roads.size(); ++a) {
				if (a == r) {
					continue;
				}
				for (const Sample &pa : smp[a]) {
					if (seg_dist(pa.p, E, F) < roads[a].edge() + R.edge() + 4.0f) {
						HOOD_REJECT(9);
					}
				}
				if (roads[a].bulb() && seg_dist(roads[a].bulb_c, E, F) < roads[a].bulb_r + 3.0f + R.edge() + 18.0f) {
					HOOD_REJECT(10);
				}
			}
		}
	}
	// Envelope: corridors plus a row of lots each side, bulbs plus their ring of lots.
	float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
	auto grow = [&](const Vector3 &p, float r) {
		x0 = Math::min(x0, p.x - r);
		x1 = Math::max(x1, p.x + r);
		z0 = Math::min(z0, p.z - r);
		z1 = Math::max(z1, p.z + r);
	};
	for (size_t r = 0; r < roads.size(); ++r) {
		for (const Sample &pa : smp[r]) {
			const Vector3 l = roads[r].left_at(pa.s) * (roads[r].edge() + row_reach);
			grow(pa.p + l, 0.0f);
			grow(pa.p - l, 0.0f);
		}
		if (roads[r].bulb()) {
			grow(roads[r].bulb_c, roads[r].bulb_r + roads[r].curb + roads[r].walk + BULB_ROW_REACH);
		}
	}
	if (x1 - x0 > HOOD_ENVELOPE || z1 - z0 > HOOD_ENVELOPE) {
		HOOD_REJECT(11);
	}
	if (!centre) {
		return true;
	}
	const Vector3 shift(-(x0 + x1) * 0.5f, 0.0f, -(z0 + z1) * 0.5f);
	for (Road &R : net.roads) {
		for (Vector3 &p : R.pts) {
			p += shift;
		}
		if (R.bulb()) {
			R.bulb_c += shift;
		}
	}
	return true;
}

// Stage 2b: the main road is a turtle path, W to E: a straight off the W port, then S-bends (arcs of
// 26-46 m centreline radius swinging the heading up to ~55 degrees either side of east, with straight runs
// between them), then back to east and a straight into the E port. 2-5 side roads, each curved (S-bends or
// bends round to due N / S for the port road), 45-95 m long.
bool hood_attempt(Rng &rng, bool embedded, int port_side, RoadNet &net, int *rej) {
	net.roads.clear();
	net.fillets.clear();
	const float ext = embedded ? 1.0f : 40.0f; // the asphalt past the last lots (standalone: into the fog)
	{
		Road M;
		const float span = HOOD_ENVELOPE * rng.range(0.78f, 0.93f); // x reach of the built stretch
		Turtle T;
		T.start(Vector3(), rng.range(-0.25f, 0.25f));
		T.straight(ext + rng.range(12.0f, 20.0f));
		float sign = rng.chance(0.5f) ? 1.0f : -1.0f;
		for (int bend = 0; bend < 12 && T.p.x < span - 46.0f; ++bend) {
			const float radius = rng.range(27.0f, 48.0f);
			// Mostly S-bends (the swing flips side); now and then a longer sweep the same way.
			const float target = sign * rng.range(0.3f, 0.95f);
			float turn = target - T.h;
			if (std::abs(turn) < 0.25f) {
				turn = sign * 0.25f;
			}
			T.arc(radius, turn);
			T.straight(rng.range(12.0f, 30.0f));
			if (rng.chance(0.8f)) {
				sign = -sign;
			}
		}
		T.arc(rng.range(28.0f, 44.0f), rng.range(-0.12f, 0.12f) - T.h);
		T.straight(rng.range(12.0f, 20.0f) + ext);
		set_road_points(M, T.pts);
		M.lots_s0 = ext;
		M.lots_s1 = M.s_end() - ext;
		M.start_kind = M.end_kind = RoadEnd::Port;
		net.roads.push_back(M);
	}
	const Road M = net.roads[0]; // a copy: net.roads grows below
	const float nroll = rng.f();
	int nb = nroll < 0.3f ? 3 : (nroll < 0.7f ? 4 : 5);
	const int port_k = port_side != 0 ? rng.irange(0, nb - 1) : -1;
	std::vector<BranchSpec> specs;
	const int first_side = rng.chance(0.5f) ? 1 : -1;
	for (int k = 0; k < nb; ++k) {
		BranchSpec sp;
		sp.port = k == port_k;
		// Sides alternate from a random start (a cul-de-sac or two on each side of the main road).
		sp.side = sp.port ? port_side : (rng.chance(0.75f) ? ((k & 1) ? first_side : -first_side) : (rng.chance(0.5f) ? 1 : -1));
		// Every station clear of the other junctions and on a gentle stretch; one of them at random.
		std::vector<float> ok;
		for (float st = M.lots_s0 + 24.0f; st <= M.lots_s1 - 24.0f; st += 1.0f) {
			bool clear = true;
			for (const BranchSpec &o : specs) {
				clear = clear && std::abs(o.st - st) >= (o.side == sp.side ? 42.0f : 20.0f);
			}
			if (clear && min_radius(M, st - 7.0f, st + 7.0f) >= JUNCTION_RADIUS_MIN) {
				ok.push_back(st);
			}
		}
		if (ok.empty()) {
			if (sp.port || (k + 1 == nb && specs.empty())) {
				HOOD_REJECT(12);
			}
			continue; // one side road fewer
		}
		sp.st = ok[rng.irange(0, int(ok.size()) - 1)];
		specs.push_back(sp);
	}
	int culs = 0;
	for (const BranchSpec &s : specs) {
		culs += s.port ? 0 : 1;
	}
	if (culs < 1 || int(specs.size()) < 2) {
		HOOD_REJECT(13); // always at least one cul-de-sac, and two side roads
	}
	// Cul-de-sacs first; the port road last, long enough to reach past everything on its side.
	std::vector<int> order;
	for (int k = 0; k < int(specs.size()); ++k) {
		if (!specs[k].port) {
			order.push_back(k);
		}
	}
	for (int k = 0; k < int(specs.size()); ++k) {
		if (specs[k].port) {
			order.push_back(k);
		}
	}
	struct Shape {
		float theta, lean, len, bulb_r;
		BranchBends bends;
		// Stage 2b: a second cul-de-sac off a long one (where the house count comes from in a big hood).
		bool sub = false;
		int sub_side = 1;
		float sub_u = 0.5f, sub_theta = 1.4f, sub_lean = 1.0f, sub_len = 45.0f, sub_bulb_r = 10.0f;
		BranchBends sub_bends;
	};
	std::vector<Shape> shapes(specs.size());
	for (int k : order) {
		Shape &h = shapes[k];
		h.theta = Math::deg_to_rad(rng.range(specs[k].port ? 70.0f : 60.0f, 90.0f));
		h.lean = rng.chance(0.5f) ? 1.0f : -1.0f;
		// Side roads sharing a side lean away from their nearest neighbour there (their bulbs need room).
		float nearest = 1e9f;
		for (const BranchSpec &o : specs) {
			if (&o != &specs[k] && o.side == specs[k].side && !specs[k].port && std::abs(o.st - specs[k].st) < nearest) {
				nearest = std::abs(o.st - specs[k].st);
				h.lean = specs[k].st > o.st ? 1.0f : -1.0f;
			}
		}
		h.len = rng.range(55.0f, 110.0f);
		h.bulb_r = rng.range(9.0f, 11.0f);
		BranchBends &b = h.bends;
		b.n = rng.irange(1, 3);
		float sg = rng.chance(0.5f) ? 1.0f : -1.0f;
		for (int i = 0; i < 3; ++i) {
			b.radius[i] = rng.range(24.0f, 40.0f);
			b.turn[i] = sg * rng.range(0.35f, 0.95f) * (i == 0 && b.n > 1 ? 0.6f : 1.0f);
			b.gap[i] = rng.range(4.0f, 14.0f);
			sg = rng.chance(0.75f) ? -sg : sg; // S-bends
		}
		if (!specs[k].port) {
			h.sub = h.len > 72.0f && rng.chance(0.6f);
			h.sub_side = rng.chance(0.5f) ? 1 : -1;
			h.sub_u = rng.range(0.38f, 0.62f);
			h.sub_theta = Math::deg_to_rad(rng.range(65.0f, 90.0f));
			h.sub_lean = rng.chance(0.5f) ? 1.0f : -1.0f;
			h.sub_len = rng.range(36.0f, 60.0f);
			h.sub_bulb_r = rng.range(9.0f, 10.5f);
			h.sub_bends.n = 1;
			h.sub_bends.radius[0] = rng.range(24.0f, 36.0f);
			h.sub_bends.turn[0] = (rng.chance(0.5f) ? 1.0f : -1.0f) * rng.range(0.25f, 0.7f);
			h.sub_bends.gap[0] = rng.range(3.0f, 8.0f);
		}
	}
	// Greedy (Stage 2b): each cul-de-sac in turn at its drawn length, shorter if it doesn't fit (never under
	// 35 m), left out if it still doesn't; its second cul-de-sac likewise; the port road last (it must fit).
	net.roads.resize(1);
	if (!hood_checks(net, rej, false)) {
		return false; // the main road alone already fails
	}
	auto try_add = [&](const Road &B) {
		net.roads.push_back(B);
		if (hood_checks(net, rej, false)) {
			return true;
		}
		net.roads.pop_back();
		return false;
	};
	int culs_built = 0;
	for (int k : order) {
		const BranchSpec &sp = specs[k];
		const Shape &h = shapes[k];
		if (sp.port) {
			continue;
		}
		int bi = -1;
		for (float scale : { 1.0f, 0.78f, 0.6f }) {
			Road B = build_branch(M, 0, sp, Math::max(35.0f, h.len * scale), h.theta, h.lean, h.bends, 0.0f);
			B.bulb_r = h.bulb_r;
			if (try_add(B)) {
				bi = int(net.roads.size()) - 1;
				++culs_built;
				break;
			}
		}
		if (bi < 0 || !h.sub) {
			continue;
		}
		const Road P = net.roads[bi];
		BranchSpec ss;
		ss.side = h.sub_side;
		std::vector<float> ok;
		for (float st = P.lots_s0 + 22.0f; st <= P.lots_s1 - 28.0f; st += 1.0f) {
			if (min_radius(P, st - 6.0f, st + 6.0f) >= JUNCTION_RADIUS_MIN) {
				ok.push_back(st);
			}
		}
		if (ok.empty()) {
			continue; // too short after the squeeze, or all bends
		}
		ss.st = ok[Math::clamp(int(h.sub_u * float(ok.size())), 0, int(ok.size()) - 1)];
		for (float scale : { 1.0f, 0.75f }) {
			Road S = build_branch(P, bi, ss, Math::max(30.0f, h.sub_len * scale), h.sub_theta, h.sub_lean, h.sub_bends, 0.0f);
			S.bulb_r = h.sub_bulb_r;
			if (try_add(S)) {
				break;
			}
		}
	}
	if (culs_built < 1) {
		HOOD_REJECT(13);
	}
	for (int k : order) {
		const BranchSpec &sp = specs[k];
		const Shape &h = shapes[k];
		if (!sp.port) {
			continue;
		}
		// Long enough to reach past everything else on its side (extra length runs due N / S).
		const float side = float(sp.side);
		float far = -1e9f;
		for (const Road &R : net.roads) {
			float s0, s1;
			core_of(R, s0, s1);
			for (float st = s0; st <= s1; st += 2.0f) {
				far = Math::max(far, R.at(st).z * side + R.edge() + ROW_REACH);
			}
			if (R.bulb()) {
				far = Math::max(far, R.bulb_c.z * side + R.bulb_r + R.curb + R.walk + BULB_ROW_REACH);
			}
		}
		const Road probe = build_branch(M, 0, sp, 60.0f, h.theta, h.lean, h.bends, ext);
		const float reach = probe.at(probe.lots_s1).z * side;
		const float len = Math::clamp(60.0f + (far - 6.0f - reach), 45.0f, 120.0f);
		if (!try_add(build_branch(M, 0, sp, len, h.theta, h.lean, h.bends, ext))) {
			return false;
		}
	}
	if (int(net.roads.size()) < 3) {
		HOOD_REJECT(13); // at least two side roads
	}
	return hood_checks(net, rej, true);
}

// The last resort: a straight main road with square side roads (always passes the checks).
void hood_fallback(bool embedded, int port_side, RoadNet &net) {
	net.roads.clear();
	net.fillets.clear();
	const float ext = embedded ? 1.0f : 40.0f;
	Road M;
	set_road_points(M, { Vector3(-100.0f - ext, 0, 0), Vector3(100.0f + ext, 0, 0) });
	M.lots_s0 = ext;
	M.lots_s1 = M.s_end() - ext;
	net.roads.push_back(M);
	BranchBends straight_road;
	straight_road.n = 0;
	auto add = [&](float st, int side, bool port, float len) {
		BranchSpec sp;
		sp.side = side;
		sp.st = st;
		sp.port = port;
		Road B = build_branch(net.roads[0], 0, sp, len, Math_PI * 0.5f, 1.0f, straight_road, port ? ext : 0.0f);
		if (!port) {
			B.bulb_r = 10.0f;
		}
		net.roads.push_back(B);
	};
	const int s0 = port_side != 0 ? port_side : 1;
	add(M.lots_s0 + 40.0f, s0, port_side != 0, 70.0f);
	add(M.lots_s0 + 80.0f, -s0, false, 60.0f);
	add(M.lots_s0 + 125.0f, s0, false, 60.0f);
	add(M.lots_s0 + 165.0f, -s0, false, 60.0f);
	// Centre it like the checks would.
	float z0 = 1e9f, z1 = -1e9f;
	for (const Road &R : net.roads) {
		for (const Vector3 &p : R.pts) {
			z0 = Math::min(z0, p.z);
			z1 = Math::max(z1, p.z);
		}
	}
	const Vector3 shift(0, 0, -(z0 + z1) * 0.5f);
	for (Road &R : net.roads) {
		for (Vector3 &p : R.pts) {
			p += shift;
		}
		if (R.bulb()) {
			R.bulb_c += shift;
		}
	}
}

// Junction corners and bulb shoulders: fillets, the band cuts they replace, where each side road's own
// asphalt and built stretch start, and the project() index.
void finish_hood_net(RoadNet &net) {
	for (Road &R : net.roads) {
		R.cuts.clear();
		R.build_index();
	}
	net.fillets.clear();
	for (int b = 1; b < int(net.roads.size()); ++b) {
		Road &B = net.roads[b];
		Road &P = net.roads[B.parent];
		const int sd = B.parent_side;
		float s_tan[2], t_tan[2];
		for (int qi = 0; qi < 2; ++qi) {
			const float q = qi == 0 ? 1.0f : -1.0f;
			// The corner: where this edge of the side road leaves the parent's asphalt edge.
			auto f = [&](float t) { return float(sd) * P.project(B.point(t, q * B.half_road)).lateral - P.half_road; };
			float lo = 0.0f, hi = 40.0f;
			for (int it = 0; it < 40; ++it) {
				const float m = (lo + hi) * 0.5f;
				(f(m) < 0.0f ? lo : hi) = m;
			}
			const float tx = (lo + hi) * 0.5f;
			const Vector3 X0 = B.point(tx, q * B.half_road);
			const float sx = P.project(X0).s;
			const float sigma = (B.left_at(0.0f) * q).dot(P.tangent_at(B.parent_s)) >= 0.0f ? 1.0f : -1.0f;
			// The two edges leaving the corner, as chords over the fillet's reach (both roads may curve a little).
			Vector3 rp = flat(P.tangent_at(sx) * sigma), rb = flat(B.tangent_at(tx));
			float phi = 0.0f, L = 0.0f;
			for (int it = 0; it < 3; ++it) {
				phi = std::acos(Math::clamp(rp.dot(rb), -1.0f, 1.0f));
				L = JUNCTION_FILLET / std::tan(phi * 0.5f);
				rp = flat(P.point(sx + sigma * L, float(sd) * P.half_road) - X0);
				rb = flat(B.point(tx + L, q * B.half_road) - X0);
			}
			phi = std::acos(Math::clamp(rp.dot(rb), -1.0f, 1.0f));
			L = JUNCTION_FILLET / std::tan(phi * 0.5f);
			RoadFillet F;
			F.c = X0 + flat(rp + rb) * (JUNCTION_FILLET / std::sin(phi * 0.5f));
			F.r = JUNCTION_FILLET;
			const Vector3 Tp = X0 + rp * L, Tb = X0 + rb * L;
			F.u0 = flat(Tp - F.c);
			F.u1 = flat(Tb - F.c);
			s_tan[qi] = P.project(Tp).s;
			t_tan[qi] = B.project(Tb).s;
			F.p0 = P.point(s_tan[qi], float(sd) * P.half_road);
			F.p1 = B.point(t_tan[qi], q * B.half_road);
			F.road = b;
			F.road0 = B.parent;
			F.s0 = s_tan[qi];
			F.lat0 = float(sd);
			F.road1 = b;
			F.s1 = t_tan[qi];
			F.lat1 = q;
			F.half_road = B.half_road;
			F.curb = B.curb;
			F.verge = B.verge;
			F.walk = B.walk;
			net.fillets.push_back(F);
		}
		P.cuts.push_back({ sd, Math::min(s_tan[0], s_tan[1]), Math::max(s_tan[0], s_tan[1]) });
		B.cuts.push_back({ 1, -1.0f, t_tan[0] });
		B.cuts.push_back({ -1, -1.0f, t_tan[1] });
		B.asphalt_s = Math::max(t_tan[0], t_tan[1]);
		B.lots_s0 = B.asphalt_s;
		if (B.bulb()) {
			const float hr = B.half_road, R = B.bulb_r, rf = BULB_FILLET;
			const float af = std::sqrt((R + rf) * (R + rf) - (hr + rf) * (hr + rf));
			const Vector3 te = B.tangent_at(B.s_end()), le = B.left_at(B.s_end()), Cb = B.bulb_c;
			B.bulb_s = B.s_end() - af;
			B.bulb_entry = std::atan2(hr + rf, af);
			for (float q : { 1.0f, -1.0f }) {
				RoadFillet F;
				F.c = Cb - te * af + le * (q * (hr + rf));
				const Vector3 Tl = Cb - te * af + le * (q * hr);
				const Vector3 Tc = Cb + flat(F.c - Cb) * R;
				F.r = rf;
				F.u0 = flat(Tl - F.c);
				F.u1 = flat(Tc - F.c);
				F.p0 = B.point(B.bulb_s, q * hr);
				F.p1 = Tc;
				F.road = b;
				F.bulb = true;
				F.road0 = b;
				F.s0 = B.s_end() - af;
				F.lat0 = q;
				F.half_road = B.half_road;
				F.curb = B.curb;
				F.verge = B.verge;
				F.walk = B.walk;
				net.fillets.push_back(F);
				B.cuts.push_back({ int(q), B.bulb_s, B.s_end() + 1.0f });
			}
			B.lots_s1 = B.bulb_s;
		}
	}
}

void plan_hood_roads(GenContext &ctx) {
	int port_side = ctx.port_side;
	if (port_side == GenContext::PORT_FROM_SEED) {
		Rng prng = ctx.rng(STAGE_STREET, 0x9047);
		const float r = prng.f();
		port_side = r < 0.4f ? 0 : (r < 0.7f ? 1 : -1);
	}
	ctx.port_side = port_side;
	bool ok = false;
	int attempt = 0;
	int rej[16] = {};
	while (!ok && attempt < HOOD_ATTEMPTS) {
		Rng rng = ctx.rng(STAGE_STREET, uint64_t(1 + attempt));
		ok = hood_attempt(rng, ctx.embedded, port_side, ctx.roads, rej);
		++attempt;
		if (ok) {
			// Stage 2b: the layout stays only if its house count lands in the band.
			finish_hood_net(ctx.roads);
			plan_hood_lots(ctx);
			const int n = int(ctx.lots.size());
			if (n < HOOD_LOTS_MIN || n > HOOD_LOTS_MAX) {
				++rej[n < HOOD_LOTS_MIN ? 14 : 15];
				ok = false;
			}
		}
	}
	if (!ok) {
		hood_fallback(ctx.embedded, port_side, ctx.roads);
		std::string why = "road layout fell back to the plain plan:";
		for (int k = 1; k < 16; ++k) {
			if (rej[k]) {
				why += " r" + std::to_string(k) + "=" + std::to_string(rej[k]);
			}
		}
		ctx.stats.notes.push_back(why);
		finish_hood_net(ctx.roads);
		plan_hood_lots(ctx);
	}
	ctx.hood_lots_ready = true;
	ctx.stats.rerolls = ok ? attempt : -1;
	ctx.stats.lot_rejects = rej[14] + rej[15];
	for (int k = 0; k < 16; ++k) {
		ctx.stats.road_rejects[k] = rej[k];
	}
	// Spawn on the main road at the junction nearest its middle, facing down the longer arm.
	const Road &M = ctx.roads.trunk();
	const float mid = (M.lots_s0 + M.lots_s1) * 0.5f;
	float js = mid, bd = 1e9f;
	for (const RoadJunction &j : ctx.roads.junctions(0)) {
		if (std::abs(j.s - mid) < bd) {
			bd = std::abs(j.s - mid);
			js = j.s;
		}
	}
	const bool forward = M.lots_s1 - js >= js - M.lots_s0;
	ctx.spawn = M.at(js + (forward ? -1.5f : 1.5f)) + Vector3(0, 0.1f, 0);
	const Vector3 t = M.tangent_at(js) * (forward ? 1.0f : -1.0f);
	ctx.spawn_yaw = std::atan2(t.x, t.z); // model front is +Z: (sin yaw, cos yaw)
	Rng rng = ctx.rng(STAGE_STREET);
	const Vector3 face(std::sin(ctx.spawn_yaw), 0, std::cos(ctx.spawn_yaw));
	const float elev = rng.range(0.15f, 0.2f);
	const Vector3 flat_dir = face.rotated(Vector3(0, 1, 0), rng.range(0.22f, 0.42f));
	ctx.moon_dir = (flat_dir * std::cos(elev) + Vector3(0, std::sin(elev), 0)).normalized();
}

// ---------------------------------------------------------------- town roads (Stage 4c)
// Per seed a template (cross streets, a grid, a bowed high street, a Y fork) and a size class (main street x
// reach 100-125 / 130-165 / 170-205 m); inside it everything comes from the seed: the bends, where the side
// streets leave, how long they run. Side streets end in a hedge under a lantern arch (DeadEnd), except the
// N/S port roads WorldPlan asked for (GenContext::TOWN_PORTS), which bend round to due north / south. The
// same turtle roads, corridor checks, fillets and lot tests as the neighbourhoods. The road net is a tree,
// so a "grid" is a straight main street crossed square-on every block (its side streets run out into the
// country rather than meeting a back street).
namespace {
constexpr int TOWN_ATTEMPTS = 48;
constexpr float TOWN_ROW_REACH = 19.0f; // a row of shop lots (13-17.5 m deep) behind its sidewalk, for the envelope

void town_dims(Road &R) {
	R.walk = 4.4f; // wide paved sidewalks up to the shop fronts: a brick-paver verge, then concrete
	R.verge = 1.1f;
}

struct TownArm {
	BranchSpec sp;
	float theta = float(Math_PI) * 0.5f, lean = 1.0f, len = 40.0f;
	BranchBends bends;
};

bool town_attempt(Rng &rng, bool embedded, TownTemplate tpl, TownSize size, int ports, RoadNet &net, int *rej) {
	net.roads.clear();
	net.fillets.clear();
	const float ext = embedded ? 1.0f : 40.0f;
	float span = size == TownSize::Small ? rng.range(82.0f, 104.0f) : (size == TownSize::Medium ? rng.range(115.0f, 150.0f) : rng.range(160.0f, 200.0f));
	const float arm_lo = size == TownSize::Small ? 22.0f : (size == TownSize::Medium ? 30.0f : 40.0f);
	const float arm_hi = size == TownSize::Small ? 34.0f : (size == TownSize::Medium ? 50.0f : 64.0f);
	const int sigma = rng.chance(0.5f) ? 1 : -1; // the side a fork, a bow's bulge or the first cross street takes
	float fork_s = -1.0f;
	// A grid's main street is as long as its blocks need: 2 (small), 2-3 (medium) or 3 (large) crossings, blocks of
	// land 32-60 m between the streets' sidewalks (a row of shops each side, back to back, service yards between),
	// and 24 m past the outer crossings to each end.
	const float block = size == TownSize::Small ? rng.range(32.0f, 40.0f) : (size == TownSize::Medium ? rng.range(36.0f, 50.0f) : rng.range(44.0f, 60.0f));
	const int crossings = size == TownSize::Small ? 2 : (size == TownSize::Medium ? rng.irange(2, 3) : 3);
	const float spacing = block + 2.0f * (4.0f + 0.3f + 4.4f); // + both corridors (town_dims)
	if (tpl == TownTemplate::Grid) {
		span = float(crossings - 1) * spacing + 50.0f + rng.range(0.0f, 24.0f);
	}
	{
		Road M;
		Turtle T;
		switch (tpl) {
			case TownTemplate::Grid:
				T.start(Vector3(), 0.0f);
				T.straight(span + 2.0f * ext);
				break;
			case TownTemplate::HighStreet: {
				// A bow: out of the W port heading east, swing one way, a long arc back across (its apex the bulge
				// the square sits on), and round to east again.
				T.start(Vector3(), 0.0f);
				T.straight(ext + rng.range(40.0f, 46.0f)); // (a straight run into town: the town cine shot needs one)
				const float a = rng.range(0.36f, 0.54f);
				const float rb = span * rng.range(0.42f, 0.56f), r1 = rb * rng.range(0.45f, 0.65f);
				T.arc(r1, float(sigma) * a);
				T.arc(rb, -2.0f * float(sigma) * a);
				T.arc(r1, float(sigma) * a);
				T.straight(Math::max(10.0f, span + 2.0f * ext - T.p.x));
				break;
			}
			case TownTemplate::Fork: {
				// Straight to the fork, then the main street swings away from the branch and back to east.
				T.start(Vector3(), rng.range(-0.05f, 0.05f));
				fork_s = ext + span * rng.range(0.34f, 0.48f);
				T.straight(fork_s + 2.0f);
				T.arc(rng.range(55.0f, 85.0f), -float(sigma) * rng.range(0.2f, 0.34f));
				T.straight(rng.range(10.0f, 24.0f));
				T.arc(rng.range(55.0f, 85.0f), rng.range(-0.05f, 0.05f) - T.h);
				T.straight(Math::max(10.0f, span + 2.0f * ext - T.p.x));
				break;
			}
			default: { // Cross: a gently bending main street
				T.start(Vector3(), rng.range(-0.1f, 0.1f));
				T.straight(ext + rng.range(10.0f, 18.0f));
				const int bends = rng.irange(0, 2);
				float sg = rng.chance(0.5f) ? 1.0f : -1.0f;
				for (int b = 0; b < bends && T.p.x < span - 40.0f; ++b) {
					T.straight(rng.range(14.0f, 30.0f));
					T.arc(rng.range(70.0f, 120.0f), sg * rng.range(0.1f, 0.28f) - T.h);
					sg = -sg;
				}
				T.straight(rng.range(6.0f, 16.0f));
				T.arc(rng.range(70.0f, 110.0f), rng.range(-0.05f, 0.05f) - T.h);
				T.straight(Math::max(10.0f, span + 2.0f * ext - T.p.x));
			}
		}
		set_road_points(M, T.pts);
		town_dims(M);
		M.lots_s0 = ext;
		M.lots_s1 = M.s_end() - ext;
		M.start_kind = M.end_kind = RoadEnd::Port;
		net.roads.push_back(M);
	}
	const Road M = net.roads[0]; // a copy: net.roads grows below
	std::vector<TownArm> arms;
	auto station_ok = [&](float st, int side) {
		if (min_radius(M, st - 7.0f, st + 7.0f) < JUNCTION_RADIUS_MIN) {
			return false;
		}
		for (const TownArm &o : arms) {
			const float d = std::abs(o.sp.st - st);
			// Never two on one side within a block; across the street either a + crossing or well staggered.
			if (o.sp.side == side ? d < 40.0f : (d > 0.01f && d < 18.0f)) {
				return false;
			}
		}
		return true;
	};
	auto pick_station = [&](int side, float lo, float hi, float &st) {
		std::vector<float> ok;
		for (float s = lo; s <= hi; s += 1.0f) {
			if (station_ok(s, side)) {
				ok.push_back(s);
			}
		}
		if (ok.empty()) {
			return false;
		}
		st = ok[rng.irange(0, int(ok.size()) - 1)];
		return true;
	};
	auto add_arm = [&](int side, float st, float theta, float lean, float len, float turn) {
		TownArm a;
		a.sp.side = side;
		a.sp.st = st;
		a.theta = theta;
		a.lean = lean;
		a.len = len;
		a.bends.n = turn > 0.0f ? 1 : 0;
		a.bends.radius[0] = rng.range(45.0f, 80.0f);
		a.bends.radius[1] = rng.range(30.0f, 55.0f); // a port road's bend round to due N / S
		a.bends.turn[0] = (rng.chance(0.5f) ? 1.0f : -1.0f) * rng.range(0.0f, Math::max(turn, 0.01f));
		a.bends.gap[0] = rng.range(4.0f, 12.0f);
		arms.push_back(a);
		return int(arms.size()) - 1;
	};
	// (Side streets keep 24 m from the main street's ends: the country road's run-on past each port stays clear.)
	const float lo = M.lots_s0 + 24.0f, hi = M.lots_s1 - 24.0f;
	switch (tpl) {
		case TownTemplate::Grid: {
			// Crossings square-on every block (see `block` above).
			const float built = hi - lo;
			const int n = crossings;
			const float c0 = lo + rng.range(0.0f, Math::max(0.0f, built - float(n - 1) * spacing));
			const float depth = Math::clamp(block * rng.range(0.85f, 1.15f), 32.0f, 60.0f);
			for (int k = 0; k < n; ++k) {
				const float st = c0 + float(k) * spacing + rng.range(-1.5f, 1.5f);
				for (int side : { sigma, -sigma }) {
					if (station_ok(st, side)) {
						add_arm(side, st, float(Math_PI) * 0.5f, 1.0f, depth * rng.range(0.92f, 1.08f), 0.0f);
					}
				}
			}
			break;
		}
		case TownTemplate::HighStreet: {
			// A lane or two off the bow (the square takes the outside of the apex: plan_town_lots).
			const int n = size == TownSize::Large ? 2 : 1;
			for (int k = 0; k < n; ++k) {
				const int side = k == 0 ? -sigma : (rng.chance(0.5f) ? 1 : -1);
				float st;
				if (pick_station(side, lo, hi, st)) {
					add_arm(side, st, Math::deg_to_rad(rng.range(72.0f, 90.0f)), rng.chance(0.5f) ? 1.0f : -1.0f, rng.range(arm_lo, arm_hi), 0.3f);
				}
			}
			break;
		}
		case TownTemplate::Fork: {
			// The fork's branch first (arm 0): leaving forward at 48-60 degrees, a long road of its own.
			add_arm(sigma, fork_s, Math::deg_to_rad(rng.range(48.0f, 60.0f)), 1.0f, rng.range(arm_hi * 0.9f, arm_hi * 1.3f), 0.0f);
			arms[0].bends.n = 1;
			arms[0].bends.turn[0] = float(sigma) * rng.range(-0.05f, 0.3f); // bending on away from the main street
			if (size != TownSize::Small) {
				float st;
				if (pick_station(-sigma, lo, hi, st)) {
					add_arm(-sigma, st, Math::deg_to_rad(rng.range(72.0f, 90.0f)), rng.chance(0.5f) ? 1.0f : -1.0f, rng.range(arm_lo, arm_hi), 0.3f);
				}
			}
			break;
		}
		default: { // Cross: one or two cross streets, each a T or a + crossing
			const int n = size == TownSize::Small ? 1 : (size == TownSize::Medium ? rng.irange(1, 2) : 2);
			for (int k = 0; k < n; ++k) {
				const int side = k == 0 ? sigma : -sigma;
				const bool plus = rng.chance(0.5f);
				float st;
				if (!pick_station(side, lo, hi, st)) {
					continue;
				}
				const float theta = Math::deg_to_rad(rng.range(72.0f, 90.0f));
				const float lean = rng.chance(0.5f) ? 1.0f : -1.0f;
				add_arm(side, st, theta, lean, rng.range(arm_lo, arm_hi), 0.3f);
				if (plus && station_ok(st, -side)) {
					add_arm(-side, st, theta, -lean, rng.range(arm_lo, arm_hi), 0.3f); // straight on across
				}
			}
		}
	}
	// The N / S port roads asked for: an arm on that side bends round and runs out (the fork's branch first),
	// else a lane of its own.
	for (int s : { 1, -1 }) {
		if (!(ports & (s > 0 ? 1 : 2))) {
			continue;
		}
		std::vector<int> cands;
		for (int k = 0; k < int(arms.size()); ++k) {
			if (arms[k].sp.side == s && !arms[k].sp.port) {
				cands.push_back(k);
			}
		}
		int pick = -1;
		if (tpl == TownTemplate::Fork && !arms.empty() && arms[0].sp.side == s) {
			pick = 0;
		} else if (!cands.empty()) {
			pick = cands[rng.irange(0, int(cands.size()) - 1)];
		} else {
			float st;
			if (!pick_station(s, lo, hi, st)) {
				HOOD_REJECT(12);
			}
			pick = add_arm(s, st, Math::deg_to_rad(rng.range(78.0f, 90.0f)), rng.chance(0.5f) ? 1.0f : -1.0f, 40.0f, 0.0f);
		}
		arms[pick].sp.port = true;
	}
	if (arms.empty()) {
		HOOD_REJECT(13);
	}
	const float skip = tpl == TownTemplate::Fork ? 62.0f : 45.0f;
	net.roads.resize(1);
	if (!hood_checks(net, rej, false, skip, TOWN_ROW_REACH)) {
		return false;
	}
	auto try_add = [&](const Road &B) {
		net.roads.push_back(B);
		if (hood_checks(net, rej, false, skip, TOWN_ROW_REACH)) {
			return true;
		}
		net.roads.pop_back();
		return false;
	};
	// Side streets at their drawn length, shorter if they don't fit (never under 26 m), left out if they still
	// don't; the port roads last, long enough to reach past everything on their side (they must fit).
	int built = 0;
	bool branch_built = false;
	for (int k = 0; k < int(arms.size()); ++k) {
		const TownArm &a = arms[k];
		if (a.sp.port) {
			continue;
		}
		for (float scale : { 1.0f, 0.8f, 0.65f }) {
			Road B = build_branch(M, 0, a.sp, Math::max(26.0f, a.len * scale), a.theta, a.lean, a.bends, 0.0f);
			town_dims(B);
			B.end_kind = RoadEnd::DeadEnd;
			B.bulb_r = 0.0f;
			if (try_add(B)) {
				++built;
				branch_built = branch_built || k == 0;
				break;
			}
		}
	}
	for (int k = 0; k < int(arms.size()); ++k) {
		const TownArm &a = arms[k];
		if (!a.sp.port) {
			continue;
		}
		const float side = float(a.sp.side);
		float far = -1e9f;
		for (const Road &R : net.roads) {
			float s0, s1;
			core_of(R, s0, s1);
			for (float st = s0; st <= s1; st += 2.0f) {
				far = Math::max(far, R.at(st).z * side + R.edge() + TOWN_ROW_REACH);
			}
		}
		Road probe = build_branch(M, 0, a.sp, 60.0f, a.theta, a.lean, a.bends, ext);
		town_dims(probe);
		const float reach = probe.at(probe.lots_s1).z * side;
		Road B = build_branch(M, 0, a.sp, Math::clamp(60.0f + (far - 6.0f - reach), Math::max(40.0f, a.len), 110.0f), a.theta, a.lean, a.bends, ext);
		town_dims(B);
		if (!try_add(B)) {
			return false;
		}
		++built;
		branch_built = branch_built || k == 0;
	}
	if (built < 1 || (tpl == TownTemplate::Grid && built < 3) || (tpl == TownTemplate::Fork && !branch_built)) {
		HOOD_REJECT(13);
	}
	return hood_checks(net, rej, true, skip, TOWN_ROW_REACH);
}

// The last resort: a straight main street with one + crossing (always passes the checks).
void town_fallback(bool embedded, int ports, RoadNet &net) {
	net.roads.clear();
	net.fillets.clear();
	const float ext = embedded ? 1.0f : 40.0f;
	Road M;
	set_road_points(M, { Vector3(-70.0f - ext, 0, 0), Vector3(70.0f + ext, 0, 0) });
	town_dims(M);
	M.lots_s0 = ext;
	M.lots_s1 = M.s_end() - ext;
	net.roads.push_back(M);
	BranchBends straight_road;
	straight_road.n = 0;
	for (int side : { 1, -1 }) {
		BranchSpec sp;
		sp.side = side;
		sp.st = M.lots_s0 + 70.0f;
		sp.port = (ports & (side > 0 ? 1 : 2)) != 0;
		Road B = build_branch(net.roads[0], 0, sp, sp.port ? 70.0f : 40.0f, float(Math_PI) * 0.5f, 1.0f, straight_road, sp.port ? ext : 0.0f);
		town_dims(B);
		if (!sp.port) {
			B.end_kind = RoadEnd::DeadEnd;
			B.bulb_r = 0.0f;
		}
		net.roads.push_back(B);
	}
}
} // namespace
} // namespace (the planners' helpers; reopened below for the lot footprints)

bool town_straight_near(const Road &M, float st) {
	const Vector3 t0 = M.tangent_at(st);
	for (float off : { 0.0f, -8.0f, 8.0f, -16.0f, 16.0f, -24.0f, 24.0f, -32.0f, 32.0f }) {
		for (float sign : { 1.0f, -1.0f }) {
			bool ok = true;
			for (float u = 0.0f; u <= 30.0f && ok; u += 2.5f) {
				const float s = st + off + sign * u;
				ok = s > M.lots_s0 && s < M.lots_s1 && std::abs(M.tangent_at(s).dot(t0)) > 0.988f;
			}
			if (ok) {
				return true;
			}
		}
	}
	return false;
}

void plan_town_roads(GenContext &ctx) {
	// The template and size from the seed alone (the same whichever ports WorldPlan asks for).
	Rng pick = ctx.rng(STAGE_STREET, 0x70E4);
	ctx.town_template = TownTemplate(pick.irange(0, int(TownTemplate::Count) - 1));
	const float sr = pick.f();
	ctx.town_size = sr < 0.35f ? TownSize::Small : (sr < 0.75f ? TownSize::Medium : TownSize::Large);
	int ports;
	if (ctx.port_side >= GenContext::TOWN_PORTS) {
		ports = (ctx.port_side - GenContext::TOWN_PORTS) & 3;
	} else {
		const float r = pick.f();
		ports = r < 0.3f ? 0 : (r < 0.55f ? 1 : (r < 0.8f ? 2 : 3));
	}
	ctx.town_ports = ports;
	bool ok = false;
	int attempt = 0;
	int rej[16] = {};
	while (!ok && attempt < TOWN_ATTEMPTS) {
		Rng rng = ctx.rng(STAGE_STREET, uint64_t(1 + attempt));
		ok = town_attempt(rng, ctx.embedded, ctx.town_template, ctx.town_size, ports, ctx.roads, rej);
		++attempt;
	}
	if (!ok) {
		town_fallback(ctx.embedded, ports, ctx.roads);
		std::string why = "town layout fell back to the plain plan:";
		for (int k = 1; k < 16; ++k) {
			if (rej[k]) {
				why += " r" + std::to_string(k) + "=" + std::to_string(rej[k]);
			}
		}
		ctx.stats.notes.push_back(why);
	}
	finish_hood_net(ctx.roads);
	for (Road &R : ctx.roads.roads) {
		if (R.end_kind == RoadEnd::DeadEnd) {
			R.lots_s1 = R.s_end() - 0.5f; // shops right up to the hedge across its end
		}
	}
	ctx.stats.rerolls = ok ? attempt : -1;
	for (int k = 0; k < 16; ++k) {
		ctx.stats.road_rejects[k] = rej[k];
	}
	// Spawn in the crossing nearest the middle of the main street that has a straight stretch near it (the town
	// cine shot runs down one), else the straight spot nearest the middle; facing down the longer arm.
	const Road &M = ctx.roads.trunk();
	const float mid = (M.lots_s0 + M.lots_s1) * 0.5f;
	std::vector<std::pair<float, float>> order;
	for (const RoadJunction &j : ctx.roads.junctions(0)) {
		order.push_back({ std::abs(j.s - mid), j.s });
	}
	std::sort(order.begin(), order.end());
	float js = mid;
	bool found = false;
	for (const auto &o : order) {
		if (town_straight_near(M, o.second)) {
			js = o.second;
			found = true;
			break;
		}
	}
	for (float d = 0.0f; d < (M.lots_s1 - M.lots_s0) * 0.5f - 10.0f && !found; d += 2.0f) {
		for (float s : { mid + d, mid - d }) {
			if (!found && town_straight_near(M, s)) {
				js = s;
				found = true;
			}
		}
	}
	if (!found && !order.empty()) {
		js = order.front().second;
	}
	const bool forward = M.lots_s1 - js >= js - M.lots_s0;
	ctx.spawn = M.at(js + (forward ? -1.5f : 1.5f)) + Vector3(0, 0.1f, 0);
	const Vector3 t = M.tangent_at(js) * (forward ? 1.0f : -1.0f);
	ctx.spawn_yaw = std::atan2(t.x, t.z);
	Rng rng = ctx.rng(STAGE_STREET);
	const Vector3 face(std::sin(ctx.spawn_yaw), 0, std::cos(ctx.spawn_yaw));
	const float elev = rng.range(0.15f, 0.2f);
	const Vector3 flat_dir = face.rotated(Vector3(0, 1, 0), rng.range(0.22f, 0.42f));
	ctx.moon_dir = (flat_dir * std::cos(elev) + Vector3(0, std::sin(elev), 0)).normalized();
}

namespace {
// ---------------------------------------------------------------- lot footprints
// The footprint in its own frame: x along the frontage [-w/2, w/2], z toward the road [-depth, 0].
struct LotRect {
	Vector3 o, x, z;
	float hw, d;
	explicit LotRect(const Lot &l) :
			o(l.xf.origin), x(flat(l.xf.basis.get_column(0))), z(flat(l.xf.basis.get_column(2))), hw(l.width * 0.5f), d(l.depth) {}
	Vector2 local(const Vector3 &p) const {
		const float vx = p.x - o.x, vz = p.z - o.z;
		return Vector2(vx * x.x + vz * x.z, vx * z.x + vz * z.z);
	}
	Vector3 world(float lx, float lz) const { return o + x * lx + z * lz; }
	float dist(const Vector2 &q) const {
		const float dx = Math::max(0.0f, std::abs(q.x) - hw);
		const float dz = Math::max(0.0f, Math::max(q.y, -d - q.y));
		return std::sqrt(dx * dx + dz * dz);
	}
	// Distance from the segment a-b (local) to the rectangle (0 when they touch or cross).
	float seg(const Vector2 &a, const Vector2 &b) const {
		// Liang-Barsky clip against the box: any part inside means they cross.
		float t0 = 0.0f, t1 = 1.0f;
		const Vector2 dd = b - a;
		const float p[4] = { -dd.x, dd.x, -dd.y, dd.y };
		const float qv[4] = { a.x + hw, hw - a.x, a.y + d, -a.y };
		bool hit = true;
		for (int k = 0; k < 4 && hit; ++k) {
			if (std::abs(p[k]) < 1e-9f) {
				hit = qv[k] >= 0.0f;
			} else {
				const float t = qv[k] / p[k];
				if (p[k] < 0.0f) {
					t0 = Math::max(t0, t);
				} else {
					t1 = Math::min(t1, t);
				}
				hit = t0 <= t1;
			}
		}
		if (hit) {
			return 0.0f;
		}
		float best = Math::min(dist(a), dist(b));
		const Vector2 corners[4] = { Vector2(-hw, 0), Vector2(hw, 0), Vector2(hw, -d), Vector2(-hw, -d) };
		for (const Vector2 &c : corners) {
			const float t = Math::clamp((c - a).dot(dd) / Math::max(dd.length_squared(), 1e-9f), 0.0f, 1.0f);
			best = Math::min(best, (a + dd * t - c).length());
		}
		return best;
	}
};
} // namespace

void plan_street(GenContext &ctx) {
	if (ctx.kind == SettlementKind::Town) {
		plan_town_roads(ctx); // Stage 4c (plan_street_legacy: the pre-4c sine street, kept for reference)
		return;
	}
	plan_hood_roads(ctx);
}

float lot_overlap(const Lot &a, const Lot &b) {
	const LotRect A(a), B(b);
	const Vector3 ca = A.world(0.0f, -A.d * 0.5f), cb = B.world(0.0f, -B.d * 0.5f);
	const Vector2 axes[4] = { Vector2(A.x.x, A.x.z), Vector2(A.z.x, A.z.z), Vector2(B.x.x, B.x.z), Vector2(B.z.x, B.z.z) };
	const Vector2 d(cb.x - ca.x, cb.z - ca.z);
	float depth = 1e9f;
	for (const Vector2 &n : axes) {
		const float ra = A.hw * std::abs(Vector2(A.x.x, A.x.z).dot(n)) + A.d * 0.5f * std::abs(Vector2(A.z.x, A.z.z).dot(n));
		const float rb = B.hw * std::abs(Vector2(B.x.x, B.x.z).dot(n)) + B.d * 0.5f * std::abs(Vector2(B.z.x, B.z.z).dot(n));
		depth = Math::min(depth, ra + rb - std::abs(d.dot(n)));
	}
	return depth;
}

void lot_corridor_reach(const GenContext &ctx, const Lot &lot, float &own, float &other) {
	own = other = -1e9f;
	const LotRect LR(lot);
	float bx0 = 1e9f, bx1 = -1e9f, bz0 = 1e9f, bz1 = -1e9f;
	for (float sx : { -1.0f, 1.0f }) {
		for (float lz : { 0.0f, -LR.d }) {
			const Vector3 c = LR.world(sx * LR.hw, lz);
			bx0 = Math::min(bx0, c.x);
			bx1 = Math::max(bx1, c.x);
			bz0 = Math::min(bz0, c.z);
			bz1 = Math::max(bz1, c.z);
		}
	}
	const RoadNet &net = ctx.roads;
	for (int r = 0; r < int(net.roads.size()); ++r) {
		const Road &R = net.roads[r];
		float &acc = r == lot.road ? own : other;
		const float e = R.edge();
		// Straight corridor (a side road's from just inside its parent's sidewalk: the mouth itself is the parent's).
		const float s_from = R.parent >= 0 ? net.roads[R.parent].edge() - 1.0f : R.s_begin() - 1.0f;
		const int nseg = int(R.pts.size()) - 1;
		const int nc = int(R.chunk_box.size() / 4);
		for (int c = 0; c < (nc > 0 ? nc : 1); ++c) {
			if (nc > 0) {
				const float *b = &R.chunk_box[c * 4];
				if (b[0] > bx1 + e + 1.0f || b[2] < bx0 - e - 1.0f || b[1] > bz1 + e + 1.0f || b[3] < bz0 - e - 1.0f) {
					continue;
				}
			}
			const int i0 = nc > 0 ? c * 16 : 0, i1 = nc > 0 ? Math::min(nseg, c * 16 + 16) : nseg;
			for (int i = i0; i < i1; ++i) {
				if (R.s[i + 1] < s_from) {
					continue;
				}
				const Vector3 a = R.s[i] < s_from ? R.at(s_from) : R.pts[i];
				acc = Math::max(acc, e - LR.seg(LR.local(a), LR.local(R.pts[i + 1])));
			}
		}
		if (R.bulb()) {
			acc = Math::max(acc, R.bulb_r + R.curb + R.walk - LR.dist(LR.local(R.bulb_c)));
		}
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) != RoadEnd::Port || R.x_station || R.straight) {
				continue;
			}
			const Vector3 E = at_end ? R.pts.back() : R.pts.front();
			const Vector3 F = E + flat(R.end_dir(at_end)) * PORT_RUN;
			acc = Math::max(acc, e - LR.seg(LR.local(E), LR.local(F)));
		}
	}
	// Fillets: the rounded corners' bands curve into what would be the block corner.
	for (const RoadFillet &F : net.fillets) {
		if (LR.dist(LR.local(F.c)) > F.r + F.half_road) {
			continue;
		}
		const int nx = Math::max(2, int(LR.hw * 4.0f)), nz = Math::max(2, int(LR.d * 2.0f));
		auto test = [&](float lx, float lz) {
			const Vector3 p = LR.world(lx, lz);
			if (!F.in_sector(p)) {
				return;
			}
			const float dd = hdist(p, F.c);
			if (dd < F.r + F.half_road) {
				other = Math::max(other, dd - F.r_out());
			}
		};
		for (int k = 0; k <= nx; ++k) {
			const float lx = -LR.hw + 2.0f * LR.hw * k / nx;
			test(lx, 0.0f);
			test(lx, -LR.d);
		}
		for (int k = 0; k <= nz; ++k) {
			const float lz = -LR.d * k / nz;
			test(-LR.hw, lz);
			test(LR.hw, lz);
		}
	}
}

float frontage_reach(const GenContext &ctx, const Lot &lot, float x) {
	const Road &R = ctx.road_of(lot);
	for (float z = 0.0f; z < 9.0f; z += 0.05f) {
		const RoadZone zn = ctx.roads.zone(lot.to_world(x, 0, z));
		if (zn == RoadZone::Curb || zn == RoadZone::Asphalt) {
			return z;
		}
	}
	return R.walk;
}

// ---------------------------------------------------------------- lots
// Neighbourhoods (Stage 2a): lots by arc length along both sides of every road, each frame from the local
// tangent (Lot::xf is the frontage frame), set back where the inside of a bend curves the sidewalk into
// their front corners and shallower there, never into another road's corridor or another lot (the same
// tests the sweep counts). First 3-5 lots round each bulb facing its centre, then the main road's rows,
// then the side roads' rows. Towns use plan_town_lots.
void plan_lots(GenContext &ctx) {
	if (!ctx.hood_lots_ready) {
		plan_hood_lots(ctx);
	}
}

void plan_hood_lots(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_LOTS);
	const RoadNet &net = ctx.roads;
	ctx.lots.clear();
	constexpr float MARGIN = 0.3f; // clearance from other corridors and fillets
	auto fits = [&](const Lot &L) {
		float own, other;
		lot_corridor_reach(ctx, L, own, other);
		if (own > 0.02f || other > -MARGIN) {
			return false;
		}
		for (const Lot &o : ctx.lots) {
			if (lot_overlap(L, o) > 0.0f) {
				return false;
			}
		}
		return true;
	};
	auto add = [&](Lot L) {
		L.index = int(ctx.lots.size());
		ctx.lots.push_back(L);
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
	auto row = [&](int road, int side) {
		const Road &R = net.roads[road];
		float x = R.lots_s0 + rng.range(0.5f, 2.5f);
		while (x < R.lots_s1 - 13.5f) {
			const float w = rng.range(14.5f, 19.0f);
			float d = rng.range(21.0f, 24.0f);
			const float mid = x + w * 0.5f;
			const float k = (R.at(mid - 6.0f) + R.at(mid + 6.0f) - R.at(mid) * 2.0f).dot(R.left_at(mid)) * float(side);
			if (k > 1e-3f) {
				// The inside of a bend: neighbours' side lines converge behind the frontage.
				const float radius = 36.0f / k;
				d = Math::min(d, Math::max(15.0f, (radius - R.edge()) * 0.32f));
			}
			bool placed = false;
			for (float slide = 0.0f; slide <= 9.0f && !placed; slide += 0.75f) {
				const float room = R.lots_s1 - 0.5f - (x + slide);
				const float w0 = Math::min(w, room);
				if (w0 < 13.5f) {
					break;
				}
				for (float ww : { w0, Math::max(13.5f, w0 - 2.0f) }) {
					for (float dd : { d, Math::max(14.0f, d - 5.0f) }) {
						const Lot L = frontage_lot(road, side, x + slide, ww, dd);
						if (fits(L)) {
							add(L);
							x += slide + ww;
							placed = true;
							break;
						}
					}
					if (placed) {
						break;
					}
				}
			}
			if (!placed) {
				x += 9.0f; // a junction corner, a bulb's ring: try further on
			}
		}
	};
	// The rings round the bulbs first (3-5 lots each), then the main road's rows, then the side roads'.
	for (int b = 1; b < int(net.roads.size()); ++b) {
		const Road &B = net.roads[b];
		if (!B.bulb()) {
			continue;
		}
		const float re = B.bulb_r + B.curb + B.walk;
		const Vector3 te = B.tangent_at(B.s_end()), le = B.left_at(B.s_end());
		const float H = Math_PI - B.bulb_entry - 0.22f; // half the ring the lots go round
		const int n = rng.irange(3, 5);
		const float slot = 2.0f * H / float(n);
		// Pie-ish lots: narrow fronts round the ring. Five round a small ring stand a little back from
		// the sidewalk (a strip of lawn) so each front is still wide enough for a house.
		const float half_sin = std::sin(slot * 0.5f);
		const float set = Math::clamp(13.4f / (2.0f * half_sin) - re, 0.0f, 3.0f);
		const float wmax = 2.0f * (re + set) * half_sin - 0.3f;
		for (int k = 0; k < n; ++k) {
			const float a0 = -H + slot * (float(k) + 0.5f) + rng.range(-0.04f, 0.04f);
			const float w0 = Math::min(rng.range(12.5f, 15.5f), wmax);
			const float d = rng.range(20.0f, 25.0f);
			bool placed = false;
			// Squeezed by a neighbour (the main road's rows, another corridor): a little narrower, shallower,
			// or nudged round the ring.
			for (float nudge : { 0.0f, 0.08f, -0.08f }) {
				for (float w : { w0, Math::max(12.5f, w0 - 1.5f) }) {
					for (float dd : { d, d - 5.0f }) {
						if (placed) {
							continue;
						}
						const float a = a0 + nudge;
						const Vector3 dir = te * std::cos(a) + le * std::sin(a);
						const Vector3 toward = -dir, up(0, 1, 0);
						Lot L;
						L.road = b;
						L.bulb = true;
						L.side = a >= 0.0f ? 1 : -1;
						L.width = w;
						L.depth = dd;
						L.s = B.s_end() + re * (float(Math_PI) - std::abs(a));
						L.xf = Transform3D(Basis(up.cross(toward), up, toward), B.bulb_c + dir * (re + set));
						if (fits(L)) {
							add(L);
							placed = true;
						}
					}
				}
			}
		}
	}
	for (int side : { 1, -1 }) {
		row(0, side);
	}
	for (int b = 1; b < int(net.roads.size()); ++b) {
		for (int side : { 1, -1 }) {
			row(b, side);
		}
	}
	link_lot_neighbours(ctx);
	ctx.occupied.assign(ctx.lots.size(), {});
	ctx.lot_mesh.assign(ctx.lots.size(), MeshBuilder());
	// Bounds: every lot corner, every corridor, every bulb ring.
	float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
	auto grow = [&](const Vector3 &p, float r) {
		x0 = Math::min(x0, p.x - r);
		x1 = Math::max(x1, p.x + r);
		z0 = Math::min(z0, p.z - r);
		z1 = Math::max(z1, p.z + r);
	};
	for (const Lot &lot : ctx.lots) {
		for (float sx : { -0.5f, 0.5f }) {
			for (float lz : { 0.0f, -lot.depth }) {
				grow(lot.to_world(sx * lot.width, 0, lz), 0.0f);
			}
		}
	}
	for (const Road &R : net.roads) {
		float s0, s1;
		core_of(R, s0, s1);
		for (float st = s0; st <= s1 + 0.01f; st += 2.0f) {
			grow(R.point(st, R.edge()), 0.0f);
			grow(R.point(st, -R.edge()), 0.0f);
		}
		if (R.bulb()) {
			grow(R.bulb_c, R.bulb_r + R.curb + R.walk);
		}
	}
	ctx.bound_x0 = x0;
	ctx.bound_x1 = x1;
	ctx.bound_z0 = z0;
	ctx.bound_z1 = z1;
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
		const Road &R = ctx.road_of(lot);
		float score = 0.0f;
		float dx = 1e9f; // to the nearest junction along its road
		for (const RoadJunction &j : ctx.roads.junctions(lot.road)) {
			dx = Math::min(dx, std::abs(lot.s - j.s));
		}
		score += Math::max(0.0f, 3.0f - dx / 10.0f);
		const Vector3 c0 = R.at(lot.s - 5), c1 = R.at(lot.s), c2 = R.at(lot.s + 5);
		const float curvature = (c0 + c2 - c1 * 2.0f).dot(R.left_at(lot.s)); // > 0: the road bends toward side +1
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
			if (li == cand.prev || li == cand.next) {
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
			for (int ni : { lot.prev, lot.next }) {
				if (ni < 0) {
					continue;
				}
				const Lot &o = ctx.lots[ni];
				if (int(o.persona) == p && (o.landmark != Landmark::None || o.index < lot.index)) { // a neighbour already decided
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
// Towns (until Stage 4c): the trunk road (ribbon, curbs, verges, sidewalks, gutters, drains), then each side road leaving it.
static void build_street_legacy(GenContext &ctx) {
	const Road &s = ctx.roads.trunk();
	const std::vector<RoadJunction> mouths = ctx.roads.junctions(0);
	MeshBuilder &m = ctx.street_mesh;
	const float hr = s.half_road, hc = s.half_road + s.curb, he = s.edge();
	const float curb_h = 0.12f, walk_y = 0.07f, verge_y = 0.05f;
	const int verge_cell = ctx.kind == SettlementKind::Town ? mat_cell(pal::BRICK, mat::PAVERS) : mat_cell(pal::LAWN, mat::GRASS);
	// Within `pad` of a side road's mouth (its curbs, on the side it leaves), or of any road crossing.
	auto in_mouth = [&](float x, int side, float pad) {
		for (const RoadJunction &j : mouths) {
			const Road &c = ctx.roads.roads[j.child];
			if (side == j.side && std::abs(x - j.s) < c.half_road + c.curb + pad) {
				return true;
			}
		}
		return false;
	};
	auto in_crossing = [&](float x, float pad) {
		for (const RoadJunction &j : mouths) {
			if (std::abs(x - j.s) < ctx.roads.roads[j.child].half_road + pad) {
				return true;
			}
		}
		return false;
	};
	auto in_crossing_hc = [&](float x, float pad) {
		for (const RoadJunction &j : mouths) {
			const Road &c = ctx.roads.roads[j.child];
			if (std::abs(x - j.s) < c.half_road + c.curb + pad) {
				return true;
			}
		}
		return false;
	};
	// Road.
	m.ribbon(s.pts, s.left, -hr - 0.1f, hr + 0.1f, 0.01f, mat_cell(pal::ASPHALT, mat::ASPHALT));
	// Curbs and sidewalks, with a gap where a side road joins.
	const size_t n = s.pts.size();
	for (int side : { 1, -1 }) {
		for (size_t i = 0; i + 1 < n; ++i) {
			const Vector3 &c0 = s.pts[i], &c1 = s.pts[i + 1];
			const Vector3 &l0 = s.left[i], &l1 = s.left[i + 1];
			const float x = (s.s[i] + s.s[i + 1]) * 0.5f;
			if (in_mouth(x, side, 0.2f)) {
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
			// Close the curb, verge and sidewalk where the street runs out into the country road.
			if (i == 0 || i + 2 == n) {
				const Vector3 &c = i == 0 ? c0 : c1, &l = i == 0 ? l0 : l1;
				const Vector3 out = (i == 0 ? c0 - c1 : c1 - c0).normalized();
				const float bands[3][3] = { { hr, hc, curb_h }, { hc, hv, verge_y }, { hv, he, walk_y } };
				for (const auto &b : bands) {
					m.quad_out(P(c, l, b[0], -0.05f), P(c, l, b[1], -0.05f), P(c, l, b[1], b[2]), P(c, l, b[0], b[2]), b[2] == verge_y ? verge_cell : tone,
							P(c, l, (b[0] + b[1]) * 0.5f, b[2] * 0.5f) - out);
				}
			}
		}
	}
	// Dashed centre line, skipping the intersection.
	for (float x = s.s_begin(); x < s.s_end() - 2.0f; x += 4.0f) {
		if (in_crossing(x, 1.0f)) {
			continue;
		}
		const Vector3 a = s.at(x), b = s.at(x + 2.0f);
		const Vector3 la = s.left_at(x), lb = s.left_at(x + 2.0f);
		Vector3 q[4] = { a - la * 0.09f, b - lb * 0.09f, b + lb * 0.09f, a + la * 0.09f };
		for (Vector3 &p : q) {
			p.y = 0.035f;
		}
		m.quad_out(q[0], q[1], q[2], q[3], pal::ROAD_LINE, (a + b) * 0.5f - Vector3(0, 1, 0));
	}
	// Gutters, storm drains and manhole covers.
	Rng drng = ctx.rng(STAGE_STREET, 7);
	for (int side : { 1, -1 }) {
		for (size_t i = 0; i + 1 < n; ++i) {
			const float x = (s.s[i] + s.s[i + 1]) * 0.5f;
			if (in_mouth(x, side, 0.2f)) {
				continue;
			}
			const float sd = float(side);
			Vector3 a0 = s.pts[i] + s.left[i] * (sd * (hr - 0.4f)), a1 = s.pts[i + 1] + s.left[i + 1] * (sd * (hr - 0.4f));
			Vector3 b0 = s.pts[i] + s.left[i] * (sd * hr), b1 = s.pts[i + 1] + s.left[i + 1] * (sd * hr);
			a0.y = a1.y = b0.y = b1.y = 0.03f;
			m.quad_out(a0, a1, b1, b0, mat_cell(pal::STONE, mat::CONCRETE), (a0 + b1) * 0.5f - Vector3(0, 1, 0));
		}
		for (float x = s.s_begin() + drng.range(6.0f, 12.0f); x < s.s_end(); x += drng.range(16.0f, 22.0f)) {
			if (in_mouth(x, side, 1.0f)) {
				continue;
			}
			const Vector3 p = s.at(x) + s.left_at(x) * (float(side) * (hr - 0.25f));
			const Vector3 t = s.tangent_at(x);
			const Vector3 lf = s.left_at(x);
			for (int k = -2; k <= 2; ++k) {
				m.beam(p + t * (k * 0.16f) - lf * 0.2f + Vector3(0, 0.02f, 0), p + t * (k * 0.16f) + lf * 0.2f + Vector3(0, 0.02f, 0), 0.06f, 0.02f,
						mat_cell(pal::IRON, mat::METAL));
			}
			m.beam(p - t * 0.42f + Vector3(0, 0.016f, 0), p + t * 0.42f + Vector3(0, 0.016f, 0), 0.48f, 0.01f, pal::CHARCOAL);
		}
	}
	for (float x = s.s_begin() + 9.0f; x < s.s_end(); x += drng.range(24.0f, 34.0f)) {
		if (in_crossing_hc(x, 2.0f)) {
			continue;
		}
		const Vector3 p = s.at(x) + s.left_at(x) * drng.range(-1.5f, 1.5f);
		m.cylinder(p + Vector3(0, 0.0f, 0), 0.38f, 0.03f, 14, mat_cell(pal::IRON, mat::METAL));
		m.cylinder(p + Vector3(0, 0.0f, 0), 0.44f, 0.022f, 14, mat_cell(pal::STONE, mat::CONCRETE));
	}
	for (const RoadJunction &j : mouths) {
		const Road &lane = ctx.roads.roads[j.child];
		// `across`: the side road's lateral axis turned to point the way its parent runs (keeps each
		// piece's vertex order as it always was); `fwd` is square to it.
		const Vector3 across = lane.left_at(0.0f) * -float(lane.parent_side);
		const Vector3 up(0, 1, 0);
		const Vector3 fwd = across.cross(up);
		// Zebra crosswalk across the side road's mouth.
		{
			const Vector3 o = lane.at(hc + 1.4f);
			for (int k = -3; k <= 3; ++k) {
				const Vector3 c = o + across * (k * 1.1f) + Vector3(0, 0.035f, 0);
				m.quad_out(c + across * -0.3f + fwd * -0.9f, c + across * 0.3f + fwd * -0.9f, c + across * 0.3f + fwd * 0.9f, c + across * -0.3f + fwd * 0.9f,
						mat_cell(pal::TRIM_WHITE, mat::CONCRETE), c - Vector3(0, 1, 0));
			}
		}
		// The side road, from the main street's edge (it doesn't overlap the main asphalt, which would
		// depth-fight).
		const float lhr = lane.half_road, lhc = lane.half_road + lane.curb, lhe = lane.edge();
		const Vector3 lat = across;
		std::vector<Vector3> lc, ll;
		for (float d = 0.0f; d <= lane.length() + 0.01f; d += 1.0f) {
			lc.push_back(lane.at(lane.s_begin() + Math::max(d, lhr)));
			ll.push_back(lat);
		}
		m.ribbon(lc, ll, -lhr, lhr, 0.012f, mat_cell(pal::ASPHALT, mat::ASPHALT));
		const Vector3 dir = lane.tangent_at(lane.s_end());
		for (float sd : { -1.0f, 1.0f }) {
			for (size_t i = 0; i + 1 < lc.size(); ++i) {
				const float d = float(i);
				if (d < lhr - 0.5f) {
					continue;
				}
				auto P = [&](size_t k, float off, float y) {
					Vector3 p = lc[k] + lat * (off * sd);
					p.y = y;
					return p;
				};
				m.quad_out(P(i, lhr, curb_h), P(i + 1, lhr, curb_h), P(i + 1, lhc, curb_h), P(i, lhc, curb_h), mat_cell(pal::STONE_LIGHT, mat::CONCRETE), P(i, (lhr + lhc) * 0.5f, -1));
				m.quad_out(P(i, lhr, 0.0f), P(i + 1, lhr, 0.0f), P(i + 1, lhr, curb_h), P(i, lhr, curb_h), mat_cell(pal::STONE, mat::CONCRETE), P(i, lhr + 1.0f, curb_h * 0.5f));
				const int tone = mat_cell((int(std::floor(d / 2.0f)) & 1) ? pal::SIDEWALK : pal::STONE_LIGHT, mat::CONCRETE);
				const float hv = lhc + lane.verge;
				m.quad_out(P(i, lhc, verge_y), P(i + 1, lhc, verge_y), P(i + 1, hv, verge_y), P(i, hv, verge_y), verge_cell, P(i, (lhc + hv) * 0.5f, -1));
				m.quad_out(P(i, hv, walk_y), P(i + 1, hv, walk_y), P(i + 1, lhe, walk_y), P(i, lhe, walk_y), tone, P(i, (hv + lhe) * 0.5f, -1));
				if (i + 2 == lc.size()) {
					// Close the lane's curb, verge and sidewalk ends.
					const float bands[3][3] = { { lhr, lhc, curb_h }, { lhc, hv, verge_y }, { hv, lhe, walk_y } };
					for (const auto &b : bands) {
						m.quad_out(P(i + 1, b[0], -0.05f), P(i + 1, b[1], -0.05f), P(i + 1, b[1], b[2]), P(i + 1, b[0], b[2]), b[2] == verge_y ? verge_cell : tone,
								P(i + 1, (b[0] + b[1]) * 0.5f, b[2] * 0.5f) - dir);
					}
				}
			}
		}
		// Lane centre dashes.
		for (float d = lhe + 1.0f; d < lane.length() - 2.0f; d += 4.0f) {
			const Vector3 a = lane.at(lane.s_begin() + d), b = lane.at(lane.s_begin() + d + 2.0f);
			Vector3 q[4] = { a - lat * 0.09f, b - lat * 0.09f, b + lat * 0.09f, a + lat * 0.09f };
			for (Vector3 &p : q) {
				p.y = 0.035f;
			}
			m.quad_out(q[0], q[1], q[2], q[3], pal::ROAD_LINE, (a + b) * 0.5f - Vector3(0, 1, 0));
		}
	}
}

// ---------------------------------------------------------------- boundary
static void hay_wall(GenContext &ctx, Vector3 center, Vector3 along, float width, Rng &rng) {
	MeshBuilder &m = ctx.boundary_mesh;
	const Vector3 up(0, 1, 0);
	const Vector3 across = along.cross(up).normalized(); // right-handed (along, up, across): a mirrored basis turns the mesh inside out
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

static void hedge_run_piece(GenContext &ctx, Vector3 a, Vector3 b, Rng &rng, float height, bool box = true);

static void hedge_run(GenContext &ctx, Vector3 a, Vector3 b, Rng &rng, float height = 3.2f) {
	if (ctx.terrain) {
		// Stage 3b: on the hills a hedge is a run of short level pieces, each on the ground at its ends.
		a.y = ctx.ground(a.x, a.z);
		b.y = ctx.ground(b.x, b.z);
		const float flat_len = Vector2(b.x - a.x, b.z - a.z).length();
		if (flat_len > 3.2f) {
			const int n = int(std::ceil(flat_len / 3.0f));
			float lo = 1e9f, hi = -1e9f;
			for (int k = 0; k < n; ++k) {
				Vector3 p = a.lerp(b, float(k) / n), q = a.lerp(b, float(k + 1) / n);
				const float gp = ctx.ground(p.x, p.z), gq = ctx.ground(q.x, q.z);
				lo = Math::min(lo, Math::min(gp, gq));
				hi = Math::max(hi, Math::max(gp, gq));
				p.y = q.y = Math::min(gp, gq) - 0.1f; // sunk, never floating
				hedge_run_piece(ctx, p, q, rng, height, false);
			}
			// One ground-level block for the whole run (a collision shape each costs load time), from its lowest
			// ground to the wall height over its highest.
			const float wall = ctx.embedded ? 0.6f : 10.0f;
			const Vector3 along = Vector3(b.x - a.x, 0.0f, b.z - a.z).normalized(), up(0, 1, 0);
			OBox box;
			box.xf = Transform3D(Basis(along, up, along.cross(up)), Vector3((a.x + b.x) * 0.5f, (lo + hi) * 0.5f + wall, (a.z + b.z) * 0.5f));
			box.half = Vector3(flat_len * 0.5f + 0.3f, wall + (hi - lo) * 0.5f, 0.7f);
			box.boundary = true;
			ctx.boxes.push_back(box);
			return;
		}
		a.y = b.y = Math::min(a.y, b.y) - 0.1f;
		hedge_run_piece(ctx, a, b, rng, height);
		return;
	}
	hedge_run_piece(ctx, a, b, rng, height);
}

static void hedge_run_piece(GenContext &ctx, Vector3 a, Vector3 b, Rng &rng, float height, bool add_box) {
	MeshBuilder &m = ctx.boundary_mesh;
	const Vector3 d = b - a;
	const float len = d.length();
	if (len < 0.1f) {
		return;
	}
	const Vector3 along = d / len, up(0, 1, 0), across = along.cross(up); // right-handed, or the core renders inside out
	Transform3D t(Basis(along, up, across), (a + b) * 0.5f);
	// A low dark core fills the gaps; overlapping leafy bushes of varied size make the hedge itself.
	m.set_transform(t);
	m.rounded_box(Vector3(0, height * 0.3f, 0), Vector3(len * 0.5f + 0.3f, height * 0.3f, 0.5f), 0.25f, mat_cell(pal::HEDGE_DARK, mat::HEDGE), mat_cell(pal::HEDGE_DARK, mat::HEDGE));
	m.set_transform(Transform3D());
	for (float s = -len * 0.5f; s <= len * 0.5f + 0.01f; s += rng.range(1.1f, 1.5f)) {
		const float sc = height / 0.92f * rng.range(0.62f, 0.85f);
		// (the yaw is drawn first: the order the call's arguments were always evaluated in, so towns keep their hedges)
		const float yaw = rng.range(0.0f, Math_TAU);
		Vector3 bp = t.xform(Vector3(s, 0, rng.range(-0.25f, 0.25f)));
		if (ctx.terrain) {
			bp.y = ctx.ground(bp.x, bp.z) - 0.05f; // Stage 3b: each bush on the ground
		}
		ctx.add_prop(-1, "bush_round", bp, yaw, sc, 0.3f);
		ctx.props.back().collide = false; // the boundary box below blocks
	}
	// In the open world the hedge only blocks at ground level, so the kid can jump over (through the
	// leafy tops); a standalone neighborhood keeps it as a tall playground wall.
	if (!add_box) {
		return;
	}
	const float wall = ctx.embedded ? 0.6f : 10.0f;
	OBox box;
	box.xf = Transform3D(Basis(along, up, across), (a + b) * 0.5f + up * wall);
	box.half = Vector3(len * 0.5f + 0.3f, wall, 0.7f);
	box.boundary = true;
	ctx.boxes.push_back(box);
}

static void build_boundary_legacy(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_STREET, 99);
	const Road &s = ctx.roads.trunk();
	// Back hedges behind every lot, plus side hedges at the outer ends of each row.
	for (const Lot &lot : ctx.lots) {
		const Road &R = ctx.road_of(lot);
		const float hw = lot.width * 0.5f;
		hedge_run(ctx, lot.to_world(-hw - 0.6f, 0, -lot.depth - 0.6f), lot.to_world(hw + 0.6f, 0, -lot.depth - 0.6f), rng);
		for (float sx : { -1.0f, 1.0f }) {
			const float es = ctx.roads.project(lot.road, lot.to_world(sx * hw, 0, 0)).s_ext; // station of the lot's front corner
			const bool outer = es < R.lots_s0 + 4.0f || es > R.lots_s1 - 4.0f;
			if (outer) {
				hedge_run(ctx, lot.to_world(sx * (hw + 0.6f), 0, -0.4f), lot.to_world(sx * (hw + 0.6f), 0, -lot.depth - 0.6f), rng);
			}
		}
	}
	// Hay-bale walls across both street ends (in the open world the street runs on as a road).
	if (!ctx.embedded) {
		for (float ex : { s.lots_s0 + 0.5f, s.lots_s1 - 0.5f }) {
			const Vector3 c = s.at(ex);
			Vector3 l = s.left_at(ex);
			hay_wall(ctx, c, l, (s.edge() + 1.2f) * 2.0f, rng);
		}
	}
	MeshBuilder &m = ctx.boundary_mesh;
	for (const RoadJunction &j : ctx.roads.junctions(0)) {
		// Side road end: a lantern arch, closed by a hedge unless a road carries on beyond it.
		const Road &lane = ctx.roads.roads[j.child];
		const Vector3 dir = lane.tangent_at(lane.s_end());
		const Vector3 across = lane.left_at(lane.s_end()) * -float(lane.parent_side); // the way the parent runs
		const Vector3 lane_end = lane.at(lane.s_end() + 0.8f);
		if (!ctx.lane_open) {
			hedge_run(ctx, lane_end + across * (-lane.edge() - 1.0f), lane_end + across * (lane.edge() + 1.0f), rng, 3.4f);
		}
		for (float sx : { -1.0f, 1.0f }) {
			m.cylinder(lane_end + across * (sx * 3.2f) + dir * -0.9f, 0.22f, 4.4f, 8, mat_cell(pal::WOOD_DARK, mat::WOOD));
		}
		for (int i = 0; i <= 10; ++i) {
			const float t = float(i) / 10.0f;
			const float ax = Math::lerp(-3.2f, 3.2f, t);
			const float ay = 4.4f + std::sin(t * Math_PI) * 1.2f;
			m.blob(lane_end + across * ax + Vector3(0, ay, 0) + dir * -0.9f, Vector3(0.2f, 0.24f, 0.2f), (i & 1) ? pal::GLOW_LANTERN : pal::GLOW_LANTERN_RED, 0);
		}
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

// ---------------------------------------------------------------- spline street surface (Stage 2a)
// Per road: an asphalt ribbon, and on each side a gutter, curb (top and road face), grass verge and
// scored sidewalk wherever that side isn't cut. A rounded corner where a side road meets its parent (the
// bands turn round it, the asphalt between the corners is one patch), a cul-de-sac's turning circle (the
// asphalt disc, the shoulders flaring into it, the bands round its ring), dashes, drains, manholes and a
// zebra crossing over every side road's mouth. Every vertex sits a fixed lift above its centreline
// point's own height (y = 0 today; Stage 3b gives roads real heights).
namespace {
constexpr float CURB_H = 0.12f, WALK_Y = 0.07f, VERGE_Y = 0.05f, GUTTER_Y = 0.03f, ASPHALT_Y = 0.01f;

// A point on a fillet's arc at fraction t (0: end 0, 1: end 1), `delta` outward from the asphalt edge
// (toward the block), joined exactly onto the straight bands (or the ring) at both ends.
Vector3 fillet_point(const RoadNet &net, const RoadFillet &f, float t, float delta) {
	const float a0 = std::atan2(f.u0.z, f.u0.x);
	float da = std::atan2(f.u1.z, f.u1.x) - a0;
	while (da > Math_PI) {
		da -= Math_TAU;
	}
	while (da < -Math_PI) {
		da += Math_TAU;
	}
	auto arc = [&](float u) {
		const float a = a0 + da * u;
		return f.c + Vector3(std::cos(a), 0.0f, std::sin(a)) * (f.r - delta);
	};
	const Road &R0 = net.roads[f.road0];
	const Vector3 e0 = R0.point(f.s0, f.lat0 * (R0.half_road + delta));
	Vector3 e1;
	if (f.bulb) {
		const Road &B = net.roads[f.road];
		e1 = B.bulb_c + flat(f.c - B.bulb_c) * (B.bulb_r + delta);
	} else {
		const Road &R1 = net.roads[f.road1];
		e1 = R1.point(f.s1, f.lat1 * (R1.half_road + delta));
	}
	Vector3 p = arc(t) + (e0 - arc(0.0f)) * (1.0f - t) + (e1 - arc(1.0f)) * t;
	p.y = Math::lerp(e0.y, e1.y, t);
	return p;
}

float fillet_length(const RoadFillet &f) {
	return f.r * std::acos(Math::clamp(f.u0.dot(f.u1), -1.0f, 1.0f));
}

// Ear-clipping fill of a simple polygon in xz (asphalt patches), facing up.
void fill_polygon(MeshBuilder &m, const std::vector<Vector3> &in, float lift, int cell) {
	std::vector<Vector3> p;
	for (const Vector3 &v : in) {
		if (p.empty() || hdist(v, p.back()) > 1e-3f) {
			p.push_back(v);
		}
	}
	while (p.size() > 1 && hdist(p.front(), p.back()) < 1e-3f) {
		p.pop_back();
	}
	if (p.size() < 3) {
		return;
	}
	float area = 0.0f;
	for (size_t i = 0; i < p.size(); ++i) {
		const Vector3 &a = p[i], &b = p[(i + 1) % p.size()];
		area += a.x * b.z - b.x * a.z;
	}
	const float orient = area >= 0.0f ? 1.0f : -1.0f;
	auto cross = [](const Vector3 &a, const Vector3 &b, const Vector3 &c) { return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x); };
	auto emit = [&](const Vector3 &a, const Vector3 &b, const Vector3 &c) {
		const Vector3 up(0, lift, 0);
		m.tri_out(a + up, b + up, c + up, cell, (a + b + c) / 3.0f - Vector3(0, 1, 0));
	};
	std::vector<int> idx(p.size());
	for (size_t i = 0; i < idx.size(); ++i) {
		idx[i] = int(i);
	}
	while (idx.size() > 3) {
		const int n = int(idx.size());
		bool clipped = false;
		for (int i = 0; i < n && !clipped; ++i) {
			const Vector3 &a = p[idx[(i + n - 1) % n]], &b = p[idx[i]], &c = p[idx[(i + 1) % n]];
			if (cross(a, b, c) * orient <= 1e-7f) {
				continue; // reflex (or flat) corner
			}
			bool empty = true;
			for (int j = 0; j < n && empty; ++j) {
				if (j == i || j == (i + n - 1) % n || j == (i + 1) % n) {
					continue;
				}
				const Vector3 &q = p[idx[j]];
				empty = !(cross(a, b, q) * orient >= 0.0f && cross(b, c, q) * orient >= 0.0f && cross(c, a, q) * orient >= 0.0f);
			}
			if (empty) {
				emit(a, b, c);
				idx.erase(idx.begin() + i);
				clipped = true;
			}
		}
		if (!clipped) {
			for (int i = 1; i + 1 < n; ++i) {
				emit(p[idx[0]], p[idx[i]], p[idx[i + 1]]); // degenerate leftovers: fan them
			}
			return;
		}
	}
	emit(p[idx[0]], p[idx[1]], p[idx[2]]);
}

// [a, b] minus the road's cuts on `side`.
std::vector<std::pair<float, float>> open_runs(const Road &R, int side, float a, float b) {
	std::vector<std::pair<float, float>> runs = { { a, b } };
	for (const BandCut &c : R.cuts) {
		if (c.side != side) {
			continue;
		}
		std::vector<std::pair<float, float>> next;
		for (const auto &r : runs) {
			if (c.s1 <= r.first || c.s0 >= r.second) {
				next.push_back(r);
				continue;
			}
			if (c.s0 > r.first) {
				next.push_back({ r.first, c.s0 });
			}
			if (c.s1 < r.second) {
				next.push_back({ c.s1, r.second });
			}
		}
		runs.swap(next);
	}
	std::vector<std::pair<float, float>> out;
	for (const auto &r : runs) {
		if (r.second - r.first > 0.05f) {
			out.push_back(r);
		}
	}
	return out;
}

// Stations from a to b: both ends exactly, every sample between.
std::vector<float> stations(const Road &R, float a, float b) {
	std::vector<float> st = { a };
	for (float s : R.s) {
		if (s > a + 0.05f && s < b - 0.05f) {
			st.push_back(s);
		}
	}
	st.push_back(b);
	return st;
}

struct StreetMesher {
	GenContext &ctx;
	MeshBuilder &m;
	const RoadNet &net;
	int verge_cell, curb_top, curb_face, gutter;
	StreetMesher(GenContext &c) :
			ctx(c), m(c.street_mesh), net(c.roads) {
		// (Stage 4c: a town's verge is brick pavers, as the old town street had.)
		verge_cell = c.kind == SettlementKind::Town ? mat_cell(pal::BRICK, mat::PAVERS) : mat_cell(pal::LAWN, mat::GRASS);
		curb_top = mat_cell(pal::STONE_LIGHT, mat::CONCRETE);
		curb_face = mat_cell(pal::STONE, mat::CONCRETE);
		gutter = mat_cell(pal::STONE, mat::CONCRETE);
	}
	static int tone(float s) {
		return mat_cell((int(std::floor(s / 2.0f)) & 1) ? pal::SIDEWALK : pal::STONE_LIGHT, mat::CONCRETE);
	}
	static Vector3 lift(Vector3 p, float y) {
		p.y += y;
		return p;
	}
	// Bands across one cross-section pair (inner edge points at offsets in `off`, outward), as quads.
	// P(k, off) gives the point at sample k, `off` outward from the asphalt edge.
	template <typename F>
	void bands(int n, const F &P, const std::vector<float> &along, bool close_start, bool close_end, const Vector3 &out_start, const Vector3 &out_end,
			float curb, float verge, float walk) {
		for (int k = 0; k + 1 < n; ++k) {
			const Vector3 below(0, 1, 0);
			auto Q = [&](float off, float y, int kk) { return lift(P(kk, off), y); };
			m.quad_out(Q(-0.4f, GUTTER_Y, k), Q(-0.4f, GUTTER_Y, k + 1), Q(0.0f, GUTTER_Y, k + 1), Q(0.0f, GUTTER_Y, k), gutter, Q(-0.2f, GUTTER_Y, k) - below);
			m.quad_out(Q(0.0f, CURB_H, k), Q(0.0f, CURB_H, k + 1), Q(curb, CURB_H, k + 1), Q(curb, CURB_H, k), curb_top, Q(curb * 0.5f, CURB_H, k) - below);
			m.quad_out(Q(0.0f, 0.0f, k), Q(0.0f, 0.0f, k + 1), Q(0.0f, CURB_H, k + 1), Q(0.0f, CURB_H, k), curb_face, Q(1.0f, CURB_H * 0.5f, k));
			m.quad_out(Q(curb, VERGE_Y, k), Q(curb, VERGE_Y, k + 1), Q(curb + verge, VERGE_Y, k + 1), Q(curb + verge, VERGE_Y, k), verge_cell,
					Q(curb + verge * 0.5f, VERGE_Y, k) - below);
			m.quad_out(Q(curb + verge, WALK_Y, k), Q(curb + verge, WALK_Y, k + 1), Q(curb + walk, WALK_Y, k + 1), Q(curb + walk, WALK_Y, k),
					tone((along[k] + along[k + 1]) * 0.5f), Q(curb + (verge + walk) * 0.5f, WALK_Y, k) - below);
		}
		// Close the curb, verge and sidewalk where a street runs out into the country road.
		for (int e = 0; e < 2; ++e) {
			if (!(e == 0 ? close_start : close_end) || n < 2) {
				continue;
			}
			const int k = e == 0 ? 0 : n - 1;
			const Vector3 out = e == 0 ? out_start : out_end;
			const float b[3][3] = { { 0.0f, curb, CURB_H }, { curb, curb + verge, VERGE_Y }, { curb + verge, curb + walk, WALK_Y } };
			for (const auto &bb : b) {
				m.quad_out(lift(P(k, bb[0]), -0.05f), lift(P(k, bb[1]), -0.05f), lift(P(k, bb[1]), bb[2]), lift(P(k, bb[0]), bb[2]),
						bb[2] == VERGE_Y ? verge_cell : tone(along[k]), lift(P(k, (bb[0] + bb[1]) * 0.5f), bb[2] * 0.5f) - out);
			}
		}
	}

	void road_sides(int r) {
		const Road &R = net.roads[r];
		const float a = R.s_begin(), b = R.s_end();
		for (int side : { 1, -1 }) {
			for (const auto &run : open_runs(R, side, a, b)) {
				const std::vector<float> st = stations(R, run.first, run.second);
				auto P = [&](int k, float off) { return R.point(st[k], float(side) * (R.half_road + off)); };
				// (Stage 4c: a town side street's dead end is closed off like a port end, a hedge across it.)
				const bool cs = (R.start_kind == RoadEnd::Port || R.start_kind == RoadEnd::DeadEnd) && run.first <= a + 0.01f;
				const bool ce = (R.end_kind == RoadEnd::Port || R.end_kind == RoadEnd::DeadEnd) && run.second >= b - 0.01f;
				bands(int(st.size()), P, st, cs, ce, -R.tangent_at(a), R.tangent_at(b), R.curb, R.verge, R.walk);
			}
		}
	}

	void asphalt(int r) {
		const Road &R = net.roads[r];
		const float a = R.parent >= 0 ? R.asphalt_s : R.s_begin();
		const float b = R.bulb() ? R.bulb_s : R.s_end();
		std::vector<Vector3> c, l;
		for (float s : stations(R, a, b)) {
			c.push_back(R.at(s));
			l.push_back(R.left_at(s));
		}
		m.ribbon_lift(c, l, -R.half_road, R.half_road, ASPHALT_Y, mat_cell(pal::ASPHALT, mat::ASPHALT));
	}

	void fillet_bands(const RoadFillet &f) {
		const int n = Math::max(6, int(fillet_length(f) / 0.8f)) + 1;
		std::vector<float> along(n);
		for (int k = 0; k < n; ++k) {
			along[k] = float(k) * 0.8f;
		}
		auto P = [&](int k, float off) { return fillet_point(net, f, float(k) / float(n - 1), off); };
		bands(n, P, along, false, false, Vector3(), Vector3(), f.curb, f.verge, f.walk);
	}

	// The asphalt between a side road's two corners, from its parent's edge to its own ribbon.
	void junction_patch(int b) {
		const Road &B = net.roads[b];
		const Road &P = net.roads[B.parent];
		const RoadFillet *f[2] = { nullptr, nullptr };
		for (const RoadFillet &x : net.fillets) {
			if (x.road == b && !x.bulb) {
				f[f[0] ? 1 : 0] = &x;
			}
		}
		if (!f[0] || !f[1]) {
			return;
		}
		if (f[0]->s0 > f[1]->s0) {
			std::swap(f[0], f[1]); // f[0]: the corner further back along the parent
		}
		const int sd = B.parent_side;
		std::vector<Vector3> poly;
		for (float s : stations(P, f[0]->s0, f[1]->s0)) {
			poly.push_back(P.point(s, float(sd) * P.half_road));
		}
		const int n1 = Math::max(6, int(fillet_length(*f[1]) / 0.8f));
		for (int k = 1; k <= n1; ++k) {
			poly.push_back(fillet_point(net, *f[1], float(k) / n1, 0.0f));
		}
		for (float s : stations(B, f[1]->s1, B.asphalt_s)) {
			poly.push_back(B.point(s, f[1]->lat1 * B.half_road));
		}
		std::vector<float> back = stations(B, f[0]->s1, B.asphalt_s);
		for (int k = int(back.size()) - 1; k >= 0; --k) {
			poly.push_back(B.point(back[k], f[0]->lat1 * B.half_road));
		}
		const int n0 = Math::max(6, int(fillet_length(*f[0]) / 0.8f));
		for (int k = n0 - 1; k >= 1; --k) {
			poly.push_back(fillet_point(net, *f[0], float(k) / n0, 0.0f));
		}
		fill_polygon(m, poly, ASPHALT_Y, mat_cell(pal::ASPHALT, mat::ASPHALT));
	}

	// The turning circle: the asphalt disc with its shoulders, and the bands round its ring.
	void bulb(int b) {
		const Road &B = net.roads[b];
		const RoadFillet *f[2] = { nullptr, nullptr }; // [0]: the left (+lateral) shoulder
		for (const RoadFillet &x : net.fillets) {
			if (x.road == b && x.bulb) {
				f[x.lat0 > 0.0f ? 0 : 1] = &x;
			}
		}
		if (!f[0] || !f[1]) {
			return;
		}
		const Vector3 te = B.tangent_at(B.s_end()), le = B.left_at(B.s_end());
		const float a_hi = float(Math_PI) - B.bulb_entry, a_lo = -a_hi; // the ring, the long way round
		const int nr = Math::max(24, int((a_hi - a_lo) * (B.bulb_r + 1.5f) / 0.9f));
		auto ring = [&](int k, float off) {
			const float a = Math::lerp(a_hi, a_lo, float(k) / float(nr));
			Vector3 p = B.bulb_c + (te * std::cos(a) + le * std::sin(a)) * (B.bulb_r + off);
			p.y = B.bulb_c.y;
			return p;
		};
		std::vector<Vector3> poly;
		const int nf = Math::max(6, int(fillet_length(*f[0]) / 0.8f));
		for (int k = 0; k <= nf; ++k) {
			poly.push_back(fillet_point(net, *f[0], float(k) / nf, 0.0f));
		}
		for (int k = 1; k < nr; ++k) {
			poly.push_back(ring(k, 0.0f));
		}
		for (int k = nf; k >= 0; --k) {
			poly.push_back(fillet_point(net, *f[1], float(k) / nf, 0.0f));
		}
		fill_polygon(m, poly, ASPHALT_Y, mat_cell(pal::ASPHALT, mat::ASPHALT));
		std::vector<float> along(nr + 1);
		for (int k = 0; k <= nr; ++k) {
			along[k] = float(k) * (a_hi - a_lo) * (B.bulb_r + B.curb + B.verge) / float(nr);
		}
		bands(nr + 1, ring, along, false, false, Vector3(), Vector3(), B.curb, B.verge, B.walk);
	}

	void markings(int r, Rng &drng) {
		const Road &R = net.roads[r];
		const float hr = R.half_road;
		const float a = R.parent >= 0 ? R.asphalt_s + 1.0f : R.s_begin();
		const float b = (R.bulb() ? R.bulb_s - 1.0f : R.s_end()) - 2.0f;
		auto near_cut = [&](float x, float pad) {
			for (const BandCut &c : R.cuts) {
				if (x > c.s0 - pad && x < c.s1 + pad && c.s0 > R.s_begin()) {
					return true;
				}
			}
			return false;
		};
		// Dashed centre line, broken at every side road's mouth.
		for (float x = a; x < b; x += 4.0f) {
			if (near_cut(x, 1.0f) || near_cut(x + 2.0f, 1.0f)) {
				continue;
			}
			const Vector3 p = R.at(x), q = R.at(x + 2.0f);
			const Vector3 lp = R.left_at(x), lq = R.left_at(x + 2.0f);
			m.quad_out(lift(p - lp * 0.09f, 0.035f), lift(q - lq * 0.09f, 0.035f), lift(q + lq * 0.09f, 0.035f), lift(p + lp * 0.09f, 0.035f), pal::ROAD_LINE,
					(p + q) * 0.5f - Vector3(0, 1, 0));
		}
		// Storm drains in the gutters, manhole covers in the lanes.
		for (int side : { 1, -1 }) {
			for (float x = R.s_begin() + drng.range(6.0f, 12.0f); x < R.s_end() - 2.0f; x += drng.range(16.0f, 22.0f)) {
				if (R.cut(side, x) || R.cut(side, x - 1.0f) || R.cut(side, x + 1.0f) || (R.parent >= 0 && x < R.asphalt_s + 1.0f) || (R.bulb() && x > R.bulb_s - 1.0f)) {
					continue;
				}
				const Vector3 p = R.at(x) + R.left_at(x) * (float(side) * (hr - 0.25f));
				const Vector3 t = R.tangent_at(x);
				const Vector3 lf = R.left_at(x);
				for (int k = -2; k <= 2; ++k) {
					m.beam(p + t * (k * 0.16f) - lf * 0.2f + Vector3(0, 0.02f, 0), p + t * (k * 0.16f) + lf * 0.2f + Vector3(0, 0.02f, 0), 0.06f, 0.02f,
							mat_cell(pal::IRON, mat::METAL));
				}
				m.beam(p - t * 0.42f + Vector3(0, 0.016f, 0), p + t * 0.42f + Vector3(0, 0.016f, 0), 0.48f, 0.01f, pal::CHARCOAL);
			}
		}
		for (float x = a + 9.0f; x < b; x += drng.range(24.0f, 34.0f)) {
			if (near_cut(x, 2.0f)) {
				continue;
			}
			const Vector3 p = R.at(x) + R.left_at(x) * drng.range(-1.5f, 1.5f);
			m.cylinder(p, 0.38f, 0.03f, 14, mat_cell(pal::IRON, mat::METAL));
			m.cylinder(p, 0.44f, 0.022f, 14, mat_cell(pal::STONE, mat::CONCRETE));
		}
		if (R.parent >= 0) {
			// Zebra crossing over the mouth, on the side road's own asphalt just past the corners.
			const float tc = R.asphalt_s + 1.4f;
			const Vector3 o = R.at(tc), across = R.left_at(tc), fwd = R.tangent_at(tc);
			for (int k = -3; k <= 3; ++k) {
				const Vector3 c = o + across * (k * 1.1f) + Vector3(0, 0.035f, 0);
				m.quad_out(c - across * 0.3f - fwd * 0.9f, c + across * 0.3f - fwd * 0.9f, c + across * 0.3f + fwd * 0.9f, c - across * 0.3f + fwd * 0.9f,
						mat_cell(pal::TRIM_WHITE, mat::CONCRETE), c - Vector3(0, 1, 0));
			}
		}
	}
};
} // namespace

static void build_street_hood(GenContext &ctx) {
	StreetMesher sm(ctx);
	Rng drng = ctx.rng(STAGE_STREET, 7);
	for (int r = 0; r < int(ctx.roads.roads.size()); ++r) {
		sm.asphalt(r);
		sm.road_sides(r);
		if (ctx.roads.roads[r].parent >= 0) {
			sm.junction_patch(r);
		}
		if (ctx.roads.roads[r].bulb()) {
			sm.bulb(r);
		}
		sm.markings(r, drng);
	}
	for (const RoadFillet &f : ctx.roads.fillets) {
		sm.fillet_bands(f);
	}
}

void build_street(GenContext &ctx) {
	if (ctx.kind == SettlementKind::Town && !ctx.terrain && !ctx.roads.trunk().x_station) {
		plan_hood_terrain(ctx); // (a town with no shop to trigger it from build_shop: still graded as WorldPlan grades it)
	}
	if (ctx.roads.trunk().x_station) {
		build_street_legacy(ctx); // the pre-Stage-4c town street (towns run on spline roads now)
		return;
	}
	build_street_hood(ctx);
}

// ---------------------------------------------------------------- spline boundary (Stage 2a)
// Hedges behind every lot and down the open sides at the ends of each row (never into another lot, a
// corridor or the country road), the lantern arch over the N/S port road, and (standalone) hay-bale walls
// across every street end plus the distant house silhouettes.
static void build_boundary_hood(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_STREET, 99);
	const RoadNet &net = ctx.roads;
	struct Run {
		Vector3 a, d;
	};
	std::vector<Run> runs;
	for (const Road &R : net.roads) {
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) == RoadEnd::Port) {
				runs.push_back({ at_end ? R.pts.back() : R.pts.front(), flat(R.end_dir(at_end)) });
			}
		}
	}
	auto free_at = [&](const Vector3 &p, int self) {
		if (net.zone(p) != RoadZone::Off) {
			return false;
		}
		for (const Run &r : runs) {
			const Vector3 v = p - r.a;
			const float along = v.x * r.d.x + v.z * r.d.z;
			if (along > -1.0f && std::abs(v.x * r.d.z - v.z * r.d.x) < 7.5f) {
				return false;
			}
		}
		for (const Lot &o : ctx.lots) {
			if (o.index == self) {
				continue;
			}
			const Vector3 l = o.to_local(p);
			if (std::abs(l.x) < o.width * 0.5f + 0.4f && l.z < 0.4f && l.z > -o.depth - 0.4f) {
				return false;
			}
		}
		if (ctx.square.valid) {
			// (Stage 4c: the town square stays open to the shops beside it.)
			const Lot &o = ctx.square.area;
			const Vector3 l = o.to_local(p);
			if (std::abs(l.x) < o.width * 0.5f + 0.4f && l.z < 0.4f && l.z > -o.depth - 0.4f) {
				return false;
			}
		}
		return true;
	};
	// A hedge from a to b, left out wherever it would stand in another lot, a corridor or a road.
	auto trimmed = [&](const Vector3 &a, const Vector3 &b, int self) {
		const float len = a.distance_to(b);
		const int n = Math::max(1, int(len / 0.5f));
		int start = -1;
		for (int k = 0; k <= n; ++k) {
			const bool ok = free_at(a.lerp(b, float(k) / n), self);
			if (ok && start < 0) {
				start = k;
			}
			if ((!ok || k == n) && start >= 0) {
				const int end = ok ? k : k - 1;
				if (float(end - start) * len / n >= 1.5f) {
					hedge_run(ctx, a.lerp(b, float(start) / n), a.lerp(b, float(end) / n), rng);
				}
				start = -1;
			}
		}
	};
	for (const Lot &lot : ctx.lots) {
		const float hw = lot.width * 0.5f;
		trimmed(lot.to_world(-hw - 0.6f, 0, -lot.depth - 0.6f), lot.to_world(hw + 0.6f, 0, -lot.depth - 0.6f), lot.index);
		for (float sx : { -1.0f, 1.0f }) {
			// Down a side no neighbour touches (a row's end, a corner, the far side of a wedge garden).
			const int nb = (sx > 0.0f) == (lot.side > 0) ? lot.prev : lot.next;
			bool touching = false;
			if (nb >= 0) {
				const Lot &o = ctx.lots[nb];
				for (float lz : { -0.5f, -lot.depth + 0.5f }) {
					const Vector3 l = o.to_local(lot.to_world(sx * hw, 0, lz));
					const float dx = Math::max(0.0f, std::abs(l.x) - o.width * 0.5f), dz = Math::max(0.0f, Math::max(l.z, -o.depth - l.z));
					touching = touching || Vector2(dx, dz).length() < 1.0f;
				}
			}
			// (Stage 4c: not beside a shop: its side is a wall, and the service yards behind a row stay open at the
			// row's ends, so they can be reached from the street.)
			if (!touching && lot.shop < 0) {
				trimmed(lot.to_world(sx * (hw + 0.6f), 0, -0.4f), lot.to_world(sx * (hw + 0.6f), 0, -lot.depth - 0.6f), lot.index);
			}
		}
	}
	MeshBuilder &m = ctx.boundary_mesh;
	for (const Road &R : net.roads) {
		if (R.parent < 0 || (R.end_kind != RoadEnd::Port && R.end_kind != RoadEnd::DeadEnd)) {
			continue;
		}
		// The N/S port road leaves under a lantern arch (the country road carries on); a town's dead-end side
		// street (Stage 4c) ends under one too, a hedge across the road beyond it.
		const bool dead = R.end_kind == RoadEnd::DeadEnd;
		const float at = dead ? R.s_end() - 1.0f : R.lots_s1 - 1.5f;
		const Vector3 dir = R.tangent_at(at), across = R.left_at(at), base = R.at(at);
		if (dead) {
			const Vector3 e = R.at(R.s_end()) + R.tangent_at(R.s_end()) * 0.9f;
			const Vector3 l = R.left_at(R.s_end());
			hedge_run(ctx, e - l * (R.edge() + 1.0f), e + l * (R.edge() + 1.0f), rng, 3.4f);
		}
		for (float sx : { -1.0f, 1.0f }) {
			m.cylinder(base + across * (sx * 3.2f), 0.22f, 4.4f, 8, mat_cell(pal::WOOD_DARK, mat::WOOD));
		}
		for (int i = 0; i <= 10; ++i) {
			const float t = float(i) / 10.0f;
			const float ay = 4.4f + std::sin(t * Math_PI) * 1.2f;
			m.blob(base + across * Math::lerp(-3.2f, 3.2f, t) + Vector3(0, ay, 0), Vector3(0.2f, 0.24f, 0.2f), (i & 1) ? pal::GLOW_LANTERN : pal::GLOW_LANTERN_RED, 0);
		}
		(void)dir;
	}
	if (ctx.embedded) {
		return;
	}
	// Standalone: hay-bale walls across every street end, distant silhouettes beyond the hedges.
	for (const Road &R : net.roads) {
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) != RoadEnd::Port) {
				continue;
			}
			const float ex = at_end ? R.lots_s1 - 0.5f : R.lots_s0 + 0.5f;
			hay_wall(ctx, R.at(ex), R.left_at(ex), (R.edge() + 1.2f) * 2.0f, rng);
		}
	}
	for (int side : { 1, -1 }) {
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

void build_boundary(GenContext &ctx) {
	if (ctx.roads.trunk().x_station) {
		build_boundary_legacy(ctx); // the pre-Stage-4c town street
		return;
	}
	build_boundary_hood(ctx);
}

} // namespace tot
