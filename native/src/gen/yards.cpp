// Stages 5-6: yard zoning and contextual prop placement. Every prop asks "where would the person
// who lives here put this?": zones, anchors, facing, clustering, co-occurrence, exclusion, budget.
// Randomness only picks among sensible candidates (candidate -> score -> weighted pick of top 3).

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

#include "gen/context.h"

using namespace godot;

namespace tot {

namespace {

struct Rect {
	float x0, x1, z0, z1; // lot-local, z0 < z1
	bool valid() const { return x1 - x0 > 0.2f && z1 - z0 > 0.2f; }
};

enum class Facing { Street, Door, AlongFence, Random, Moon, Away };

const char *JACKS[] = { "jackolantern_happy", "jackolantern_wink", "jackolantern_surprised", "jackolantern_toothy", "jackolantern_sleepy" };
const char *TREES[] = { "tree_maple_red", "tree_maple_orange", "tree_maple_gold" };

// A parked car: the hatchback, a station wagon or a little pickup (whichever are published).
std::string pick_car(GenContext &ctx, Rng &rng) {
	static const char *cars[] = { "parked_car", "parked_wagon", "parked_pickup" };
	const char *id = cars[rng.irange(0, 2)];
	return ctx.assets->get(id)->published ? id : "parked_car";
}

bool is_car(const std::string &id) {
	return id.rfind("parked_", 0) == 0;
}

struct Yard {
	GenContext &ctx;
	Lot &lot;
	Rng &rng;
	float fence, zf, zb, deck, steps_front;
	int budget;
	Vector3 moon_local;
	Vector3 wind_local;

	Yard(GenContext &c, Lot &l, Rng &r) :
			ctx(c), lot(l), rng(r) {
		const HouseSpec &h = l.house;
		fence = l.width * 0.5f - 0.3f;
		zf = h.front_z;
		zb = h.front_z - h.depth;
		deck = h.deck_h;
		steps_front = zf + h.porch_d + 3 * 0.36f;
		const Basis inv = l.xf.basis.inverse();
		moon_local = inv.xform(Vector3(c.moon_dir.x, 0, c.moon_dir.z)).normalized();
		wind_local = inv.xform(Vector3(1, 0, 0)).normalized();
		switch (l.persona) {
			case Persona::Minimalists:
				budget = 9;
				break;
			case Persona::PumpkinFanatics:
			case Persona::CraftyArtists:
			case Persona::SpookyCute:
				budget = 34;
				break;
			default:
				budget = 24;
		}
		if (l.landmark != Landmark::None) {
			budget += 30;
		}
	}

	Rect front_yard() const { return { -fence + 0.6f, fence - 0.6f, zf + 0.4f, -1.1f }; }
	Rect backyard() const { return { -fence + 0.8f, fence - 0.8f, -lot.depth + 1.2f, zb - 0.9f }; }

	float yaw_for(Facing f, float x, float z) {
		switch (f) {
			case Facing::Street:
				return rng.range(-0.25f, 0.25f);
			case Facing::Door:
				return std::atan2(lot.house.door_x - x, (zf + lot.house.porch_d) - z);
			case Facing::AlongFence:
				return (x > 0 ? -1.0f : 1.0f) * Math_PI * 0.5f + rng.range(-0.1f, 0.1f);
			case Facing::Moon:
				return std::atan2(moon_local.x, moon_local.z) + rng.range(-0.2f, 0.2f);
			case Facing::Away:
				return Math_PI + rng.range(-0.3f, 0.3f);
			default:
				return rng.range(0.0f, Math_TAU);
		}
	}

	void put(const std::string &id, Vector3 p, float yaw, float scale = 1.0f, bool collide = true, float lean = 0.0f, float wind = 0.0f, int cost = 1) {
		PropPlacement pp;
		pp.id = id;
		Basis b = Basis(Vector3(0, 1, 0), yaw) * Basis(Vector3(1, 0, 0), lean);
		pp.xf = lot.xf * Transform3D(b.scaled(Vector3(scale, scale, scale)), p);
		pp.lot = lot.index;
		pp.collide = collide;
		pp.wind = wind;
		ctx.props.push_back(pp);
		budget -= cost;
	}

	// Candidate -> score -> weighted pick among the best three.
	bool place(const std::string &id, float radius, Rect zone, Facing facing, const std::function<float(float, float)> &score,
			Vector3 *out = nullptr, float scale = 1.0f, float route_clear = 1.1f, bool collide = true, float wind = 0.0f, int cost = 1) {
		if (budget <= 0 || !zone.valid()) {
			return false;
		}
		struct Cand {
			float s, x, z;
		};
		std::vector<Cand> cands;
		for (int i = 0; i < 18; ++i) {
			const float x = rng.range(zone.x0, zone.x1), z = rng.range(zone.z0, zone.z1);
			if (ctx.occupied_at(lot.index, x, z, radius) || ctx.route_distance(lot.index, x, z) < route_clear + radius) {
				continue;
			}
			cands.push_back({ score(x, z) + rng.range(0.0f, 0.35f), x, z });
		}
		if (cands.empty()) {
			return false;
		}
		std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.s > b.s; });
		const int top = std::min<int>(3, int(cands.size()));
		std::vector<float> w = { 3.0f, 2.0f, 1.0f };
		w.resize(top);
		const Cand &c = cands[rng.weighted(w)];
		ctx.occupy(lot.index, c.x, c.z, radius);
		put(id, Vector3(c.x, 0, c.z), yaw_for(facing, c.x, c.z), scale, collide, rng.range(-0.03f, 0.03f), wind, cost);
		if (out) {
			*out = Vector3(c.x, 0, c.z);
		}
		return true;
	}

	const char *jack_face() {
		// Faces that neither street neighbour already uses.
		int banned = 0;
		for (const Lot &o : ctx.lots) {
			if (o.side == lot.side && std::abs(o.index - lot.index) == 1) {
				banned |= o.faces_used;
			}
		}
		std::vector<int> ok;
		for (int i = 0; i < 5; ++i) {
			if (!(banned & (1 << i))) {
				ok.push_back(i);
			}
		}
		const int f = ok.empty() ? rng.irange(0, 4) : ok[rng.next() % ok.size()];
		lot.faces_used |= 1 << f;
		return JACKS[f];
	}

