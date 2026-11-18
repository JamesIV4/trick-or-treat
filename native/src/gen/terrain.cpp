// Stage 3b: rolling hills under a neighbourhood. The roads are graded from the natural relief (<= 8%,
// level through junctions and turning circles), every lot gets a level pad, and one heightfield on the
// open world's terrain lattice carries the cut and fill: the road corridors at road height, each lot at
// its pad (a bank up or down to the sidewalk at its front, a soft step to its neighbours, a little of the
// natural roll kept in the yard), embankments out to the natural hills between and around them.
// Plan-side only (no engine calls, no shared state): WorldPlan::base() runs it too, on worker threads.

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

#include "core/noise.h"
#include "gen/context.h"

using namespace godot;

namespace tot {

namespace {
// Unscaled relief: the broad hills and dips, a long gentle swell under them, and a faint roll.
float relief_raw(const HoodTerrain &t, float x, float z) {
	float v = 0.45f * noise::fbm(t.seed ^ 0x3B11u, x / 130.0f, z / 130.0f, 2) + 0.08f * noise::fbm(t.seed ^ 0x3B22u, x / 38.0f, z / 38.0f, 2);
	for (const HoodTerrain::Swell &s : t.swells) {
		const float dx = x - s.x, dz = z - s.z;
		v += s.a * std::exp(-(dx * dx + dz * dz) / (s.r * s.r));
	}
	return v;
}
} // namespace

float hood_natural(const HoodTerrain &t, float x, float z) {
	return t.bias + t.scale * relief_raw(t, x, z);
}

float HoodTerrain::vertex(int ix, int iz) const {
	if (ix < 0 || iz < 0 || ix >= nx || iz >= nz) {
		return hood_natural(*this, x0 + float(ix) * TERRAIN_LATTICE, z0 + float(iz) * TERRAIN_LATTICE);
	}
	return h[size_t(iz) * size_t(nx) + size_t(ix)];
}

float HoodTerrain::at(float x, float z) const {
	// The same triangles as the terrain chunks (WorldStreamer::build_chunk): each lattice square split on
	// its (x, z) - (x + 1, z + 1) diagonal.
	const float fx = (x - x0) / TERRAIN_LATTICE, fz = (z - z0) / TERRAIN_LATTICE;
	const int ix = int(std::floor(fx)), iz = int(std::floor(fz));
	const float u = fx - float(ix), v = fz - float(iz);
	const float h00 = vertex(ix, iz), h10 = vertex(ix + 1, iz), h01 = vertex(ix, iz + 1), h11 = vertex(ix + 1, iz + 1);
	return u >= v ? h00 + (h10 - h00) * u + (h11 - h10) * v : h00 + (h11 - h01) * u + (h01 - h00) * v;
}

float GenContext::lot_ground(const Lot &lot, float lx, float lz) const {
	if (!terrain) {
		return 0.0f;
	}
	const Vector3 w = lot.to_world(lx, 0.0f, lz);
	return terrain->at(w.x, w.z) - lot.xf.origin.y;
}

bool GenContext::in_structure(const Lot &lot, float lx, float lz, float pad) const {
	const HouseSpec &h = lot.house;
	const float zf = h.front_z, zb = h.front_z - h.depth;
	auto in = [&](float x0, float x1, float z0, float z1) { return lx > x0 - pad && lx < x1 + pad && lz > z0 - pad && lz < z1 + pad; };
	if (lot.shop == -2) {
		return false; // the town square (Stage 4c): all open paving
	}
	if (lot.shop >= 0) {
		// Stage 4c: the shop on its plinth (the body and the paved strip in front of it, out to the lot front),
		// and the doorstep with its treads down onto the sidewalk (build_shop).
		const float tz = zf + 0.9f + 0.32f * float(Math::max(0, lot.porch_steps - 3));
		return in(-h.width * 0.5f - 0.05f, h.width * 0.5f + 0.05f, zb - 0.15f, 0.0f) || in(h.door_x - 1.5f, h.door_x + 1.5f, 0.0f, tz);
	}
	if (in(h.house_x - h.width * 0.5f - 0.15f, h.house_x + h.width * 0.5f + 0.15f, zb - 0.15f, zf)) {
		return true;
	}
	// Porch deck and its wraparound flight (3 treads, each lower one wider by a tread each side). Extra treads
	// below them (Stage 3b) are yard_lift's business: things there stand on them.
	const float flight = 0.36f * 3.0f;
	const float fw = 0.85f + 0.36f * 2.0f;
	if (in(h.porch_x - h.porch_w * 0.5f, h.porch_x + h.porch_w * 0.5f, zf, zf + h.porch_d) || in(h.door_x - fw, h.door_x + fw, zf + h.porch_d, zf + h.porch_d + flight)) {
		return true;
	}
	if (h.garage) {
		const float g = float(h.garage_side);
		const float gx = h.house_x + g * (h.width * 0.5f + 3.8f * 0.5f);
		const float gz1 = zf - 0.4f, gz0 = gz1 - 6.0f;
		if (in(gx - 1.98f, gx + 1.98f, gz0 - 0.08f, gz1 + 0.08f)) {
			return true;
		}
	}
	return false;
}

float GenContext::yard_lift(const Lot &lot, float lx, float lz) const {
	if (!terrain || in_structure(lot, lx, lz)) {
		return 0.0f;
	}
	const HouseSpec &h = lot.house;
	if (lot.shop < 0) {
		// Stage 3c: on the treads past the deck's own three (the flight as built): the top of the tread there.
		for (size_t i = 3; i < lot.flight.size(); ++i) {
			const Lot::Tread &t = lot.flight[i];
			if (std::abs(lx - h.door_x) <= t.hw && lz > t.z0 && lz <= t.z1) {
				return Math::max(t.top, lot_ground(lot, lx, lz)); // (where the slope runs up across a tread, the ground)
			}
		}
	}
	return lot_ground(lot, lx, lz);
}

PropSettle prop_settle_kind(const std::string &id) {
	if (id.rfind("parked_", 0) == 0) {
		return PropSettle::Car;
	}
	// Tall things stand plumb, as in the real world, their base sunk to the lowest ground under it.
	const char *const upright[] = { "tree_", "lamp_post", "mailbox", "brick_pillar", "sunflower", "corn_stalks", "scarecrow", "swing_set", "inflatable_",
		"ghost_figure", "sign", "fence_post", "signpost" };
	for (const char *u : upright) {
		if (id.rfind(u, 0) == 0) {
			return PropSettle::Upright;
		}
	}
	return PropSettle::Flat;
}

GroundFit fit_footprint(const std::function<float(float, float)> &surface, const Vector3 &p, float yaw, float scale, float x0, float x1, float z0, float z1, float max_tilt, int n) {
	// Samples on an n x n grid over the footprint (in the prop's frame, turned by yaw), a least-squares plane through
	// them about the footprint's centre, its tilt clamped, then lowered until no sample is under it (nothing floats).
	const Vector3 ax(std::cos(yaw), 0.0f, -std::sin(yaw)), az(std::sin(yaw), 0.0f, std::cos(yaw)); // Basis(up, yaw) columns
	const float cu = (x0 + x1) * 0.5f * scale, cv = (z0 + z1) * 0.5f * scale;
	const float hu = (x1 - x0) * 0.5f * scale, hv = (z1 - z0) * 0.5f * scale;
	const Vector3 centre = p + ax * cu + az * cv;
	struct S {
		float u, v, g;
	};
	std::vector<S> s;
	float mean = 0.0f;
	n = Math::max(2, n);
	for (int i = 0; i < n; ++i) {
		for (int k = 0; k < n; ++k) {
			const float u = hu * (2.0f * float(i) / float(n - 1) - 1.0f), v = hv * (2.0f * float(k) / float(n - 1) - 1.0f);
			const Vector3 q = centre + ax * u + az * v;
			const float g = surface(q.x, q.z);
			if (std::isnan(g)) {
				continue; // (a sample the surface leaves out: under a building's wall)
			}
			s.push_back({ u, v, g });
			mean += s.back().g;
		}
	}
	if (s.empty()) {
		s.push_back({ 0.0f, 0.0f, 0.0f });
	}
	mean /= float(s.size());
	// Least squares g ~ A + b u + c v over the samples kept (a full grid is symmetric; one with samples left out isn't).
	float um = 0.0f, vm = 0.0f;
	for (const S &q : s) {
		um += q.u;
		vm += q.v;
	}
	um /= float(s.size());
	vm /= float(s.size());
	float suu = 0.0f, svv = 0.0f, suv = 0.0f, sug = 0.0f, svg = 0.0f;
	GroundFit f;
	f.low = 1e9f;
	for (const S &q : s) {
		const float du = q.u - um, dv = q.v - vm, dg = q.g - mean;
		suu += du * du;
		svv += dv * dv;
		suv += du * dv;
		sug += du * dg;
		svg += dv * dg;
		f.low = Math::min(f.low, q.g);
	}
	float b = 0.0f, c = 0.0f;
	const float det = suu * svv - suv * suv;
	if (det > 1e-8f) {
		b = (sug * svv - svg * suv) / det;
		c = (svg * suu - sug * suv) / det;
	} else {
		b = suu > 1e-6f ? sug / suu : 0.0f;
		c = svv > 1e-6f ? svg / svv : 0.0f;
	}
	const float tilt = std::sqrt(b * b + c * c), lim = std::tan(max_tilt);
	if (tilt > lim) {
		b *= lim / tilt;
		c *= lim / tilt;
	}
	const float A = mean - b * um - c * vm; // the plane's height at the footprint's centre
	float over = -1e9f, lo = 1e9f, hi = -1e9f;
	for (const S &q : s) {
		const float r = A + b * q.u + c * q.v - q.g; // > 0: the plane is above the ground here
		over = Math::max(over, r);
		lo = Math::min(lo, r);
		hi = Math::max(hi, r);
	}
	f.twist = (hi - lo) * 0.5f; // a centred plane's worst miss either way (the four wheels of a car)
	f.bury = hi - lo;
	const Vector3 grad = ax * b + az * c;
	f.centre_y = A;
	f.y = A - over + grad.dot(p - centre);
	f.y_centred = A - (hi + lo) * 0.5f + grad.dot(p - centre);
	f.up = Vector3(-grad.x, 1.0f, -grad.z).normalized();
	return f;
}

float footprint_low(const std::function<float(float, float)> &surface, const Vector3 &p, float r) {
	float low = 1e9f;
	for (int k = 0; k <= 8; ++k) {
		const float a = float(k) * float(Math_TAU) / 8.0f;
		const float g = k == 8 ? surface(p.x, p.z) : surface(p.x + std::cos(a) * r, p.z + std::sin(a) * r);
		if (!std::isnan(g)) {
			low = Math::min(low, g); // (samples the surface leaves out are skipped)
		}
	}
	return low > 1e8f ? 0.0f : low;
}

float street_surface(const GenContext &ctx, const Vector3 &p) {
	float s = ctx.ground(p.x, p.z);
	int which = -1;
	const RoadZone zn = ctx.roads.zone(p, &which);
	if (zn != RoadZone::Off && which >= 0) {
		const float lift = zn == RoadZone::Walk ? 0.07f : (zn == RoadZone::Verge ? 0.05f : (zn == RoadZone::Curb ? 0.12f : 0.01f));
		float ry = ctx.roads.roads[which].project(p).y;
		// On a rounded corner's bands the surface is laid between its two roads' heights (street.cpp fillet_point).
		for (const RoadFillet &F : ctx.roads.fillets) {
			const float dd = Vector2(p.x - F.c.x, p.z - F.c.z).length();
			if (!F.bulb && F.in_sector(p) && dd > F.r_out() - 0.2f && dd < F.r + 0.2f) {
				const Vector3 u = Vector3(p.x - F.c.x, 0.0f, p.z - F.c.z) / Math::max(dd, 1e-4f);
				const float a = std::acos(Math::clamp(F.u0.dot(F.u1), -1.0f, 1.0f));
				const float t = a > 1e-4f ? Math::clamp(std::acos(Math::clamp(F.u0.dot(u), -1.0f, 1.0f)) / a, 0.0f, 1.0f) : 0.5f;
				ry = Math::lerp(F.p0.y, F.p1.y, t);
			}
		}
		s = Math::max(s, ry + lift);
	}
	return s;
}

int shop_support(const Lot &lot, float lx, float lz) {
	const HouseSpec &h = lot.house;
	if (lz <= 0.0f && std::abs(lx) <= h.width * 0.5f + 0.05f && lz >= h.front_z - h.depth - 0.15f) {
		return 2; // the level paving strip in front of the shop (and the shop itself)
	}
	for (const Lot::Tread &t : lot.flight) {
		if (std::abs(lx - h.door_x) <= t.hw && lz >= t.z0 && lz <= t.z1) {
			return 1; // the doorstep or one of its treads
		}
	}
	return 0;
}

float shop_surface(const GenContext &ctx, const Lot &lot, float lx, float lz) {
	switch (shop_support(lot, lx, lz)) {
		case 2:
			return 0.072f;
		case 1: {
			float top = -1e9f;
			for (const Lot::Tread &t : lot.flight) {
				if (std::abs(lx - lot.house.door_x) <= t.hw && lz >= t.z0 && lz <= t.z1) {
					top = Math::max(top, t.top);
				}
			}
			return top;
		}
		default:
			break;
	}
	const float g = ctx.lot_ground(lot, lx, lz);
	return ctx.terrain ? Math::max(g, street_surface(ctx, lot.to_world(lx, 0.0f, lz)) - lot.xf.origin.y) : g;
}

void settle_street_props(GenContext &ctx) {
	if (!ctx.terrain) {
		return;
	}
	auto surf = [&](float x, float z) { return street_surface(ctx, Vector3(x, 0.0f, z)); };
	for (PropPlacement &pp : ctx.props) {
		if (pp.lot >= 0 || pp.settled || pp.id == "bush_round" || prop_settle_kind(pp.id) == PropSettle::Car) {
			continue; // lot props settle as they are put (Yard::put); hedge bushes sit in their hedge; cars are fitted
		}
		const Vector3 o = pp.xf.origin;
		const float s0 = surf(o.x, o.z);
		if (o.y - s0 > 0.15f || o.y - s0 < -0.8f) {
			continue; // stacked on something (a hay pyramid's upper rows), or not on the ground
		}
		const AssetInfo *a = ctx.assets->get(pp.id);
		const AABB &bb = a->aabb;
		const float sc = pp.xf.basis.get_scale().x;
		const Basis rot = pp.xf.basis.orthonormalized();
		const Vector3 fwd = rot.xform(Vector3(0, 0, 1));
		const float yaw = std::atan2(fwd.x, fwd.z);
		if (prop_settle_kind(pp.id) == PropSettle::Upright) {
			const float r = Math::clamp(a->radius > 0.0f ? a->radius : Math::min(bb.size.x, bb.size.z) * 0.5f, 0.1f, 0.45f) * sc;
			pp.xf.origin.y = footprint_low(surf, o, r);
		} else {
			const GroundFit f = fit_footprint(surf, o, yaw, sc, bb.position.x, bb.get_end().x, bb.position.z, bb.get_end().z, 0.5f, 3);
			pp.xf.origin.y = f.y;
			pp.xf.basis = (tilt_to(f.up) * Basis(Vector3(0, 1, 0), yaw)).scaled(Vector3(sc, sc, sc));
		}
	}
}

Basis tilt_to(const Vector3 &up) {
	const Vector3 y(0, 1, 0);
	const Vector3 axis = y.cross(up);
	const float s = axis.length();
	if (s < 1e-6f) {
		return Basis();
	}
	return Basis(axis / s, std::atan2(s, y.dot(up)));
}

void drape_lot_rect(const GenContext &ctx, MeshBuilder &m, const Lot &lot, float x0, float x1, float z0, float z1, float lift, int cell) {
	if (!ctx.terrain || x1 - x0 < 1e-3f || z1 - z0 < 1e-3f) {
		if (x1 - x0 >= 1e-3f && z1 - z0 >= 1e-3f) {
			m.quad_out(Vector3(x0, lift, z1), Vector3(x1, lift, z1), Vector3(x1, lift, z0), Vector3(x0, lift, z0), cell, Vector3((x0 + x1) * 0.5f, -1, (z0 + z1) * 0.5f));
		}
		return;
	}
	const HoodTerrain &T = *ctx.terrain;
	const Transform3D inv = lot.xf.affine_inverse();
	float ax0 = 1e9f, ax1 = -1e9f, az0 = 1e9f, az1 = -1e9f;
	for (float x : { x0, x1 }) {
		for (float z : { z0, z1 }) {
			const Vector3 w = lot.to_world(x, 0.0f, z);
			ax0 = Math::min(ax0, w.x);
			ax1 = Math::max(ax1, w.x);
			az0 = Math::min(az0, w.z);
			az1 = Math::max(az1, w.z);
		}
	}
	const int ix0 = int(std::floor((ax0 - T.x0) / TERRAIN_LATTICE)), ix1 = int(std::floor((ax1 - T.x0) / TERRAIN_LATTICE));
	const int iz0 = int(std::floor((az0 - T.z0) / TERRAIN_LATTICE)), iz1 = int(std::floor((az1 - T.z0) / TERRAIN_LATTICE));
	std::vector<Vector3> poly, next;
	auto clip = [&](int axis, float bound, bool keep_below) {
		next.clear();
		for (size_t i = 0; i < poly.size(); ++i) {
			const Vector3 &p = poly[i], &q = poly[(i + 1) % poly.size()];
			const float dp = (keep_below ? bound - p[axis] : p[axis] - bound), dq = (keep_below ? bound - q[axis] : q[axis] - bound);
			if (dp >= 0.0f) {
				next.push_back(p);
			}
			if ((dp >= 0.0f) != (dq >= 0.0f)) {
				next.push_back(p.lerp(q, dp / (dp - dq)));
			}
		}
		poly.swap(next);
	};
	for (int iz = iz0; iz <= iz1; ++iz) {
		for (int ix = ix0; ix <= ix1; ++ix) {
			const float wx = T.x0 + float(ix) * TERRAIN_LATTICE, wz = T.z0 + float(iz) * TERRAIN_LATTICE;
			const Vector3 a(wx, T.vertex(ix, iz), wz), b(wx + TERRAIN_LATTICE, T.vertex(ix + 1, iz), wz);
			const Vector3 c(wx + TERRAIN_LATTICE, T.vertex(ix + 1, iz + 1), wz + TERRAIN_LATTICE), d(wx, T.vertex(ix, iz + 1), wz + TERRAIN_LATTICE);
			const Vector3 tris[2][3] = { { a, b, c }, { a, c, d } };
			for (const auto &tri : tris) {
				poly = { inv.xform(tri[0]), inv.xform(tri[1]), inv.xform(tri[2]) };
				clip(0, x0, false);
				clip(0, x1, true);
				clip(2, z0, false);
				clip(2, z1, true);
				if (poly.size() < 3) {
					continue;
				}
				for (Vector3 &p : poly) {
					p.y += lift;
				}
				Vector3 cen;
				for (const Vector3 &p : poly) {
					cen += p;
				}
				cen /= float(poly.size());
				for (size_t i = 1; i + 1 < poly.size(); ++i) {
					m.tri_out(poly[0], poly[i], poly[i + 1], cell, cen - Vector3(0, 1, 0));
				}
			}
		}
	}
}
namespace {

float smooth01(float e0, float e1, float x) {
	const float t = Math::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

constexpr float MAX_GRADE = 0.08f; // road grade
constexpr float TOWN_GRADE = 0.05f; // Stage 4c: a town's streets
constexpr float JUNCTION_GRADE = 0.012f; // the parent road through a junction
constexpr float SQUARE_RAMP = 3.0f; // Stage 4c: the paved ramp from a town square's front strip to its level pad
constexpr float SQUARE_MARGIN = 3.0f; // ... and the ground stays level this far round its sides and back (a lattice square's reach)
constexpr float FRONT_BANK = 4.2f; // lot-local depth of the bank from the sidewalk up or down to the pad (under the porch flight)
// Every lattice square touching a sidewalk must have all its corners at the sidewalk's level (a square's far
// corner is 2.83 m away): the ground stays level this far past a corridor's edge before any bank or embankment.
constexpr float WALK_APRON = 2.4f;
constexpr float ROAD_FADE = 8.0f, LOT_FADE = 9.0f; // embankments from a corridor edge / a lot edge to the natural hills
constexpr float PAD_BELOW = 1.8f, PAD_ABOVE = 2.4f; // pad - road clamp (round 2: was 1.2 / 1.8)
constexpr float RETAIN_STEP = 1.1f; // neighbours' pads further apart than this: a retaining wall, not a bank
constexpr float WALK_LIFT = 0.07f; // sidewalk top above the road's centreline height (street.cpp WALK_Y)
constexpr float CAP_SLOPE = 0.55f; // Stage 3c: the steepest the yard rises past a building's 2.9 m level ring
constexpr float GARAGE_APRON = 1.2f; // Stage 3c: the level apron in front of a garage door
constexpr float NB_BLEND = 6.0f; // Ship: how far into a yard a house lot of another street behind or beside it is blended
constexpr float OTHER_BANK_K = 1.9f, OTHER_BANK_MAX = 9.0f; // Ship: a bank to another street's sidewalk: width per metre of height
constexpr float WALL_SLOPE = 1.0f; // Ship: a lattice edge in a yard steeper than this (45 deg) gets a retaining wall across it

// Grades one road: the natural height along it, smoothed, then held to the grade limits (per segment: g[i]
// between samples i and i + 1); y0 >= -1e8: the first sample is fixed there (a side road at its junction).
void grade_road(Road &R, const HoodTerrain &T, const std::vector<float> &g, float y0, float max_grade = MAX_GRADE) {
	const int n = int(R.pts.size());
	std::vector<float> y(n);
	for (int i = 0; i < n; ++i) {
		y[i] = hood_natural(T, R.pts[i].x, R.pts[i].z);
	}
	// Box-filter the target twice over +-10 samples (~ +-10 m): the road follows the swells, not the bumps.
	for (int pass = 0; pass < 2; ++pass) {
		std::vector<float> s(n);
		for (int i = 0; i < n; ++i) {
			float sum = 0.0f;
			int cnt = 0;
			for (int k = Math::max(0, i - 10); k <= Math::min(n - 1, i + 10); ++k) {
				sum += y[k];
				++cnt;
			}
			s[i] = sum / float(cnt);
		}
		y.swap(s);
	}
	const bool fixed = y0 > -1e8f;
	if (fixed) {
		y[0] = y0;
	}
	auto ds = [&](int i) { return R.s[i + 1] - R.s[i]; };
	auto forward = [&]() {
		for (int i = 1; i < n; ++i) {
			const float lim = g[i - 1] * ds(i - 1);
			y[i] = Math::clamp(y[i], y[i - 1] - lim, y[i - 1] + lim);
		}
	};
	for (int it = 0; it < 3; ++it) {
		forward();
		for (int i = n - 2; i >= (fixed ? 1 : 0); --i) {
			const float lim = g[i] * ds(i);
			y[i] = Math::clamp(y[i], y[i + 1] - lim, y[i + 1] + lim);
		}
	}
	// Round off the kinks the clamps leave (graded stretches only), then make it feasible again from the start.
	for (int pass = 0; pass < 3; ++pass) {
		std::vector<float> s = y;
		for (int i = 1; i + 1 < n; ++i) {
			if (g[i - 1] > Math::min(0.03f, max_grade * 0.5f) && g[i] > Math::min(0.03f, max_grade * 0.5f)) {
				s[i] = (y[i - 1] + y[i] * 2.0f + y[i + 1]) * 0.25f;
			}
		}
		y.swap(s);
	}
	forward();
	for (int i = 0; i < n; ++i) {
		R.pts[i].y = y[i];
	}
	if (R.bulb()) {
		R.bulb_c.y = R.pts.back().y;
	}
}

// A lot's ground at lot-local (x, z) inside its footprint (see plan_hood_terrain).
struct LotShape {
	const Lot *lot = nullptr;
	float pad = 0.0f, med = 0.0f;
	float side_pad[2] = { 0.0f, 0.0f }; // neighbour's pad across the -x / +x side (when it touches)
	bool side_touch[2] = { false, false };
	float side_w[2] = { 1.0f, 1.0f };
	float roll = 0.55f; // how much of the natural roll the yard keeps (Stage 4c: 0 for shops and the square)
	float bank = 4.2f; // FRONT_BANK; the town square's ramp is shorter
	bool drape = false; // Stage 3c: a house lot whose yard lies on the natural slope (only the buildings levelled)
	const LotShape *nb[2] = { nullptr, nullptr }; // Stage 3c: the side neighbours (their buildings' cut reaches across)
};

// Stage 3c: horizontal distance (lot-local) from (lx, lz) to the house, the porch and its 3-tread flight, and the
// garage (0 inside): the same footprints as GenContext::in_structure.
float structure_dist(const Lot &lot, float lx, float lz) {
	const HouseSpec &h = lot.house;
	auto rd = [&](float x0, float x1, float z0, float z1) {
		const float dx = Math::max(0.0f, Math::max(x0 - lx, lx - x1)), dz = Math::max(0.0f, Math::max(z0 - lz, lz - z1));
		return std::sqrt(dx * dx + dz * dz);
	};
	const float zf = h.front_z, zb = zf - h.depth;
	float d = rd(h.house_x - h.width * 0.5f - 0.15f, h.house_x + h.width * 0.5f + 0.15f, zb - 0.15f, zf);
	d = Math::min(d, rd(h.porch_x - h.porch_w * 0.5f, h.porch_x + h.porch_w * 0.5f, zf, zf + h.porch_d));
	const float fw = 0.85f + 0.36f * 2.0f;
	d = Math::min(d, rd(h.door_x - fw, h.door_x + fw, zf + h.porch_d, zf + h.porch_d + 0.36f * 3.0f));
	if (h.garage) {
		const float gx = h.house_x + float(h.garage_side) * (h.width * 0.5f + 3.8f * 0.5f);
		const float gz1 = zf - 0.4f, gz0 = gz1 - 6.0f;
		d = Math::min(d, rd(gx - 1.98f, gx + 1.98f, gz0 - 0.08f, gz1 + 0.08f));
	}
	return d;
}

// Stage 3c: the natural relief relative to the street a lot fronts, compressed softly past [-lo, hi] (a yard can't
// sit 7 m over its sidewalk where the road is cut deep into a hill), so the yard keeps the hill's shape. A function
// of the world point alone along one road, so touching neighbours on that road meet seamlessly.
float soft_band(float x, float lo, float hi) {
	const float kh = hi * 0.6f, kl = lo * 0.6f;
	if (x > kh) {
		return hi - (hi - kh) * std::exp(-(x - kh) / (hi - kh));
	}
	if (x < -kl) {
		return -lo + (lo - kl) * std::exp((x + kl) / (lo - kl));
	}
	return x;
}

} // namespace

void plan_hood_terrain(GenContext &ctx) {
	// Stage 4c: towns too, gentler: streets <= 5%, relief 5-8.5 m, each shop a level pad at its sidewalk's
	// higher end (a plinth below it where the street falls away: build_shop), the square a level pad of its own
	// behind a front strip that runs with the street and a paved ramp.
	const bool town = ctx.kind == SettlementKind::Town;
	const float max_grade = town ? TOWN_GRADE : MAX_GRADE;
	RoadNet &net = ctx.roads;
	const uint64_t seed = sub_seed(ctx.seed, 0x7E44);
	auto HoodTerrainPtr = std::make_shared<HoodTerrain>();
	HoodTerrain &T = *HoodTerrainPtr;
	T.seed = seed;
	// ---- the natural relief (round 2): 1-2 broad hills and 1-2 dips placed over the footprint, scaled so the
	// hood's peak-to-trough relief is 10-14 m (wavelengths ~90-150 m: crests and hollows, not bumps).
	{
		Rng rr(sub_seed(ctx.seed, 0x7E45), 9);
		const float cx = (ctx.bound_x0 + ctx.bound_x1) * 0.5f, cz = (ctx.bound_z0 + ctx.bound_z1) * 0.5f;
		const float hx = (ctx.bound_x1 - ctx.bound_x0) * 0.5f, hz = (ctx.bound_z1 - ctx.bound_z0) * 0.5f;
		const int hills = rr.irange(1, 2), dips = rr.irange(1, 2);
		for (int k = 0; k < hills + dips; ++k) {
			HoodTerrain::Swell w;
			w.x = cx + rr.range(-0.7f, 0.7f) * hx;
			w.z = cz + rr.range(-0.7f, 0.7f) * hz;
			w.r = rr.range(45.0f, 75.0f);
			w.a = k < hills ? rr.range(0.85f, 1.1f) : -rr.range(0.6f, 1.0f);
			T.swells.push_back(w);
		}
		const float want = town ? rr.range(5.0f, 8.5f) : rr.range(10.0f, 14.0f);
		float lo = 1e9f, hi = -1e9f;
		for (float z = ctx.bound_z0; z <= ctx.bound_z1; z += 6.0f) {
			for (float x = ctx.bound_x0; x <= ctx.bound_x1; x += 6.0f) {
				const float v = relief_raw(T, x, z);
				lo = Math::min(lo, v);
				hi = Math::max(hi, v);
			}
		}
		T.scale = want / Math::max(0.05f, hi - lo);
		T.bias = -(hi + lo) * 0.5f * T.scale;
	}
	// ---- roads, parents first (a child's index is always above its parent's)
	for (int r = 0; r < int(net.roads.size()); ++r) {
		Road &R = net.roads[r];
		const int n = int(R.pts.size());
		std::vector<float> g(Math::max(1, n - 1), max_grade);
		for (int i = 0; i + 1 < n; ++i) {
			const float sm = (R.s[i] + R.s[i + 1]) * 0.5f;
			for (const RoadJunction &j : net.junctions(r)) {
				const Road &C = net.roads[j.child];
				if (std::abs(sm - j.s) < C.edge() + 6.0f + (town ? 1.0f : 5.0f)) {
					g[i] = JUNCTION_GRADE;
				}
			}
			if (R.parent >= 0 && sm < net.roads[R.parent].edge() + 6.0f + 3.0f) {
				g[i] = 0.0f; // level across its parent's corridor and the junction patch
			}
			if (R.bulb() && sm > R.bulb_s - 3.0f) {
				g[i] = 0.0f; // the turning circle is level
			}
		}
		const float y0 = R.parent >= 0 ? net.roads[R.parent].at(R.parent_s).y : -1e9f;
		grade_road(R, T, g, y0, max_grade);
	}
	for (RoadFillet &f : net.fillets) {
		f.p0.y = net.roads[f.road0].at(f.s0).y;
		f.p1.y = f.bulb ? net.roads[f.road].bulb_c.y : net.roads[f.road1].at(f.s1).y;
	}
	ctx.spawn.y = net.trunk().project(ctx.spawn).y + 0.1f;
	// ---- lot pads: road height at the front + clamp(natural median - road, -1.2, +1.8)
	// (Stage 4c: one shape per lot, then the town square's at index lots.size().)
	const bool has_square = town && ctx.square.valid;
	std::vector<LotShape> shape(ctx.lots.size() + (has_square ? 1 : 0));
	auto front_y = [&](const Lot &lot, const Vector3 &w) {
		const Road &R = net.roads[lot.road];
		return lot.bulb ? R.bulb_c.y : R.project(w).y;
	};
	// Stage 3c: a house lot's yard lies on the natural slope (relative to its street, softly banded); the house floor
	// is that ground along its front wall, and only the buildings' footprints are cut or built up to it.
	auto yard_y = [&](const Lot &lot, const Vector3 &w) {
		Vector3 f = w;
		f.y = 0.0f;
		const float road = front_y(lot, f);
		return road + soft_band(hood_natural(T, w.x, w.z) - road, PAD_BELOW, PAD_ABOVE);
	};
	for (Lot &lot : ctx.lots) {
		if (lot.shop < 0) {
			const HouseSpec &h = lot.house;
			float sum = 0.0f;
			for (float x : { h.house_x - h.width * 0.5f + 0.5f, h.house_x, h.house_x + h.width * 0.5f - 0.5f }) {
				sum += yard_y(lot, lot.to_world(x, 0.0f, h.front_z));
			}
			// Never so far over the sidewalk that the porch flight (risers <= 0.21 m, 0.36 m treads, a landing every 8,
			// houses.cpp) could not reach it before the walk's end: the floor comes down to what the flight can carry.
			int fit = 3;
			for (float z = h.front_z + h.porch_d + 0.36f * 3.0f; fit < 60;) {
				const float d = fit >= 8 && fit % 8 == 0 ? 1.1f : 0.36f;
				if (z + d > -0.3f) {
					break;
				}
				z += d;
				++fit;
			}
			Vector3 door = lot.to_world(h.door_x, 0.0f, 0.0f);
			door.y = 0.0f;
			const float walk_y = front_y(lot, door) + WALK_LIFT;
			// ... and never more than 0.7 m under it (a house down a bank from its street sits at the bank's foot, its
			// yard falling away behind it on a walk-out foundation).
			lot.xf.origin.y = Math::clamp(sum / 3.0f, walk_y - 0.7f, walk_y + 0.21f * float(fit) - h.deck_h);
			shape[lot.index].lot = &lot;
			shape[lot.index].pad = lot.xf.origin.y;
			shape[lot.index].med = lot.xf.origin.y;
			shape[lot.index].drape = true;
			continue;
		}
		std::vector<float> s;
		for (float x = -lot.width * 0.5f + 1.0f; x <= lot.width * 0.5f - 1.0f + 1e-3f; x += 2.0f) {
			for (float z = -1.0f; z >= -lot.depth + 1.0f - 1e-3f; z -= 2.0f) {
				const Vector3 w = lot.to_world(x, 0.0f, z);
				s.push_back(hood_natural(T, w.x, w.z));
			}
		}
		std::sort(s.begin(), s.end());
		const float med = s.empty() ? 0.0f : s[s.size() / 2];
		Vector3 o = lot.xf.origin;
		o.y = 0.0f;
		const float road = front_y(lot, o);
		if (lot.shop >= 0) {
			// Stage 4c: a shop's floor is level with its sidewalk's higher end, so the sidewalk never stands above it.
			float hi = -1e9f;
			for (float x = -lot.width * 0.5f; x <= lot.width * 0.5f + 1e-3f; x += lot.width / 8.0f) {
				Vector3 f = lot.to_world(x, 0.0f, 0.0f);
				f.y = 0.0f;
				hi = Math::max(hi, front_y(lot, f));
			}
			lot.xf.origin.y = hi;
			shape[lot.index].roll = 0.0f;
		} else {
			lot.xf.origin.y = road + Math::clamp(med - road, -PAD_BELOW, PAD_ABOVE);
		}
		shape[lot.index].lot = &lot;
		shape[lot.index].pad = lot.xf.origin.y;
		shape[lot.index].med = med;
	}
	if (has_square) {
		// Its pad level with the sidewalk at the middle of its front; flat, a shorter ramp.
		Lot &sq = ctx.square.area;
		Vector3 o = sq.xf.origin;
		o.y = 0.0f;
		sq.xf.origin.y = front_y(sq, o) + WALK_LIFT;
		LotShape &sh = shape[ctx.lots.size()];
		sh.lot = &sq;
		sh.pad = sq.xf.origin.y;
		sh.med = sh.pad;
		sh.roll = 0.0f;
		sh.bank = SQUARE_RAMP;
	}
	// Neighbours touching each side (as build_boundary_hood decides it), and the soft step between pads.
	for (Lot &lot : ctx.lots) {
		LotShape &sh = shape[lot.index];
		for (int k = 0; k < 2; ++k) {
			const float sx = k == 0 ? -1.0f : 1.0f;
			const int nb = (sx > 0.0f) == (lot.side > 0) ? lot.prev : lot.next;
			if (nb < 0) {
				continue;
			}
			const Lot &o = ctx.lots[nb];
			bool touching = false;
			for (float lz : { -0.5f, -lot.depth + 0.5f }) {
				Vector3 l = o.to_local(lot.to_world(sx * lot.width * 0.5f, o.xf.origin.y - lot.xf.origin.y, lz));
				const float dx = Math::max(0.0f, std::abs(l.x) - o.width * 0.5f), dz = Math::max(0.0f, Math::max(l.z, -o.depth - l.z));
				touching = touching || Vector2(dx, dz).length() < 1.0f;
			}
			if (touching && sh.drape && shape[nb].drape) {
				// Stage 3c: both yards on the slope: they meet seamlessly along one road; across two roads (a corner) the
				// yards' street-relative bands may differ: a soft blend, or a retaining wall where it is too steep to drape.
				float step = 0.0f;
				for (float lz : { -2.0f, -lot.depth * 0.5f, -lot.depth + 1.0f }) {
					const Vector3 w = lot.to_world(sx * lot.width * 0.5f, 0.0f, lz);
					step += (yard_y(lot, w) - yard_y(o, w)) / 3.0f;
				}
				sh.nb[k] = &shape[nb];
				if (std::abs(step) > RETAIN_STEP) {
					lot.retain[k] = step > 0.0f ? step : 0.0f;
					continue;
				}
				sh.side_touch[k] = std::abs(step) > 0.02f;
				sh.side_w[k] = Math::clamp(std::abs(step) * 1.4f, 0.6f, 1.9f);
				continue;
			}
			if (touching) {
				const float step = sh.pad - o.xf.origin.y;
				if (std::abs(step) > RETAIN_STEP && lot.shop < 0) { // (shops: a soft step between service yards)
					// Too big a step for a soft bank between the yards: each stays level to the line, and the higher
					// lot builds a painted stone retaining wall down it (houses.cpp).
					lot.retain[k] = step > 0.0f ? step : 0.0f;
					continue;
				}
				sh.side_touch[k] = true;
				sh.side_pad[k] = o.xf.origin.y;
				sh.side_w[k] = Math::clamp(std::abs(step) * 1.4f, 0.6f, 1.9f);
			}
		}
	}
	// Stage 3c: a building's floor is never buried: the ground within 2.9 m of it (a lattice square's reach) stays at
	// or below the floor, then may rise at most CAP_SLOPE (a cut bank that reads as a graded yard, not a cliff).
	auto building_cap = [&](const LotShape &sh, const Vector3 &w) {
		const Vector3 l = sh.lot->to_local(Vector3(w.x, sh.pad, w.z));
		return sh.pad + CAP_SLOPE * Math::max(0.0f, structure_dist(*sh.lot, l.x, l.z) - 2.9f);
	};
	auto drape_value = [&](const LotShape &sh, float lx, float lz) {
		const Lot &lot = *sh.lot;
		const HouseSpec &h = lot.house;
		const float hw = lot.width * 0.5f;
		lx = Math::clamp(lx, -hw, hw);
		lz = Math::clamp(lz, -lot.depth, 0.0f);
		const Vector3 w = lot.to_world(lx, 0.0f, lz);
		float v = yard_y(lot, w);
		for (int k = 0; k < 2; ++k) {
			if (sh.side_touch[k] && sh.nb[k]) {
				// Across a corner (two streets): blend toward the neighbour's yard near the shared side.
				const float d = hw - (k == 0 ? -lx : lx);
				const float t = 1.0f - smooth01(0.0f, sh.side_w[k], d);
				v = Math::lerp(v, (v + yard_y(*sh.nb[k]->lot, w)) * 0.5f, t);
			}
		}
		// The bank down (or up) to the sidewalk along the front.
		const float t = smooth01(-WALK_APRON - sh.bank, -WALK_APRON, lz);
		if (t > 0.0f) {
			Vector3 f = lot.to_world(lx, 0.0f, 0.0f);
			f.y = 0.0f;
			v = Math::lerp(v, front_y(lot, f) + WALK_LIFT, t);
		}
		// The drive meets the garage door on a short level apron, blending into the drive's slope.
		if (h.garage) {
			const float gz1 = h.front_z - 0.4f;
			const float tx = 1.0f - smooth01(2.2f, 3.4f, std::abs(lx - lot.drive_x)); // (wide enough for the bales stacked by the door)
			const float tz = 1.0f - smooth01(gz1 + GARAGE_APRON, gz1 + GARAGE_APRON + 3.0f, lz);
			// Ship: faded out behind the door line too (it stopped dead 0.5 m inside it, a step beside the garage on fill
			// where the bales by the door stand).
			const float tb = smooth01(gz1 - 3.0f, gz1 - 0.5f, lz);
			if (tx * tz * tb > 0.0f) {
				v = Math::lerp(v, sh.pad, tx * tz * tb);
			}
		}
		// (Never above a floor: every lot's buildings cap the ground round them, the cap raster below.)
		return v;
	};
	auto lot_value = [&](const LotShape &sh, float lx, float lz) {
		if (sh.drape) {
			return drape_value(sh, lx, lz);
		}
		const Lot &lot = *sh.lot;
		const float hw = lot.width * 0.5f;
		lx = Math::clamp(lx, -hw, hw);
		lz = Math::clamp(lz, -lot.depth, 0.0f);
		float base = sh.pad;
		for (int k = 0; k < 2; ++k) {
			if (!sh.side_touch[k]) {
				continue;
			}
			const float d = hw - (k == 0 ? -lx : lx); // distance in from that side
			const float t = 1.0f - smooth01(0.0f, sh.side_w[k], d);
			base = Math::lerp(base, (sh.pad + sh.side_pad[k]) * 0.5f, t);
		}
		// A little of the natural roll kept in the yard, faded out toward the sides and the back.
		const Vector3 w = lot.to_world(lx, 0.0f, lz);
		float v = base;
		if (sh.roll > 0.0f) {
			float rel = Math::clamp(sh.roll * (hood_natural(T, w.x, w.z) - sh.med), -1.4f, 0.8f);
			rel *= smooth01(0.0f, 2.5f, hw - std::abs(lx)) * smooth01(0.0f, 2.5f, lz + lot.depth);
			v += rel;
		}
		// The bank down (or up) to the sidewalk along the front (the town square: its ramp).
		const float t = smooth01(-WALK_APRON - sh.bank, -WALK_APRON, lz);
		if (t > 0.0f) {
			Vector3 f = lot.to_world(lx, 0.0f, 0.0f);
			f.y = 0.0f;
			v = Math::lerp(v, front_y(lot, f) + WALK_LIFT, t);
		}
		// Never above the floor under the house, the porch and its steps, or the garage, nor within 2.9 m of them (a
		// lattice square's far corner reaches 2.83 m: a bank or a rise in the yard never swallows a step or a wall's foot).
		if (ctx.in_structure(lot, lx, lz, 2.9f)) {
			v = Math::min(v, sh.pad);
		}
		return v;
	};
	// ---- the heightfield over the footprint and the blend margin, on the terrain lattice
	const float margin = 34.0f;
	T.x0 = std::floor((ctx.bound_x0 - margin) / TERRAIN_LATTICE) * TERRAIN_LATTICE;
	T.z0 = std::floor((ctx.bound_z0 - margin) / TERRAIN_LATTICE) * TERRAIN_LATTICE;
	T.nx = int(std::ceil((ctx.bound_x1 + margin - T.x0) / TERRAIN_LATTICE)) + 1;
	T.nz = int(std::ceil((ctx.bound_z1 + margin - T.z0) / TERRAIN_LATTICE)) + 1;
	const size_t N = size_t(T.nx) * size_t(T.nz);
	auto vx = [&](int ix) { return T.x0 + float(ix) * TERRAIN_LATTICE; };
	auto vz = [&](int iz) { return T.z0 + float(iz) * TERRAIN_LATTICE; };
	// Road raster: per vertex the smallest offset past a corridor's edge (<= 0: inside it) and that road's height.
	std::vector<float> road_off(N, 1e9f), road_y(N, 0.0f);
	std::vector<int> road_id(N, -1); // (Ship: which road that is)
	int stamping = -1;
	auto stamp_seg = [&](const Vector3 &a, const Vector3 &b, float edge, float reach) {
		const float x0 = Math::min(a.x, b.x) - reach, x1 = Math::max(a.x, b.x) + reach;
		const float z0 = Math::min(a.z, b.z) - reach, z1 = Math::max(a.z, b.z) + reach;
		const int ix0 = Math::max(0, int(std::floor((x0 - T.x0) / TERRAIN_LATTICE))), ix1 = Math::min(T.nx - 1, int(std::ceil((x1 - T.x0) / TERRAIN_LATTICE)));
		const int iz0 = Math::max(0, int(std::floor((z0 - T.z0) / TERRAIN_LATTICE))), iz1 = Math::min(T.nz - 1, int(std::ceil((z1 - T.z0) / TERRAIN_LATTICE)));
		const float abx = b.x - a.x, abz = b.z - a.z, l2 = abx * abx + abz * abz;
		for (int iz = iz0; iz <= iz1; ++iz) {
			for (int ix = ix0; ix <= ix1; ++ix) {
				const float px = vx(ix), pz = vz(iz);
				const float t = l2 > 1e-9f ? Math::clamp(((px - a.x) * abx + (pz - a.z) * abz) / l2, 0.0f, 1.0f) : 0.0f;
				const float dx = a.x + abx * t - px, dz = a.z + abz * t - pz;
				const float off = std::sqrt(dx * dx + dz * dz) - edge;
				const size_t k = size_t(iz) * T.nx + ix;
				if (off < road_off[k]) {
					road_off[k] = off;
					road_y[k] = Math::lerp(a.y, b.y, t);
					road_id[k] = stamping;
				}
			}
		}
	};
	for (const Road &R : net.roads) {
		stamping = int(&R - &net.roads[0]);
		const float reach = R.edge() + WALK_APRON + ROAD_FADE + 2.0f;
		for (size_t i = 0; i + 1 < R.pts.size(); ++i) {
			stamp_seg(R.pts[i], R.pts[i + 1], R.edge(), reach);
		}
		if (R.bulb()) {
			stamp_seg(R.bulb_c, R.bulb_c, R.bulb_r + R.curb + R.walk, R.bulb_r + R.curb + R.walk + WALK_APRON + ROAD_FADE + 2.0f);
		}
		for (bool at_end : { false, true }) {
			if ((at_end ? R.end_kind : R.start_kind) != RoadEnd::Port || R.x_station || R.straight) {
				continue;
			}
			// The country road carries on straight past a port at the port's height (WorldPlan joins it there).
			const Vector3 E = at_end ? R.pts.back() : R.pts.front();
			Vector3 d = R.end_dir(at_end);
			d.y = 0.0f;
			d = d.normalized();
			stamp_seg(E, E + d * 35.0f, R.edge(), reach);
		}
	}
	// Lot raster: the lot a vertex stands in, else the nearest lot within the embankment reach.
	std::vector<int> lot_in(N, -1), lot_near(N, -1);
	std::vector<float> lot_d(N, 1e9f);
	for (size_t si = 0; si < shape.size(); ++si) {
		const Lot &lot = *shape[si].lot; // (a lot's shape index is its lot index; the town square's comes last)
		const float hw = lot.width * 0.5f, reach = LOT_FADE + 1.0f + (lot.shop == -2 ? SQUARE_MARGIN : 0.0f);
		float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
		for (float sx : { -1.0f, 1.0f }) {
			for (float lz : { 0.0f, -lot.depth }) {
				const Vector3 c = lot.to_world(sx * hw, 0.0f, lz);
				x0 = Math::min(x0, c.x);
				x1 = Math::max(x1, c.x);
				z0 = Math::min(z0, c.z);
				z1 = Math::max(z1, c.z);
			}
		}
		const int ix0 = Math::max(0, int(std::floor((x0 - reach - T.x0) / TERRAIN_LATTICE))), ix1 = Math::min(T.nx - 1, int(std::ceil((x1 + reach - T.x0) / TERRAIN_LATTICE)));
		const int iz0 = Math::max(0, int(std::floor((z0 - reach - T.z0) / TERRAIN_LATTICE))), iz1 = Math::min(T.nz - 1, int(std::ceil((z1 + reach - T.z0) / TERRAIN_LATTICE)));
		const Vector3 ax = lot.xf.basis.get_column(0).normalized(), az = lot.xf.basis.get_column(2).normalized();
		for (int iz = iz0; iz <= iz1; ++iz) {
			for (int ix = ix0; ix <= ix1; ++ix) {
				const float px = vx(ix) - lot.xf.origin.x, pz = vz(iz) - lot.xf.origin.z;
				const float lx = px * ax.x + pz * ax.z, lz = px * az.x + pz * az.z;
				const float dx = Math::max(0.0f, std::abs(lx) - hw), dz = Math::max(0.0f, Math::max(lz, -lot.depth - lz));
				const float d = std::sqrt(dx * dx + dz * dz);
				const size_t k = size_t(iz) * T.nx + ix;
				if (d <= 0.0f) {
					lot_in[k] = int(si);
				} else if (d < lot_d[k]) {
					lot_d[k] = d;
					lot_near[k] = int(si);
				}
			}
		}
	}
	// Stage 3c: the building cap raster: every house's floor caps the ground round its buildings (level within 2.9 m,
	// rising at most CAP_SLOPE beyond), whichever lot or embankment the vertex is in (a shallow lot's back neighbour,
	// the neighbour across a side line, the open ground at a row's end).
	std::vector<float> cap(N, 1e9f);
	for (const LotShape &sh : shape) {
		if (!sh.drape) {
			continue;
		}
		const Lot &lot = *sh.lot;
		const float hw = lot.width * 0.5f + LOT_FADE;
		float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
		for (float sx : { -1.0f, 1.0f }) {
			for (float lz : { LOT_FADE, -lot.depth - LOT_FADE }) {
				const Vector3 c = lot.to_world(sx * hw, 0.0f, lz);
				x0 = Math::min(x0, c.x);
				x1 = Math::max(x1, c.x);
				z0 = Math::min(z0, c.z);
				z1 = Math::max(z1, c.z);
			}
		}
		const int ix0 = Math::max(0, int(std::floor((x0 - T.x0) / TERRAIN_LATTICE))), ix1 = Math::min(T.nx - 1, int(std::ceil((x1 - T.x0) / TERRAIN_LATTICE)));
		const int iz0 = Math::max(0, int(std::floor((z0 - T.z0) / TERRAIN_LATTICE))), iz1 = Math::min(T.nz - 1, int(std::ceil((z1 - T.z0) / TERRAIN_LATTICE)));
		for (int iz = iz0; iz <= iz1; ++iz) {
			for (int ix = ix0; ix <= ix1; ++ix) {
				const size_t k = size_t(iz) * T.nx + ix;
				const float cv = building_cap(sh, Vector3(vx(ix), 0.0f, vz(iz)));
				if (cv < cap[k]) {
					cap[k] = cv;
				}
			}
		}
	}
	// Ship: where a yard meets a yard of another street behind or beside it (or a lot of the same street that is not its
	// side neighbour: round a bulb), the two street-relative yards can stand metres apart. Each vertex in or near a house
	// lot takes the weighted mean of every house lot whose footprint comes within NB_BLEND of it, relative to the nearest
	// one (its own lot inside it: weight 1), so the height change is spread over both back yards instead of one lattice
	// square. Neighbours on one street already meet seamlessly: there the mean changes nothing. Hoods only.
	std::vector<float> mix_v, mix_w;
	if (!town) {
		mix_v.assign(N, 0.0f);
		mix_w.assign(N, 0.0f);
		for (size_t si = 0; si < shape.size(); ++si) {
			const LotShape &sh = shape[si];
			if (!sh.drape) {
				continue;
			}
			const Lot &lot = *sh.lot;
			const float hw = lot.width * 0.5f, reach = Math::max(NB_BLEND, LOT_FADE) + 1.0f;
			float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
			for (float sx : { -1.0f, 1.0f }) {
				for (float lz : { 0.0f, -lot.depth }) {
					const Vector3 c = lot.to_world(sx * hw, 0.0f, lz);
					x0 = Math::min(x0, c.x);
					x1 = Math::max(x1, c.x);
					z0 = Math::min(z0, c.z);
					z1 = Math::max(z1, c.z);
				}
			}
			const int ix0 = Math::max(0, int(std::floor((x0 - reach - T.x0) / TERRAIN_LATTICE))), ix1 = Math::min(T.nx - 1, int(std::ceil((x1 + reach - T.x0) / TERRAIN_LATTICE)));
			const int iz0 = Math::max(0, int(std::floor((z0 - reach - T.z0) / TERRAIN_LATTICE))), iz1 = Math::min(T.nz - 1, int(std::ceil((z1 + reach - T.z0) / TERRAIN_LATTICE)));
			for (int iz = iz0; iz <= iz1; ++iz) {
				for (int ix = ix0; ix <= ix1; ++ix) {
					const size_t k = size_t(iz) * T.nx + ix;
					const float dmin = lot_in[k] >= 0 ? 0.0f : lot_d[k];
					const Vector3 l = lot.to_local(Vector3(vx(ix), lot.xf.origin.y, vz(iz)));
					const float dx = Math::max(0.0f, std::abs(l.x) - hw), dz = Math::max(0.0f, Math::max(l.z, -lot.depth - l.z));
					const float d = lot_in[k] == int(si) ? 0.0f : std::sqrt(dx * dx + dz * dz);
					if (d - dmin >= NB_BLEND || dmin > LOT_FADE) {
						continue;
					}
					const float w = 1.0f - smooth01(0.0f, NB_BLEND, d - dmin);
					mix_v[k] += w * lot_value(sh, l.x, l.z);
					mix_w[k] += w;
				}
			}
		}
	}
	auto mixed = [&](size_t k, float own) { return !mix_w.empty() && mix_w[k] > 1e-4f ? mix_v[k] / mix_w[k] : own; };
	T.h.assign(N, 0.0f);
	for (int iz = 0; iz < T.nz; ++iz) {
		for (int ix = 0; ix < T.nx; ++ix) {
			const size_t k = size_t(iz) * T.nx + ix;
			const float px = vx(ix), pz = vz(iz);
			float v;
			if (road_off[k] <= 0.0f) {
				v = road_y[k]; // the road corridor, level across at its centreline height
			} else if (lot_in[k] >= 0) {
				const LotShape &sh = shape[lot_in[k]];
				const Vector3 l = sh.lot->to_local(Vector3(px, sh.pad, pz));
				const float lv = sh.drape ? mixed(k, lot_value(sh, l.x, l.z)) : lot_value(sh, l.x, l.z);
				v = Math::min(lv, cap[k]);
				// Ship: the bank up or down to ANOTHER street's sidewalk (a back or side line against a road behind the lot)
				// is as wide as its height needs to stay walkable (the lot's own street keeps the 3b bank under the porch).
				const bool other = sh.drape && road_id[k] >= 0 && road_id[k] != sh.lot->road;
				const float bank = other ? Math::clamp(std::abs(road_y[k] + WALK_LIFT - v) * OTHER_BANK_K, sh.bank, OTHER_BANK_MAX) : sh.bank;
				if (road_off[k] < WALK_APRON + bank) {
					// Any street's sidewalk, not only its own (a corner lot's side runs along the other road): level with
					// it for the apron, then the bank (and never above the floor round the house).
					const float t = 1.0f - smooth01(WALK_APRON, WALK_APRON + bank, road_off[k]);
					v = Math::lerp(v, road_y[k] + WALK_LIFT, t);
					if (ctx.in_structure(*sh.lot, l.x, l.z, road_off[k] < WALK_APRON ? 0.9f : 2.9f)) {
						v = Math::min(v, sh.pad); // the porch flight is never buried, even by a sidewalk above the pad
					} else if (road_off[k] < WALK_APRON) {
						v = road_y[k] + WALK_LIFT; // otherwise the sidewalk's level wins
					} else if (other) {
						v = Math::min(v, cap[k]); // (and never over a house's level ring and cut bank, whichever lot's)
					}
				}
			} else {
				const float n = hood_natural(T, px, pz);
				float wsum = 0.0f, vsum = 0.0f, wmax = 0.0f;
				if (road_off[k] < WALK_APRON + ROAD_FADE) {
					const float w = 1.0f - smooth01(WALK_APRON, WALK_APRON + ROAD_FADE, road_off[k]);
					wsum += w;
					vsum += w * (road_y[k] + WALK_LIFT);
					wmax = Math::max(wmax, w);
				}
				const float flat = lot_near[k] >= 0 && shape[lot_near[k]].lot->shop == -2 ? SQUARE_MARGIN : 0.0f; // (a level ring round the square)
				if (lot_near[k] >= 0 && lot_d[k] < LOT_FADE + flat) {
					const LotShape &sh = shape[lot_near[k]];
					const Vector3 l = sh.lot->to_local(Vector3(px, sh.pad, pz));
					float w = 1.0f - smooth01(flat, flat + LOT_FADE, lot_d[k]);
					const float lv = sh.drape ? mixed(k, lot_value(sh, l.x, l.z)) : lot_value(sh, l.x, l.z);
					if (sh.drape && road_id[k] >= 0 && road_id[k] != sh.lot->road && road_off[k] < WALK_APRON + ROAD_FADE) {
						// Ship: between a lot's corner and another street's sidewalk, the street's level holds for a bank as
						// wide as the height between them needs (the same as inside the lot), not half way at once.
						const float bank = Math::clamp(std::abs(road_y[k] + WALK_LIFT - lv) * OTHER_BANK_K, FRONT_BANK, OTHER_BANK_MAX);
						w *= smooth01(WALK_APRON, WALK_APRON + bank, road_off[k]);
					}
					wsum += w;
					vsum += w * lv;
					wmax = Math::max(wmax, w);
				}
				v = Math::min(wsum > 0.0f ? Math::lerp(n, vsum / wsum, wmax) : n, cap[k]);
				if (road_off[k] < WALK_APRON) {
					v = road_y[k] + WALK_LIFT; // level with the sidewalk next to it, whatever lot is near
				}
			}
			T.h[k] = v;
		}
	}
	// Ship: the retaining walls. The 3b side walls (retain[k]) are recorded as they are; then every lattice edge still
	// steeper than WALL_SLOPE that crosses a house lot's side or back line (a house's level ring right against a much
	// higher or lower yard or sidewalk of another street: no room for a walkable bank) gets a painted stone wall along
	// that line, built by the lot holding the higher end (else the lot holding the lower one), over the run of such
	// crossings plus 1.2 m each way. build_house builds them (houses.cpp).
	if (!town) {
		for (Lot &lot : ctx.lots) {
			lot.walls.clear();
			for (int k = 0; k < 2; ++k) {
				if (lot.retain[k] > 0.0f) {
					const float x = (k == 0 ? -0.5f : 0.5f) * lot.width;
					Lot::Wall w;
					w.a = Vector3(x, 0.0f, -lot.depth);
					w.b = Vector3(x, 0.0f, -0.6f);
					w.step = lot.retain[k];
					w.side = true;
					w.out = Vector3(k == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
					lot.walls.push_back(w);
				}
			}
		}
		struct Cross {
			int lot, side;
			float s, dh;
			bool high;
		};
		std::vector<Cross> cr;
		for (int iz = 0; iz < T.nz; ++iz) {
			for (int ix = 0; ix < T.nx; ++ix) {
				for (int d = 0; d < 2; ++d) {
					const int jx = ix + (d == 0), jz = iz + (d == 1);
					if (jx >= T.nx || jz >= T.nz) {
						continue;
					}
					const size_t k0 = size_t(iz) * T.nx + ix, k1 = size_t(jz) * T.nx + jx;
					const float dh = T.h[k1] - T.h[k0];
					if (std::abs(dh) <= WALL_SLOPE * TERRAIN_LATTICE || lot_in[k0] == lot_in[k1]) {
						continue;
					}
					const size_t hi = dh > 0.0f ? k1 : k0, lo = dh > 0.0f ? k0 : k1;
					auto drape_lot = [&](size_t k) { return lot_in[k] >= 0 && lot_in[k] < int(ctx.lots.size()) && shape[lot_in[k]].drape ? lot_in[k] : -1; };
					const int owner = drape_lot(hi) >= 0 ? drape_lot(hi) : drape_lot(lo);
					if (owner < 0) {
						continue;
					}
					const Lot &L = ctx.lots[owner];
					const size_t in_k = lot_in[k0] == owner ? k0 : k1, out_k = in_k == k0 ? k1 : k0;
					const Vector3 a = L.to_local(Vector3(vx(int(in_k % T.nx)), L.xf.origin.y, vz(int(in_k / T.nx))));
					const Vector3 b = L.to_local(Vector3(vx(int(out_k % T.nx)), L.xf.origin.y, vz(int(out_k / T.nx))));
					// Where the edge leaves the lot: the first of its side / back / front lines it crosses.
					const float hw = L.width * 0.5f;
					float best_t = 2.0f, s = 0.0f;
					int side = -1;
					auto try_line = [&](int sd, float av, float bv, float line, float ao, float bo, float lo_o, float hi_o) {
						if ((av - line) * (bv - line) > 0.0f || std::abs(bv - av) < 1e-6f) {
							return;
						}
						const float t = (line - av) / (bv - av);
						const float o = Math::lerp(ao, bo, t);
						if (t >= 0.0f && t < best_t && o >= lo_o - 0.05f && o <= hi_o + 0.05f) {
							best_t = t;
							side = sd;
							s = o;
						}
					};
					try_line(0, a.x, b.x, -hw, a.z, b.z, -L.depth, 0.0f);
					try_line(1, a.x, b.x, hw, a.z, b.z, -L.depth, 0.0f);
					try_line(2, a.z, b.z, -L.depth, a.x, b.x, -hw, hw);
					try_line(3, a.z, b.z, 0.0f, a.x, b.x, -hw, hw);
					if (side < 0 || side == 3) {
						continue; // (the front is the sidewalk's business)
					}
					cr.push_back({ owner, side, s, std::abs(dh), hi == in_k });
				}
			}
		}
		std::sort(cr.begin(), cr.end(), [](const Cross &p, const Cross &q) { return p.lot != q.lot ? p.lot < q.lot : (p.side != q.side ? p.side < q.side : p.s < q.s); });
		for (size_t i = 0; i < cr.size();) {
			size_t j = i;
			float dh = cr[i].dh;
			int highs = 0;
			while (j + 1 < cr.size() && cr[j + 1].lot == cr[i].lot && cr[j + 1].side == cr[i].side && cr[j + 1].s - cr[j].s < 3.0f) {
				++j;
				dh = Math::max(dh, cr[j].dh);
			}
			for (size_t m = i; m <= j; ++m) {
				highs += cr[m].high ? 1 : 0;
			}
			Lot &L = ctx.lots[cr[i].lot];
			const float hw = L.width * 0.5f;
			Lot::Wall w;
			w.step = dh;
			const bool high = highs * 2 >= int(j - i + 1);
			if (cr[i].side == 2) {
				const float s0 = Math::max(-hw, cr[i].s - 1.2f), s1 = Math::min(hw, cr[j].s + 1.2f);
				w.a = Vector3(s0, 0.0f, -L.depth);
				w.b = Vector3(s1, 0.0f, -L.depth);
				w.out = Vector3(0.0f, 0.0f, high ? -1.0f : 1.0f);
			} else {
				const float x = (cr[i].side == 0 ? -1.0f : 1.0f) * hw;
				const float s0 = Math::max(-L.depth, cr[i].s - 1.2f), s1 = Math::min(-0.6f, cr[j].s + 1.2f);
				w.a = Vector3(x, 0.0f, s0);
				w.b = Vector3(x, 0.0f, Math::max(s0 + 0.5f, s1));
				w.out = Vector3((cr[i].side == 0 ? -1.0f : 1.0f) * (high ? 1.0f : -1.0f), 0.0f, 0.0f);
			}
			L.walls.push_back(w);
			i = j + 1;
		}
	}
	ctx.terrain = HoodTerrainPtr;
}

} // namespace tot
