#pragma once
// The game: an endless Halloween night across a streamed world of neighborhoods, towns and woods.
// Settlements are infested; destroying every monster nest and burning every thorn vine frees an
// area and wakes its travel gate, and freed gates link up for teleporting. Candy from doorbells,
// platforming runs and defeated monsters fills the bucket; getting hit spills some of it (grab it
// before it blinks away), and candy buys upgrades: hotter, quicker, triple fire, a bigger
// ground-pound blast, a bucket lid, a stronger magnet and the Pumpkin Cruiser ride-on car.
// Also hosts the command-line capture / seed-sweep / preview / autopilot review modes.

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/rng.h"
#include "play/monster_art.h"
#include "play/monsters.h"

namespace tot {

class NeighborhoodGenerator;
class DragonController;
class FestivalCamera;
class StormSky;
class WorldStreamer;
class Combat;
class LooseCandy;
class RideOnCar;
struct Area;

class FestivalGame : public godot::Node3D {
	GDCLASS(FestivalGame, godot::Node3D)

public:
	void _ready() override;
	void _process(double delta) override;
	void _exit_tree() override;

	void start_world(int64_t seed);
	void set_seed(int64_t s) { seed_ = s; }
	int64_t get_seed() const { return seed_; }
	void set_lightning(bool on);
	bool get_lightning() const { return lightning_; }
	godot::Dictionary get_stats() const { return stats_; }
	int get_wallet() const { return wallet_; }
	// Saved games (scripts/saves.gd writes them to disk): everything the night has achieved, and
	// where the kid stands. load_state rebuilds the world from the save's seed with that progress.
	godot::Dictionary save_state();
	bool load_state(const godot::Dictionary &d);
	// A night is under way, with no menu up (play time and autosaves count only then).
	bool is_playing() const { return started_ && !in_menu_ && !menu_open_ && teleport_t_ < 0.0f && cine_mode_.is_empty(); }

protected:
	static void _bind_methods();

private:
	struct Upgrades {
		int fire = 0, quick = 0, triple = 0, blast = 0, lid = 0, magnet = 0, car = 0;
		int *level(const godot::String &id);
	};
	struct FreedArea {
		uint64_t key = 0;
		std::string name;
		godot::Vector3 gate;
	};