	// Odd-sized cluster: a hero piece with smaller pieces falling off around it.
	void pumpkin_cluster(Vector3 c, int count, bool lit) {
		const std::string hero = lit ? jack_face() : (rng.chance(0.5f) ? "pumpkin_round" : "pumpkin_tall");
		put(hero, c, yaw_for(Facing::Street, c.x, c.z), rng.range(0.95f, 1.1f), true, rng.range(-0.04f, 0.04f));
		const float base_a = rng.range(0.0f, Math_TAU);
		for (int i = 1; i < count; ++i) {
			const float a = base_a + i * Math_TAU / (count - 1) + rng.range(-0.3f, 0.3f);
			const float r = rng.range(0.5f, 0.8f);
			const Vector3 p = c + Vector3(std::cos(a) * r, 0, std::sin(a) * r);
			const char *id = (i % 3 == 0) ? "gourd_cluster" : (rng.chance(0.6f) ? "pumpkin_small" : "pumpkin_round");
			const float s = (std::string(id) == "pumpkin_round") ? rng.range(0.6f, 0.75f) : rng.range(0.85f, 1.05f);
			put(id, p, rng.range(0.0f, Math_TAU), s, false, rng.range(-0.08f, 0.08f));
		}
	}

	void bale_stack(Vector3 p, float yaw, int high, bool topper) {
		for (int k = 0; k < high; ++k) {
			put("hay_bale", p + Vector3(0, 0.55f * k, 0), yaw + rng.range(-0.08f, 0.08f), 1.0f, true, 0.0f, 0.0f, 1);
		}
		if (topper) {
			put(rng.chance(0.5f) ? jack_face() : "pumpkin_round", p + Vector3(rng.range(-0.2f, 0.2f), 0.55f * high, 0), yaw_for(Facing::Street, p.x, p.z), 0.8f, true);
		}
		ctx.occupy(lot.index, p.x, p.z, 0.7f);
	}

	int pumpkin_level() const {
		switch (lot.persona) {
			case Persona::PumpkinFanatics:
				return 3;
			case Persona::SpookyCute:
			case Persona::CraftyArtists:
			case Persona::Bakers:
				return 2;
			case Persona::Minimalists:
				return 0;
			default:
				return 1;
		}
	}
};

} // namespace

