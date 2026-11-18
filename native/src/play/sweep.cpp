#include "play/sweep.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/movement.h"
#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "play/monsters.h"
#include "world/world_plan.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

namespace {

// FNV-1a over exact bit patterns, one hash per category; entries counted.
struct Cat {
	const char *name;
	uint64_t h = 1469598103934665603ull;
	int n = 0;
	bool detail = false;
	std::string line, text;
	void bytes(const void *p, size_t len) {
		const unsigned char *c = static_cast<const unsigned char *>(p);
		for (size_t i = 0; i < len; ++i) {
			h = (h ^ c[i]) * 1099511628211ull;
		}
	}
	void f(float v) {
		uint32_t b;
		std::memcpy(&b, &v, 4);
		bytes(&b, 4);
		if (detail) {
			char buf[16];
			snprintf(buf, sizeof(buf), " %08x", b);
			line += buf;
		}
	}
	void i(int64_t v) {
		bytes(&v, 8);
		if (detail) {
			line += " " + std::to_string(v);
		}
	}
	void s(const std::string &v) {
		bytes(v.data(), v.size());
		i(int64_t(v.size()));
		if (detail) {
			line += " " + v;
		}
	}
	void v(const Vector3 &p) {
		f(p.x);
		f(p.y);
		f(p.z);
	}
	void v2(const Vector2 &p) {
		f(p.x);
		f(p.y);
	}
	void c(const Color &col) {
		f(col.r);
		f(col.g);
		f(col.b);
		f(col.a);
	}
	void xf(const Transform3D &t) {
		for (int r = 0; r < 3; ++r) {
			v(t.basis.rows[r]);
		}
		v(t.origin);
	}
	void mesh(const MeshBuilder &m) {
		for (const Vector3 &p : m.vertices()) {
			v(p);
		}
		for (const Vector2 &p : m.cell_uvs()) {
			v2(p);
		}
		i(m.vertex_count());
	}
	void end() {
		if (detail) {
			text += std::string("D ") + name + " " + std::to_string(n) + ":" + line + "\n";
			line.clear();
		}
		++n;
	}
};

std::string hex(uint64_t h) {
	char buf[24];
	snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
	return buf;
}

void write_line(std::ofstream &out, const std::string &head, std::vector<Cat> &cats) {
	out << "{" << head << ",\"cats\":{";
	for (size_t k = 0; k < cats.size(); ++k) {
		out << (k ? "," : "") << "\"" << cats[k].name << "\":[" << cats[k].n << ",\"" << hex(cats[k].h) << "\"]";
	}
	out << "}}\n";
	for (const Cat &c : cats) {
		out << c.text;
	}
}

} // namespace

bool SweepDump::open(const std::string &path, const std::string &detail) {
	out_.open(path, std::ios::out | std::ios::trunc);
	detail_ = detail;
	return out_.is_open();
}

void SweepDump::settlement(const std::string &label, int64_t seed, const GenContext &ctx, const InfestationLayout *inf) {
	if (!out_.is_open()) {
		return;
	}
	const char *names[] = { "meta", "street", "lots", "boxes", "props", "lights", "candy", "doors", "strings", "scatter", "magic", "signs", "springs",
		"street_mesh", "boundary_mesh", "lot_mesh", "occupied", "infest", "vines" };
	std::vector<Cat> cats;
	for (const char *n : names) {
		Cat c;
		c.name = n;
		c.detail = detail_ == n;
		cats.push_back(c);
	}
	auto cat = [&](const char *n) -> Cat & {
		for (Cat &c : cats) {
			if (std::strcmp(c.name, n) == 0) {
				return c;
			}
		}
		return cats[0];
	};
	{
		Cat &c = cat("meta");
		c.v(ctx.spawn);
		c.f(ctx.spawn_yaw);
		c.f(ctx.bound_x0);
		c.f(ctx.bound_x1);
		c.f(ctx.bound_z0);
		c.f(ctx.bound_z1);
		c.v(ctx.moon_dir);
		c.i(ctx.stats.candy);
		c.i(ctx.stats.candy_raised);
		c.i(ctx.stats.candy_dropped);
		c.i(ctx.stats.platforms);
		c.i(ctx.stats.reachable_platforms);
		c.i(ctx.stats.openness_failures);
		for (const std::string &note : ctx.stats.notes) {
			c.s(note);
		}
		c.end();
	}
	{
		// The street as the generator sees it: main centreline samples, then the side lane.
		Cat &c = cat("street");
		// (Stage 0 keeps the pre-RoadNet order: main samples, dimensions, then the lane's junction.)
		const Road &s = ctx.roads.trunk();
		for (size_t k = 0; k < s.pts.size(); ++k) {
			c.v(s.pts[k]);
			c.v(s.left[k]);
			c.end();
		}
		c.f(s.half_road);
		c.f(s.curb);
		c.f(s.walk);
		c.f(s.verge);
		if (ctx.roads.roads.size() > 1) {
			const Road &lane = ctx.roads.roads[1];
			c.f(lane.parent_s);
			c.i(lane.parent_side);
			c.f(lane.length());
			c.v(lane.pts.front());
		}
		// Stage 2a: the other roads, bulbs and fillets (none on the legacy town layout, which hashes as before).
		for (size_t r = 2; r < ctx.roads.roads.size(); ++r) {
			const Road &b = ctx.roads.roads[r];
			c.f(b.parent_s);
			c.i(b.parent_side);
			c.f(b.length());
			c.v(b.pts.front());
			c.v(b.pts.back());
		}
		for (const Road &b : ctx.roads.roads) {
			if (b.bulb()) {
				c.v(b.bulb_c);
				c.f(b.bulb_r);
			}
		}
		for (const RoadFillet &f : ctx.roads.fillets) {
			c.v(f.c);
			c.f(f.r);
		}
		c.end();
	}
	{
		Cat &c = cat("lots");
		for (const Lot &l : ctx.lots) {
			c.i(l.index);
			c.i(l.side);
			c.xf(l.xf);
			c.f(l.width);
			c.f(l.depth);
			c.f(l.s);
			c.i(int(l.persona));
			c.i(int(l.landmark));
			c.i(l.shop);
			c.i(l.faces_used);
			c.f(l.drive_x);
			c.i(l.passage_side);
			c.v(l.doorbell);
			c.v(l.porch_light);
			for (const auto *route : { &l.loop_route, &l.roof_route, &l.summit_route }) {
				c.i(int64_t(route->size()));
				for (const Vector3 &p : *route) {
					c.v(p);
				}
			}
			const HouseSpec &h = l.house;
			for (float x : { h.width, h.depth, h.storey_h, h.pitch_deg, h.house_x, h.front_z, h.porch_x, h.porch_w, h.porch_d, h.deck_h, h.door_x }) {
				c.f(x);
			}
			for (int x : { h.storeys, int(h.ridge_along_x), int(h.garage), h.garage_side, int(h.chimney), h.chimney_side, int(h.roof_steps), h.wall_cell,
						 h.roof_cell, h.trim_cell, h.door_cell, h.garage_cell, h.fence_type, h.fence_cell, h.wall_mat, h.upper_mat, h.roof_mat, h.upper_cell,
						 int(h.shutters), h.shutter_cell, int(h.flower_boxes), int(h.dormer), int(h.gingerbread), int(h.pediment), h.curtain_cell, h.drive_mat }) {
				c.i(x);
			}
			c.end();
		}
	}
	{
		Cat &c = cat("boxes");
		for (const OBox &b : ctx.boxes) {
			c.xf(b.xf);
			c.v(b.half);
			c.i(int(b.solid) | int(b.walkable) << 1 | int(b.bouncy) << 2 | int(b.cylinder) << 3 | int(b.boundary) << 4 | int(b.shot_through) << 5);
			c.f(b.bounce);
			c.i(b.lot);
			c.end();
		}
	}
	{
		Cat &c = cat("props");
		for (const PropPlacement &p : ctx.props) {
			c.s(p.id);
			c.xf(p.xf);
			c.i(p.lot);
			c.f(p.wind);
			c.i(int(p.collide) | int(p.bouncy) << 1);
			c.end();
		}
	}
	{
		Cat &c = cat("lights");
		for (const LightSpec &l : ctx.lights) {
			c.v(l.pos);
			c.c(l.color);
			c.f(l.energy);
			c.f(l.range);
			c.i(l.lot);
			c.i(int(l.kind));
			c.end();
		}
	}
	{
		Cat &c = cat("candy");
		for (const CandySpec &k : ctx.candy) {
			c.v(k.pos);
			c.i(k.kind);
			c.i(k.lot);
			c.i(k.group);
			c.end();
		}
	}
	{
		Cat &c = cat("doors");
		for (const DoorSpec &d : ctx.doors) {
			c.i(d.lot);
			c.xf(d.hinge);
			c.f(d.width);
			c.f(d.height);
			c.i(d.cell);
			c.i(d.trim_cell);
			c.end();
		}
	}
	{
		Cat &c = cat("strings");
		for (const StringLight &s : ctx.strings) {
			c.i(s.lot);
			c.v(s.a);
			c.v(s.b);
			c.f(s.sag);
			c.i(int(s.style));
			c.v(s.out);
			c.end();
		}
	}
	{
		Cat &c = cat("scatter");
		for (const ScatterItem &s : ctx.scatter) {
			c.i(s.kind);
			c.xf(s.xf);
			c.i(s.lot);
			c.c(s.color);
			c.end();
		}
	}
	{
		Cat &c = cat("magic");
		for (const MagicPath &m : ctx.magic) {
			c.i(m.lot);
			for (const Vector3 &p : m.points) {
				c.v(p);
			}
			c.end();
		}
	}
	{
		Cat &c = cat("signs");
		for (const SignSpec &s : ctx.signs) {
			c.s(s.text);
			c.xf(s.xf);
			c.c(s.color);
			c.f(s.size);
			c.f(s.width);
			c.f(s.height);
			c.end();
		}
	}
	{
		Cat &c = cat("springs");
		for (const SpringSpec &s : ctx.springs) {
			c.xf(s.xf);
			c.v(s.anchor);
			c.f(s.reach);
			c.mesh(s.mesh);
			c.end();
		}
	}
	cat("street_mesh").mesh(ctx.street_mesh);
	cat("street_mesh").end();
	cat("boundary_mesh").mesh(ctx.boundary_mesh);
	cat("boundary_mesh").end();
	for (const MeshBuilder &m : ctx.lot_mesh) {
		cat("lot_mesh").mesh(m);
		cat("lot_mesh").end();
	}
	{
		Cat &c = cat("occupied");
		for (const std::vector<Circle> &o : ctx.occupied) {
			for (const Circle &k : o) {
				c.f(k.x);
				c.f(k.z);
				c.f(k.r);
			}
			c.end();
		}
	}
	if (inf) {
		Cat &c = cat("infest");
		c.v(inf->gate);
		c.xf(inf->cart);
		for (int k : inf->kinds) {
			c.i(k);
		}
		c.i(inf->want_nests);
		c.i(inf->want_hearts);
		c.i(inf->nest_candidates);
		c.i(inf->heart_candidates);
		c.end();
		for (size_t k = 0; k < inf->nests.size(); ++k) {
			c.v(inf->nests[k]);
			c.i(inf->nest_seeds[k]);
			c.end();
		}
		for (size_t k = 0; k < inf->hearts.size(); ++k) {
			c.v(inf->hearts[k]);
			c.f(inf->heart_yaw[k]);
			c.i(inf->heart_seeds[k]);
			c.end();
		}
		Cat &v = cat("vines"); // Stage 1d: the thorn vines' growth moved into the pure planner
		for (const auto &t : inf->tendrils) {
			v.i(t.nest);
			for (const Vector3 &p : t.pts) {
				v.v(p);
			}
			v.end();
		}
		for (const auto &k : inf->knots) {
			v.v(k.first);
			v.i(k.second);
			v.end();
		}
	}
	const char *kind = ctx.kind == SettlementKind::Town ? "town" : "neighborhood";
	write_line(out_, "\"label\":\"" + label + "\",\"seed\":" + std::to_string(seed) + ",\"kind\":\"" + kind + "\"", cats);
}

void SweepDump::world(int64_t seed, WorldPlan &plan) {
	if (!out_.is_open()) {
		return;
	}
	std::vector<Cat> cats(2);
	cats[0].name = "regions";
	cats[1].name = "roads";
	for (Cat &c : cats) {
		c.detail = detail_ == c.name;
	}
	plan.reset(uint64_t(seed));
	for (int i = -2; i <= 2; ++i) {
		for (int j = -2; j <= 2; ++j) {
			const RegionPlan &r = plan.region(i, j);
			Cat &c = cats[0];
			c.i(r.i);
			c.i(r.j);
			c.i(int(r.kind));
			c.i(int64_t(r.seed));
			c.v(r.origin);
			c.f(r.x0);
			c.f(r.x1);
			c.f(r.z0);
			c.f(r.z1);
			c.v(r.spawn);
			c.f(r.spawn_yaw);
			c.i(int(r.lane_open));
			c.i(r.lane_side);
			for (const Port &p : r.ports) {
				c.i(int(p.valid));
				c.v(p.pos);
				c.v(p.dir);
			}
			c.s(r.name);
			c.end();
		}
	}
	for (int i = -2; i <= 2; ++i) {
		for (int j = -2; j <= 2; ++j) {
			for (int d = 0; d < 2; ++d) {
				const RoadPlan *road = plan.road(i, j, i + (d == 0 ? 1 : 0), j + (d == 1 ? 1 : 0));
				Cat &c = cats[1];
				c.i(road ? int64_t(road->pts.size()) : -1);
				if (road) {
					for (const Vector3 &p : road->pts) {
						c.v(p);
					}
				}
				c.end();
			}
		}
	}
	write_line(out_, "\"label\":\"world\",\"seed\":" + std::to_string(seed), cats);
}

