#pragma once
// Monster infestations. Each unfreed settlement has nests (one monster type each) that keep
// replacing the monsters you defeat, and big thorn vines blocking paths and doorsteps. Monsters
// wander, then wobble their way toward the player and telegraph simple attacks: lunges, hops,
// swoops, thrown bones and goo, charges, pounces and spore puffs. Destroy every nest and burn
// every vine to free the area and wake its travel gate.

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

#include <functional>
#include <memory>
#include <vector>

#include "core/rng.h"
#include "play/combat.h"
#include "play/monster_anim.h"
#include "play/monster_art.h"
#include "play/monster_rig.h"
#include "play/tick_smooth.h"

namespace tot {

struct Area;
struct AreaState;
struct RegionPlan;

// Everything monsters need from the game.
struct MonsterWorld {
	Combat *combat = nullptr;
	MonsterArtKit *art = nullptr;
	godot::Ref<godot::Material> toon;
	std::function<godot::Vector3()> player_pos;
	std::function<void(const godot::Vector3 &from)> hurt_player;
	std::function<float(float, float)> ground;
	std::function<void(const char *event, const godot::Vector3 &pos)> event;
	std::function<void(const godot::Vector3 &pos, int count)> drop_candy;
	// A brief fountain of candy shooting up out of a destroyed spawner or thorn nest.
	std::function<void(const godot::Vector3 &pos, int count)> candy_fountain;
	// Walkable route from rom to 	o around houses, fences and hedges (waypoints, last = goal);
	// false when no settlement grid covers rom (open country: go straight).
	std::function<bool(const godot::Vector3 &from, const godot::Vector3 &to, std::vector<godot::Vector3> &path)> route;
};

struct MonsterTuning {
	enum Attack { LUNGE, HOP, SWOOP, THROW, CHARGE, POUNCE, PUFF, GRAB };
	float speed, hp, reach, aggro, windup, cooldown;
	Attack attack;
	bool flying, phase, hops;
	float hover;
	int max_alive;
	float nest_hp;
	godot::Color color;
};
const MonsterTuning &tuning(MonsterType t);

// Review aid (TOT_MONSTER_TEST in festival_game.cpp): a line of text in the top-left corner of the
// picture (created under `parent` on the first call), so a recording says what it shows.
void monster_review_label(godot::Node *parent, const godot::String &text);

class MonsterNest;

class Monster : public godot::CharacterBody3D, public Damageable {
	GDCLASS(Monster, godot::CharacterBody3D)

public:
	void init(MonsterType type, MonsterWorld *world, MonsterNest *home, uint64_t seed, const godot::Rect2 &leash);
	void _physics_process(double delta) override;
	void _process(double delta) override;
	godot::Vector3 target_point() const override;
	float target_radius() const override;
	bool target_alive() const override { return !dead_; }
	bool is_monster() const override { return true; }
	bool take_hit(float damage, const godot::Vector3 &from) override;
	// Poof away with no reward (the area was freed).
	void vanish();
	MonsterType type() const { return type_; }
	MonsterNest *home() const { return home_; }
	std::function<void(Monster *)> on_gone;
	// Showcase hooks (festival_showcase.cpp records each monster through a fixed timeline). Additive:
	// nothing in play calls them, and they only set gameplay state (where it may roam, where it is
	// heading, which way it faces, start a wind-up), never the art or the animation.
	void showcase_leash(const godot::Rect2 &r) { leash_ = r; } // an empty rect: stand where it is
	void showcase_goal(const godot::Vector3 &goal, float seconds) {
		goal_ = goal;
		goal_t_ = seconds;
		path_.clear();
	}
	void showcase_face(float yaw) { yaw_ = yaw; }
	// Review aid (TOT_MONSTER_TEST in festival_game.cpp): read-only, for timing a test hit.
	MonsterAnimState review_state() const { return MonsterAnimState(int(state_)); }
	float review_state_time() const { return state_t_; }
	// Start the wind-up now, whatever the range and the cooldown (the attack follows as in play).
	void showcase_windup() {
		if (!dead_) {
			cooldown_ = 0.0f;
			set_state(State::Windup);
			if (world_ && world_->event) {
				world_->event("telegraph", get_global_position());
			}
		}
	}

protected:
	static void _bind_methods();

private:
	enum class State { Rise, Roam, Windup, Attack, Recover, Hurt, Dying };
	void set_state(State s);
	void begin_attack();
	void attack_step(float dt, const godot::Vector3 &player);
	void flash(const godot::Color &c, bool peak = false); // peak: a hit (the body near white for its first frames)
	void die(bool reward);
	godot::Vector3 pick_goal(const godot::Vector3 &player, bool chasing);
	godot::Vector3 steer_toward(const godot::Vector3 &pos, const godot::Vector3 &target, float dt);