// ---------------------------------------------------------------- per-lot dressing
void dress_yard(GenContext &ctx, Lot &lot) {
	Rng rng = ctx.rng(STAGE_PROPS, lot.index);
	Yard y(ctx, lot, rng);
	const HouseSpec &h = lot.house;
	const float ps = float(lot.passage_side), ds = -ps;
	const float px0 = h.porch_x - h.porch_w * 0.5f, px1 = h.porch_x + h.porch_w * 0.5f;
	const float pz1 = y.zf + h.porch_d;

	// Mailbox where the front path meets the sidewalk, on the side away from the driveway.
	{
		const float side = (h.door_x - lot.drive_x) >= 0 ? 1.0f : -1.0f;
		const float mx = Math::clamp(h.door_x + side * 1.3f, -y.fence + 0.5f, y.fence - 0.5f);
		y.put("mailbox", Vector3(mx, 0, -0.75f), rng.range(-0.08f, 0.08f), 1.0f, true, rng.range(-0.04f, 0.04f));
		ctx.occupy(lot.index, mx, -0.75f, 0.45f);
	}
	// Porch lanterns flanking the door.
	for (float sx : { -1.0f, 1.0f }) {
		y.put("porch_lantern", Vector3(h.door_x + sx * 0.95f, y.deck + 1.75f, y.zf + 0.02f), 0.0f, 1.0f, false);
	}
	// Jack-o'-lanterns climbing the steps, ascending in size, alternating sides.
	const int level = y.pumpkin_level();
	if (level > 0) {
		for (int i = 2; i >= 0; --i) {
			const float top = y.deck - (y.deck / 3.0f) * i;
			const float sz = pz1 + 0.36f * (i + 0.5f);
			const float scale = 0.62f + (2 - i) * 0.08f;
			const bool both = level >= 3;
			const float side = ((i & 1) ? 1.0f : -1.0f) * (rng.chance(0.5f) ? 1.0f : -1.0f);
			for (float sx : { -1.0f, 1.0f }) {
				if (!both && sx != side) {
					continue;
				}
				if (level == 1 && i == 2) {
					continue;
				}
				y.put(y.jack_face(), Vector3(h.door_x + sx * 0.66f, top, sz), rng.range(-0.3f, 0.3f), scale, false, rng.range(-0.04f, 0.04f));
			}
		}
	}
	// Bushes along the foundation where the porch is not.
	if (lot.persona != Persona::Minimalists || rng.chance(0.5f)) {
		const float x0 = h.house_x - h.width * 0.5f, x1 = h.house_x + h.width * 0.5f;
		for (float x = x0 + 0.6f; x < x1 - 0.4f; x += rng.range(1.2f, 1.7f)) {
			if (x > px0 - 0.6f && x < px1 + 0.6f) {
				continue;
			}
			y.put("bush_round", Vector3(x, 0, y.zf + 0.55f), rng.range(0.0f, Math_TAU), rng.range(0.75f, 1.0f), true);
			ctx.occupy(lot.index, x, y.zf + 0.55f, 0.55f);
		}
	}
	// Front tree on the lawn (>= 3 m from the house), leaf pile downwind; one in the backyard.
	Vector3 tree;
	bool has_tree = false;
	auto tree_score = [&](float x, float z) {
		const float d_house = std::abs(z - y.zf);
		return (d_house > 3.0f ? 1.0f : -3.0f) + (std::abs(x - lot.drive_x) > 3.0f ? 0.5f : -2.0f);
	};
	if (lot.persona != Persona::Minimalists || rng.chance(0.5f)) {
		const char *tid = TREES[rng.irange(0, 2)];
		if (y.place(tid, 1.4f, y.front_yard(), Facing::Random, tree_score, &tree, rng.range(0.85f, 1.1f), 1.3f, true, 1.0f, 2)) {
			has_tree = true;
			const Vector3 leaf = tree + y.wind_local * rng.range(1.0f, 1.6f);
			if (!ctx.occupied_at(lot.index, leaf.x, leaf.z, 0.6f) && ctx.route_distance(lot.index, leaf.x, leaf.z) > 1.0f) {
				y.put("leaf_pile", leaf, rng.range(0.0f, Math_TAU), rng.range(0.8f, 1.1f), false);
				ctx.occupy(lot.index, leaf.x, leaf.z, 0.6f);
			}
		}
	}
	Vector3 back_tree;
	const bool has_back_tree = lot.landmark != Landmark::TreeHouse &&
			y.place(TREES[rng.irange(0, 2)], 1.4f, y.backyard(), Facing::Random, [&](float, float z) { return z < y.zb - 3.0f ? 1.0f : 0.0f; }, &back_tree,
					rng.range(0.9f, 1.15f), 1.3f, true, 1.0f, 2);
	(void)has_back_tree;

	// String lights from the porch roof corner to the front tree's branch.
	if (has_tree && rng.chance(lot.persona == Persona::Minimalists ? 0.0f : 0.65f)) {
		const Vector3 branch(0.9f, 2.6f, 0.0f);
		for (const PropPlacement &pp : ctx.props) {
			if (pp.lot == lot.index && pp.id.rfind("tree_", 0) == 0) {
				const AssetInfo *a = ctx.assets->get(pp.id);
				auto it = a->sockets.find("branch");
				const Vector3 local_branch = it != a->sockets.end() ? it->second.origin : branch;
				StringLight s;
				s.lot = lot.index;
				const float corner_x = tree.x > h.porch_x ? px1 + 0.2f : px0 - 0.2f;
				s.a = lot.to_world(corner_x, y.deck + 2.1f, pz1 + 0.3f);
				s.b = pp.xf.xform(local_branch);
				s.sag = rng.range(0.4f, 0.8f);
				ctx.strings.push_back(s);
				break;
			}
		}
	}
	// Paper lanterns hanging along the porch roof edge, or else a scalloped string of mini
	// jack-o'-lanterns and/or bunting along the eave.
	const bool lanterns = lot.persona == Persona::CraftyArtists || lot.persona == Persona::CozyGrandparents || lot.persona == Persona::Stargazers ||
			lot.landmark == Landmark::LanternPorch || rng.chance(0.25f);
	if (!lanterns && lot.persona != Persona::Minimalists) {
		const bool jacks = lot.persona == Persona::SpookyCute || lot.persona == Persona::PumpkinFanatics || lot.persona == Persona::YoungFamily ||
				rng.chance(0.6f);
		const bool bunting = !jacks || lot.persona == Persona::Bakers || lot.persona == Persona::PetLovers || rng.chance(0.35f);
		const int spans = std::max(1, int(std::round((px1 - px0) / 1.7f)));
		const Vector3 out = lot.xf.basis.xform(Vector3(0, 0, 1));
		for (int k = 0; k < spans; ++k) {
			const float xa = Math::lerp(px0 + 0.1f, px1 - 0.1f, float(k) / spans), xb = Math::lerp(px0 + 0.1f, px1 - 0.1f, float(k + 1) / spans);
			if (jacks) {
				StringLight s;
				s.lot = lot.index;
				s.style = STRING_JACKS;
				s.a = lot.to_world(xa, y.deck + 2.06f, pz1 + 0.16f);
				s.b = lot.to_world(xb, y.deck + 2.06f, pz1 + 0.16f);
				s.sag = 0.16f;
				s.out = out;
				ctx.strings.push_back(s);
			}
			if (bunting) {
				StringLight s;
				s.lot = lot.index;
				s.style = STRING_BUNTING;
				s.a = lot.to_world(xa, y.deck + 2.1f, pz1 + 0.24f);
				s.b = lot.to_world(xb, y.deck + 2.1f, pz1 + 0.24f);
				s.sag = jacks ? 0.28f : 0.2f;
				s.out = out;
				ctx.strings.push_back(s);
			}
		}
	}
	if (lanterns) {
		const int n = lot.landmark == Landmark::LanternPorch ? 7 : rng.irange(3, 5);
		for (int i = 0; i < n; ++i) {
			const float x = Math::lerp(px0 + 0.3f, px1 - 0.3f, n == 1 ? 0.5f : float(i) / (n - 1));
			y.put("paper_lantern", Vector3(x, y.deck + 2.02f, pz1 + 0.15f), rng.range(0.0f, Math_TAU), rng.range(0.85f, 1.1f), false);
		}
	}
	// Front-lawn pumpkin clusters near the porch apron, never on the path.
	const int clusters = std::max(0, level + (lot.persona == Persona::PumpkinFanatics ? 1 : 0) - (rng.chance(0.4f) ? 1 : 0));
	for (int c = 0; c < clusters; ++c) {
		Vector3 at;
		const int count = (c == 0 && level >= 2) ? 5 : 3;
		auto score = [&](float x, float z) { return -std::abs(z - (pz1 + 1.8f)) * 0.5f - std::abs(x - h.door_x) * 0.05f; };
		if (y.place("pumpkin_round", 1.0f, y.front_yard(), Facing::Street, score, &at, 1.0f, 1.0f, true, 0.0f, 0)) {
			ctx.props.pop_back();
			y.pumpkin_cluster(at, count, rng.chance(0.5f));
		}
	}
	// Hay bales against a side fence with a pumpkin on top (farm-ish personas).
	if (lot.persona == Persona::GreenThumbs || lot.persona == Persona::Bakers || lot.persona == Persona::PumpkinFanatics ||
			lot.persona == Persona::YoungFamily || lot.persona == Persona::Tinkerers || rng.chance(0.25f)) {
		const float fx = (rng.chance(0.5f) ? 1.0f : -1.0f) * (y.fence - 0.65f);
		for (int tries = 0; tries < 6; ++tries) {
			const float z = rng.range(y.zf + 1.5f, -1.8f);
			if (!ctx.occupied_at(lot.index, fx, z, 0.7f) && ctx.route_distance(lot.index, fx, z) > 1.6f) {
				y.bale_stack(Vector3(fx, 0, z), Math_PI * 0.5f, rng.chance(0.5f) ? 2 : 1, true);
				break;
			}
		}
	}
	// Garage roof helper: a bale stack by the garage door so the roof (and its lollipop) is a route.
	if (h.garage) {
		const float g = float(h.garage_side);
		const float gx = h.house_x + g * (h.width * 0.5f + 3.8f * 0.5f);
		const Vector3 p(gx + g * 1.25f, 0, y.zf - 0.4f + 0.75f);
		y.bale_stack(p, 0.0f, 2, false);
		y.put("hay_bale", p + Vector3(-g * 0.9f, 0, 0.35f), 0.2f * g, 1.0f, true);
	}
	// Flower pots at the foot of the steps.
	if (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs || lot.persona == Persona::CraftyArtists ||
			lot.persona == Persona::PetLovers) {
		for (float sx : { -1.0f, 1.0f }) {
			y.put("flower_pot", Vector3(h.door_x + sx * 1.15f, 0, y.steps_front + 0.35f), rng.range(0.0f, Math_TAU), rng.range(0.85f, 1.0f), true);
			ctx.occupy(lot.index, h.door_x + sx * 1.15f, y.steps_front + 0.35f, 0.35f);
		}
	}
	// Autumn mums: a bed along the front of the porch (two colours alternating, never in front of
	// the steps) and, where there are no flower pots, a pair flanking the foot of the steps.
	static const char *MUMS[] = { "mums_orange", "mums_purple", "mums_white" };
	const int mum_a = rng.irange(0, 2), mum_b = (mum_a + rng.irange(1, 2)) % 3;
	if (lot.persona != Persona::Minimalists) {
		int k = 0;
		for (float x = px0 + 0.35f; x < px1 - 0.25f; x += rng.range(0.55f, 0.7f)) {
			if (std::abs(x - h.door_x) < 1.95f) { // clear of the wraparound steps
				continue;
			}
			const float z = pz1 + rng.range(0.3f, 0.42f);
			y.put(MUMS[(k++ & 1) ? mum_b : mum_a], Vector3(x, 0, z), rng.range(0.0f, Math_TAU), rng.range(0.85f, 1.15f), false);
			ctx.occupy(lot.index, x, z, 0.3f);
		}
		const bool pots = lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs || lot.persona == Persona::CraftyArtists ||
				lot.persona == Persona::PetLovers;
		if (!pots) {
			for (float sx : { -1.0f, 1.0f }) {
				const Vector3 p(h.door_x + sx * 1.15f, 0, y.steps_front + 0.3f);
				y.put(MUMS[mum_a], p, rng.range(0.0f, Math_TAU), rng.range(1.0f, 1.2f), false);
				ctx.occupy(lot.index, p.x, p.z, 0.35f);
			}
		}
	}
	// Brick gateposts where the front walk meets the sidewalk, sometimes with a black cat on one.
	if (lot.persona != Persona::Minimalists && (lot.persona == Persona::CozyGrandparents || lot.persona == Persona::GreenThumbs ||
			lot.persona == Persona::Bakers || lot.persona == Persona::SpookyCute || rng.chance(0.4f))) {
		const float cat_side = rng.chance(0.5f) ? 1.0f : -1.0f;
		const bool cat = lot.persona == Persona::SpookyCute || lot.persona == Persona::PetLovers || rng.chance(0.35f);
		for (float sx : { -1.0f, 1.0f }) {
			const Vector3 p(h.door_x + sx * 1.05f, 0, -0.5f);
			if (std::abs(p.x - lot.drive_x) < 2.0f || std::abs(p.x) > y.fence - 0.3f) {
				continue;
			}
			y.put("brick_pillar", p, 0.0f, 1.0f, true);
			ctx.occupy(lot.index, p.x, p.z, 0.4f);
			if (cat && sx == cat_side) {
				const AssetInfo *a = ctx.assets->get("brick_pillar");
				auto it = a->sockets.find("top");
				const float top = it != a->sockets.end() ? it->second.origin.y : 1.1f;
				y.put("black_cat_sitting", p + Vector3(0, top, 0), rng.range(-0.5f, 0.5f), 1.0f, false);
			}
		}
	}
	// A raised brick planter of mums and little pumpkins out on the lawn (it doubles as a step up).
	if (lot.persona == Persona::GreenThumbs || lot.persona == Persona::CozyGrandparents || lot.persona == Persona::Bakers ||
			lot.persona == Persona::CraftyArtists || lot.persona == Persona::PumpkinFanatics || rng.chance(0.35f)) {
		y.place("brick_planter", 1.25f, y.front_yard(), Facing::Street,
				[&](float x, float z) { return -std::abs(z + 1.6f) * 0.4f + std::abs(x - h.door_x) * 0.1f; }, nullptr, 1.0f, 1.2f, true, 0.0f, 2);
	}
	// A friendly sheet ghost keeping watch on the porch.
	if (lot.persona == Persona::SpookyCute || lot.persona == Persona::YoungFamily || rng.chance(0.3f)) {
		const float gx = h.porch_x - ps * (h.porch_w * 0.5f - 0.55f);
		if (std::abs(gx - h.door_x) > 1.0f) {
			y.put("ghost_figure", Vector3(gx, y.deck, y.zf + 0.6f), rng.range(-0.35f, 0.35f), 1.0f, false);
		}
	}
	// Corn stalks tied to the porch posts.
	if (lot.persona == Persona::PumpkinFanatics || lot.persona == Persona::GreenThumbs || lot.persona == Persona::Bakers) {
		for (float x : { px0 + 0.15f, px1 - 0.15f }) {
			y.put("corn_stalks", Vector3(x, y.deck, pz1 - 0.05f), rng.range(-0.4f, 0.4f), rng.range(0.75f, 0.9f), false, 0.0f, 0.6f);
		}
	}
	// Sunflowers line the fence whose inner side faces the moon.
	if (lot.persona == Persona::GreenThumbs || lot.landmark == Landmark::Sunflower || rng.chance(0.15f)) {
		const float fs = y.moon_local.x < 0 ? 1.0f : -1.0f; // fence at +x has inner normal -x
		const int n = lot.landmark == Landmark::Sunflower ? 7 : rng.irange(3, 5);
		for (int i = 0; i < n; ++i) {
			const float z = Math::lerp(-1.6f, y.zb, float(i) / std::max(1, n - 1));
			const float x = fs * (y.fence - 0.5f);
			if (ctx.occupied_at(lot.index, x, z, 0.35f) || ctx.route_distance(lot.index, x, z) < 1.0f) {
				continue;
			}
			y.put("sunflower", Vector3(x, 0, z), y.yaw_for(Facing::Moon, x, z), rng.range(0.85f, 1.1f), true, rng.range(-0.05f, 0.05f), 0.7f);
			ctx.occupy(lot.index, x, z, 0.35f);
		}
	}

	// Persona signatures.
	auto front = y.front_yard();
	auto back = y.backyard();
	auto any = [](float, float) { return 0.0f; };
	auto near_street = [&](float, float z) { return z * 0.1f; };
	switch (lot.persona) {
		case Persona::YoungFamily:
			y.place("swing_set", 1.7f, back, Facing::Street, any, nullptr, 1.0f, 1.3f, true, 0.0f, 3);
			if (rng.chance(0.6f)) {
				y.place("inflatable_cat", 0.8f, front, Facing::Street, near_street);
			}
			break;
		case Persona::CozyGrandparents:
			y.put("rocking_chair", Vector3(h.porch_x + ps * (h.porch_w * 0.5f - 0.7f), y.deck, y.zf + 0.75f), rng.range(-0.4f, 0.4f), 1.0f, false);
			y.place("bench", 1.0f, front, Facing::Street, near_street);
			break;
		case Persona::Minimalists:
			y.place(y.jack_face(), 1.3f, front, Facing::Street, [&](float x, float) { return -std::abs(x) * 0.2f; }, nullptr, 2.2f, 1.2f);
			break;
		case Persona::SpookyCute:
			y.place("inflatable_ghost", 0.9f, front, Facing::Street, near_street);
			y.place("cauldron", 0.7f, front, Facing::Street, any);
			break;
		case Persona::Bakers:
			y.place("crate", 0.55f, front, Facing::Street, any);
			y.place("barrel", 0.45f, front, Facing::Random, any);
			y.place("wheelbarrow", 0.8f, front, Facing::Random, any);
			break;
		case Persona::Sporty:
			y.place("bench", 1.0f, back, Facing::Street, any);
			y.place("crate", 0.55f, back, Facing::Random, any);
			break;
		case Persona::PetLovers:
			y.place("bench", 1.0f, front, Facing::Door, any);
			break;
		case Persona::Stargazers:
			y.place("bench", 1.0f, back, Facing::Moon, any);
			break;
		case Persona::Tinkerers:
			y.place("wheelbarrow", 0.8f, back, Facing::Random, any);
			y.place("barrel", 0.45f, back, Facing::Random, any);
			y.place("cauldron", 0.7f, back, Facing::Random, any);
			break;
		case Persona::CraftyArtists:
			y.place("flower_pot", 0.4f, front, Facing::Random, any);
			y.place("flower_pot", 0.4f, front, Facing::Random, any);
			break;
		default:
			break;
	}
	// A few backyard extras so the loop's back half has things to look at.
	const char *extras[] = { "leaf_pile", "barrel", "crate", "pumpkin_round", "gourd_cluster", "flower_pot" };
	const int n_extra = lot.persona == Persona::Minimalists ? 0 : rng.irange(1, 3);
	for (int i = 0; i < n_extra; ++i) {
		const char *id = extras[rng.irange(0, 5)];
		y.place(id, 0.6f, back, Facing::Random, any, nullptr, 1.0f, 1.1f, std::string(id) != "leaf_pile");
	}
	// Parked car in an open driveway.
	if (!h.garage && rng.chance(0.4f) && lot.landmark != Landmark::HayRide) {
		y.put(pick_car(ctx, rng), Vector3(lot.drive_x, 0, y.zf + 0.2f + 2.0f), rng.range(-0.05f, 0.05f), 1.0f, true);
	}

	// Trampolines: now and then a big one in the backyard, and little ones parked where a kid would
	// want to get up onto something (a porch or garage roof just out of jumping reach).
	{
		const float big = (lot.persona == Persona::YoungFamily || lot.persona == Persona::Sporty) ? 0.7f : (lot.persona == Persona::Minimalists ? 0.0f : 0.3f);
		if (lot.landmark != Landmark::TreeHouse && rng.chance(big)) {
			y.budget = std::max(y.budget, 3);
			y.place("trampoline_large", 2.1f, back, Facing::Random, [&](float, float z) { return -z * 0.05f; }, nullptr, 1.0f, 1.2f, true, 0.0f, 3);
		}
		if (rng.chance(lot.persona == Persona::Minimalists ? 0.25f : 0.7f)) {
			const Transform3D inv = lot.xf.affine_inverse();
			Vector3 best;
			float best_score = -1e9f;
			for (const OBox &b : ctx.boxes) {
				if (b.lot != lot.index || !b.walkable || b.top() < 2.2f || b.top() > 5.2f) {
					continue;
				}
				const Transform3D lb = inv * b.xf;
				for (int axis : { 0, 2 }) {
					for (float sgn : { -1.0f, 1.0f }) {
						Vector3 nrm = lb.basis.get_column(axis).normalized() * sgn;
						nrm.y = 0.0f;
						if (nrm.length() < 0.9f) {
							continue;
						}
						nrm = nrm.normalized();
						const Vector3 side = Vector3(0, 1, 0).cross(nrm);
						const float along = b.half[axis == 0 ? 2 : 0];
						for (int c = 0; c < 6; ++c) {
							const float slide = (c % 3 - 1) * 0.5f, out = c < 3 ? 1.3f : 1.8f;
							Vector3 p = lb.origin + nrm * (b.half[axis] + out) + side * (slide * along);
							p.y = 0.0f;
							if (std::abs(p.x) > y.fence - 0.7f || p.z > -0.9f || p.z < -lot.depth + 0.9f || ctx.occupied_at(lot.index, p.x, p.z, 0.85f) ||
									ctx.route_distance(lot.index, p.x, p.z) < 0.8f) {
								continue;
							}
							bool clear = true;
							for (const OBox &o : ctx.boxes) {
								if (o.lot != lot.index || o.xf.origin.y - o.half.y > 0.5f) {
									continue;
								}
								const Vector3 l = (inv * o.xf).affine_inverse().xform(p);
								if (std::abs(l.x) < o.half.x + 0.65f && std::abs(l.z) < o.half.z + 0.65f) {
									clear = false;
									break;
								}
							}
							const float score = p.z * 0.15f + rng.range(0.0f, 0.5f); // street side first
							if (clear && score > best_score) {
								best_score = score;
								best = p;
							}
						}
					}
				}
			}
			if (best_score > -1e8f) {
				y.budget = std::max(y.budget, 1);
				y.put("trampoline_mini", best, rng.range(0.0f, Math_TAU), 1.25f, true);
				ctx.occupy(lot.index, best.x, best.z, 0.85f);
			}
		}
	}

	place_landmark(ctx, lot);
}

