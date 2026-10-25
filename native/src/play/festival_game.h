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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
	int stairs_lot_ = -1;
	float stairs_best_ = 0.0f;
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

	int64_t seed_ = 0;
	bool lightning_ = true;
	godot::Dictionary stats_;
	int wallet_ = 0, bonus_ = 0;
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