	void call_ui(const char *method, const godot::Array &args = godot::Array());
	void call_audio(const char *method, const godot::Array &args = godot::Array());
	// Signals.
	void on_candy(int kind, int value, int lot, godot::Vector3 pos, int id, int64_t key);
	void on_house_cleared(int lot, int64_t key);
	void bump_combo();
	int fire_lead() const;
	int combo_ = 0;
	float combo_t_ = 0.0f;
	void on_sequence(int size, godot::Vector3 pos, int64_t key);
	void on_pounded(godot::Vector3 pos);
	void on_landed(float impact);
	void on_jumped();
	void on_flapped();
	void on_bounced();
	void on_spun();
	void on_fired();
	void on_lightning(float strength);
	void on_stepped(int parity);
	void on_purchase(godot::String id);
	void on_start_mode(int mode);
	void on_pause_choice(godot::String choice);
	void on_shop_tab(int dir);
	void on_travel(int index);
	void on_menu_closed();
	// World.
	void area_loaded(Area &a);
	void area_unloading(Area &a);
	void area_freed(uint64_t key);
	void update_area_hud(bool force);
	void hurt_player(const godot::Vector3 &from);
	void monster_event(const char *event, const godot::Vector3 &pos);
	void gather_targets(std::vector<Damageable *> &out);
	void toggle_car();
	void open_shop();
	// Shop tabs: 0 upgrades, 1 car paint, 2 costumes (switched with the bumpers / Q and E).
	int shop_tab_ = 0;
	godot::String shop_focus_; // keep this item focused when the shop refreshes
	int car_paint_ = 0;
	std::vector<char> paint_owned_;
	int costume_ = 0;
	std::vector<char> costume_found_;
	// Hidden costumes: one floats high above a rooftop in some settlements until it's found.
	struct CostumePickup {
		godot::Node3D *node = nullptr;
		int costume = 0;
		uint64_t key = 0;
		godot::Vector3 pos;
	};
	std::vector<CostumePickup> costume_pickups_;
	float costume_t_ = 0.0f;
	void place_costume(Area &a);
	// Wild monsters: a few roam the woods around the player (no nests), coming and going with them.
	std::vector<Monster *> wild_;
	godot::Node3D *wild_root_ = nullptr;
	float wild_t_ = 0.0f;
	Rng wild_rng_;
	void update_wild(float dt);
	void clear_wild();
	void update_costume_pickups(float dt);
	void begin_teleport(const godot::Vector3 &to);
	godot::Vector3 player_point() const;
	float fire_damage() const;
	// Juice: floating score text, speech bubbles, confetti and fireworks.
	void popup(const godot::Vector3 &pos, const godot::String &text, const godot::Color &color, float size = 1.0f, float life = 0.9f);
	void burst_fx(const godot::Vector3 &pos, bool firework);
	void update_popups(float dt);
	void update_wisps(float dt);
	struct Popup {
		godot::Label3D *node = nullptr;
		godot::Vector3 from;
		float t = 1.0f, size = 1.0f, life = 0.9f;
		godot::Color color;
	};
	std::vector<Popup> popups_;
	int next_popup_ = 0;
	std::vector<godot::CPUParticles3D *> fx_;
	int next_fx_ = 0;
	bool fx_test_ = false, review_car_ = false, review_shop_ = false, review_free_ = false, review_travel_ = false, review_accept_ = false;
	float fly_test_ = 0.0f, fly_t_ = 0.0f, fly_worst_ = 0.0f;
	int stairs_lot_ = -1, mantle_lot_ = -1;
	float stairs_best_ = 0.0f;
	int ledge_lot_ = -1, ledge_frame_ = -1; // --ledge-test: jump at a roof edge, hang, climb
	godot::Vector3 ledge_target_, ledge_dir_;
	float ledge_top_ = 0.0f;
	bool drop_pressed_ = false;
	float drop_test_ = -1.0f, drop_base_ = 0.0f, drop_peak_ = 0.0f; // --drop-test with prop:ID
	// Cinematic director (festival_cine.cpp): --cine=reel plays scripted showcase shots back to back
	// (for Godot's movie writer, with captions); --cine=NAME plays one; --cine-out=DIR saves a still
	// of each shot; --cine-clean leaves out the captions.
	struct CineShot {
		std::string name, caption, sub;
		float duration = 5.0f;
		float still_at = 0.5f; // fraction of the shot where its still is taken
		bool avoid = true; // keep the camera out of walls (pull in in front of anything between it and the kid)
		bool play_only = false; // a menu screenshot: only in --cine-play runs
		int64_t seed = 0; // a world of its own (0: the reel's seed); the world reloads when it changes
		bool gameplay = false; // seen as the player sees it: the normal camera and HUD, no caption
		bool trailer = true; // part of the trailer reel (false: screenshots only)
		bool face_camera = false; // gameplay camera: look at the kid's front (lineups), not from behind
		bool vines = true; // false: the thorn vines are hidden (shots where the kid or the car travels, so nobody passes through them)
		bool caption_top = false; // the caption at the top of the frame (a menu fills the middle)
		bool cutout = false; // the gameplay camera's occluder cutout: anything between the camera and the kid dithers away
		std::function<void()> setup;
		std::function<void(float t)> tick; // t: seconds into the shot
		std::function<godot::Transform3D(float t, float &fov)> cam;
	};
	std::vector<CineShot> cine_;
	godot::String cine_mode_, cine_out_;
	bool cine_clean_ = false, cine_still_ = false, cine_fading_ = false;
	bool cine_play_ = false; // --cine-play: the same action, seen through the normal game camera and HUD
	int cine_i_ = -1;
	float cine_t_ = 0.0f, cine_fire_ = 0.0f;
	godot::Vector3 cine_a_, cine_b_, cine_c_, cine_d_; // per-shot anchors
	godot::Vector3 cine_town_, cine_town_dir_;
	bool cine_town_found_ = false;
	void cine_build();
	void cine_begin(int i);
	void cine_step(float dt);
	void cine_reset_player();
	void cine_home(godot::Vector3 &spawn, godot::Vector3 &street); // the current home area's spawn and street direction
	int64_t cine_base_seed_ = 0, cine_bounce_seed_ = 0;
	int64_t cine_seed_override_ = 0; // --cine-seed: try a single shot in another world
	int cine_wait_ = 0; // frames left before a shot that just loaded its own world is set up
	bool cine_aim_on_ = false; // shots that fire at a set point instead of the auto-aim's pick
	godot::Vector3 cine_aim_;
	float cine_cut_at_ = -1.0f; // a shot that ends early (once its action is done)
	std::vector<godot::Node3D *> cine_props_; // extra models a shot places (the costume lineup), freed at the next shot
	bool cine_aggro_ = false; // gameplay segments: monsters come for the kid as usual (who can't be hurt)
	bool cine_clear(const godot::Vector3 &a, const godot::Vector3 &b); // nothing solid between a and b
	// The first camera offset (side, back) with a clear view of every target from both ends of its move.
	godot::Vector2 cine_lane(const godot::Vector3 &anchor, const godot::Vector3 &back_dir, std::initializer_list<float> sides, std::initializer_list<float> backs, float push, float height, std::initializer_list<godot::Vector3> targets);
	void parse_cmdline();
	void capture_and_maybe_quit();