// ---------------------------------------------------------------- landmarks
void place_landmark(GenContext &ctx, Lot &lot) {
	if (lot.landmark == Landmark::None) {
		return;
	}
	Rng rng = ctx.rng(STAGE_PROPS, lot.index, 7);
	Yard y(ctx, lot, rng);
	y.budget = 1000;
	const HouseSpec &h = lot.house;
	const float ds = -float(lot.passage_side);
	const float px0 = h.porch_x - h.porch_w * 0.5f, px1 = h.porch_x + h.porch_w * 0.5f;
	const float pz1 = y.zf + h.porch_d;
	MeshBuilder &m = ctx.lot_mesh[lot.index];
	auto any = [](float, float) { return 0.0f; };
	const Rect front = y.front_yard();
	switch (lot.landmark) {
		case Landmark::PumpkinPatch: {
			// A whole front yard of pumpkins in loose rows on vines; some are bouncy.
			for (float z = -1.8f; z > y.zf + 1.0f; z -= 1.25f) {
				for (float x = -y.fence + 1.0f; x < y.fence - 0.8f; x += 1.3f) {
					const float jx = x + rng.range(-0.25f, 0.25f), jz = z + rng.range(-0.2f, 0.2f);
					if (ctx.occupied_at(lot.index, jx, jz, 0.45f) || ctx.route_distance(lot.index, jx, jz) < 1.0f) {
						continue;
					}
					if (rng.chance(0.3f)) {
						y.put("vine_patch", Vector3(jx, 0, jz), rng.range(0.0f, Math_TAU), rng.range(0.8f, 1.1f), false);
					}
					const char *id = rng.chance(0.15f) ? y.jack_face() : (rng.chance(0.5f) ? "pumpkin_round" : (rng.chance(0.5f) ? "pumpkin_tall" : "pumpkin_small"));
					y.put(id, Vector3(jx, 0, jz), rng.range(0.0f, Math_TAU), rng.range(0.85f, 1.2f), true, rng.range(-0.06f, 0.06f));
					if (rng.chance(0.25f)) {
						ctx.props.back().bouncy = true;
					}
					ctx.occupy(lot.index, jx, jz, 0.45f);
				}
			}
			y.place("giant_pumpkin", 1.2f, front, Facing::Street, [&](float x, float z) { return -std::abs(z - (pz1 + 2.5f)) - std::abs(x - h.door_x) * 0.1f; });
			y.place("wheelbarrow", 0.8f, front, Facing::Random, any);
			// Vine ramp: a twisting green ramp from the lawn up to the porch roof (walkable, about 33 degrees).
			{
				const float roof = h.deck_h + 2.3f;
				const float len = roof / std::tan(Math::deg_to_rad(33.0f));
				const float x_end = ds > 0 ? px1 + 0.1f : px0 - 0.1f;
				const float x_start = x_end + ds * len;
				const float z = y.zf + h.porch_d * 0.5f;
				const Vector3 a(x_start, 0.0f, z), b(x_end, roof, z);
				const Vector3 d = b - a;
				const float L = d.length();
				const float ang = std::atan2(roof, len);
				// Low end on the lawn, high end at the porch roof: with ds > 0 it climbs toward -x.
				const Basis tilt = Basis(Vector3(0, 0, 1), ds > 0 ? -ang : ang);
				const Vector3 c = (a + b) * 0.5f + Vector3(0, -0.12f, 0);
				m.set_transform(Transform3D(tilt, c));
				m.rounded_box(Vector3(), Vector3(L * 0.5f, 0.15f, 0.7f), 0.08f, mat_cell(pal::VINE_DARK, mat::BARK), mat_cell(pal::VINE, mat::HEDGE));
				for (float s = -L * 0.5f; s < L * 0.5f; s += 0.7f) {
					m.blob(Vector3(s, 0.18f, (int(s * 3) & 1) ? 0.55f : -0.55f), Vector3(0.32f, 0.1f, 0.28f), mat_cell(pal::VINE, mat::LEAVES), 0);
				}
				m.set_transform(Transform3D());
				ctx.add_box(lot.index, c, Vector3(L * 0.5f, 0.15f, 0.7f), true, 0.0f, tilt);
				lot.summit_route.insert(lot.summit_route.begin(), Vector3((x_start + x_end) * 0.5f, roof * 0.5f + 0.1f, z));
				ctx.occupy(lot.index, (x_start + x_end) * 0.5f, z, 1.0f);
			}
			break;
		}
		case Landmark::Scarecrow: {
			Vector3 sc;
			if (y.place("scarecrow", 0.9f, front, Facing::Street, [&](float x, float z) { return -std::abs(z - (pz1 + 3.0f)) * 0.3f; }, &sc)) {
				y.put("hay_bale", sc + Vector3(0.9f, 0, 0.6f), 0.4f, 1.0f, true);
				y.put("corn_stalks", sc + Vector3(-0.8f, 0, -0.3f), 0.0f, 1.0f, true, 0.0f, 0.6f);
				y.put(y.jack_face(), sc + Vector3(0.9f, 0.55f, 0.6f), 0.0f, 0.75f, true);
			}
			// Pumpkin-stack stairs up to the porch roof: 1, 2, 3 bales high with pumpkins on top.
			const float x_end = ds > 0 ? px1 + 0.7f : px0 - 0.7f;
			for (int k = 0; k < 3; ++k) {
				const Vector3 p(x_end + ds * (2 - k) * 1.05f, 0, y.zf + h.porch_d * 0.5f);
				y.bale_stack(p, Math_PI * 0.5f, k + 1, k == 0);
			}
			// Corn-stalk mini maze in the backyard.
			const Vector3 mc(rng.range(-2.0f, 2.0f), 0, (y.zb + (-lot.depth)) * 0.5f);
			for (int i = 0; i < 14; ++i) {
				const float a = i * Math_TAU / 14.0f;
				if (i == 3 || i == 10) {
					continue; // entrances
				}
				y.put("corn_stalks", mc + Vector3(std::cos(a) * 3.0f, 0, std::sin(a) * 2.4f), a, rng.range(0.9f, 1.1f), true, 0.0f, 0.6f);
			}
			ctx.occupy(lot.index, mc.x, mc.z, 3.2f);
			break;
		}
		case Landmark::Sunflower: {
			// Sunflower heads near the porch are swaying platforms up to the porch roof.
			const float x0 = ds > 0 ? px1 + 0.9f : px0 - 0.9f;
			for (int k = 0; k < 3; ++k) {
				const Vector3 p(x0 + ds * k * 1.2f, 0, y.zf + h.porch_d * 0.5f + (k & 1) * 0.6f);
				const float s = 0.75f + 0.12f * (2 - k);
				y.put("sunflower", p, std::atan2(y.moon_local.x, y.moon_local.z), s, true, 0.0f, 0.4f);
				const AssetInfo *a = ctx.assets->get("sunflower");
				const float head = (a->aabb.position.y + a->aabb.size.y) * s - 0.25f;
				ctx.add_box(lot.index, p + Vector3(0, head, 0), Vector3(0.45f, 0.1f, 0.45f), true);
				ctx.occupy(lot.index, p.x, p.z, 0.5f);
				lot.summit_route.insert(lot.summit_route.begin(), p + Vector3(0, head + 0.1f, 0));
			}
			std::reverse(lot.summit_route.begin(), lot.summit_route.begin() + 3);
			for (int i = 0; i < 6; ++i) {
				y.place("flower_pot", 0.4f, front, Facing::Random, any);
			}
			break;
		}
		case Landmark::TreeHouse: {
			// A giant maple in the backyard with platforms spiralling up to a little hut.
			const Vector3 t(rng.range(-1.5f, 1.5f), 0, (y.zb + (-lot.depth)) * 0.5f - 0.3f);
			m.cylinder(t, 0.65f, 7.5f, 9, mat_cell(pal::BARK, mat::BARK), 0.45f);
			for (int i = 0; i < 6; ++i) {
				const float a = i * Math_TAU / 6.0f;
				m.blob(t + Vector3(std::cos(a) * 2.2f, 7.6f + (i & 1) * 0.8f, std::sin(a) * 2.2f), Vector3(2.0f, 1.5f, 2.0f),
						mat_cell((i % 3 == 0) ? pal::MAPLE_ORANGE : pal::MAPLE_RED, mat::LEAVES), 1, 0.15f, uint32_t(rng.next()));
			}
			m.blob(t + Vector3(0, 9.0f, 0), Vector3(2.4f, 1.6f, 2.4f), mat_cell(pal::GOLDEN_LEAF, mat::LEAVES), 1, 0.15f, 5);
			ctx.add_box(lot.index, t + Vector3(0, 3.75f, 0), Vector3(0.6f, 3.75f, 0.6f), false);
			lot.summit_route.clear();
			const float a0 = rng.range(0.0f, Math_TAU);
			for (int k = 0; k < 5; ++k) {
				const float a = a0 + k * Math_PI * 0.5f;
				const float yy = 1.35f + k * 1.3f;
				const Vector3 p = t + Vector3(std::cos(a) * 1.9f, yy, std::sin(a) * 1.9f);
				const Basis rot(Vector3(0, 1, 0), -a);
				m.set_transform(Transform3D(rot, p));
				m.rounded_box(Vector3(), Vector3(0.85f, 0.1f, 0.75f), 0.04f, mat_cell(pal::PLANK, mat::PLANKS), mat_cell(pal::WOOD_LIGHT, mat::PLANKS));
				m.box(Vector3(0, -0.5f, 0), Vector3(0.06f, 0.45f, 0.06f), mat_cell(pal::WOOD_DARK, mat::WOOD));
				m.set_transform(Transform3D());
				ctx.add_box(lot.index, p, Vector3(0.85f, 0.1f, 0.75f), true, -a);
				lot.summit_route.push_back(p + Vector3(0, 0.1f, 0));
			}
			// Hut on the top platform.
			const Vector3 hut = t + Vector3(0, 7.4f, 0);
			m.rounded_box(hut + Vector3(0, -0.1f, 0), Vector3(1.6f, 0.12f, 1.6f), 0.05f, mat_cell(pal::PLANK, mat::PLANKS));
			m.box(hut + Vector3(0, 0.7f, -0.6f), Vector3(1.2f, 0.7f, 0.6f), mat_cell(pal::WOOD_LIGHT, mat::PLANKS));
			m.box(hut + Vector3(0, 0.75f, -0.0f), Vector3(0.3f, 0.35f, 0.02f), pal::GLOW_WINDOW);
			m.set_transform(Transform3D(Basis(), hut + Vector3(0, 1.4f, -0.6f)));
			m.prism_xy({ { -1.35f, 0 }, { 1.35f, 0 }, { 0, 0.9f } }, 0.0f, 1.5f, mat_cell(pal::ROOF_RED, mat::SHINGLES));
			m.set_transform(Transform3D());
			ctx.add_box(lot.index, hut + Vector3(0, -0.1f, 0), Vector3(1.6f, 0.12f, 1.6f), true);
			ctx.add_box(lot.index, hut + Vector3(0, 0.7f, -0.6f), Vector3(1.2f, 0.7f, 0.6f), true);
			lot.summit_route.push_back(hut + Vector3(0, 0.05f, 0.9f));
			lot.summit_route.push_back(hut + Vector3(0, 1.45f, -0.6f));
			ctx.occupy(lot.index, t.x, t.z, 2.8f);
			break;
		}
		case Landmark::HayRide: {
			// Hay wagon on the lawn and a bale pyramid up to the porch (or garage) roof.
			y.place("hay_wagon", 1.6f, front, Facing::Street, [&](float x, float) { return -std::abs(x - lot.drive_x) * 0.1f; });
			const float x_end = ds > 0 ? px1 + 0.7f : px0 - 0.7f;
			const float z = y.zf + h.porch_d * 0.5f;
			lot.summit_route.clear();
			for (int k = 0; k < 3; ++k) {
				const Vector3 p(x_end + ds * (2 - k) * 1.05f, 0, z);
				y.bale_stack(p, Math_PI * 0.5f, k + 1, false);
				lot.summit_route.push_back(p + Vector3(0, 0.55f * (k + 1), 0));
			}
			lot.summit_route.push_back(Vector3(h.porch_x, h.deck_h + 2.3f, y.zf + h.porch_d * 0.5f));
			lot.summit_route.push_back(Vector3(h.porch_x - ds * (h.porch_w * 0.5f - 0.6f), h.deck_h + 2.3f, y.zf + 0.5f));
			break;
		}
		case Landmark::Cauldron: {
			y.place("cauldron", 0.8f, front, Facing::Street, [&](float x, float z) { return -std::abs(x - h.door_x) * 0.1f - std::abs(z - (pz1 + 2.5f)); }, nullptr, 1.5f);
			for (int i = 0; i < 3; ++i) {
				Vector3 at;
				if (y.place("pumpkin_round", 0.9f, front, Facing::Street, any, &at, 1.0f, 1.0f, true, 0.0f, 0)) {
					ctx.props.pop_back();
					y.pumpkin_cluster(at, 3, true);
				}
			}
			break;
		}
		case Landmark::Inflatables: {
			const char *ids[] = { "inflatable_ghost", "inflatable_cat", "inflatable_ghost", "inflatable_cat", "inflatable_ghost" };
			for (int i = 0; i < 5; ++i) {
				if (y.place(ids[i], 0.9f, front, Facing::Street, any, nullptr, rng.range(0.85f, 1.15f))) {
					ctx.props.back().bouncy = true;
				}
			}
			break;
		}
		case Landmark::LanternPorch:
			for (int i = 0; i < 3; ++i) {
				y.place("bench", 1.0f, front, Facing::Door, any);
			}
			break;
		case Landmark::CarvingContest: {
			// Long tables of carved pumpkins and a judges' podium.
			for (int t = 0; t < 2; ++t) {
				const float z = pz1 + 2.2f + t * 2.0f;
				const float x0 = h.door_x + (t == 0 ? 1.4f : -4.6f);
				if (z > -1.5f) {
					continue;
				}
				const Vector3 c(x0 + 1.6f, 0, z);
				if (ctx.route_distance(lot.index, c.x, c.z) < 1.2f) {
					continue;
				}
				m.rounded_box(c + Vector3(0, 0.76f, 0), Vector3(1.6f, 0.05f, 0.5f), 0.02f, mat_cell(pal::FLANNEL_RED, mat::FABRIC), mat_cell(pal::CANDY_CORN_WHITE, mat::FABRIC));
				for (float lx : { -1.4f, 1.4f }) {
					m.box(c + Vector3(lx, 0.36f, 0), Vector3(0.06f, 0.36f, 0.4f), mat_cell(pal::WOOD, mat::WOOD));
				}
				ctx.add_box(lot.index, c + Vector3(0, 0.4f, 0), Vector3(1.6f, 0.41f, 0.5f), true);
				for (int k = 0; k < 4; ++k) {
					y.put(JACKS[(k + t) % 5], c + Vector3(-1.2f + k * 0.8f, 0.81f, 0), rng.range(-0.3f, 0.3f), 0.7f, false);
				}
				ctx.occupy(lot.index, c.x, c.z, 1.7f);
			}
			break;
		}
		default:
			break;
	}
}