namespace {
// Stage 3c: walks, stairs, prop contact, bounce columns, roof decks, platform reach.
void sweep_stage3c_stats(const GenContext &ctx, Dictionary &st) {
	const RoadNet &net = ctx.roads;
	auto r2 = [](float v) { return std::round(v * 100.0f) / 100.0f; };
	// The walking surface under a settlement point: the lattice, or a street surface laid over it.
	auto surface = [&](const Vector3 &p) { return street_surface(ctx, p); };
	// ---- platform reach (walkable boxes and bounce pads), and the ones left unreached
	{
		const std::vector<char> reach = platform_reach(ctx);
		int n = 0, got = 0;
		Array un;
		for (size_t i = 0; i < ctx.boxes.size(); ++i) {
			const OBox &b = ctx.boxes[i];
			if (!b.walkable && !b.bouncy) {
				continue;
			}
			++n;
			got += reach[i] ? 1 : 0;
			if (!reach[i] && un.size() < 10) {
				String w = "S";
				if (b.lot >= 0) {
					const Lot &lt = ctx.lots[b.lot];
					const Vector3 l = lt.to_local(b.xf.origin);
					w = String("L") + String::num_int64(b.lot) + (lt.shop >= 0 ? "s(" : "h(") + String::num(l.x, 1) + "," + String::num(l.z, 1) + ")";
				}
				un.push_back(w + " top+" + String::num(b.top() - ctx.ground(b.xf.origin.x, b.xf.origin.z), 2) + " half " + String::num(b.half.x, 2) + "/" + String::num(b.half.y, 2) + "/" +
						String::num(b.half.z, 2));
			}
		}
		st["platforms_3c"] = n;
		st["platforms_reached_3c"] = got;
		st["platform_reach_pct"] = n ? std::round(1000.0 * got / n) / 10.0 : 100.0;
		st["platforms_unreached"] = un;
		{
			const std::vector<char> nb = platform_reach(ctx, false); // without the bounce pads' throws (Stage 4c's measure)
			int g2 = 0;
			Array un2;
			for (size_t i = 0; i < ctx.boxes.size(); ++i) {
				const OBox &b = ctx.boxes[i];
				g2 += (b.walkable || b.bouncy) && nb[i] ? 1 : 0;
				if ((b.walkable || b.bouncy) && !nb[i] && un2.size() < 10) {
					String w = "S";
					if (b.lot >= 0) {
						const Lot &lt = ctx.lots[b.lot];
						const Vector3 l = lt.to_local(b.xf.origin);
						w = String("L") + String::num_int64(b.lot) + (lt.shop >= 0 ? "s(" : "h(") + String::num(l.x, 1) + "," + String::num(l.z, 1) + " y" + String::num(l.y, 2) + ")";
					}
					un2.push_back(w + (b.bouncy ? " bouncy" : "") + " top+" + String::num(b.top() - ctx.ground(b.xf.origin.x, b.xf.origin.z), 2) + " half " + String::num(b.half.x, 2) + "/" +
							String::num(b.half.y, 2) + "/" + String::num(b.half.z, 2));
				}
			}
			st["platforms_reached_nobounce"] = g2;
			st["platforms_unreached_nobounce"] = un2;
		}
		// Roof decks reached (the deck's own walkable box).
		int decks = 0, decks_ok = 0;
		for (const Lot &lot : ctx.lots) {
			if (lot.deck_half.x <= 0.0f) {
				continue;
			}
			++decks;
			const Vector3 dc = lot.to_world(lot.deck_c);
			for (size_t i = 0; i < ctx.boxes.size(); ++i) {
				const OBox &b = ctx.boxes[i];
				if (b.walkable && std::abs(b.top() - dc.y) < 0.02f && Vector2(b.xf.origin.x - dc.x, b.xf.origin.z - dc.z).length() < 0.05f) {
					decks_ok += reach[i] ? 1 : 0;
					break;
				}
			}
		}
		st["roof_decks"] = decks;
		st["roof_decks_reached"] = decks_ok;
	}
	if (!ctx.terrain) {
		return;
	}
	// ---- front walks: the flagstones reach the sidewalk and the flight's foot, joints under 0.1 m, flush, clear
	{
		int lots = 0, bad = 0, not_walk = 0, blocked = 0;
		float gap_max = 0.0f, float_max = 0.0f, slope_max = 0.0f;
		Array bad_at;
		for (const Lot &lot : ctx.lots) {
			if (lot.shop >= 0) {
				continue;
			}
			++lots;
			const HouseSpec &h = lot.house;
			float g_side = 0.0f, g_foot = 0.0f, joint = 0.0f;
			if (lot.walk_flags.empty()) {
				g_side = Math::max(0.0f, lot.walk_end - lot.flight_foot); // the flight's foot on the sidewalk's edge itself
			} else {
				const Lot::Flag &f0 = lot.walk_flags.front(), &f1 = lot.walk_flags.back();
				g_foot = (f0.c.z - f0.hl) - lot.flight_foot;
				g_side = lot.walk_end - (f1.c.z + f1.hl);
				for (size_t i = 1; i < lot.walk_flags.size(); ++i) {
					joint = Math::max(joint, (lot.walk_flags[i].c.z - lot.walk_flags[i].hl) - (lot.walk_flags[i - 1].c.z + lot.walk_flags[i - 1].hl));
					slope_max = Math::max(slope_max, std::abs(lot.walk_flags[i].c.y - lot.walk_flags[i - 1].c.y) / Math::max(0.05f, lot.walk_flags[i].c.z - lot.walk_flags[i - 1].c.z));
				}
				for (const Lot::Flag &f : lot.walk_flags) {
					// The slab's centre against the ground under it (the fit lowers it onto the soil: never above it).
					float_max = Math::max(float_max, f.c.y - ctx.lot_ground(lot, f.c.x, f.c.z));
					// Anything solid standing on the walk (knee height over the slab).
					const Vector3 w = lot.to_world(Vector3(f.c.x, f.c.y + 0.5f, f.c.z));
					for (const OBox &b : ctx.boxes) {
						const Vector3 l = b.xf.affine_inverse().xform(w);
						if (std::abs(l.x) < b.half.x && std::abs(l.y) < b.half.y && std::abs(l.z) < b.half.z) {
							++blocked;
							break;
						}
					}
				}
			}
			const float worst = Math::max(Math::max(g_side, g_foot), joint);
			gap_max = Math::max(gap_max, worst);
			if (worst >= 0.1f) {
				++bad;
				if (bad_at.size() < 8) {
					bad_at.push_back(String("L") + String::num_int64(lot.index) + " side " + String::num(g_side, 2) + " foot " + String::num(g_foot, 2) + " joint " + String::num(joint, 2));
				}
			}
			// Just past the walk's street end: the sidewalk (or its verge).
			const RoadZone zn = net.zone(lot.to_world(h.door_x, 0.0f, lot.walk_end + 0.1f));
			not_walk += (zn == RoadZone::Walk || zn == RoadZone::Verge || zn == RoadZone::Curb || zn == RoadZone::Asphalt) ? 0 : 1;
		}
		st["walk_lots"] = lots;
		st["walk_gap_bad"] = bad;
		st["walk_gap_max"] = r2(gap_max);
		st["walk_gap_at"] = bad_at;
		st["walk_end_not_sidewalk"] = not_walk;
		st["walk_blocked_flags"] = blocked;
		st["walk_slope_max"] = r2(slope_max);
		st["walk_flag_over_ground_max"] = std::round(float_max * 1000.0f) / 1000.0f;
	}
	// ---- stairs: every flight's bottom tread a riser over the ground at its foot (within 0.08 m)
	{
		int flights = 0, bad = 0, treads_max = 0, landings = 0, steep = 0;
		float err_max = 0.0f, riser_max = 0.0f;
		Array bad_at;
		for (const Lot &lot : ctx.lots) {
			if (lot.flight.empty()) {
				continue;
			}
			++flights;
			const Lot::Tread &t = lot.flight.back();
			const float zf = lot.flight_foot + 0.15f;
			const float g = surface(lot.to_world(lot.house.door_x, 0.0f, zf)) - lot.xf.origin.y;
			const float err = (t.top - lot.flight_riser) - g;
			err_max = Math::max(err_max, std::abs(err));
			riser_max = Math::max(riser_max, lot.flight_riser);
			steep += lot.flight_riser > (lot.shop >= 0 ? 0.185f : 0.215f) ? 1 : 0;
			treads_max = std::max(treads_max, int(lot.flight.size()));
			for (const Lot::Tread &q : lot.flight) {
				landings += q.z1 - q.z0 > 0.9f && &q - &lot.flight.front() >= 3 ? 1 : 0; // (the deck's own three wrap round: not landings)
			}
			if (std::abs(err) > 0.08f) {
				++bad;
				if (bad_at.size() < 8) {
					bad_at.push_back(String("L") + String::num_int64(lot.index) + (lot.shop >= 0 ? "s" : "h") + " n" + String::num_int64(lot.flight.size()) + " err " + String::num(err, 2) + " foot " + String::num(lot.flight_foot, 2));
				}
			}
		}
		st["stair_flights"] = flights;
		st["stair_bottom_bad"] = bad;
		st["stair_bottom_err_max"] = r2(err_max);
		st["stair_riser_max"] = r2(riser_max);
		st["stair_treads_max"] = treads_max;
		st["stair_landings"] = landings;
		st["stair_steep_risers"] = steep;
		st["stair_bad_at"] = bad_at;
	}
	// ---- prop contact: flat props' footprint corners, cars' four wheels, upright props' bases
	{
		int flat_n = 0, flat_bad = 0, car_n = 0, car_bad = 0, up_n = 0, up_bad = 0;
		float flat_max = 0.0f, car_max = 0.0f, up_max = 0.0f;
		Array bad_at;
		for (const PropPlacement &pp : ctx.props) {
			const AssetInfo *a = ctx.assets->get(pp.id);
			const AABB &bb = a->aabb;
			const Lot *lt = pp.lot >= 0 && pp.lot < int(ctx.lots.size()) ? &ctx.lots[pp.lot] : nullptr;
			// The surface: a lot's yard (yard_lift: treads, and nothing on its buildings), else the street / ground.
			// What a point stands on in a lot: 0 the ground / sidewalk, 1 a tread of a long porch flight, 2 a building (a
			// shop's paving strip). A footprint corner over another support than its centre's is left out (Stage 3c: a
			// prop against a step's edge or a plinth stays on its centre's level).
			auto support = [&](const Vector3 &w) {
				if (!lt) {
					return 0;
				}
				const Vector3 l = lt->to_local(w);
				if (lt->shop >= 0) {
					return shop_support(*lt, l.x, l.z);
				}
				if (ctx.in_structure(*lt, l.x, l.z)) {
					return 2;
				}
				return lt->shop < 0 && ctx.yard_lift(*lt, l.x, l.z) > ctx.lot_ground(*lt, l.x, l.z) + 1e-4f ? 1 : 0;
			};
			const int base_sup = support(pp.xf.origin);
			auto surf = [&](const Vector3 &w) {
				if (lt) {
					const Vector3 l = lt->to_local(w);
					const HouseSpec &h = lt->house;
					const float dz1 = h.garage ? h.front_z - 0.4f : h.front_z + 1.0f;
					const bool drive = lt->shop < 0 && std::abs(l.x - lt->drive_x) <= 1.45f && l.z >= dz1 - 0.2f && l.z <= 0.0f;
					const int sp = support(w);
					if (sp != base_sup || (sp == 2 && lt->shop < 0)) {
						return std::nanf("");
					}
					if (lt->shop >= 0) {
						return lt->xf.origin.y + shop_surface(ctx, *lt, l.x, l.z); // paving strip, doorstep and treads, sidewalk
					}
					const float y = ctx.yard_lift(*lt, l.x, l.z);
					return Math::max(lt->xf.origin.y + y + (drive && prop_settle_kind(pp.id) == PropSettle::Car ? 0.025f : 0.0f), surface(w));
				}
				return surface(w);
			};
			const Vector3 base = pp.xf.origin;
			const float s0 = surf(base);
			if (std::isnan(s0) || base.y - s0 > 0.12f || base.y - s0 < -0.6f || (!lt && pp.id == "bush_round")) {
				continue; // on a building, stacked on something, or not a ground prop
			}
			const Basis rot = pp.xf.basis.orthonormalized();
			const float sc = pp.xf.basis.get_scale().x;
			const PropSettle kind = prop_settle_kind(pp.id);
			auto at = [&](float x, float z) { return base + rot.xform(Vector3(x * sc, 0.0f, z * sc)); };
			if (kind == PropSettle::Car) {
				const float cx = bb.get_center().x, cz = bb.get_center().z, hx = bb.size.x * 0.5f * 0.78f, hz = bb.size.z * 0.5f * 0.66f;
				float worst = 0.0f;
				for (float sx : { -1.0f, 1.0f }) {
					for (float sz : { -1.0f, 1.0f }) {
						const Vector3 w = at(cx + sx * hx, cz + sz * hz);
						const float s = surf(w);
						worst = std::isnan(s) ? worst : Math::max(worst, std::abs(w.y - s));
					}
				}
				++car_n;
				car_max = Math::max(car_max, worst);
				if (worst > 0.03f) {
					++car_bad;
					if (bad_at.size() < 10) {
						String ws;
						for (float sx : { -1.0f, 1.0f }) {
							for (float sz : { -1.0f, 1.0f }) {
								const Vector3 w = at(cx + sx * hx, cz + sz * hz);
								const Vector3 l = lt ? lt->to_local(w) : w;
								ws += " " + String::num(w.y - surf(w), 3) + "@" + String::num(l.x, 2) + "," + String::num(l.z, 2);
							}
						}
						bad_at.push_back(String("car ") + String(pp.id.c_str()) + (lt ? " L" + String::num_int64(pp.lot) : String(" S")) + " " + String::num(worst, 3) + ws);
					}
				}
			} else if (kind == PropSettle::Upright) {
				const float r = Math::clamp(a->radius > 0.0f ? a->radius : Math::min(bb.size.x, bb.size.z) * 0.5f, 0.1f, 0.45f) * sc;
				float low = surf(base);
				for (int k = 0; k < 8; ++k) {
					const float a = float(k) * float(Math_TAU) / 8.0f;
					const float s = surf(base + Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r));
					low = std::isnan(s) ? low : Math::min(low, s);
				}
				const float gap = base.y - low;
				++up_n;
				up_max = Math::max(up_max, gap);
				if (gap > 0.03f) {
					++up_bad;
					if (bad_at.size() < 10) {
						bad_at.push_back(String("up ") + String(pp.id.c_str()) + (lt ? " L" + String::num_int64(pp.lot) : String(" S")) + " " + String::num(gap, 3));
					}
				}
			} else {
				float worst = 0.0f;
				for (float u : { 0.0f, 1.0f }) {
					for (float v : { 0.0f, 1.0f }) {
						const Vector3 w = at(Math::lerp(bb.position.x, bb.get_end().x, u), Math::lerp(bb.position.z, bb.get_end().z, v));
						const float s = surf(w);
						worst = std::isnan(s) ? worst : Math::max(worst, w.y - s); // > 0: that corner floats (over a building: not a ground corner)
					}
				}
				++flat_n;
				flat_max = Math::max(flat_max, worst);
				if (worst > 0.03f) {
					++flat_bad;
					if (bad_at.size() < 10) {
						String cs;
						for (float u : { 0.0f, 1.0f }) {
							for (float v : { 0.0f, 1.0f }) {
								const Vector3 w = at(Math::lerp(bb.position.x, bb.get_end().x, u), Math::lerp(bb.position.z, bb.get_end().z, v));
								const Vector3 l = lt ? lt->to_local(w) : w;
								cs += " " + String::num(w.y - surf(w), 3) + "@" + String::num(l.x, 2) + "," + String::num(l.z, 2);
							}
						}
						const Vector3 lb = lt ? lt->to_local(base) : base;
						bad_at.push_back(String("flat ") + String(pp.id.c_str()) + (lt ? " L" + String::num_int64(pp.lot) : String(" S")) + " " + String::num(worst, 3) + " base " +
								String::num(lb.x, 2) + "," + String::num(lb.z, 2) + " dy " + String::num(base.y - s0, 3) + cs);
					}
				}
			}
		}
		st["contact_flat"] = flat_n;
		st["contact_flat_bad"] = flat_bad;
		st["contact_flat_max"] = r2(flat_max);
		st["contact_cars"] = car_n;
		st["contact_cars_bad"] = car_bad;
		st["contact_car_max"] = std::round(car_max * 1000.0f) / 1000.0f;
		st["contact_upright"] = up_n;
		st["contact_upright_bad"] = up_bad;
		st["contact_upright_max"] = r2(up_max);
		st["contact_bad_at"] = bad_at;
	}
	// ---- bounce columns: share of the pads, and each top piece between the full-jump and the full-jump + flap reach
	{
		int pads = 0, cols = 0, top_bad = 0, top_other = 0, top_leap = 0;
		for (const OBox &b : ctx.boxes) {
			pads += b.bouncy ? 1 : 0;
		}
		for (const CandySpec &c : ctx.candy) {
			if (c.pattern != CPAT_BOUNCE || c.kind != CANDY_LOLLIPOP) {
				continue;
			}
			++cols;
			const OBox *pad = nullptr; // (the nearest bounce pad: an arc's top leans up to 2.4 m off its axis)
			float best = 2.6f;
			for (const OBox &b : ctx.boxes) {
				const float d = Vector2(b.xf.origin.x - c.pos.x, b.xf.origin.z - c.pos.z).length();
				if (b.bouncy && d < best) {
					best = d;
					pad = &b;
				}
			}
			if (!pad) {
				++top_bad;
				continue;
			}
			float top = -1e9f;
			for (int i = 0; i < 8; ++i) {
				const Vector3 k((i & 1) ? pad->half.x : -pad->half.x, (i & 2) ? pad->half.y : -pad->half.y, (i & 4) ? pad->half.z : -pad->half.z);
				top = Math::max(top, pad->xf.xform(k).y);
			}
			const float dy = c.pos.y - top;
			const float a2 = move::bounce_apex(pad->bounce, true, false, true), a3 = move::bounce_apex(pad->bounce, true, true, false);
			top_bad += (dy <= a2 + move::COLLECT_HIGH + 0.15f || dy >= a3 + move::COLLECT_HIGH - 0.15f) ? 1 : 0;
			bool long_leap = false;
			for (const OBox &b : ctx.boxes) {
				if (!b.walkable || b.bouncy || b.top() + 3.4f + move::COLLECT_HIGH < c.pos.y - 0.2f) {
					continue;
				}
				const Vector3 l = b.xf.affine_inverse().xform(Vector3(c.pos.x, b.xf.origin.y, c.pos.z));
				const Vector3 sc = b.xf.basis.get_scale();
				const float dx = Math::max(0.0f, std::abs(l.x) - b.half.x) * sc.x, dz = Math::max(0.0f, std::abs(l.z) - b.half.z) * sc.z;
				const float dist = std::sqrt(dx * dx + dz * dz), drop = Math::max(0.0f, b.top() - (c.pos.y - move::COLLECT_HIGH));
				if (dist < 8.5f + 3.4f * drop) {
					long_leap = true; // a full running leap + flap off its edge (~5.8 m out at the flap's peak) could get it
				}
				if (dist < 6.65f + 3.4f * drop) {
					++top_other;
					break;
				}
			}
			top_leap += long_leap ? 1 : 0;
		}
		// The apexes (feet over the pad): plain bounce jump not held (cut) / plain; full jump (held on contact) / + spin;
		// full jump + flap / + spin. 14: car hoods, mini trampolines, awnings; 17: big trampolines; 13.5: inflatables.
		for (float sp : { 13.5f, 14.0f, 17.0f }) {
			Array ap;
			ap.push_back(r2(move::bounce_apex(sp, false, false, false, true)));
			ap.push_back(r2(move::bounce_apex(sp, false, false)));
			ap.push_back(r2(move::bounce_apex(sp, true, false)));
			ap.push_back(r2(move::bounce_apex(sp, true, false, true)));
			ap.push_back(r2(move::bounce_apex(sp, true, true)));
			ap.push_back(r2(move::bounce_apex(sp, true, true, true)));
			st[(String("bounce_apex_") + String::num(sp, 1))] = ap;
		}
		Array rej;
		for (int v : ctx.stats.bounce_reject) {
			rej.push_back(v);
		}
		st["bounce_rejects"] = rej; // axis blocked, canopy, another platform in reach
		st["bounce_pads"] = pads;
		st["bounce_columns"] = cols;
		st["bounce_share"] = pads ? std::round(1000.0 * cols / pads) / 1000.0 : 0.0;
		st["bounce_top_out_of_band"] = top_bad;
		st["bounce_top_reachable_otherwise"] = top_other;
		st["bounce_top_long_leap"] = top_leap; // (informational: a precise full-speed leap + flap from a deck could reach it)
	}
}
} // namespace