	MonsterType type_ = MonsterType::PumpkinHopper;
	MonsterWorld *world_ = nullptr;
	MonsterNest *home_ = nullptr;
	godot::Rect2 leash_;
	Rng rng_;
	State state_ = State::Rise;
	float state_t_ = 0.0f, cooldown_ = 1.0f, goal_t_ = 0.0f, stuck_t_ = 0.0f, hop_t_ = 0.0f;
	float hp_ = 1.0f, flash_t_ = 0.0f, anim_t_ = 0.0f, orbit_ = 0.0f;
	godot::Color flash_color_;
	bool flash_peak_ = false;
	godot::Vector3 goal_, attack_dir_, knock_;
	std::vector<godot::Vector3> path_; // waypoints toward path_goal_
	godot::Vector3 path_goal_;
	float repath_t_ = 0.0f;
	bool hit_landed_ = false, dead_ = false, airborne_attack_ = false;
	float yaw_ = 0.0f;
	godot::Node3D *visual_ = nullptr;
	// The look in motion: the parts under visual_ and the animator that poses them (monster_rig.h,
	// monster_anim.h). Everything below only feeds the animator; none of it steers the AI.
	MonsterRig rig_;
	std::unique_ptr<MonsterAnimator> anim_;
	float rise_time_ = MONSTER_RISE_TIME, death_time_ = MONSTER_DEATH_TIME; // from the animator, read once
	bool chasing_ = false; // the player is inside the aggro range (as of the last physics tick)
	godot::Vector3 hit_dir_ = godot::Vector3(0, 0, 1); // the way the last hit pushed it (world, flat, unit)
	float since_hit_ = 1000.0f;
	godot::Vector3 accel_; // smoothed acceleration, world
	State anim_state_ = State::Rise, anim_prev_state_ = State::Rise; // last state the animator saw, and the one before
	bool anim_started_ = false, far_ = false;
	uint32_t anim_seed_ = 0;
	int lod_frame_ = 0;
	float lod_dt_ = 0.0f, lod_check_ = 0.0f, anim_state_t_ = 0.0f;
	// Drawn between physics ticks (play/tick_smooth.h): the animator's root transform is kept here and
	// the visual is placed from it every frame with the tick offset added.
	void physics_step(double delta);
	TickSmooth tick_smooth_;
	godot::Transform3D anim_root_;
	// For the animator only (AnimInput::surface): the walkable height under (x, z) near y, by a ray down
	// through the ground and the structures (kerbs, sidewalks, porch steps).
	std::function<float(float, float, float)> surface_;
};

class MonsterNest : public godot::Node3D, public Damageable {
	GDCLASS(MonsterNest, godot::Node3D)

public:
	void init(MonsterType type, MonsterWorld *world, uint64_t seed);
	void _process(double delta) override;
	godot::Vector3 target_point() const override;
	float target_radius() const override;
	bool target_alive() const override { return !destroyed_; }
	bool is_nest() const override { return true; }
	bool take_hit(float damage, const godot::Vector3 &from) override;
	MonsterType type() const { return type_; }
	bool destroyed() const { return destroyed_; }
	void set_dormant(bool d) { dormant_ = d; }
	void preset_destroyed();
	int alive = 0; // monsters from this nest currently out
	std::function<void(MonsterNest *)> spawn_monster;
	std::function<void(MonsterNest *)> on_destroyed;

protected:
	static void _bind_methods();

private:
	MonsterType type_ = MonsterType::PumpkinHopper;
	MonsterWorld *world_ = nullptr;
	Rng rng_;
	float hp_ = 8.0f, timer_ = 2.0f, wobble_ = 0.0f, flash_t_ = 0.0f, sink_ = 0.0f;
	bool destroyed_ = false, dormant_ = false;
	godot::MeshInstance3D *mesh_ = nullptr;
	godot::StaticBody3D *body_ = nullptr;
	godot::OmniLight3D *glow_ = nullptr;
	godot::MeshInstance3D *beacon_ = nullptr;
};

class ThornVine : public godot::Node3D, public Damageable {
	GDCLASS(ThornVine, godot::Node3D)

public:
	void init(MonsterWorld *world, int variant, float scale, uint64_t seed);
	void _process(double delta) override;
	godot::Vector3 target_point() const override;
	float target_radius() const override { return 1.3f * scale_; }
	bool target_alive() const override { return !burned_ && !hidden_; }
	bool is_nest() const override { return heart_; }
	bool take_hit(float damage, const godot::Vector3 &from) override;
	bool burned() const { return burned_; }
	void preset_burned();
	// The nest died: burn away after `delay` seconds (no reward).
	void wither(float delay);
	// Turn this cluster into a thorn nest (a beating heart, a beacon, lots of hit points).
	void make_heart();
	bool is_heart() const { return heart_; }
	// Trailer shots: hidden vines are gone in every way (no mesh, no collision, no thorns, not a target).
	void set_hidden(bool on);
	std::function<void(ThornVine *)> on_burned;

protected:
	static void _bind_methods();

private:
	MonsterWorld *world_ = nullptr;
	float hp_ = 6.0f, scale_ = 1.0f, flash_t_ = 0.0f, burn_t_ = -1.0f, shake_ = 0.0f, wither_in_ = -1.0f, pulse_t_ = 0.0f;
	bool heart_ = false;
	godot::MeshInstance3D *heart_core_ = nullptr, *heart_beacon_ = nullptr;
	godot::OmniLight3D *heart_light_ = nullptr;
	bool burned_ = false, hidden_ = false;
	godot::MeshInstance3D *mesh_ = nullptr;
	godot::StaticBody3D *body_ = nullptr;
	godot::Transform3D rest_;
};

// A creeping thorn vine spreading over the ground from its nest. Brushing against it hurts (jump
// over it, or burn it); when its nest is destroyed it withers and burns away.
class VineTendril : public godot::Node3D, public Damageable {
	GDCLASS(VineTendril, godot::Node3D)

public:
	void init(MonsterWorld *world, const std::vector<godot::Vector3> &pts, float width, uint64_t seed);
	void _process(double delta) override;
	godot::Vector3 target_point() const override;
	float target_radius() const override { return width_ * 0.6f; }
	float surface_distance(const godot::Vector3 &p) const override;
	float aim_weight() const override { return 2.5f; }
	bool target_alive() const override { return !burned_ && !hidden_; }
	bool take_hit(float damage, const godot::Vector3 &from) override;
	// The nest died: burn away after `delay` seconds (no reward).
	void wither(float delay);
	void preset_burned();
	bool burned() const { return burned_; }
	void set_hidden(bool on) {
		hidden_ = on;
		set_visible(!on);
	}
	std::function<void(VineTendril *)> on_burned;

protected:
	static void _bind_methods();

private:
	float nearest(const godot::Vector3 &p, godot::Vector3 &at) const;
	void start_burning(bool reward);
	MonsterWorld *world_ = nullptr;
	std::vector<godot::Vector3> pts_; // world
	float width_ = 0.5f, hp_ = 3.0f, flash_t_ = 0.0f, burn_t_ = -1.0f, wither_in_ = -1.0f;
	bool burned_ = false, hidden_ = false;
	godot::MeshInstance3D *mesh_ = nullptr;
	Rng rng_;
};

struct GenContext;

// Where a settlement's travel gate, candy cart, monster nests, thorn nests and their vines go
// (settlement-local), drawn from `rng` in the order Infestation::build consumes it, so saves (per-index
// nest state) line up from one load to the next. Pure: no nodes, so the --sweep can call it. `origin`
// is the area's world origin (the game's local -> world -> local round trips are kept bit for bit).
//
// Nest rules (Stage 1d): monster nests on lawns, sidewalks, service yards and on the asphalt (0-2.5 m off
// a road's centreline, 15-85% along its built stretch, >= 10 m from junctions and the spawn); thorn
// hearts anywhere a player can reach (lawns, backyards, service yards, squares and parks, sidewalks,
// roads), at most one on a road. One spacing rule across all of them: heart-heart 18 m, heart-nest
// 12 m, nest-nest 14 m, relaxed by 15% (compounding: x0.85, x0.7225) at most twice when the draws run dry.
struct InfestationLayout {
	godot::Vector3 gate; // the gate pad
	godot::Transform3D cart; // the shop cart; its front (+Z) faces the middle of the side street
	std::vector<int> kinds; // the area's monster mix (MonsterType), nest i is kinds[i % size]
	std::vector<godot::Vector3> nests;
	std::vector<uint32_t> nest_seeds;
	std::vector<godot::Vector3> hearts; // thorn nests
	std::vector<float> heart_yaw;
	std::vector<uint32_t> heart_seeds;
	std::vector<char> nest_on_road, heart_on_road; // on a road's asphalt
	// Thorn vines creeping out of the hearts (ground points), and the thorn knots blocking front doors.
	struct Tendril {
		std::vector<godot::Vector3> pts;
		int nest = 0; // heart index
	};
	std::vector<Tendril> tendrils;
	std::vector<std::pair<godot::Vector3, int>> knots;
	int want_nests = 0, want_hearts = 0, nest_candidates = 0, heart_candidates = 0;
	int road_nest_candidates = 0, road_heart_candidates = 0;
	int relax_hearts = 0, relax = 0; // spacing relax steps in force after the hearts / at the end (0-2)
	int tries = 0; // whole-layout picks made (a jammed pick starts over, up to 4)
};
InfestationLayout plan_infestation(const GenContext &ctx, const MonsterArtKit &art, const godot::Vector3 &origin, bool peaceful, Rng &rng);

// One settlement's infestation: nests, vines, the monsters out roaming, and the travel gate.
class Infestation {
public:
	// peaceful (Trick or Treat mode): just the travel gate and the shop cart, awake from the start;
	// no nests, thorn nests, vines or monsters.
	void build(Area &area, MonsterWorld *world, godot::Node3D *root, bool peaceful = false);
	// Area preloading: build()'s pure-data half (walk grid, nest layout and the draws they take) from the
	// settlement's plan, safe on a worker thread; a build() with the same `peaceful` then starts from it.
	void prepare(const GenContext &ctx, const RegionPlan &plan, const MonsterArtKit &art, bool peaceful);
	// build() in slices: build_begin() makes the gate and the cart, build_step() the nests, thorn nests,
	// tendrils and knots in order until `deadline_us` (NeighborhoodGenerator::build_clock_us(); 0: none)
	// passes, after at least one; true when all are made. build() is the two together.
	void build_begin(Area &area, MonsterWorld *world, godot::Node3D *root, bool peaceful = false);
	bool build_step(uint64_t deadline_us);
	bool route(const godot::Vector3 &from, const godot::Vector3 &to, std::vector<godot::Vector3> &path) const;
	void update(float dt);
	void gather(std::vector<Damageable *> &out);
	int nests_total() const { return int(nests_.size()); }
	godot::Vector3 nest_position(int i) const { return i >= 0 && i < int(nests_.size()) ? nests_[i]->get_global_position() : godot::Vector3(); }
	int vines_total() const { return int(vines_.size()); }
	int nests_left() const;
	int vines_left() const;
	int hearts_total() const { return int(hearts_.size()); }
	godot::Vector3 heart_position(int i) const { return i >= 0 && i < int(hearts_.size()) ? hearts_[i]->get_global_position() : godot::Vector3(); }
	int hearts_left() const;
	bool freed() const { return freed_; }
	godot::Vector3 gate_position() const { return gate_pos_; }
	godot::Vector3 shop_position() const { return shop_pos_; } // the cart's doormat
	int monsters_out() const { return int(monsters_.size()); }
	// The area is cleared: monsters poof away, the gate lights up.
	void set_freed(bool celebrate);
	// Trailer shots: the nest nearest p sends out its full brood right away (its monsters around it),
	// so a nest on screen reads as a monster nest.
	void fill_nest_near(const godot::Vector3 &p);
	// How far p is from the nearest live thorn (vine clusters, thorn nests and creeping tendrils), in metres.
	float thorn_distance(const godot::Vector3 &p) const;
	// Trailer shots: hide the vine clusters and tendrils (never a thorn nest) that come within r of the
	// segment a-b, so a camera or the kid has room. Returns how many were hidden.
	int hide_thorns_near(const godot::Vector3 &a, const godot::Vector3 &b, float r);
	// Trailer shots: show or hide every thorn vine, tendril and thorn nest (they stay in play).
	void set_vines_visible(bool on) {
		for (ThornVine *v : vines_) v->set_hidden(!on);
		for (VineTendril *t : tendrils_) t->set_hidden(!on);
		for (ThornVine *h : hearts_) h->set_hidden(!on);
	}
	// --sweep nav probe: the walk grid alone (as build() makes it), and what the last route() did.
	void build_nav_only(const Area &area) { build_nav(area); }
	// The walk grid from the settlement's plan alone (plain data; plan_infestation's reachability test, Stage 3c).
	void build_nav(const GenContext &ctx, const RegionPlan &plan);
	int last_route_expanded() const { return route_expanded_; }
	// Connected piece of the walk grid at p (nearest open cell within 4, as route() snaps; 0: none).
	int nav_label(const godot::Vector3 &p) const;
	bool last_route_capped() const { return route_capped_; }
	static int route_cap; // A* expansions per search (monsters.cpp)
	std::function<void()> on_freed;
	std::function<void(int index)> on_nest_down;
	std::function<void(int index)> on_vine_down;
	std::function<void(int index)> on_heart_down;
	~Infestation();

private:
	void spawn_from(MonsterNest *nest);
	void build_nav(const Area &area);
	bool prepared_ = false, prepared_peaceful_ = false;
	InfestationLayout prepared_layout_;
	InfestationLayout build_layout_; // build_begin() .. the last build_step()
	AreaState *build_state_ = nullptr;
	godot::Vector3 build_origin_;
	bool build_peaceful_ = false, building_ = false;
	size_t build_unit_ = 0;
	bool nav_blocked(int x, int z) const { return x < 0 || z < 0 || x >= nav_w_ || z >= nav_h_ || nav_[z * nav_w_ + x]; }
	bool nav_line(const godot::Vector3 &a, const godot::Vector3 &b) const;
	// Walk grid over the settlement: 1 = blocked (houses, fences, hedges, big props).
	std::vector<uint8_t> nav_;
	int nav_w_ = 0, nav_h_ = 0;
	float nav_x0_ = 0.0f, nav_z0_ = 0.0f, nav_y_ = 0.0f;
	static constexpr float NAV_CELL = 0.6f;
	void ensure_labels() const; // connected pieces of the walk grid (built with it)
	mutable std::vector<int> labels_;
	mutable int route_expanded_ = 0;
	mutable bool route_capped_ = false;
	void check_freed();

	MonsterWorld *world_ = nullptr;
	godot::Node3D *root_ = nullptr;
	Rng rng_;
	godot::Rect2 leash_;
	std::vector<MonsterNest *> nests_;
	std::vector<ThornVine *> vines_;
	std::vector<VineTendril *> tendrils_;
	std::vector<ThornVine *> hearts_; // thorn nests: every vine grows from one
	std::vector<int> vine_nest_, tendril_nest_; // owning thorn nest of each knot and tendril
	std::vector<Monster *> monsters_;
	godot::Vector3 gate_pos_, shop_pos_;
	godot::MeshInstance3D *gate_lantern_ = nullptr;
	godot::OmniLight3D *gate_light_ = nullptr;
	float gate_glow_ = 0.0f, gate_t_ = 0.0f;
	bool freed_ = false;
};

} // namespace tot