// ---------------------------------------------------------------- pocket parks
// Frontage the lots couldn't use (beside the side lane, at the row ends) becomes a little
// festival corner, so the street never shows an empty lawn.
void place_pocket_parks(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_RHYTHM, 31);
	const StreetPlan &s = ctx.street;
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
			const float hw = w * 0.5f - 1.0f;
			// Pumpkin pyramid on hay bales, the centrepiece.
			const float px = rng.range(-hw * 0.3f, hw * 0.3f);
			for (int k = 0; k < 3; ++k) {
				put("hay_bale", px - 0.6f + k * 0.6f, -3.0f, Math_PI * 0.5f);
			}
			put("hay_bale", px - 0.3f, -3.0f, Math_PI * 0.5f, 1.0f, 0.55f);
			put("hay_bale", px + 0.3f, -3.0f, Math_PI * 0.5f, 1.0f, 0.55f);
			put("giant_pumpkin", px, -3.0f, rng.range(-0.3f, 0.3f), 0.55f, 1.1f);
			const char *jacks[] = { "jackolantern_happy", "jackolantern_toothy", "jackolantern_wink", "jackolantern_surprised", "jackolantern_sleepy" };
			for (int k = 0; k < 5; ++k) {
				put(jacks[k], px - 1.6f + k * 0.8f, -1.5f, rng.range(-0.3f, 0.3f), rng.range(0.75f, 1.0f));
			}
			if (hw > 3.0f) {
				put("bench", px + hw * 0.7f, -1.4f, 0.0f);
				put(TREES[rng.irange(0, 2)], -px - hw * 0.6f, -5.5f, rng.range(0.0f, Math_TAU), 1.0f);
				put("leaf_pile", -px - hw * 0.6f + 1.2f, -4.8f, rng.range(0.0f, Math_TAU));
				put("corn_stalks", px + hw * 0.8f, -4.5f, 0.0f);
				put("corn_stalks", px - hw * 0.8f, -2.2f, 0.4f);
			}
			// Lamp posts with a string of lights between them.
			const Vector3 la = frame.xform(Vector3(-hw, 0, -0.8f)), lb = frame.xform(Vector3(hw, 0, -0.8f));
			put("lamp_post", -hw, -0.8f, 0.0f);
			put("lamp_post", hw, -0.8f, 0.0f);
			StringLight sl;
			sl.a = la + Vector3(0, 3.0f, 0);
			sl.b = lb + Vector3(0, 3.0f, 0);
			sl.sag = 0.7f;
			ctx.strings.push_back(sl);
			// A lawn under it all, and a festival banner arch.
			MeshBuilder &m = ctx.boundary_mesh;
			m.set_transform(frame);
			m.quad_out(Vector3(-w * 0.5f, 0.004f, 0), Vector3(w * 0.5f, 0.004f, 0), Vector3(w * 0.5f, 0.004f, -9.0f), Vector3(-w * 0.5f, 0.004f, -9.0f),
					mat_cell(pal::LAWN, mat::GRASS), Vector3(0, -1, -4));
			const int banner[] = { pal::BANNER_ORANGE, pal::BANNER_PURPLE, pal::BANNER_GREEN };
			const float span = Math::min(hw, 5.0f);
			for (int k = 0; k < 9; ++k) {
				const float t = float(k) / 8.0f;
				const float bx = Math::lerp(-span, span, t);
				const float by = 2.4f - std::sin(t * Math_PI) * 0.35f;
				m.tri_out(Vector3(bx - 0.25f, by, -0.85f), Vector3(bx + 0.25f, by, -0.85f), Vector3(bx, by - 0.45f, -0.85f),
						mat_cell(banner[k % 3], mat::FABRIC), Vector3(bx, by, -2.0f));
				m.tri_out(Vector3(bx - 0.25f, by, -0.85f), Vector3(bx + 0.25f, by, -0.85f), Vector3(bx, by - 0.45f, -0.85f),
						mat_cell(banner[k % 3], mat::FABRIC), Vector3(bx, by, 0.5f));
			}
			m.beam(Vector3(-span, 2.4f, -0.85f), Vector3(span, 2.4f, -0.85f), 0.02f, 0.02f, pal::CHARCOAL);
			m.set_transform(Transform3D());
		}
	}
}