void sweep_layout_stats(const GenContext &ctx, const InfestationLayout *inf, Dictionary &st) {
	const RoadNet &net = ctx.roads;
	st["roads"] = int(net.roads.size());
	st["boxes"] = int(ctx.boxes.size()); // one collision shape each in the game (load time)
	float road_len = 0.0f;
	for (const Road &r : net.roads) {
		road_len += r.length();
	}
	st["road_length"] = std::round(road_len * 10.0f) / 10.0f;
	// Lot footprints overlapping each other (deeper than 5 cm).
	int overlaps = 0, neighbour_overlaps = 0;
	float overlap_max = 0.0f;
	for (size_t i = 0; i < ctx.lots.size(); ++i) {
		for (size_t j = i + 1; j < ctx.lots.size(); ++j) {
			const float d = lot_overlap(ctx.lots[i], ctx.lots[j]);
			overlaps += d > 0.05f ? 1 : 0;
			neighbour_overlaps += (d > 0.05f && (ctx.lots[i].next == int(j) || ctx.lots[i].prev == int(j))) ? 1 : 0;
			overlap_max = Math::max(overlap_max, d);
		}
	}
	st["lot_overlaps"] = overlaps;
	st["lot_overlaps_neighbours"] = neighbour_overlaps; // of those, prev/next pairs (the inside of a bend)
	st["lot_overlap_max"] = std::round(overlap_max * 100.0f) / 100.0f;
	// Lots reaching into a road corridor (asphalt, curb, verge, sidewalk) by more than 10 cm: the
	// footprint sampled every 0.5 m against every road it lies alongside.
	int intruding = 0;
	float intrusion_max = 0.0f;
	std::vector<int> by_road_n(net.roads.size(), 0);
	std::vector<float> by_road_max(net.roads.size(), 0.0f);
	for (const Lot &lot : ctx.lots) {
		float worst = 0.0f;
		std::vector<float> worst_r(net.roads.size(), 0.0f);
		const int nx = std::max(1, int(lot.width / 0.5f)), nz = std::max(1, int(lot.depth / 0.5f));
		for (int ix = 0; ix <= nx; ++ix) {
			for (int iz = 0; iz <= nz; ++iz) {
				const Vector3 p = lot.to_world(-lot.width * 0.5f + lot.width * ix / nx, 0, -lot.depth * iz / nz);
				for (int r = 0; r < int(net.roads.size()); ++r) {
					const RoadHit h = net.project(r, p);
					if (h.beyond <= 0.0f) {
						worst_r[r] = Math::max(worst_r[r], net.roads[r].edge() - std::abs(h.lateral));
						worst = Math::max(worst, worst_r[r]);
					}
				}
			}
		}
		intruding += worst > 0.1f ? 1 : 0;
		intrusion_max = Math::max(intrusion_max, worst);
		for (size_t r = 0; r < worst_r.size(); ++r) {
			by_road_n[r] += worst_r[r] > 0.1f ? 1 : 0;
			by_road_max[r] = Math::max(by_road_max[r], worst_r[r]);
		}
	}
	st["lots_in_road"] = intruding;
	st["road_intrusion_max"] = std::round(intrusion_max * 100.0f) / 100.0f;
	Array in_n, in_max; // per road (0: main street, 1: side lane)
	for (size_t r = 0; r < by_road_n.size(); ++r) {
		in_n.push_back(by_road_n[r]);
		in_max.push_back(std::round(by_road_max[r] * 100.0f) / 100.0f);
	}
	st["lots_in_road_by_road"] = in_n;
	st["road_intrusion_max_by_road"] = in_max;
	// Stage 2a: the full corridor test the lot placer uses (straight corridors, bulb rings, fillets, Port
	// run-ons), and the surface classifier sampled over every footprint (0.1 m inset from its edges).
	{
		int in_corr = 0, in_zone = 0;
		float corr_max = 0.0f;
		for (const Lot &lot : ctx.lots) {
			float own, other;
			lot_corridor_reach(ctx, lot, own, other);
			const float worst = Math::max(own, other);
			in_corr += worst > 0.1f ? 1 : 0;
			corr_max = Math::max(corr_max, worst);
			bool hit = false;
			for (float x = -lot.width * 0.5f + 0.1f; x <= lot.width * 0.5f - 0.1f + 1e-3f && !hit; x += 0.5f) {
				for (float z = -0.1f; z >= -lot.depth + 0.1f - 1e-3f && !hit; z -= 0.5f) {
					hit = net.zone(lot.to_world(x, 0, z)) != RoadZone::Off;
				}
			}
			in_zone += hit ? 1 : 0;
		}
		// Stage 1e's worry on bulbs and bends: yard candy (and the takeoffs under it) over the curb or the road.
		int lot_candy_on_road = 0;
		for (const CandySpec &c : ctx.candy) {
			if (c.lot >= 0) {
				const RoadZone z = net.zone(c.pos);
				lot_candy_on_road += (z == RoadZone::Asphalt || z == RoadZone::Curb) ? 1 : 0;
			}
		}
		st["lot_candy_on_road"] = lot_candy_on_road;
		st["lots_in_corridor"] = in_corr;
		st["corridor_reach_max"] = std::round(corr_max * 100.0f) / 100.0f;
		st["lots_in_zone"] = in_zone;
	}
	{
		int branches = 0, bulbs = 0, port = 0, bulb_lots = 0;
		Array lens, lots_by, kinds;
		for (const Road &r : net.roads) {
			branches += r.parent >= 0 ? 1 : 0;
			bulbs += r.bulb() ? 1 : 0;
			if (r.parent >= 0 && r.end_kind == RoadEnd::Port) {
				port = r.parent_side;
			}
			lens.push_back(std::round(r.length() * 10.0f) / 10.0f);
			kinds.push_back(r.parent < 0 ? "main" : (r.bulb() ? "bulb" : (r.end_kind == RoadEnd::Port ? "port" : "end")));
		}
		std::vector<int> per(net.roads.size(), 0);
		for (const Lot &lot : ctx.lots) {
			per[lot.road]++;
			bulb_lots += lot.bulb ? 1 : 0;
		}
		for (int n : per) {
			lots_by.push_back(n);
		}
		st["branches"] = branches;
		st["bulbs"] = bulbs;
		st["port_branch"] = port;
		st["road_lengths"] = lens;
		st["road_kinds"] = kinds;
		st["lots_by_road"] = lots_by;
		st["bulb_lots"] = bulb_lots;
		// Lots round each bulb (the brief: 3-5 per turning circle).
		Array ring;
		int ring_min = 1 << 30;
		for (int r = 0; r < int(net.roads.size()); ++r) {
			if (!net.roads[r].bulb()) {
				continue;
			}
			int n = 0;
			for (const Lot &lot : ctx.lots) {
				n += (lot.bulb && lot.road == r) ? 1 : 0;
			}
			ring.push_back(n);
			ring_min = std::min(ring_min, n);
		}
		st["bulb_ring_lots"] = ring;
		st["bulb_ring_min"] = ring.is_empty() ? -1 : ring_min;
		st["fillets"] = int(net.fillets.size());
		st["road_attempts"] = ctx.stats.rerolls;
		st["lot_rejects"] = ctx.stats.lot_rejects; // Stage 2b: layouts rerolled for a house count outside 38-56
		{
			Array rj;
			for (int k = 0; k < 16; ++k) {
				rj.push_back(ctx.stats.road_rejects[k]);
			}
			st["road_rejects"] = rj;
		}
		Array fp;
		fp.push_back(std::round((ctx.bound_x1 - ctx.bound_x0) * 10.0f) / 10.0f);
		fp.push_back(std::round((ctx.bound_z1 - ctx.bound_z0) * 10.0f) / 10.0f);
		st["footprint"] = fp;
		const float reach = Math::max(Math::max(-ctx.bound_x0, ctx.bound_x1), Math::max(-ctx.bound_z0, ctx.bound_z1));
		st["footprint_reach"] = std::round(reach * 10.0f) / 10.0f;
	}
	// Stage 4c: towns: the template and size drawn, the N/S port roads, the square, the stepped shop fronts.
	if (ctx.kind == SettlementKind::Town) {
		static const char *SIZES[] = { "small", "medium", "large" };
		static const char *PLACES[] = { "centre", "corner", "end" };
		st["town_template"] = town_template_name(ctx.town_template);
		st["town_size"] = SIZES[int(ctx.town_size) % 3];
		st["town_ports"] = ctx.town_ports;
		int port_roads = 0, dead_ends = 0;
		float built = 0.0f;
		for (const Road &R : net.roads) {
			port_roads += (R.parent >= 0 && R.end_kind == RoadEnd::Port) ? 1 : 0;
			dead_ends += R.end_kind == RoadEnd::DeadEnd ? 1 : 0;
			built += Math::max(0.0f, R.lots_s1 - R.lots_s0);
		}
		st["town_port_roads"] = port_roads;
		st["town_dead_ends"] = dead_ends;
		st["town_built_len"] = std::round(built * 10.0f) / 10.0f;
		st["square_valid"] = int(ctx.square.valid);
		if (ctx.square.valid) {
			const Lot &A = ctx.square.area;
			st["square_place"] = PLACES[int(ctx.square.place) % 3];
			Array sz;
			sz.push_back(std::round(A.width * 10.0f) / 10.0f);
			sz.push_back(std::round(A.depth * 10.0f) / 10.0f);
			st["square_size"] = sz;
			// The square against lots and corridors (0 by construction), and its pad against its street.
			int sq_hits = 0;
			for (const Lot &lot : ctx.lots) {
				sq_hits += lot_overlap(lot, A) > 0.05f ? 1 : 0;
			}
			float own, other;
			lot_corridor_reach(ctx, A, own, other);
			st["square_lot_overlaps"] = sq_hits;
			st["square_corridor_reach"] = std::round(Math::max(own, other) * 100.0f) / 100.0f;
			if (ctx.terrain) {
				Vector3 o = A.xf.origin;
				st["square_pad_minus_road"] = std::round((A.xf.origin.y - net.roads[A.road].project(o).y) * 100.0f) / 100.0f;
				// How level the pad really is: the ground over the pad part, against the pad.
				float lo = 1e9f, hi = -1e9f;
				for (float x = -A.width * 0.5f + 2.0f; x <= A.width * 0.5f - 2.0f; x += 1.0f) {
					for (float z = ctx.square.pad_z - 2.0f; z >= -A.depth + 0.5f; z -= 1.0f) { // (past the ramp's and the edges' lattice squares)
						const float g = ctx.lot_ground(A, x, z);
						lo = Math::min(lo, g);
						hi = Math::max(hi, g);
					}
				}
				Array lv;
				lv.push_back(std::round(lo * 100.0f) / 100.0f);
				lv.push_back(std::round(hi * 100.0f) / 100.0f);
				st["square_pad_ground"] = lv;
			}
		}
		st["plazas"] = int(ctx.plazas.size()) - (ctx.square.valid ? 1 : 0);
		// Spawn: on a straight enough for the town cine shot (festival_cine.cpp town_road).
		{
			const Road &M = net.trunk();
			st["cine_straight"] = int(town_straight_near(M, M.project(ctx.spawn).s));
		}
		// Stepped fronts: the step between touching neighbours' floors, the deepest plinth, the doorstep treads.
		float step_max = 0.0f, plinth_max = 0.0f;
		int steps = 0, stepped = 0;
		for (const Lot &lot : ctx.lots) {
			plinth_max = Math::max(plinth_max, -lot.found_house);
			steps += lot.porch_steps - 3;
			if (lot.next >= 0 && lot_overlap(lot, ctx.lots[lot.next]) > -0.6f) { // (touching: not across a side street)
				const float d = std::abs(lot.xf.origin.y - ctx.lots[lot.next].xf.origin.y);
				step_max = Math::max(step_max, d);
				stepped += d > 0.15f ? 1 : 0;
			}
		}
		st["facade_step_max"] = std::round(step_max * 100.0f) / 100.0f;
		st["facades_stepped"] = stepped;
		st["shop_plinth_max"] = std::round(plinth_max * 100.0f) / 100.0f;
		st["shop_door_treads"] = steps;
	}
	// Stage 0 parity of street_yaw_at (camera guide, not in the dump): RoadNet::nearest's road tangent
	// against the pre-RoadNet rule, over the footprint every metre.
	if (net.roads.size() == 2 && net.roads[1].straight) {
		const Road &m = net.roads[0], &lane = net.roads[1];
		int checked = 0, differ = 0;
		for (float z = ctx.bound_z0 - 10.0f; z <= ctx.bound_z1 + 10.0f; z += 1.0f) {
			for (float x = ctx.bound_x0 - 10.0f; x <= ctx.bound_x1 + 10.0f; x += 1.0f) {
				const Vector3 p(x, 0, z);
				Vector3 t_old = (m.at(p.x + 1.0f) - m.at(p.x - 1.0f)).normalized();
				if (std::abs(p.x - lane.parent_s) < m.edge() && (p.z - lane.pts[0].z) * lane.dir.z > m.edge()) {
					t_old = lane.dir;
				}
				const RoadHit h = net.nearest(p);
				const Vector3 t_new = net.roads[h.road].tangent_at(h.s_ext);
				++checked;
				differ += (t_old - t_new).length() > 1e-4f ? 1 : 0;
			}
		}
		st["yaw_checked"] = checked;
		st["yaw_differ"] = differ;
	}
	// Stage 3b: the hills. Road grade (per 1 m sample and over 4 m), lot pads against their road and the
	// natural ground, foundations against the lowest ground under each building, candy and props against
	// the ground under them.
	if (ctx.terrain) {
		float grade_max = 0.0f, grade4_max = 0.0f, relief_lo = 1e9f, relief_hi = -1e9f;
		for (const Road &R : net.roads) {
			for (size_t i = 0; i + 1 < R.pts.size(); ++i) {
				grade_max = Math::max(grade_max, std::abs(R.pts[i + 1].y - R.pts[i].y) / Math::max(1e-3f, R.s[i + 1] - R.s[i]));
			}
			for (float st = R.s_begin(); st + 4.0f <= R.s_end(); st += 1.0f) {
				grade4_max = Math::max(grade4_max, std::abs(R.at(st + 4.0f).y - R.at(st).y) / 4.0f);
			}
		}
		for (int iz = 0; iz < ctx.terrain->nz; iz += 3) {
			for (int ix = 0; ix < ctx.terrain->nx; ix += 3) {
				relief_lo = Math::min(relief_lo, ctx.terrain->vertex(ix, iz));
				relief_hi = Math::max(relief_hi, ctx.terrain->vertex(ix, iz));
			}
		}
		st["road_grade_max"] = std::round(grade_max * 1000.0f) / 1000.0f;
		st["road_grade4_max"] = std::round(grade4_max * 1000.0f) / 1000.0f;
		st["terrain_relief"] = std::round((relief_hi - relief_lo) * 100.0f) / 100.0f;
		// Pads: pad - road at the lot front (clamped to -1.2 .. +1.8 of the natural median by construction),
		// the yard's ground against the pad behind the front bank, and the cut / fill against the natural relief.
		float pad_road_lo = 1e9f, pad_road_hi = -1e9f, yard_lo = 1e9f, yard_hi = -1e9f, cut = 0.0f, fill = 0.0f;
		int found_bad = 0, found_shown = 0, steps_added = 0;
		float found_gap_max = -1e9f, found_depth_max = 0.0f;
		for (const Lot &lot : ctx.lots) {
			const float pad = lot.xf.origin.y;
			Vector3 o = lot.xf.origin;
			const float road = lot.bulb ? net.roads[lot.road].bulb_c.y : net.roads[lot.road].project(o).y;
			pad_road_lo = Math::min(pad_road_lo, pad - road);
			pad_road_hi = Math::max(pad_road_hi, pad - road);
			for (float x = -lot.width * 0.5f + 0.5f; x <= lot.width * 0.5f - 0.5f; x += 1.0f) {
				for (float z = -3.5f; z >= -lot.depth + 0.5f; z -= 1.0f) {
					const float g = ctx.lot_ground(lot, x, z);
					yard_lo = Math::min(yard_lo, g);
					yard_hi = Math::max(yard_hi, g);
					const Vector3 w = lot.to_world(x, 0.0f, z);
					const float n = hood_natural(*ctx.terrain, w.x, w.z) - (pad + g);
					cut = Math::max(cut, n);
					fill = Math::max(fill, -n);
				}
			}
			// Each foundation's bottom against the lowest ground under its footprint (fine samples, edges included).
			const HouseSpec &h = lot.house;
			auto gmin = [&](float x0, float x1, float z0, float z1) {
				float g = 1e9f;
				const int nx = Math::max(1, int(std::ceil((x1 - x0) / 0.15f))), nz = Math::max(1, int(std::ceil((z1 - z0) / 0.15f)));
				for (int i = 0; i <= nx; ++i) {
					for (int k = 0; k <= nz; ++k) {
						g = Math::min(g, ctx.lot_ground(lot, Math::lerp(x0, x1, float(i) / nx), Math::lerp(z0, z1, float(k) / nz)));
					}
				}
				return g;
			};
			auto check = [&](float bottom, float x0, float x1, float z0, float z1) {
				const float g = gmin(x0, x1, z0, z1);
				found_gap_max = Math::max(found_gap_max, bottom - g);
				found_bad += bottom > g + 0.005f ? 1 : 0;
				found_shown += g < -0.05f ? 1 : 0;
				found_depth_max = Math::max(found_depth_max, -g);
			};
			const float zf = h.front_z, zb = h.front_z - h.depth;
			if (lot.shop >= 0) {
				// Stage 4c: a shop's plinth runs under the body and the paved strip out to the lot front.
				check(lot.found_house, -h.width * 0.5f, h.width * 0.5f, zb - 0.08f, 0.0f);
				steps_added += lot.porch_steps - 3;
				continue;
			}
			check(lot.found_house, h.house_x - h.width * 0.5f - 0.12f, h.house_x + h.width * 0.5f + 0.12f, zb - 0.12f, zf + 0.12f);
			if (h.garage) {
				const float gx = h.house_x + float(h.garage_side) * (h.width * 0.5f + 1.9f);
				check(lot.found_garage, gx - 1.98f, gx + 1.98f, zf - 6.48f, zf - 0.32f);
			}
			check(lot.found_porch, h.porch_x - h.porch_w * 0.5f, h.porch_x + h.porch_w * 0.5f, zf, zf + h.porch_d);
			steps_added += lot.porch_steps - 3;
		}
		auto pair = [](float a, float b) {
			Array r;
			r.push_back(std::round(a * 100.0f) / 100.0f);
			r.push_back(std::round(b * 100.0f) / 100.0f);
			return r;
		};
		st["pad_minus_road"] = pair(pad_road_lo, pad_road_hi);
		st["yard_vs_pad"] = pair(yard_lo, yard_hi);
		st["cut_max"] = std::round(cut * 100.0f) / 100.0f;
		st["fill_max"] = std::round(fill * 100.0f) / 100.0f;
		st["foundations_above_ground"] = found_bad;
		st["foundation_gap_max"] = std::round(found_gap_max * 1000.0f) / 1000.0f;
		st["foundations_showing"] = found_shown;
		st["foundation_depth_max"] = std::round(found_depth_max * 100.0f) / 100.0f;
		st["porch_steps_added"] = steps_added;
		// Round 2: retaining walls (the tallest step between neighbouring yards), the deepest exposed foundation
		// (house, garage or porch), and how steep the ground gets (lattice edges: max and 99th percentile).
		{
			int walls = 0;
			float retain_max = 0.0f, perch_max = -1e9f;
			int line_walls = 0;
			float line_len = 0.0f, line_step = 0.0f;
			for (const Lot &lot : ctx.lots) {
				for (float r : lot.retain) {
					walls += r > 0.0f ? 1 : 0;
					retain_max = Math::max(retain_max, r);
				}
				for (const Lot::Wall &w : lot.walls) {
					if (!w.side) {
						++line_walls; // Ship: the walls along back / side lines against another street's yard or sidewalk
						line_len += (w.b - w.a).length();
						line_step = Math::max(line_step, w.step);
					}
				}
				const Vector3 o = lot.xf.origin;
				perch_max = Math::max(perch_max, o.y - (lot.bulb ? net.roads[lot.road].bulb_c.y : net.roads[lot.road].project(o).y));
			}
			std::vector<float> slopes;
			const HoodTerrain &T = *ctx.terrain;
			for (int iz = 0; iz + 1 < T.nz; ++iz) {
				for (int ix = 0; ix + 1 < T.nx; ++ix) {
					slopes.push_back(std::abs(T.vertex(ix + 1, iz) - T.vertex(ix, iz)) / TERRAIN_LATTICE);
					slopes.push_back(std::abs(T.vertex(ix, iz + 1) - T.vertex(ix, iz)) / TERRAIN_LATTICE);
				}
			}
			// Stage 3c: where the ground is steeper than 45 degrees inside a lot (a cut or fill too steep to drape: a wall's
			// case), how often, and the worst one (which lot, where in it, and what is there).
			{
				int steep = 0, unwalled = 0;
				float worst = 0.0f, worst_open = 0.0f;
				String at;
				Array steep_list; // Ship: every steep edge: its lots (each end), slope, walled or not
				auto lot_of = [&](const Vector3 &q) {
					for (const Lot &lt : ctx.lots) {
						const Vector3 l = lt.to_local(Vector3(q.x, lt.xf.origin.y, q.z));
						if (std::abs(l.x) <= lt.width * 0.5f && l.z <= 0.0f && l.z >= -lt.depth) {
							return lt.index;
						}
					}
					return -1;
				};
				for (int iz = 0; iz + 1 < T.nz; ++iz) {
					for (int ix = 0; ix + 1 < T.nx; ++ix) {
						for (int d = 0; d < 2; ++d) {
							const float s = std::abs(T.vertex(ix + (d == 0), iz + (d == 1)) - T.vertex(ix, iz)) / TERRAIN_LATTICE;
							if (s <= 1.0f) {
								continue;
							}
							const Vector3 q(T.x0 + (float(ix) + (d == 0 ? 0.5f : 0.0f)) * TERRAIN_LATTICE, 0.0f, T.z0 + (float(iz) + (d == 1 ? 0.5f : 0.0f)) * TERRAIN_LATTICE);
							const Vector3 e0(T.x0 + float(ix) * TERRAIN_LATTICE, 0.0f, T.z0 + float(iz) * TERRAIN_LATTICE);
							const Vector3 e1 = e0 + Vector3(d == 0 ? TERRAIN_LATTICE : 0.0f, 0.0f, d == 1 ? TERRAIN_LATTICE : 0.0f);
							// Walled: a retaining wall segment crosses this lattice edge (segments in world xz, from Lot::walls).
							bool walled = false;
							for (const Lot &wl : ctx.lots) {
								for (const Lot::Wall &w : wl.walls) {
									const Vector3 a = wl.to_world(w.a), b = wl.to_world(w.b);
									auto cross2 = [](float ux, float uz, float vx, float vz) { return ux * vz - uz * vx; };
									const float d1 = cross2(b.x - a.x, b.z - a.z, e0.x - a.x, e0.z - a.z), d2 = cross2(b.x - a.x, b.z - a.z, e1.x - a.x, e1.z - a.z);
									const float d3 = cross2(e1.x - e0.x, e1.z - e0.z, a.x - e0.x, a.z - e0.z), d4 = cross2(e1.x - e0.x, e1.z - e0.z, b.x - e0.x, b.z - e0.z);
									walled = walled || (d1 * d2 <= 0.0f && d3 * d4 <= 0.0f);
								}
							}
							for (const Lot &lt : ctx.lots) {
								const Vector3 l = lt.to_local(Vector3(q.x, lt.xf.origin.y, q.z));
								if (std::abs(l.x) <= lt.width * 0.5f && l.z <= 0.0f && l.z >= -lt.depth) {
									++steep;
									unwalled += walled ? 0 : 1;
									if (!walled) {
										worst_open = Math::max(worst_open, s);
									}
									if (steep_list.size() < 40) {
										const int la = lot_of(e0), lb = lot_of(e1);
										// (each end's height)
										const String ends = " h" + String::num(T.at(e0.x, e0.z), 2) + "/" + String::num(T.at(e1.x, e1.z), 2);
										steep_list.push_back(String::num(s, 2) + (walled ? " W " : " - ") + "L" + String::num_int64(la) + "/r" + String::num_int64(la >= 0 ? ctx.lots[la].road : -1) + " L" +
												String::num_int64(lb) + "/r" + String::num_int64(lb >= 0 ? ctx.lots[lb].road : -1) + " at L" + String::num_int64(lt.index) + "(" + String::num(l.x, 1) + "," +
												String::num(l.z, 1) + ") d" + String::num(lt.depth, 1) + " back " + String::num(lt.house.front_z - lt.house.depth, 1) + ends);
									}
									if (s > worst) {
										worst = s;
										at = String("L") + String::num_int64(lt.index) + (lt.shop >= 0 ? "s(" : "h(") + String::num(l.x, 1) + "," + String::num(l.z, 1) + ") w" +
												String::num(lt.width, 1) + " d" + String::num(lt.depth, 1) + " house z" + String::num(lt.house.front_z, 1) + ".." +
												String::num(lt.house.front_z - lt.house.depth, 1);
									}
									break;
								}
							}
						}
					}
				}
				st["lot_steep_edges"] = steep;
				st["lot_steep_max"] = std::round(worst * 100.0f) / 100.0f;
				st["lot_steep_at"] = at;
				st["lot_steep_unwalled"] = unwalled;
				st["lot_steep_unwalled_max"] = std::round(worst_open * 100.0f) / 100.0f;
				st["lot_steep_list"] = steep_list;
			}
			std::sort(slopes.begin(), slopes.end());
			// The ground against the street's surfaces: along every straight curb / verge / sidewalk band, the visible
			// terrain (4 cm under the lattice) poking up through the surface, or falling away under its outer edge.
			int pokes = 0;
			float poke_max = -1e9f, gap_max = 0.0f;
			Array poke_at;
			String gap_at;
			for (const Road &R : net.roads) {
				for (float stn = R.s_begin() + 0.5f; stn < R.s_end() - 0.5f; stn += 1.0f) {
					for (int side : { 1, -1 }) {
						if (R.cut(side, stn) || (R.bulb() && stn > R.bulb_s)) {
							continue;
						}
						for (float off : { R.half_road + R.curb + 0.1f, R.half_road + R.curb + R.verge + 0.1f, R.edge() - 0.05f }) {
							const Vector3 p = R.point(stn, float(side) * off);
							const RoadZone zn = net.zone(p);
							if (zn != RoadZone::Walk && zn != RoadZone::Verge) {
								continue;
							}
							const float surf = R.at(stn).y + (zn == RoadZone::Verge ? 0.05f : 0.07f);
							const float g = ctx.ground(p.x, p.z);
							const float poke = (g - 0.04f) - surf;
							poke_max = Math::max(poke_max, poke);
							if (poke > 0.0f) {
								++pokes;
								if (poke > poke_max - 1e-4f) {
									poke_at.clear(); // the worst one
								}
								if (poke_at.size() < 1) { // (then its square's corners)
									poke_at.push_back(String::num_int64(&R - &net.roads[0]) + "@" + String::num(stn, 1) + "/" + String::num_int64(side) + "/" + String::num(off, 1) + ":" + String::num(poke, 2) + " g=" + String::num(g, 2) + " r=" + String::num(R.at(stn).y, 2));
									// The lattice square's corners: height, the surface zone there and the lot it stands in.
									const HoodTerrain &TT = *ctx.terrain;
									const int ix = int(std::floor((p.x - TT.x0) / TERRAIN_LATTICE)), iz = int(std::floor((p.z - TT.z0) / TERRAIN_LATTICE));
									for (int c = 0; c < 4; ++c) {
										const int vx = ix + (c & 1), vz = iz + (c >> 1);
										const Vector3 q(TT.x0 + vx * TERRAIN_LATTICE, 0.0f, TT.z0 + vz * TERRAIN_LATTICE);
										int in_lot = -1;
										for (const Lot &lt : ctx.lots) {
											const Vector3 lq = lt.to_local(Vector3(q.x, lt.xf.origin.y, q.z));
											if (std::abs(lq.x) <= lt.width * 0.5f && lq.z <= 0.0f && lq.z >= -lt.depth) {
												in_lot = lt.index;
											}
										}
										const RoadHit nh = net.nearest(q);
										poke_at.push_back(String::num(TT.vertex(vx, vz), 2) + " z" + String::num_int64(int(net.zone(q))) + " lot" + String::num_int64(in_lot) + " road" + String::num_int64(nh.road) + " lat" + String::num(nh.lateral, 1) + " ry" + String::num(nh.y, 2));
									}
								}
							}
							if (R.at(stn).y - g > gap_max) {
								gap_max = R.at(stn).y - g;
								gap_at = String::num_int64(&R - &net.roads[0]) + "@" + String::num(stn, 1) + "/" + String::num_int64(side) + "/" + String::num(off, 1) + " zone" + String::num_int64(int(zn));
							}
						}
					}
				}
			}
			st["walk_pokes"] = pokes;
			st["walk_poke_at"] = poke_at;
			st["walk_gap_at"] = gap_at;
			st["walk_poke_max"] = std::round(poke_max * 100.0f) / 100.0f;
			st["walk_gap_max"] = std::round(gap_max * 100.0f) / 100.0f;
			st["retaining_walls"] = walls;
			st["line_walls"] = line_walls;
			st["line_wall_len"] = std::round(line_len * 10.0f) / 10.0f;
			st["line_wall_step_max"] = std::round(line_step * 100.0f) / 100.0f;
			st["retaining_step_max"] = std::round(retain_max * 100.0f) / 100.0f;
			st["pad_above_road_max"] = std::round(perch_max * 100.0f) / 100.0f;
			st["ground_slope_max"] = slopes.empty() ? 0.0 : std::round(slopes.back() * 100.0f) / 100.0f;
			st["ground_slope_p99"] = slopes.empty() ? 0.0 : std::round(slopes[slopes.size() * 99 / 100] * 100.0f) / 100.0f;
		}
		// Candy: every piece's height over the ground under it (a piece below 0.3 m is buried or scraping).
		int candy_low = 0;
		float candy_min = 1e9f;
		Array low_where; // pattern / lot @ height over the ground, the first few
		for (const CandySpec &c : ctx.candy) {
			const float dy = c.pos.y - ctx.ground(c.pos.x, c.pos.z);
			candy_min = Math::min(candy_min, dy);
			if (dy < 0.3f) {
				++candy_low;
				if (low_where.size() < 10) {
					low_where.push_back(String::num_int64(c.pattern) + "/L" + String::num_int64(c.lot) + "@" + String::num(dy, 2));
				}
			}
		}
		st["candy_low"] = candy_low;
		st["candy_low_where"] = low_where;
		st["candy_over_ground_min"] = std::round(candy_min * 100.0f) / 100.0f;
		// Props within 0.45 m of the ground: off it by more than 5 cm with nothing under them is floating;
		// more than 15 cm into it is buried (hedge bushes are set 5-10 cm in on purpose).
		int floating = 0, buried = 0;
		int strict_flat = 0, strict_up = 0, strict_car = 0;
		Array strict_ids;
		float float_max = 0.0f;
		Array float_ids; // the first few floating / buried props, for a look
		// (Stage 4c: which lot, and where in it: lot-local x, z)
		auto where = [&](const PropPlacement &pp) {
			if (pp.lot < 0 || pp.lot >= int(ctx.lots.size())) {
				return String("S");
			}
			const Lot &lt = ctx.lots[pp.lot];
			const Vector3 l = lt.to_local(pp.xf.origin);
			return String("L") + String::num_int64(pp.lot) + "(" + String::num(l.x, 1) + "," + String::num(l.z, 1) + " y" + String::num(l.y, 2) + " g" +
					String::num(ctx.lot_ground(lt, l.x, l.z), 2) + " st" + String::num_int64(lt.porch_steps) + " pz" + String::num(lt.house.front_z + lt.house.porch_d, 1) + ")";
		};
		for (const PropPlacement &pp : ctx.props) {
			const Vector3 &p = pp.xf.origin;
			// (Stage 4c: on a street surface, against that surface: a sidewalk's top is 7 cm over the ground under it.)
			float surf = ctx.ground(p.x, p.z);
			{
				int which = -1;
				const RoadZone zn = net.zone(p, &which);
				if (zn != RoadZone::Off && which >= 0) {
					const float lift = zn == RoadZone::Walk ? 0.07f : (zn == RoadZone::Verge ? 0.05f : (zn == RoadZone::Curb ? 0.12f : 0.01f));
					float ry = net.roads[which].project(p).y;
					// On a rounded corner's bands the surface is laid between its two roads' heights (street.cpp fillet_point).
					for (const RoadFillet &F : net.fillets) {
						const float dd = Vector2(p.x - F.c.x, p.z - F.c.z).length();
						if (!F.bulb && F.in_sector(p) && dd > F.r_out() - 0.2f && dd < F.r + 0.2f) {
							const Vector3 u = Vector3(p.x - F.c.x, 0.0f, p.z - F.c.z) / Math::max(dd, 1e-4f);
							const float a = std::acos(Math::clamp(F.u0.dot(F.u1), -1.0f, 1.0f));
							const float t = a > 1e-4f ? Math::clamp(std::acos(Math::clamp(F.u0.dot(u), -1.0f, 1.0f)) / a, 0.0f, 1.0f) : 0.5f;
							ry = Math::lerp(F.p0.y, F.p1.y, t);
						}
					}
					surf = Math::max(surf, ry + lift);
				}
			}
			const float dy = p.y - surf;
			// Stage 3c: buried means under the lowest surface round its base (an upright thing is sunk to that on a slope
			// by design, and a flat one's plane is lowered until no corner floats).
			float low_ring = surf;
			for (int k = 0; k < 8; ++k) {
				const float a = float(k) * float(Math_TAU) / 8.0f;
				low_ring = Math::min(low_ring, street_surface(ctx, p + Vector3(std::cos(a) * 0.3f, 0.0f, std::sin(a) * 0.3f)));
			}
			if (p.y - low_ring < -0.15f && dy > -1.0f) {
				++buried;
				if (float_ids.size() < 12) {
					float_ids.push_back(String("buried:") + String(pp.id.c_str()) + "@" + String::num(dy, 2) + where(pp));
				}
			}
			// Ship: the strict pre-3c measure as well (3b / 4c: the origin more than 15 cm under the surface at the
			// origin), by settle kind: an upright thing sunk to the lowest ground round its base counts here by design.
			if (dy < -0.15f && dy > -1.0f) {
				const PropSettle k = prop_settle_kind(pp.id);
				++(k == PropSettle::Upright ? strict_up : (k == PropSettle::Car ? strict_car : strict_flat));
				if (strict_ids.size() < 16) {
					strict_ids.push_back(String(pp.id.c_str()) + "@" + String::num(dy, 2) + where(pp));
				}
			}
			if (dy <= 0.05f || dy > 0.45f || prop_settle_kind(pp.id) == PropSettle::Car) {
				continue; // (Stage 3c: a car stands on its wheels, checked as such: a dip under its body is no gap)
			}
			bool under = false;
			for (const OBox &b : ctx.boxes) {
				if (std::abs(b.top() - p.y) > 0.15f) {
					continue;
				}
				const Vector3 l = b.xf.affine_inverse().xform(Vector3(p.x, b.xf.origin.y, p.z));
				if (std::abs(l.x) <= b.half.x + 0.1f && std::abs(l.z) <= b.half.z + 0.1f) {
					under = true;
					break;
				}
			}
			if (!under) {
				++floating;
				float_max = Math::max(float_max, dy);
				if (float_ids.size() < 12) {
					float_ids.push_back(String(pp.id.c_str()) + "@" + String::num(dy, 2) + where(pp));
				}
			}
		}
		st["props_floating"] = floating;
		st["props_floating_max"] = std::round(float_max * 100.0f) / 100.0f;
		st["props_buried"] = buried;
		st["props_floating_ids"] = float_ids;
		st["props_buried_strict_flat"] = strict_flat;
		st["props_buried_strict_upright"] = strict_up;
		st["props_buried_strict_car"] = strict_car;
		st["props_buried_strict_ids"] = strict_ids;
	}
	sweep_stage3c_stats(ctx, st);
	// Candy per lot (after validation), and on the street.
	std::vector<int> per(ctx.lots.size(), 0);
	int street = 0;
	for (const CandySpec &c : ctx.candy) {
		if (c.lot >= 0 && c.lot < int(per.size())) {
			per[c.lot]++;
		} else {
			street++;
		}
	}
	Array per_lot;
	int lo = 1 << 30, hi = 0, sum = 0;
	for (int n : per) {
		per_lot.push_back(n);
		lo = std::min(lo, n);
		hi = std::max(hi, n);
		sum += n;
	}
	st["candy_per_lot"] = per_lot;
	st["candy_lot_min"] = per.empty() ? 0 : lo;
	st["candy_lot_max"] = hi;
	st["candy_lot_mean"] = per.empty() ? 0.0 : std::round(double(sum) / per.size() * 10.0) / 10.0;
	st["candy_street"] = street;
	// Candy value per lot (candy_value: 1 / 5 / 25 / 100 by tier), tier counts and pieces per placement
	// pattern over the lots, and validation's drops / lifts per pattern (Stage 1e).
	{
		static const char *PAT[CPAT_COUNT] = { "other", "path", "drive", "obstacle", "sequence", "crown", "roof", "ring", "summit", "cluster", "legacy", "deck", "bounce" };
		std::vector<int> val(ctx.lots.size(), 0);
		int street_val = 0;
		int tiers[4] = {}, pats[CPAT_COUNT] = {};
		std::vector<std::vector<int>> lot_pats(ctx.lots.size(), std::vector<int>(CPAT_COUNT, 0));
		for (const CandySpec &c : ctx.candy) {
			if (c.lot >= 0 && c.lot < int(val.size())) {
				val[c.lot] += candy_value(c.kind);
				tiers[candy_tier(c.kind) & 3]++;
				pats[c.pattern]++;
				lot_pats[c.lot][c.pattern]++;
			} else {
				street_val += candy_value(c.kind);
			}
		}
		Array value_lot;
		int vlo = 1 << 30, vhi = 0, vsum = 0;
		for (int v : val) {
			value_lot.push_back(v);
			vlo = std::min(vlo, v);
			vhi = std::max(vhi, v);
			vsum += v;
		}
		st["candy_value_per_lot"] = value_lot;
		st["candy_value_lot_min"] = val.empty() ? 0 : vlo;
		st["candy_value_lot_max"] = vhi;
		st["candy_value_lot_mean"] = val.empty() ? 0.0 : std::round(double(vsum) / val.size() * 10.0) / 10.0;
		st["candy_value_street"] = street_val;
		Array tier_n;
		for (int t : tiers) {
			tier_n.push_back(t);
		}
		st["candy_lot_tiers"] = tier_n;
		Dictionary pat_n, pat_drop, pat_raise;
		for (int p = 0; p < CPAT_COUNT; ++p) {
			pat_n[PAT[p]] = pats[p];
			pat_drop[PAT[p]] = ctx.stats.pattern_dropped[p];
			pat_raise[PAT[p]] = ctx.stats.pattern_raised[p];
		}
		st["candy_lot_patterns"] = pat_n;
		st["candy_pattern_dropped"] = pat_drop;
		st["candy_pattern_raised"] = pat_raise;
		// Per lot: pieces by pattern (cluster, drive, obstacle, sequence, crown, roof, ring), for picking a lot to look at.
		// Then: landmark lot (1/0), parked car in the drive (1/0).
		Array lp;
		for (size_t li = 0; li < lot_pats.size(); ++li) {
			const std::vector<int> &v = lot_pats[li];
			Array a;
			for (int p : { CPAT_CLUSTER, CPAT_DRIVE, CPAT_OBSTACLE, CPAT_SEQUENCE, CPAT_CROWN, CPAT_ROOF, CPAT_RING }) {
				a.push_back(v[p]);
			}
			a.push_back(ctx.lots[li].landmark != Landmark::None ? 1 : 0);
			a.push_back(ctx.lots[li].car_in_drive ? 1 : 0);
			lp.push_back(a);
		}
		st["candy_lot_pattern_detail"] = lp;
		// Landmark rings: per landmark, [lots, with a centrepiece, ring placed].
		Dictionary rings;
		for (const Lot &lot : ctx.lots) {
			if (lot.landmark == Landmark::None) {
				continue;
			}
			const String key = landmark_name(lot.landmark);
			Array a;
			if (rings.has(key)) {
				a = rings[key];
			} else {
				a.push_back(0);
				a.push_back(0);
				a.push_back(0);
			}
			a[0] = int(a[0]) + 1;
			a[1] = int(a[1]) + (lot.has_centrepiece ? 1 : 0);
			a[2] = int(a[2]) + (lot_pats[lot.index][CPAT_RING] > 0 ? 1 : 0);
			rings[key] = a;
		}
		st["candy_rings"] = rings;
		// Lots with a parked car in the drive, and how many got the hop onto its hood (a drive line
		// with more than 5 pieces).
		int cars = 0, car_hops = 0;
		for (const Lot &lot : ctx.lots) {
			cars += lot.car_in_drive ? 1 : 0;
			car_hops += (lot.car_in_drive && lot_pats[lot.index][CPAT_DRIVE] > 5) ? 1 : 0;
		}
		Array car_n;
		car_n.push_back(cars);
		car_n.push_back(car_hops);
		st["candy_cars"] = car_n;
	}
	// Nest layout (plan_infestation, the same draw the game makes from this seed).
	if (inf) {
		auto min_gap = [](const std::vector<Vector3> &a, const std::vector<Vector3> &b, bool same) {
			float g = 1e9f;
			for (size_t i = 0; i < a.size(); ++i) {
				for (size_t j = same ? i + 1 : 0; j < b.size(); ++j) {
					g = Math::min(g, a[i].distance_to(b[j]));
				}
			}
			return g > 1e8f ? -1.0 : std::round(double(g) * 10.0) / 10.0;
		};
		auto on_road = [&](const std::vector<Vector3> &pts) {
			int n = 0;
			for (const Vector3 &p : pts) {
				const RoadHit h = net.nearest(p);
				n += (h.road >= 0 && h.beyond <= 0.0f && std::abs(h.lateral) < net.roads[h.road].half_road) ? 1 : 0;
			}
			return n;
		};
		st["nests"] = int(inf->nests.size());
		st["nests_wanted"] = inf->want_nests;
		st["hearts"] = int(inf->hearts.size());
		st["hearts_wanted"] = inf->want_hearts;
		st["nest_gap_min"] = min_gap(inf->nests, inf->nests, true);
		st["heart_gap_min"] = min_gap(inf->hearts, inf->hearts, true);
		st["heart_nest_gap_min"] = min_gap(inf->hearts, inf->nests, false);
		st["nests_on_road"] = on_road(inf->nests);
		st["hearts_on_road"] = on_road(inf->hearts);
		// Stage 4c: how evenly the nests cover the streets: every 5 m along every road's built stretch, the distance
		// to the nearest monster nest (max, mean, and the share of street further than 30 m from any nest).
		{
			float gmax = 0.0f, gsum = 0.0f;
			int n = 0, far = 0;
			for (const Road &R : net.roads) {
				for (float s = R.lots_s0; s <= R.lots_s1; s += 5.0f) {
					const Vector3 p = R.at(s);
					float d = 1e9f;
					for (const Vector3 &q : inf->nests) {
						d = Math::min(d, Vector2(p.x - q.x, p.z - q.z).length());
					}
					if (inf->nests.empty()) {
						continue;
					}
					gmax = Math::max(gmax, d);
					gsum += d;
					far += d > 30.0f ? 1 : 0;
					++n;
				}
			}
			st["nest_street_gap_max"] = std::round(gmax * 10.0f) / 10.0f;
			st["nest_street_gap_mean"] = n ? std::round(gsum / float(n) * 10.0f) / 10.0f : 0.0;
			st["nest_street_far_share"] = n ? std::round(float(far) / float(n) * 1000.0f) / 1000.0f : 0.0;
		}
		// Stage 1d: the planner's own on-road flags (cross-check of the above), the spacing relax steps,
		// the worst pair against its rule (heart-heart 18, heart-nest 12, nest-nest 14; 1 = exactly at
		// the rule), and how the thorn vines grew (tendrils per heart, front doors knotted).
		auto flags = [](const std::vector<char> &f) {
			int n = 0;
			for (char c : f) {
				n += c ? 1 : 0;
			}
			return n;
		};
		st["nests_road_flag"] = flags(inf->nest_on_road);
		st["hearts_road_flag"] = flags(inf->heart_on_road);
		st["road_nest_candidates"] = inf->road_nest_candidates;
		st["road_heart_candidates"] = inf->road_heart_candidates;
		st["relax_hearts"] = inf->relax_hearts;
		st["relax"] = inf->relax;
		st["nest_tries"] = inf->tries;
		{
			struct P {
				Vector3 p;
				bool heart;
			};
			std::vector<P> all;
			for (const Vector3 &p : inf->hearts) {
				all.push_back({ p, true });
			}
			for (const Vector3 &p : inf->nests) {
				all.push_back({ p, false });
			}
			float worst = 1e9f;
			for (size_t i = 0; i < all.size(); ++i) {
				for (size_t j = i + 1; j < all.size(); ++j) {
					const float rule = all[i].heart && all[j].heart ? 18.0f : (all[i].heart != all[j].heart ? 12.0f : 14.0f);
					const Vector3 d = all[i].p - all[j].p;
					worst = Math::min(worst, Vector2(d.x, d.z).length() / rule);
				}
			}
			st["gap_ratio_min"] = worst > 1e8f ? -1.0 : std::round(double(worst) * 1000.0) / 1000.0;
		}
		{
			std::vector<int> per(inf->hearts.size(), 0), knot(inf->hearts.size(), 0);
			for (const auto &t : inf->tendrils) {
				per[t.nest]++;
			}
			for (const auto &k : inf->knots) {
				knot[k.second]++;
			}
			int lo = 1 << 30, off_road = 0, off_road_knotted = 0, road = 0, road_knotted = 0;
			for (size_t i = 0; i < per.size(); ++i) {
				lo = std::min(lo, per[i]);
				const bool r = i < inf->heart_on_road.size() && inf->heart_on_road[i];
				(r ? road : off_road)++;
				(r ? road_knotted : off_road_knotted) += knot[i];
			}
			// Where they stand: road (asphalt), front (yard in front of the building's front wall), back
			// (behind it: backyards, service yards), walk (sidewalk/verge strip), open (squares, parks, off-street).
			auto where = [&](const Vector3 &p, bool road) -> std::string {
				if (road) {
					return "road";
				}
				for (const Lot &lot : ctx.lots) {
					const Vector3 l = lot.to_local(p);
					if (std::abs(l.x) <= lot.width * 0.5f && l.z <= 0.0f && l.z >= -lot.depth) {
						return l.z > lot.house.front_z ? "front" : "back";
					}
				}
				const RoadHit h = net.nearest(p);
				return h.road >= 0 && std::abs(h.lateral) <= net.roads[h.road].edge() + 0.01f ? "walk" : "open";
			};
			Dictionary nw, hw, hk;
			for (size_t i = 0; i < inf->nests.size(); ++i) {
				const String k = where(inf->nests[i], inf->nest_on_road[i]).c_str();
				nw[k] = int(nw.get(k, 0)) + 1;
			}
			for (size_t i = 0; i < inf->hearts.size(); ++i) {
				const String k = where(inf->hearts[i], inf->heart_on_road[i]).c_str();
				hw[k] = int(hw.get(k, 0)) + 1;
				if (knot[i] == 0) {
					hk[k] = int(hk.get(k, 0)) + 1;
				}
			}
			st["nests_where"] = nw;
			st["hearts_where"] = hw;
			st["hearts_unknotted_where"] = hk;
			st["tendrils"] = int(inf->tendrils.size());
			st["tendrils_per_heart_min"] = per.empty() ? -1 : lo;
			st["doors_knotted"] = int(inf->knots.size());
			st["hearts_off_road"] = off_road;
			st["hearts_off_road_knotted"] = off_road_knotted;
			st["hearts_road_knotted"] = road_knotted;
		}
	}
}

