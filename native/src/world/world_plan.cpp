#include "world/world_plan.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <algorithm>
#include <cmath>

#include "core/noise.h"
#include "gen/context.h"

using namespace godot;

namespace tot {

namespace {

constexpr float SETTLE_BLEND = 26.0f; // hills -> settlement plateau
constexpr float JUNCTION_FLAT = 9.0f, JUNCTION_BLEND = 16.0f;

float smooth01(float e0, float e1, float x) {
	const float t = Math::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

const char *NAME_A[] = { "Maple", "Pumpkin", "Willow", "Cider", "Lantern", "Acorn", "Bramble", "Moonbeam", "Hazel", "Crow's", "Candlewick", "Owl",
	"Thistle", "Cobweb", "Gingersnap", "Hollyhock", "Mossy", "Toffee", "Juniper", "Fiddle" };
const char *NAME_HOOD[] = { "Lane", "Hollow", "Court", "Row", "Drive", "Way", "Close", "Gardens" };
const char *NAME_TOWN[] = { "ville", " Falls", " Crossing", " Market", " Junction", "ton", " Square", " Mills" };

} // namespace

void WorldPlan::reset(uint64_t seed) {
	seed_ = seed;
	base_.clear();
	regions_.clear();
	roads_.clear();
}

void WorldPlan::region_of(float x, float z, int &i, int &j) {
	i = int(std::floor(x / REGION + 0.5f));
	j = int(std::floor(z / REGION + 0.5f));
}

float WorldPlan::natural_low(float x, float z) const {
	return 10.0f * noise::fbm(seed_ ^ 0x11, x / 320.0f, z / 320.0f, 2);
}

float WorldPlan::natural(float x, float z) const {
	return natural_low(x, z) + 2.6f * noise::fbm(seed_ ^ 0x22, x / 60.0f, z / 60.0f, 3);
}

float WorldPlan::forest_density(float x, float z) const {
	return smooth01(-0.35f, 0.3f, noise::fbm(seed_ ^ 0x33, x / 90.0f, z / 90.0f, 3));
}

// Kind, footprint and street ports, without anything that depends on the neighbours.
const RegionPlan &WorldPlan::base(int i, int j) {
	const uint64_t k = key(i, j);
	auto it = base_.find(k);
	if (it != base_.end()) {
		return it->second;
	}
	RegionPlan r;
	r.i = i;
	r.j = j;
	r.seed = sub_seed(seed_, 0x5E77, uint64_t(int64_t(i) + 100000), uint64_t(int64_t(j) + 100000));
	Rng rng(r.seed, 17);
	if (i == 0 && j == 0) {
		r.kind = RegionKind::Neighborhood;
	} else {
		const float roll = rng.f();
		r.kind = roll < 0.35f ? RegionKind::Woods : (roll < 0.75f ? RegionKind::Neighborhood : RegionKind::Town);
	}
	const Vector3 center(float(i) * REGION + rng.range(-14.0f, 14.0f), 0.0f, float(j) * REGION + rng.range(-14.0f, 14.0f));
	r.origin = Vector3(center.x, natural(center.x, center.z), center.z);
	if (r.settlement()) {
		// Plan just the street and lots (cheap and deterministic) to know the footprint and exits.
		GenContext ctx;
		ctx.seed = r.seed;
		ctx.kind = r.kind == RegionKind::Town ? SettlementKind::Town : SettlementKind::Neighborhood;
		ctx.embedded = true;
		plan_street(ctx);
		if (r.kind == RegionKind::Town) {
			plan_town_lots(ctx);
		} else {
			plan_lots(ctx);
		}
		const StreetPlan &s = ctx.street;
		r.x0 = r.origin.x + ctx.bound_x0 - 3.0f;
		r.x1 = r.origin.x + ctx.bound_x1 + 3.0f;
		r.z0 = r.origin.z + ctx.bound_z0 - 4.0f;
		r.z1 = r.origin.z + ctx.bound_z1 + 4.0f;
		r.spawn = r.origin + ctx.spawn;
		r.spawn_yaw = ctx.spawn_yaw;
		auto port = [&](Vector3 local, Vector3 dir) {
			Port p;
			p.valid = true;
			p.pos = r.origin + Vector3(local.x, 0.0f, local.z);
			p.dir = Vector3(dir.x, 0.0f, dir.z).normalized();
			return p;
		};
		const Vector3 e = s.center.back(), w = s.center.front();
		r.ports[PORT_E] = port(e, e - s.center[s.center.size() - 3]);
		r.ports[PORT_W] = port(w, w - s.center[2]);
		const Vector3 lane_end = s.lane_origin + Vector3(0, 0, float(s.lane_dir) * (s.lane_len + 0.5f));
		r.lane_side = s.lane_dir > 0 ? PORT_N : PORT_S;
		r.ports[r.lane_side] = port(lane_end, Vector3(0, 0, float(s.lane_dir)));
		const char **suffix = r.kind == RegionKind::Town ? NAME_TOWN : NAME_HOOD;
		r.name = std::string(NAME_A[rng.irange(0, 19)]) + (r.kind == RegionKind::Town ? "" : " ") + suffix[rng.irange(0, 7)];
	} else {
		static const Vector3 dirs[4] = { Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 0, 1), Vector3(0, 0, -1) };
		for (int s = 0; s < 4; ++s) {
			r.ports[s].valid = true;
			r.ports[s].pos = r.origin;
			r.ports[s].dir = dirs[s];
		}
		r.name = std::string(NAME_A[rng.irange(0, 19)]) + " Woods";
	}
	return base_.emplace(k, r).first->second;
}

const RegionPlan &WorldPlan::region(int i, int j) {
	const uint64_t k = key(i, j);
	auto it = regions_.find(k);
	if (it != regions_.end()) {
		return it->second;
	}
	RegionPlan r = base(i, j);
	if (r.settlement()) {
		const int nj = r.lane_side == PORT_N ? j + 1 : j - 1;
		const RegionPlan &n = base(i, nj);
		r.lane_open = n.ports[r.lane_side == PORT_N ? PORT_S : PORT_N].valid;
	}
	return regions_.emplace(k, r).first->second;
}

const RoadPlan *WorldPlan::road(int i0, int j0, int i1, int j1) {
	if (i1 < i0 || j1 < j0) {
		std::swap(i0, i1);
		std::swap(j0, j1);
	}
	const bool horizontal = i1 == i0 + 1 && j1 == j0;
	const bool vertical = j1 == j0 + 1 && i1 == i0;
	if (!horizontal && !vertical) {
		return nullptr;
	}
	const uint64_t k = hash64(key(i0, j0) * 3 + (horizontal ? 1 : 2));
	auto it = roads_.find(k);
	if (it != roads_.end()) {
		return it->second.get();
	}
	const RegionPlan &a = base(i0, j0);
	const RegionPlan &b = base(i1, j1);
	const Port &pa = a.ports[horizontal ? PORT_E : PORT_N];
	const Port &pb = b.ports[horizontal ? PORT_W : PORT_S];
	std::unique_ptr<RoadPlan> road;
	if (pa.valid && pb.valid) {
		Port qa = pa, qb = pb;
		qa.pos.y = a.origin.y;
		qb.pos.y = b.origin.y;
		road = std::make_unique<RoadPlan>();
		build_road(*road, qa, qb, k);
	}
	return roads_.emplace(k, std::move(road)).first->second.get();
}

// Cubic curve leaving each port along its direction, with a lazy meander, then a height profile
// that eases between the two level ends and follows the big swells (never the small bumps).
void WorldPlan::build_road(RoadPlan &road, const Port &a, const Port &b, uint64_t k) {
	const float len = a.pos.distance_to(b.pos);
	const Vector3 c0 = a.pos + a.dir * len * 0.38f, c1 = b.pos + b.dir * len * 0.38f;
	const int n = std::max(4, int(len / 3.0f));
	const float wobble = 5.0f * noise::perlin(seed_ ^ k, 0.37f, 0.71f) * 2.0f;
	std::vector<Vector3> pts;
	for (int s = 0; s <= n; ++s) {
		const float t = float(s) / n, u = 1.0f - t;
		Vector3 p = a.pos * (u * u * u) + c0 * (3.0f * u * u * t) + c1 * (3.0f * u * t * t) + b.pos * (t * t * t);
		const Vector3 tan = (c0 - a.pos) * (3.0f * u * u) + (c1 - c0) * (6.0f * u * t) + (b.pos - c1) * (3.0f * t * t);
		const Vector3 side = Vector3(-tan.z, 0, tan.x).normalized();
		p += side * (std::sin(t * Math_PI) * wobble * std::sin(t * Math_PI * 2.0f + 0.6f));
		const float base_y = Math::lerp(a.pos.y, b.pos.y, t);
		const float dev = natural_low(p.x, p.z) - Math::lerp(natural_low(a.pos.x, a.pos.z), natural_low(b.pos.x, b.pos.z), t);
		p.y = base_y + dev * std::sin(t * Math_PI) * 0.8f;
		pts.push_back(p);
	}
	// Smooth the grade.
	for (int pass = 0; pass < 3; ++pass) {
		std::vector<Vector3> q = pts;
		for (size_t s = 1; s + 1 < pts.size(); ++s) {
			q[s].y = (pts[s - 1].y + pts[s].y * 2.0f + pts[s + 1].y) * 0.25f;
		}
		pts.swap(q);
	}
	road.pts = pts;
	road.x0 = road.z0 = 1e9f;
	road.x1 = road.z1 = -1e9f;
	for (const Vector3 &p : pts) {
		road.x0 = Math::min(road.x0, p.x);
		road.x1 = Math::max(road.x1, p.x);
		road.z0 = Math::min(road.z0, p.z);
		road.z1 = Math::max(road.z1, p.z);
	}
}

void WorldPlan::nearby_roads(float x, float z, const RoadPlan **out, int &count) {
	int i, j;
	region_of(x, z, i, j);
	count = 0;
	for (int di = -1; di <= 1; ++di) {
		for (int dj = -1; dj <= 1; ++dj) {
			const RoadPlan *cand[2] = { road(i + di, j + dj, i + di + 1, j + dj), road(i + di, j + dj, i + di, j + dj + 1) };
			for (const RoadPlan *r : cand) {
				if (!r) {
					continue;
				}
				const float m = 40.0f;
				if (x < r->x0 - m || x > r->x1 + m || z < r->z0 - m || z > r->z1 + m) {
					continue;
				}
				out[count++] = r;
			}
		}
	}
}

namespace {
// Distance from p to the polyline (xz), with the interpolated centreline height.
float polyline_distance(const std::vector<Vector3> &pts, float x, float z, float &y) {
	float best = 1e18f;
	y = 0.0f;
	for (size_t s = 0; s + 1 < pts.size(); ++s) {
		const Vector3 &a = pts[s], &b = pts[s + 1];
		const float abx = b.x - a.x, abz = b.z - a.z;
		const float l2 = abx * abx + abz * abz;
		const float t = l2 > 1e-6f ? Math::clamp(((x - a.x) * abx + (z - a.z) * abz) / l2, 0.0f, 1.0f) : 0.0f;
		const float px = a.x + abx * t - x, pz = a.z + abz * t - z;
		const float d2 = px * px + pz * pz;
		if (d2 < best) {
			best = d2;
			y = Math::lerp(a.y, b.y, t);
		}
	}
	return std::sqrt(best);
}
} // namespace

float WorldPlan::road_distance(float x, float z) {
	const RoadPlan *roads[18];
	int n = 0;
	nearby_roads(x, z, roads, n);
	float best = 1e9f;
	for (int k = 0; k < n; ++k) {
		float y;
		best = Math::min(best, polyline_distance(roads[k]->pts, x, z, y));
	}
	return best;
}

float WorldPlan::settlement_mask(float x, float z) {
	int i, j;
	region_of(x, z, i, j);
	float m = 0.0f;
	for (int di = -1; di <= 1; ++di) {
		for (int dj = -1; dj <= 1; ++dj) {
			const RegionPlan &r = base(i + di, j + dj);
			if (!r.settlement()) {
				continue;
			}
			const float dx = Math::max(0.0f, Math::max(r.x0 - x, x - r.x1));
			const float dz = Math::max(0.0f, Math::max(r.z0 - z, z - r.z1));
			m = Math::max(m, 1.0f - smooth01(0.0f, SETTLE_BLEND, std::sqrt(dx * dx + dz * dz)));
		}
	}
	return m;
}

float WorldPlan::height(float x, float z) {
	float h = natural(x, z);
	int i, j;
	region_of(x, z, i, j);
	for (int di = -1; di <= 1; ++di) {
		for (int dj = -1; dj <= 1; ++dj) {
			const RegionPlan &r = base(i + di, j + dj);
			float m;
			if (r.settlement()) {
				const float dx = Math::max(0.0f, Math::max(r.x0 - x, x - r.x1));
				const float dz = Math::max(0.0f, Math::max(r.z0 - z, z - r.z1));
				m = 1.0f - smooth01(0.0f, SETTLE_BLEND, std::sqrt(dx * dx + dz * dz));
			} else {
				m = 1.0f - smooth01(JUNCTION_FLAT, JUNCTION_FLAT + JUNCTION_BLEND, Vector2(x - r.origin.x, z - r.origin.z).length());
			}
			if (m > 0.0f) {
				h = Math::lerp(h, r.origin.y, m);
			}
		}
	}
	const RoadPlan *roads[18];
	int n = 0;
	nearby_roads(x, z, roads, n);
	for (int k = 0; k < n; ++k) {
		float y;
		const float d = polyline_distance(roads[k]->pts, x, z, y);
		const float m = 1.0f - smooth01(ROAD_HALF + 0.8f, ROAD_HALF + 9.0f, d);
		if (m > 0.0f) {
			h = Math::lerp(h, y, m);
		}
	}
	return h;
}

} // namespace tot