// ---------------------------------------------------------------- street rhythm
void place_street_rhythm(GenContext &ctx) {
	Rng rng = ctx.rng(STAGE_RHYTHM);
	const StreetPlan &s = ctx.street;
	auto lot_at = [&](const Vector3 &p, Vector3 &local) -> Lot * {
		for (Lot &lot : ctx.lots) {
			local = lot.to_local(p);
			if (std::abs(local.x) < lot.width * 0.5f - 0.4f && local.z < 0.0f && local.z > -2.5f) {
				return &lot;
			}
		}
		return nullptr;
	};
	for (int side : { 1, -1 }) {
		float x = ctx.bound_x0 + rng.range(3.0f, 7.0f);
		int count = 0;
		while (x < ctx.bound_x1 - 3.0f) {
			// Street trees and lamps stand in the grass verge, with gaps at driveways and paths.
			const Vector3 p = s.at(x) + s.left_at(x) * float(side) * (s.edge() + 1.0f);
			const Vector3 verge = s.at(x) + s.left_at(x) * float(side) * (s.half_road + s.curb + s.verge * 0.5f);
			const bool in_lane = side == s.lane_dir && std::abs(x - s.lane_x) < s.edge() + 1.5f;
			Vector3 local;
			Lot *lot = lot_at(p, local);
			const bool lamp = (count % 2 == 1);
			if (lot && !in_lane && std::abs(local.x - lot->drive_x) > 2.3f && std::abs(local.x - lot->house.door_x) > 1.6f) {
				const float yaw = std::atan2(-s.left_at(x).x * side, -s.left_at(x).z * side);
				if (lamp) {
					ctx.add_prop(-1, "lamp_post", verge, yaw);
				} else {
					ctx.add_prop(-1, TREES[rng.irange(0, 2)], verge, rng.range(0.0f, Math_TAU), rng.range(0.78f, 0.92f), 1.0f);
				}
				++count;
				x += rng.range(6.5f, 8.0f);
			} else {
				x += 1.5f;
			}
		}
	}
	// Lamps at the intersection corners.
	for (float sx : { -1.0f, 1.0f }) {
		const Vector3 p = s.lane_origin + Vector3(sx * (s.edge() + 0.6f), 0, float(s.lane_dir) * (s.edge() + 0.6f));
		ctx.add_prop(-1, "lamp_post", p, sx > 0 ? -Math_PI * 0.25f : Math_PI * 0.25f);
	}
	// Parked cars at the curb, clear of the intersection and the row of candy hops across the road.
	const int cars = rng.irange(1, 3);
	for (int i = 0; i < cars; ++i) {
		for (int tries = 0; tries < 10; ++tries) {
			const float x = rng.range(ctx.bound_x0 + 8.0f, ctx.bound_x1 - 8.0f);
			if (std::abs(x - s.lane_x) < 14.0f) {
				continue;
			}
			bool clash = false;
			for (const PropPlacement &pp : ctx.props) {
				if (is_car(pp.id) && pp.lot < 0 && std::abs(pp.xf.origin.x - x) < 7.0f) {
					clash = true;
				}
			}
			if (clash) {
				continue;
			}
			const int side = rng.chance(0.5f) ? 1 : -1;
			const Vector3 p = s.at(x) + s.left_at(x) * float(side) * (s.half_road - 1.15f);
			const Vector3 tangent = (s.at(x + 1) - s.at(x - 1)).normalized();
			const float yaw = std::atan2(tangent.x, tangent.z) + (side > 0 ? 0.0f : Math_PI);
			ctx.add_prop(-1, pick_car(ctx, rng), p, yaw);
			break;
		}
	}
}

} // namespace tot