void sweep_world_stats(int64_t seed, Dictionary &st) {
	WorldPlan plan;
	plan.reset(uint64_t(seed));
	static const int OPP[4] = { PORT_W, PORT_E, PORT_S, PORT_N };
	static const int DI[4] = { 1, -1, 0, 0 }, DJ[4] = { 0, 0, 1, -1 };
	int hoods = 0, towns = 0, woods = 0, ports = 0, matched = 0, roaded = 0, hood_ns = 0, hood_ns_open = 0;
	int town_lanes = 0, town_lanes_open = 0, rule_bad = 0, clashes = 0;
	int roads_checked = 0, road_lot_pts = 0, road_lot_roads = 0, road_corr_pts = 0, road_corr_roads = 0;
	float reach_max = 0.0f, hood_w = 0.0f, hood_d = 0.0f;
	Array town_spots; // Stage 4c
	const float limit = (WorldPlan::REGION - 2.0f * WorldPlan::JITTER) * 0.5f; // half-extent + blend may reach this far
	for (int i = -2; i <= 2; ++i) {
		for (int j = -2; j <= 2; ++j) {
			const RegionPlan &r = plan.region(i, j);
			if (!r.settlement()) {
				++woods;
				continue;
			}
			const bool town = r.kind == RegionKind::Town;
			(town ? towns : hoods)++;
			const float reach = Math::max(Math::max(r.origin.x - r.x0, r.x1 - r.origin.x), Math::max(r.origin.z - r.z0, r.z1 - r.origin.z));
			reach_max = Math::max(reach_max, reach);
			rule_bad += reach + WorldPlan::BLEND > limit ? 1 : 0;
			if (!town) {
				hood_w = Math::max(hood_w, r.x1 - r.x0);
				hood_d = Math::max(hood_d, r.z1 - r.z0);
			}
			for (int s = 0; s < 4; ++s) {
				if (!r.ports[s].valid) {
					continue;
				}
				++ports;
				const RegionPlan &n = plan.region(i + DI[s], j + DJ[s]);
				const bool ok = n.ports[OPP[s]].valid;
				matched += ok ? 1 : 0;
				roaded += plan.road(i, j, i + DI[s], j + DJ[s]) ? 1 : 0;
				if (s >= PORT_N) {
					(town ? town_lanes : hood_ns)++;
					(town ? town_lanes_open : hood_ns_open) += r.lane_open ? 1 : 0;
				}
			}
			// The country roads leaving this settlement against its own lots and street corridors (the same plan the
			// streamer generates from): points more than 3 m past the port.
			{
				GenContext g;
				g.seed = settlement_seed(r);
				g.kind = town ? SettlementKind::Town : SettlementKind::Neighborhood;
				g.embedded = true;
				g.port_side = r.hood_port;
				plan_street(g);
				if (town) {
					plan_town_lots(g);
					// Stage 4c: the pads too (as WorldPlan::base does), for the review spots below.
					assign_shops(g);
					for (Lot &lot : g.lots) {
						plan_shop(g, lot);
					}
					plan_hood_terrain(g);
					// Review spots (world space) for the capture round: the square, and the biggest step between two
					// touching shop floors, seen from across the street (stand x, z; look at x, z).
					Dictionary tw;
					tw["region"] = String::num_int64(i) + "," + String::num_int64(j);
					tw["template"] = town_template_name(g.town_template);
					tw["size"] = int(g.town_size);
					tw["ports"] = g.town_ports;
					tw["lots"] = int(g.lots.size());
					if (g.square.valid) {
						const Vector3 c = r.origin + g.square.area.to_world(0.0f, 0.0f, -g.square.area.depth * 0.5f);
						tw["square"] = String::num(c.x, 1) + "," + String::num(c.z, 1);
						tw["square_place"] = int(g.square.place);
					}
					float best = 0.0f;
					for (const Lot &lot : g.lots) {
						if (lot.next < 0 || lot_overlap(lot, g.lots[lot.next]) <= -0.6f) {
							continue;
						}
						const Lot &o = g.lots[lot.next];
						const float step = std::abs(lot.xf.origin.y - o.xf.origin.y);
						if (step > best) {
							best = step;
							// The shared side line's front end (lot's +x or -x side, whichever faces the neighbour).
							const float sx = o.to_local(lot.xf.origin).x < 0.0f ? -1.0f : 1.0f; // (in o's frame lot sits at -x or +x)
							const Vector3 at = r.origin + o.to_world(sx * o.width * 0.5f, 0.0f, 0.0f);
							const Vector3 from = r.origin + o.to_world(sx * o.width * 0.5f + sx * 1.5f, 0.0f, g.roads.roads[o.road].edge() * 2.0f - 1.0f);
							tw["facade_look"] = String::num(at.x, 1) + "," + String::num(at.z, 1);
							tw["facade_from"] = String::num(from.x, 1) + "," + String::num(from.z, 1);
							tw["facade_step"] = std::round(step * 100.0f) / 100.0f;
						}
					}
					town_spots.push_back(tw);
				} else {
					plan_lots(g);
				}
				for (int s = 0; s < 4; ++s) {
					const RoadPlan *cr = r.ports[s].valid ? plan.road(i, j, i + DI[s], j + DJ[s]) : nullptr;
					if (!cr) {
						continue;
					}
					bool lot_hit = false, corr_hit = false;
					for (const Vector3 &wp : cr->pts) {
						if (Vector2(wp.x - r.ports[s].pos.x, wp.z - r.ports[s].pos.z).length() < 3.0f) {
							continue;
						}
						const Vector3 p(wp.x - r.origin.x, 0.0f, wp.z - r.origin.z);
						for (const Lot &lot : g.lots) {
							const Vector3 l = lot.to_local(p);
							const float dx = Math::max(0.0f, std::abs(l.x) - lot.width * 0.5f), dz = Math::max(0.0f, Math::max(l.z, -lot.depth - l.z));
							if (Vector2(dx, dz).length() < WorldPlan::ROAD_HALF + 0.6f) {
								++road_lot_pts;
								lot_hit = true;
							}
						}
						if (g.roads.zone(p) != RoadZone::Off) {
							++road_corr_pts;
							corr_hit = true;
						}
					}
					road_lot_roads += lot_hit ? 1 : 0;
					road_corr_roads += corr_hit ? 1 : 0;
					++roads_checked;
				}
			}
			// The blended footprints of the E and N neighbours must not meet.
			for (int s : { int(PORT_E), int(PORT_N) }) {
				const RegionPlan &n = plan.region(i + DI[s], j + DJ[s]);
				if (n.settlement()) {
					const float gap = s == PORT_E ? n.x0 - r.x1 : n.z0 - r.z1;
					clashes += gap < 2.0f * WorldPlan::BLEND ? 1 : 0;
				}
			}
		}
	}
	st["world_hoods"] = hoods;
	st["world_towns"] = towns;
	st["world_woods"] = woods;
	st["world_ports"] = ports;
	st["world_ports_matched"] = matched;
	st["world_ports_roaded"] = roaded;
	st["world_hood_ns_ports"] = hood_ns;
	st["world_hood_ns_open"] = hood_ns_open;
	st["world_town_lanes"] = town_lanes;
	st["world_town_lanes_open"] = town_lanes_open;
	st["world_reach_max"] = std::round(reach_max * 10.0f) / 10.0f;
	st["world_reach_limit"] = limit - WorldPlan::BLEND;
	st["world_rule_violations"] = rule_bad;
	st["world_footprint_clashes"] = clashes;
	st["world_roads_checked"] = roads_checked;
	st["world_road_lot_hits"] = road_lot_pts;
	st["world_road_lot_roads"] = road_lot_roads;
	st["world_road_corridor_hits"] = road_corr_pts;
	st["world_road_corridor_roads"] = road_corr_roads;
	Array hs;
	hs.push_back(std::round(hood_w * 10.0f) / 10.0f);
	hs.push_back(std::round(hood_d * 10.0f) / 10.0f);
	st["world_hood_size_max"] = hs;
	st["world_town_spots"] = town_spots;
}