	NeighborhoodGenerator *gen_ = nullptr; // material/asset template; also the single-neighborhood sweep
	WorldStreamer *streamer_ = nullptr;
	Combat *combat_ = nullptr;
	LooseCandy *loose_ = nullptr;
	RideOnCar *car_ = nullptr;
	DragonController *player_ = nullptr;
	FestivalCamera *camera_ = nullptr;
	StormSky *sky_ = nullptr;
	godot::DirectionalLight3D *moon_ = nullptr;
	godot::DirectionalLight3D *flash_ = nullptr;
	godot::WorldEnvironment *env_ = nullptr;
	godot::Node *hud_ = nullptr;
	godot::Node *audio_ = nullptr;

	MonsterArtKit art_;
	MonsterWorld mworld_;
	std::unordered_map<uint64_t, std::unique_ptr<Infestation>> infest_;
	std::vector<FreedArea> freed_;
	std::vector<uint64_t> travel_keys_;
	Upgrades up_;
	// Game modes: 0 Trick or Treat (no monsters or vines, every gate awake), 1 Save the Town.
	int mode_ = 1;
	bool in_menu_ = false; // the main menu is up over the live world
	bool started_ = false; // a mode has been picked since the world last loaded
	uint64_t menu_closed_frame_ = 0;
	void show_main_menu();
	void reset_progress(); // a new game: no candy, upgrades, paints or found costumes
	godot::Dictionary pending_states_; // a loading save's area progress, handed to the streamer in start_world

	int64_t seed_ = 0;
	bool lightning_ = true;
	godot::Dictionary stats_;
	int wallet_ = 0, bonus_ = 0;
	int fill_wallet_ = -1; // the wallet the bucket's candy heap last showed
	float invuln_ = 0.0f, since_pickup_ = 0.0f, nudge_cooldown_ = 0.0f, hud_t_ = 0.0f;
	bool menu_open_ = false, gate_armed_ = true, shop_armed_ = true, told_shop_ = false;
	uint64_t current_area_ = 0;
	bool in_area_ = false;
	std::string current_region_;
	float teleport_t_ = -1.0f;
	godot::Vector3 teleport_to_;
	bool visual_set_ = false;

	struct Wisp {
		godot::MeshInstance3D *node;
		godot::Vector3 from, to;
		float t;
	};
	std::vector<Wisp> wisps_;

	// Command line review modes.
	godot::String capture_path_;
	int capture_frames_ = 0;
	int frame_ = 0;
	godot::String capture_view_ = "play";
	int sweep_ = 0;
	bool quit_after_capture_ = false;

	// Asset preview (review pipeline): one asset on a lawn plot, four views stitched into one image.
	void setup_preview();
	void step_preview();
	godot::String preview_path_;
	godot::Node3D *preview_root_ = nullptr;
	godot::Ref<godot::Image> preview_sheet_;
	godot::AABB preview_box_;
	int preview_view_ = 0, preview_wait_ = 0;
	bool preview_icon_ = false, preview_icon_white_ = false, preview_icon_studio_ = false;
	godot::DirectionalLight3D *studio_ = nullptr;

	// Autopilot review bot: chases candy (and fights), reports collection rate and stuck time.
	void drive_bot(float dt);
	float autopilot_ = 0.0f;
	float bot_time_ = 0.0f, bot_retarget_ = 0.0f, bot_stuck_ = 0.0f, bot_stuck_total_ = 0.0f;
	float bot_escape_ = 0.0f, bot_log_ = 0.0f;
	int bot_doorbells_ = 0, bot_hits_ = 0, bot_kills_ = 0;
	godot::Vector3 bot_target_;
	bool bot_has_target_ = false;
	float bot_target_age_ = 0.0f, bot_last_pickup_ = 0.0f;
	int bot_last_count_ = 0;
	std::vector<std::pair<godot::Vector3, float>> bot_blacklist_;
};

} // namespace tot