void sweep_geo_dump(const GenContext &ctx, const InfestationLayout &inf, const std::string &path) {
	std::ofstream out(path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return;
	}
	char buf[160];
	out << "{\"tris\":[";
	bool first = true;
	for (const MeshBuilder *mb : { &ctx.street_mesh, &ctx.boundary_mesh }) {
		const std::vector<Vector3> &v = mb->vertices();
		const std::vector<Vector2> &uv = mb->cell_uvs();
		for (size_t i = 0; i + 2 < v.size(); i += 3) {
			const Vector3 n = (v[i + 1] - v[i]).cross(v[i + 2] - v[i]);
			if (std::abs(n.y) < 0.5f * n.length()) {
				continue; // walls, curb faces: not seen from above
			}
			const float u = uv[i].x - std::floor(uv[i].x);
			const int cell = Math::clamp(int(uv[i].y * pal::GRID), 0, pal::GRID - 1) * pal::GRID + Math::clamp(int(u * pal::GRID), 0, pal::GRID - 1);
			snprintf(buf, sizeof(buf), "%s[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%u,%.3f]", first ? "" : ",", v[i].x, v[i].z, v[i + 1].x, v[i + 1].z, v[i + 2].x, v[i + 2].z,
					pal::RGB[cell], (v[i].y + v[i + 1].y + v[i + 2].y) / 3.0f);
			out << buf;
			first = false;
		}
	}
	out << "],\"lots\":[";
	for (size_t k = 0; k < ctx.lots.size(); ++k) {
		const Lot &l = ctx.lots[k];
		out << (k ? "," : "") << "[";
		int c = 0;
		for (const Vector3 &p : { l.to_world(-l.width * 0.5f, 0, 0), l.to_world(l.width * 0.5f, 0, 0), l.to_world(l.width * 0.5f, 0, -l.depth), l.to_world(-l.width * 0.5f, 0, -l.depth) }) {
			snprintf(buf, sizeof(buf), "%s%.2f,%.2f", c++ ? "," : "", p.x, p.z);
			out << buf;
		}
		snprintf(buf, sizeof(buf), ",%d,%d]", l.bulb ? 1 : 0, int(l.landmark != Landmark::None));
		out << buf;
	}
	out << "],\"props\":[";
	first = true;
	for (const PropPlacement &p : ctx.props) {
		if (p.lot >= 0) {
			continue;
		}
		snprintf(buf, sizeof(buf), "%s[%.2f,%.2f,\"%s\"]", first ? "" : ",", p.xf.origin.x, p.xf.origin.z, p.id.c_str());
		out << buf;
		first = false;
	}
	out << "],\"marks\":[";
	first = true;
	auto mark = [&](const Vector3 &p, const char *what) {
		snprintf(buf, sizeof(buf), "%s[%.2f,%.2f,\"%s\"]", first ? "" : ",", p.x, p.z, what);
		out << buf;
		first = false;
	};
	mark(inf.gate, "gate");
	mark(inf.cart.origin, "cart");
	mark(ctx.spawn, "spawn");
	for (const Vector3 &p : inf.nests) {
		mark(p, "nest");
	}
	for (const Vector3 &p : inf.hearts) {
		mark(p, "heart");
	}
	for (const CandySpec &c : ctx.candy) {
		if (c.lot < 0) {
			mark(c.pos, "candy");
		}
	}
	for (const MagicPath &mp : ctx.magic) {
		if (mp.lot < 0) {
			for (const Vector3 &p : mp.points) {
				mark(p, "spine");
			}
		}
	}
	snprintf(buf, sizeof(buf), "],\"bounds\":[%.2f,%.2f,%.2f,%.2f]}\n", ctx.bound_x0, ctx.bound_z0, ctx.bound_x1, ctx.bound_z1);
	out << buf;
}

void sweep_nav_probe(NeighborhoodGenerator *gen, const InfestationLayout &inf, Dictionary &st) {
	const GenContext &ctx = *gen->context();
	Area area;
	area.plan.x0 = ctx.bound_x0 - 3.0f;
	area.plan.x1 = ctx.bound_x1 + 3.0f;
	area.plan.z0 = ctx.bound_z0 - 4.0f;
	area.plan.z1 = ctx.bound_z1 + 4.0f;
	area.gen = gen;
	Infestation probe;
	const auto t0 = std::chrono::steady_clock::now();
	probe.build_nav_only(area);
	const auto t1 = std::chrono::steady_clock::now();
	st["nav_build_ms"] = double(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	// Stage 3b sanity: the spawn on a walkable cell, and every nest and thorn heart reachable from it.
	{
		const int sl = probe.nav_label(ctx.spawn);
		int cut = 0;
		for (const Vector3 &p : inf.nests) {
			cut += probe.nav_label(p) != sl ? 1 : 0;
		}
		for (const Vector3 &p : inf.hearts) {
			cut += probe.nav_label(p) != sl ? 1 : 0;
		}
		st["nav_spawn_walkable"] = sl != 0 ? 1 : 0;
		st["nav_nests_cut_off"] = cut;
	}
	// The route set: chases (two points 4-16 m apart, inside aggro range) and a monster past its leash
	// (the area + 12 m) walking back to its nest. Only a search between connected cells can be cut short
	// by the cap; the rest have no way through at all.
	struct Pair {
		Vector3 a, b;
		bool home, connected;
	};
	std::vector<Pair> pairs;
	Rng rng(uint64_t(ctx.seed) ^ 0x9A7Eull, 5);
	const RegionPlan &P = area.plan;
	for (int k = 0; k < 200; ++k) {
		const Vector3 p(rng.range(P.x0, P.x1), 0.0f, rng.range(P.z0, P.z1));
		const float a = rng.range(0.0f, Math_TAU), d = rng.range(4.0f, 16.0f);
		pairs.push_back({ p, p + Vector3(std::cos(a), 0.0f, std::sin(a)) * d, false, false });
	}
	for (const Vector3 &nest : inf.nests) {
		for (int k = 0; k < 12; ++k) {
			const float u = rng.f();
			Vector3 p;
			switch (k % 4) {
				case 0: p = Vector3(Math::lerp(P.x0, P.x1, u), 0, P.z0 - 12.0f); break;
				case 1: p = Vector3(Math::lerp(P.x0, P.x1, u), 0, P.z1 + 12.0f); break;
				case 2: p = Vector3(P.x0 - 12.0f, 0, Math::lerp(P.z0, P.z1, u)); break;
				default: p = Vector3(P.x1 + 12.0f, 0, Math::lerp(P.z0, P.z1, u)); break;
			}
			pairs.push_back({ p, nest, true, false });
		}
	}
	int chases = 0, homes = 0;
	for (Pair &q : pairs) {
		q.connected = probe.nav_label(q.a) != 0 && probe.nav_label(q.a) == probe.nav_label(q.b);
		(q.home ? homes : chases)++;
	}
	st["nav_chase_routes"] = chases;
	st["nav_home_routes"] = homes;
	std::vector<Vector3> path;
	const int keep = Infestation::route_cap;
	for (int cap : { 9000, 40000, 100000 }) {
		Infestation::route_cap = cap;
		int cap_chase = 0, cap_home = 0, cap_chase_ok = 0, cap_home_ok = 0, worst_ok = 0;
		const auto r0 = std::chrono::steady_clock::now();
		float worst_ms = 0.0f;
		for (const Pair &q : pairs) {
			const auto q0 = std::chrono::steady_clock::now();
			probe.route(q.a, q.b, path);
			const float ms = float(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - q0).count()) / 1000.0f;
			worst_ms = Math::max(worst_ms, ms);
			const bool capped = probe.last_route_capped();
			(q.home ? cap_home : cap_chase) += capped ? 1 : 0;
			if (q.connected) {
				(q.home ? cap_home_ok : cap_chase_ok) += capped ? 1 : 0;
				worst_ok = Math::max(worst_ok, probe.last_route_expanded());
			}
		}
		const auto r1 = std::chrono::steady_clock::now();
		const std::string k = "nav" + std::to_string(cap / 1000) + "k_";
		st[(k + "chase_capped").c_str()] = cap_chase;
		st[(k + "home_capped").c_str()] = cap_home;
		st[(k + "chase_capped_connected").c_str()] = cap_chase_ok;
		st[(k + "home_capped_connected").c_str()] = cap_home_ok;
		st[(k + "expanded_max_connected").c_str()] = worst_ok;
		st[(k + "route_ms").c_str()] = double(std::chrono::duration_cast<std::chrono::microseconds>(r1 - r0).count()) / 1000.0;
		st[(k + "worst_route_ms").c_str()] = worst_ms;
	}
	Infestation::route_cap = keep;
}

std::string roadnet_selfcheck() {
	// A synthetic net exercising what the legacy street never does: a road that doubles back in x
	// (a U of radius 20 m), sampled by arc length (binary-searched stations), climbing (y != 0), with a
	// curved branch leaving it at a T junction.
	RoadNet net;
	Road a;
	const float R = 20.0f;
	float cum = 0.0f;
	Vector3 prev;
	for (int i = 0; i <= 80; ++i) {
		const float ang = -0.4f + float(i) / 80.0f * (Math_PI + 0.8f); // just over a half turn: x goes out and back
		const Vector3 p(R * std::sin(ang), 0.0f, -R * std::cos(ang));
		if (i > 0) {
			cum += Vector2(p.x - prev.x, p.z - prev.z).length();
		}
		prev = p;
		a.pts.push_back(Vector3(p.x, 0.5f + 0.05f * cum, p.z));
		a.s.push_back(cum);
		const Vector3 t(std::cos(ang), 0, std::sin(ang));
		a.left.push_back(Vector3(-t.z, 0, t.x));
	}
	net.roads.push_back(a);
	Road b;
	const float js = 12.0f;
	const Vector3 o = net.roads[0].at(js);
	const Vector3 bl = net.roads[0].left_at(js);
	cum = 0.0f;
	for (int i = 0; i <= 30; ++i) {
		const float ang = float(i) * 0.02f; // gentle bend
		const Vector3 dir = bl.rotated(Vector3(0, 1, 0), ang);
		const Vector3 p = i == 0 ? o : b.pts.back() + dir * 1.0f;
		if (i > 0) {
			cum += 1.0f;
		}
		b.pts.push_back(Vector3(p.x, o.y, p.z));
		b.s.push_back(cum);
		b.left.push_back(Vector3(-dir.z, 0, dir.x));
	}
	b.parent = 0;
	b.parent_s = js;
	b.parent_side = 1;
	b.start_kind = RoadEnd::Junction;
	b.end_kind = RoadEnd::Bulb;
	net.roads.push_back(b);
	int checks = 0, fails = 0;
	std::string first;
	auto fail = [&](const std::string &what) {
		++fails;
		if (first.empty()) {
			first = what;
		}
	};
	for (int r = 0; r < 2; ++r) {
		const Road &road = net.roads[r];
		for (float st = road.s_begin() + 1.5f; st < road.s_end() - 1.5f; st += 0.7f) {
			for (float lat : { -3.0f, -0.6f, 0.0f, 2.5f }) {
				const Vector3 p = road.point(st, lat);
				const RoadHit h = net.project(r, p);
				++checks;
				if (std::abs(h.s - st) > 0.15f || std::abs(h.lateral - lat) > 0.01f || std::abs(h.y - road.at(st).y) > 0.02f || h.beyond != 0.0f) {
					fail("project road " + std::to_string(r) + " s=" + std::to_string(st) + " lat=" + std::to_string(lat) + " -> s=" + std::to_string(h.s) +
							" lat=" + std::to_string(h.lateral));
				}
			}
		}
		// Past each end: beyond and s_ext carry on in a straight line.
		for (bool end : { false, true }) {
			const Vector3 e = end ? road.pts.back() : road.pts.front();
			const Vector3 out = road.end_dir(end).normalized();
			const RoadHit h = net.project(r, e + out * 2.0f);
			++checks;
			const float want = end ? road.s_end() + 2.0f : road.s_begin() - 2.0f;
			if (std::abs(h.beyond - 2.0f) > 0.05f || std::abs(h.s_ext - want) > 0.05f) {
				fail("beyond road " + std::to_string(r) + (end ? " end" : " start") + " -> beyond=" + std::to_string(h.beyond));
			}
		}
	}
	// nearest(): inside the branch past its mouth -> the branch; elsewhere along the U -> the trunk.
	for (float st = 8.0f; st < 28.0f; st += 2.0f) {
		const RoadHit h = net.nearest(net.roads[1].point(st, 1.0f));
		++checks;
		if (h.road != 1) {
			fail("nearest branch s=" + std::to_string(st) + " -> road " + std::to_string(h.road));
		}
	}
	for (float st = 30.0f; st < net.roads[0].s_end() - 2.0f; st += 3.0f) {
		const RoadHit h = net.nearest(net.roads[0].point(st, -2.0f));
		++checks;
		if (h.road != 0) {
			fail("nearest trunk s=" + std::to_string(st) + " -> road " + std::to_string(h.road));
		}
	}
	return std::to_string(checks) + " checks, " + std::to_string(fails) + " failed" + (first.empty() ? "" : " (first: " + first + ")");
}

} // namespace tot
