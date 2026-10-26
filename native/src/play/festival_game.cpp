#include "play/festival_game.h"
#include "core/fonts.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/camera_attributes.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

#include "core/rng.h"
#include "gen/asset_library.h"
#include "gen/candy_kinds.h"
#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "gen/types.h"
#include "mesh/materials.gen.h"
#include "mesh/mesh_builder.h"
#include "play/candy_field.h"
#include "play/combat.h"
#include "play/dragon_controller.h"
#include "play/festival_camera.h"
#include "play/loose_candy.h"
#include "play/ride_on_car.h"
#include "play/storm_sky.h"
#include "play/vfx.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

namespace {

struct UpgradeDef {
	const char *id, *name, *desc;
	int max;
	int cost[3];
};
const UpgradeDef UPGRADES[] = {
	{ "fire", "Hotter Flames", "Fireballs hit harder.", 3, { 60, 150, 300 } },
	{ "quick", "Quick Breath", "Breathe fire more often.", 2, { 80, 200, 0 } },
	{ "triple", "Triple Flame", "Three fireballs in a fan.", 1, { 250, 0, 0 } },
	{ "blast", "Bigger Blast", "The ground-pound fire ring reaches further and burns hotter.", 2, { 100, 220, 0 } },
	{ "lid", "Bucket Lid", "Spill less candy when you get hit.", 2, { 90, 200, 0 } },
	{ "magnet", "Candy Magnet", "Candy flies to you from further away.", 1, { 120, 0, 0 } },
	{ "car", "Pumpkin Cruiser", "A magic ride-on car. Summon it with {summon_car}.", 1, { 350, 0, 0 } },
};

Color candy_color(int kind) {
	return candy_glow(kind);
}

// Player costumes: the dragon onesie, and the ones hidden around the world.
struct CostumeDef {
	const char *id, *name;
};
const CostumeDef COSTUMES[] = {
	{ "dragon_kid", "Dragon" },
	{ "kid_ghost", "Bedsheet Ghost" },
	{ "kid_cat", "Black Cat" },
	{ "kid_witch", "Little Witch" },
	{ "kid_skeleton", "Skeleton" },
	{ "kid_pumpkin", "Pumpkin" },
};
constexpr int COSTUME_COUNT = int(sizeof(COSTUMES) / sizeof(COSTUMES[0]));
const char *COSTUME_HINT = "Hidden high above a rooftop in another neighborhood or town.";

} // namespace

int *FestivalGame::Upgrades::level(const String &id) {
	if (id == "fire") return &fire;
	if (id == "quick") return &quick;
	if (id == "triple") return &triple;
	if (id == "blast") return &blast;
	if (id == "lid") return &lid;
	if (id == "magnet") return &magnet;
	if (id == "car") return &car;
	return nullptr;
}

void FestivalGame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_world", "seed"), &FestivalGame::start_world);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &FestivalGame::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &FestivalGame::get_seed);
	ClassDB::bind_method(D_METHOD("set_lightning", "on"), &FestivalGame::set_lightning);
	ClassDB::bind_method(D_METHOD("get_lightning"), &FestivalGame::get_lightning);
	ClassDB::bind_method(D_METHOD("get_stats"), &FestivalGame::get_stats);
	ClassDB::bind_method(D_METHOD("get_wallet"), &FestivalGame::get_wallet);
	ClassDB::bind_method(D_METHOD("save_state"), &FestivalGame::save_state);
	ClassDB::bind_method(D_METHOD("load_state", "state"), &FestivalGame::load_state);
	ClassDB::bind_method(D_METHOD("is_playing"), &FestivalGame::is_playing);
	ClassDB::bind_method(D_METHOD("_on_candy", "kind", "value", "lot", "position", "id", "key"), &FestivalGame::on_candy);
	ClassDB::bind_method(D_METHOD("_on_house_cleared", "lot", "key"), &FestivalGame::on_house_cleared);
	ClassDB::bind_method(D_METHOD("_on_sequence", "size", "position", "key"), &FestivalGame::on_sequence);
	ClassDB::bind_method(D_METHOD("_on_pounded", "position"), &FestivalGame::on_pounded);
	ClassDB::bind_method(D_METHOD("_on_landed", "impact"), &FestivalGame::on_landed);
	ClassDB::bind_method(D_METHOD("_on_jumped"), &FestivalGame::on_jumped);
	ClassDB::bind_method(D_METHOD("_on_flapped"), &FestivalGame::on_flapped);
	ClassDB::bind_method(D_METHOD("_on_bounced"), &FestivalGame::on_bounced);
	ClassDB::bind_method(D_METHOD("_on_spun"), &FestivalGame::on_spun);
	ClassDB::bind_method(D_METHOD("_on_fired"), &FestivalGame::on_fired);
	ClassDB::bind_method(D_METHOD("_on_lightning", "strength"), &FestivalGame::on_lightning);
	ClassDB::bind_method(D_METHOD("_on_stepped", "parity"), &FestivalGame::on_stepped);
	ClassDB::bind_method(D_METHOD("_on_purchase", "id"), &FestivalGame::on_purchase);
	ClassDB::bind_method(D_METHOD("_on_shop_tab", "dir"), &FestivalGame::on_shop_tab);
	ClassDB::bind_method(D_METHOD("_on_start_mode", "mode"), &FestivalGame::on_start_mode);
	ClassDB::bind_method(D_METHOD("_on_pause_choice", "choice"), &FestivalGame::on_pause_choice);
	ClassDB::bind_method(D_METHOD("_on_travel", "index"), &FestivalGame::on_travel);
	ClassDB::bind_method(D_METHOD("_on_menu_closed"), &FestivalGame::on_menu_closed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lightning"), "set_lightning", "get_lightning");
	ADD_SIGNAL(MethodInfo("world_started", PropertyInfo(Variant::INT, "seed")));
}

void FestivalGame::set_lightning(bool on) {
	lightning_ = on;
	if (sky_) {
		sky_->set_lightning_enabled(on);
	}
}

void FestivalGame::parse_cmdline() {
	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	for (int i = 0; i < args.size(); ++i) {
		const String a = args[i];
		if (a.begins_with("--seed=")) {
			seed_ = a.substr(7).to_int();
		} else if (a.begins_with("--capture=")) {
			capture_path_ = a.substr(10);
			if (capture_frames_ == 0) {
				capture_frames_ = 150;
			}
			quit_after_capture_ = true;
		} else if (a.begins_with("--capture-frames=")) {
			capture_frames_ = a.substr(17).to_int();
		} else if (a.begins_with("--capture-view=")) {
			capture_view_ = a.substr(15);
		} else if (a.begins_with("--sweep=")) {
			sweep_ = a.substr(8).to_int();
		} else if (a == "--no-lightning") {
			lightning_ = false;
		} else if (a.begins_with("--autopilot=")) {
			autopilot_ = float(a.substr(12).to_float());
		} else if (a.begins_with("--preview=")) {
			preview_path_ = a.substr(10);
		} else if (a == "--fx-test") {
			fx_test_ = true;
		} else if (a.begins_with("--wallet=")) {
			wallet_ = a.substr(9).to_int();
		} else if (a.begins_with("--cine=")) {
			cine_mode_ = a.substr(7);
		} else if (a.begins_with("--cine-out=")) {
			cine_out_ = a.substr(11);
		} else if (a == "--cine-clean") {
			cine_clean_ = true;
		} else if (a.begins_with("--cine-bounce-seed=")) {
			cine_bounce_seed_ = a.substr(19).to_int();
		} else if (a == "--cine-play") {
			cine_play_ = true;
			cine_clean_ = true;
		} else if (a == "--give-car") {
			up_.car = 1;
			review_car_ = true;
		} else if (a == "--open-shop") {
			review_shop_ = true;
		} else if (a.begins_with("--overview-dist=")) {
			camera_->set_overview_distance(float(a.substr(16).to_float()));
		} else if (a.begins_with("--ledge-test=")) {
			ledge_lot_ = a.substr(13).to_int();
		} else if (a.begins_with("--mantle-test=")) {
			mantle_lot_ = a.substr(14).to_int();
		} else if (a.begins_with("--stairs-test=")) {
			stairs_lot_ = a.substr(14).to_int();
		} else if (a == "--press-accept") {
			review_accept_ = true;
		} else if (a == "--free-home") {
			review_free_ = true;
		} else if (a == "--travel-test") {
			review_free_ = true;
			review_travel_ = true;
		} else if (a.begins_with("--fly-test=")) {
			fly_test_ = float(a.substr(11).to_float());
		}
	}
}

void FestivalGame::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	gen_ = Object::cast_to<NeighborhoodGenerator>(get_node_or_null("Generator"));
	player_ = Object::cast_to<DragonController>(get_node_or_null("Player"));
	camera_ = Object::cast_to<FestivalCamera>(get_node_or_null("CameraRig"));
	sky_ = Object::cast_to<StormSky>(get_node_or_null("StormSky"));
	moon_ = Object::cast_to<DirectionalLight3D>(get_node_or_null("Moon"));
	flash_ = Object::cast_to<DirectionalLight3D>(get_node_or_null("LightningFlash"));
	env_ = Object::cast_to<WorldEnvironment>(get_node_or_null("WorldEnvironment"));
	hud_ = get_node_or_null("HUD");
	audio_ = get_node_or_null("Audio");
	if (!gen_ || !player_ || !camera_) {
		UtilityFunctions::push_error("FestivalGame needs Generator, Player and CameraRig children.");
		return;
	}
	parse_cmdline();
	if (!preview_path_.is_empty()) {
		setup_preview();
		set_process(true);
		return;
	}
	if (sweep_ > 0) {
		// Headless validation sweep: generate N seeds (neighborhoods, then towns), print stats, quit.
		const int64_t base = seed_ != 0 ? seed_ : 1;
		int failures = 0;
		for (int i = 0; i < sweep_; ++i) {
			const bool town = i % 2 == 1;
			Dictionary st = town ? gen_->generate_settlement(base + i, SettlementKind::Town, true) : gen_->generate(base + i);
			failures += int(st["openness_failures"]);
			st.erase("households");
			st["kind"] = town ? "town" : "neighborhood";
			UtilityFunctions::print("SWEEP ", JSON::stringify(st));
		}
		UtilityFunctions::print("SWEEP_DONE seeds=", sweep_, " openness_failures=", failures);
		get_tree()->quit();
		return;
	}
	gen_->set_visible(false);
	gen_->assets(); // load the shared asset library once
	art_.build();
	art_.apply_assets(gen_->assets());
	streamer_ = memnew(WorldStreamer);
	streamer_->set_name("World");
	add_child(streamer_);
	streamer_->on_area_loaded = [this](Area &a) { area_loaded(a); };
	streamer_->on_area_unloading = [this](Area &a) { area_unloading(a); };
	Vfx *vfx = memnew(Vfx);
	vfx->set_name("Vfx");
	add_child(vfx);
	combat_ = memnew(Combat);
	combat_->set_name("Combat");
	add_child(combat_);
	combat_->gather_targets = [this](std::vector<Damageable *> &out) { gather_targets(out); };
	combat_->player_position = [this]() { return player_->get_global_position(); };
	combat_->on_player_hit = [this](const Vector3 &from) { hurt_player(from); };
	combat_->on_target_hit = [this](const Vector3 &pos, bool destroyed) {
		Array a;
		a.push_back(destroyed ? 1 : 0);
		call_audio("play_hit", a);
		(void)pos;
	};
	combat_->on_fizzle = [this](const Vector3 &) { call_audio("play_fizzle"); };
	loose_ = memnew(LooseCandy);
	loose_->set_name("LooseCandy");
	add_child(loose_);
	{
		register_candy_kinds(gen_->assets());
		std::vector<Ref<Mesh>> meshes;
		for (int k = 0; k < candy_kind_count(); ++k) {
			meshes.push_back(gen_->assets().merged_mesh(candy_kind_id(k)));
		}
		loose_->setup(meshes, gen_->get_candy_material());
		loose_->on_pickup = [this](int kind, const Vector3 &pos) {
			bump_combo();
			Array a;
			a.push_back(candy_tier(kind));
			call_audio("play_pickup", a);
			player_->celebrate(kind);
			if (Vfx *v = Vfx::get()) {
				v->candy_pickup(pos, candy_color(kind), candy_tier(kind));
			}
		};
	}
	mworld_.combat = combat_;
	mworld_.art = &art_;
	mworld_.toon = gen_->get_toon_material();
	mworld_.player_pos = [this]() {
		// Trailer shots: monsters mind their own business (no charging at, or shoving, the kid).
		return cine_i_ >= 0 && !cine_aggro_ ? Vector3(0, -1000.0f, 0) : player_->get_global_position();
	};
	mworld_.hurt_player = [this](const Vector3 &from) { hurt_player(from); };
	mworld_.ground = [this](float x, float z) { return streamer_->ground(x, z); };
	mworld_.event = [this](const char *e, const Vector3 &p) { monster_event(e, p); };
	mworld_.drop_candy = [this](const Vector3 &p, int count) { loose_->spill(p, count, false); };
	mworld_.candy_fountain = [this](const Vector3 &p, int count) { loose_->fountain(p, count); };
	mworld_.route = [this](const Vector3 &from, const Vector3 &to, std::vector<Vector3> &path) {
		for (auto &kv : infest_) {
			if (kv.second && kv.second->route(from, to, path)) {
				return true;
			}
		}
		return false;
	};

	player_->connect("pounded", Callable(this, "_on_pounded"));
	player_->connect("landed", Callable(this, "_on_landed"));
	player_->connect("jumped", Callable(this, "_on_jumped"));
	player_->connect("flapped", Callable(this, "_on_flapped"));
	player_->connect("bounced", Callable(this, "_on_bounced"));
	player_->connect("stepped", Callable(this, "_on_stepped"));
	player_->connect("spun", Callable(this, "_on_spun"));
	player_->connect("fired", Callable(this, "_on_fired"));
	if (audio_ && audio_->has_method("play_land")) {
		player_->connect("grabbed", Callable(audio_, "play_land").bind(3.0)); // a soft catch thud
	}
	if (hud_) {
		if (hud_->has_signal("purchase")) {
			hud_->connect("purchase", Callable(this, "_on_purchase"));
		}
		if (hud_->has_signal("start_mode")) {
			hud_->connect("start_mode", Callable(this, "_on_start_mode"));
		}
		if (hud_->has_signal("pause_choice")) {
			hud_->connect("pause_choice", Callable(this, "_on_pause_choice"));
		}
		if (hud_->has_signal("shop_tab")) {
			hud_->connect("shop_tab", Callable(this, "_on_shop_tab"));
		}
		if (hud_->has_signal("travel")) {
			hud_->connect("travel", Callable(this, "_on_travel"));
		}
		if (hud_->has_signal("menu_closed")) {
			hud_->connect("menu_closed", Callable(this, "_on_menu_closed"));
		}
	}
	if (sky_) {
		sky_->connect("lightning", Callable(this, "_on_lightning"));
		sky_->set_lightning_enabled(lightning_);
		sky_->set_flash_light(flash_);
	}
	wild_root_ = memnew(Node3D);
	wild_root_->set_name("WildMonsters");
	add_child(wild_root_);
	camera_->set_target(player_);
	if (seed_ == 0) {
		seed_ = int64_t(Time::get_singleton()->get_ticks_usec() % 1000000) + 1;
	}
	// Plain launches open on the main menu over a peaceful night; review runs (any command-line
	// arguments) go straight into Save the Town.
	const bool menu = OS::get_singleton()->get_cmdline_user_args().is_empty();
	mode_ = menu ? 0 : 1;
	started_ = !menu;
	in_menu_ = menu; // no area banners under the title
	start_world(seed_);
	if (menu) {
		show_main_menu();
	}
	set_process(true);
}

// Children have left the tree but still exist here: let go of everything that points at them
// before the tree frees them.
void FestivalGame::_exit_tree() {
	infest_.clear();
	for (Monster *m : wild_) {
		m->on_gone = nullptr;
	}
	wild_.clear();
	if (streamer_) {
		streamer_->on_area_loaded = nullptr;
		streamer_->on_area_unloading = nullptr;
	}
	if (combat_) {
		combat_->gather_targets = nullptr;
		combat_->on_player_hit = nullptr;
		combat_->on_target_hit = nullptr;
		combat_->on_fizzle = nullptr;
	}
}

void FestivalGame::start_world(int64_t seed) {
	seed_ = seed;
	infest_.clear();
	freed_.clear();
	costume_pickups_.clear(); // their nodes go with the areas
	clear_wild();
	wild_rng_ = Rng(uint64_t(seed) ^ 0x5EED5u, 31);
	if (paint_owned_.empty()) {
		paint_owned_.assign(RideOnCar::scheme_count(), 0);
		paint_owned_[0] = 1;
		costume_found_.assign(COSTUME_COUNT, 0);
		costume_found_[0] = 1;
	}
	combat_->clear();
	loose_->clear();
	if (car_) {
		camera_->set_follow_body(nullptr);
		car_->queue_free();
		car_ = nullptr;
	}
	player_->set_mounted(false);
	streamer_->setup(gen_, uint64_t(seed));
	if (!pending_states_.is_empty()) {
		streamer_->import_states(pending_states_); // a save loading: areas come back with its progress
		pending_states_ = Dictionary();
	}
	const RegionPlan &home = streamer_->plan().region(0, 0);
	streamer_->update(home.spawn, true);
	if (!visual_set_) {
		player_->set_visual(gen_->instantiate_asset("dragon_kid"));
		player_->set_wand(gen_->instantiate_asset("fire_wand"));
		visual_set_ = true;
	}
	Vector3 spawn = home.spawn;
	spawn.y = streamer_->ground(spawn.x, spawn.z) + 0.2f;
	player_->teleport(spawn, home.spawn_yaw);
	player_->set_input_enabled(true);
	static const float cooldown[3] = { 0.45f, 0.32f, 0.22f };
	player_->set_fire_cooldown(cooldown[Math::clamp(up_.quick, 0, 2)]); // Quick Breath carries over
	stats_ = Dictionary();
	stats_["seed"] = seed;
	stats_["home"] = String(home.name.c_str());
	UtilityFunctions::print("Trick or Treat: world seed ", seed, " home ", String(home.name.c_str()));
	const Vector3 moon_dir = streamer_->moon_direction();
	if (moon_) {
		// Moonlight comes from high and slightly behind the camera (stylised, not from the sky
		// moon) so fronts read clearly and shadows fall away down the street.
		const Vector3 flat = Vector3(-moon_dir.x, 0, -moon_dir.z).normalized().rotated(Vector3(0, 1, 0), 0.7f);
		const float elev = Math::deg_to_rad(52.0f);
		const Vector3 to_light = (flat * std::cos(elev) + Vector3(0, std::sin(elev), 0)).normalized();
		moon_->set_global_transform(Transform3D(Basis::looking_at(-to_light, Vector3(0, 1, 0)), Vector3()));
	}
	if (flash_) {
		flash_->set_global_transform(Transform3D(Basis::looking_at(-(moon_dir + Vector3(0.4f, 0.3f, 0.2f)).normalized(), Vector3(0, 1, 0)), Vector3()));
		flash_->set_param(Light3D::PARAM_ENERGY, 0.0f);
	}
	if (env_ && env_->get_environment().is_valid() && env_->get_environment()->get_sky().is_valid()) {
		Ref<ShaderMaterial> sm = env_->get_environment()->get_sky()->get_material();
		if (sm.is_valid()) {
			sm->set_shader_parameter("moon_dir", moon_dir);
			sm->set_shader_parameter("cloud_cover", 0.42f + float(sub_seed(uint64_t(seed), 5) % 100) / 100.0f * 0.2f);
			if (sky_) {
				sky_->set_sky_material(sm);
			}
		}
	}
	if (sky_) {
		sky_->setup(gen_, spawn, moon_dir, uint64_t(seed));
	}
	if (Area *a = streamer_->area(WorldStreamer::area_key(0, 0))) {
		camera_->set_guide_yaw(a->gen->street_yaw_at(a->to_local(spawn)));
	}
	camera_->set_heading_sign(std::sin(home.spawn_yaw) >= 0.0f ? 1.0f : -1.0f);
	camera_->set_overview(false);
	camera_->snap();
	for (Wisp &w : wisps_) {
		w.node->queue_free();
	}
	wisps_.clear();
	since_pickup_ = 0.0f;
	in_area_ = false;
	current_region_.clear();
	menu_open_ = false;
	teleport_t_ = -1.0f;
	call_audio("start_music");
	Array a;
	a.push_back(seed);
	call_ui("world_started", a);
	Array w;
	w.push_back(wallet_);
	call_ui("set_candy", w);
	emit_signal("world_started", seed);
	frame_ = 0;
	if (capture_view_.begins_with("lot:")) {
		const int lot = capture_view_.substr(4).to_int();
		if (Area *ar = streamer_->area(WorldStreamer::area_key(0, 0))) {
			PackedVector3Array bells = ar->gen->get_doorbells();
			if (lot >= 0 && lot < bells.size()) {
				const Vector3 b = ar->to_world(bells[lot]);
				const Vector3 toward_street = spawn - b;
				Vector3 p = b + Vector3(toward_street.x, 0, toward_street.z).normalized() * 9.0f;
				p.y = streamer_->ground(p.x, p.z) + 0.2f;
				player_->teleport(p, std::atan2(b.x - p.x, b.z - p.z));
				camera_->face_direction(b - p);
				camera_->snap();
			}
		}
	} else if (capture_view_ == "overview") {
		camera_->set_overview(true);
		camera_->snap();
	} else if (capture_view_.begins_with("nest:") || capture_view_.begins_with("heart:")) {
		// nest:K / heart:K: stand a few metres from the home area's K-th monster or thorn nest.
		const bool heart = capture_view_.begins_with("heart:");
		const int k = capture_view_.substr(heart ? 6 : 5).to_int();
		auto it = infest_.find(WorldStreamer::area_key(0, 0));
		if (it != infest_.end() && k < (heart ? it->second->hearts_total() : it->second->nests_total())) {
			const Vector3 n = heart ? it->second->heart_position(k) : it->second->nest_position(k);
			// Approach from the first direction (street side first) where neither the kid nor the
			// camera behind them has a house in the way.
			Vector3 p = n + (spawn - n).normalized() * 7.0f;
			PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
			const float base = std::atan2(spawn.x - n.x, spawn.z - n.z);
			for (int a = 0; a < 12 && space; ++a) {
				const float ang = base + (a % 2 ? 1.0f : -1.0f) * float((a + 1) / 2) * (Math_TAU / 12.0f);
				const Vector3 dir(std::sin(ang), 0, std::cos(ang));
				const Vector3 eye = n + dir * 14.0f + Vector3(0, 6.0f, 0);
				Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(eye, n + Vector3(0, 1.0f, 0), 1);
				const Dictionary hit = space->intersect_ray(q);
				if (hit.is_empty() || Vector3(hit["position"]).distance_to(n) < 2.5f) {
					p = n + dir * 7.0f;
					break;
				}
			}
			p.y = streamer_->ground(p.x, p.z) + 0.3f;
			player_->teleport(p, std::atan2(n.x - p.x, n.z - p.z));
			camera_->face_direction(n - p);
			camera_->snap();
			player_->set_input_enabled(false); // just watch
		}
	} else if (capture_view_.begins_with("prop:")) {
		// prop:ID[#K]: look at the home area's K-th placed prop with that id from 7 m.
		PackedStringArray parts = capture_view_.substr(5).split("#");
		const std::string id = parts[0].utf8().get_data();
		int want = parts.size() > 1 ? parts[1].to_int() : 0;
		Area *ar = streamer_->area(WorldStreamer::area_key(0, 0));
		int found = 0;
		for (const PropPlacement &pp : ar->gen->context()->props) {
			if (pp.id != id) {
				continue;
			}
			if (want-- > 0) {
				++found;
				continue;
			}
			const Vector3 g = ar->to_world(pp.xf.origin);
			if (OS::get_singleton()->get_cmdline_user_args().has("--drop-test")) {
				// Drop onto the prop from 3 m and report how high it throws the kid.
				drop_test_ = 0.0f;
				drop_base_ = g.y;
				player_->teleport(g + Vector3(0, 3.0f, 0), 0.0f);
				player_->set_input_enabled(false);
				camera_->snap();
				++found;
				break;
			}
			Vector3 p = g + Vector3(spawn.x - g.x, 0, spawn.z - g.z).normalized() * 7.0f;
			p.y = streamer_->ground(p.x, p.z) + 0.3f;
			player_->teleport(p, std::atan2(g.x - p.x, g.z - p.z));
			camera_->face_direction(g - p);
			camera_->snap();
			player_->set_input_enabled(false);
			++found;
			break;
		}
		int total = 0;
		for (const PropPlacement &pp : ar->gen->context()->props) {
			total += pp.id == id ? 1 : 0;
		}
		UtilityFunctions::print("PROP_VIEW ", parts[0], " count=", total);
	} else if (capture_view_ == "gate" || capture_view_ == "shop") {
		// gate / shop: look at the home area's travel gate or candy shop cart from 7 m.
		auto it = infest_.find(WorldStreamer::area_key(0, 0));
		if (it != infest_.end()) {
			const Vector3 g = capture_view_ == "gate" ? it->second->gate_position() : it->second->shop_position();
			Vector3 p = g + Vector3(spawn.x - g.x, 0, spawn.z - g.z).normalized() * 7.0f;
			p.y = streamer_->ground(p.x, p.z) + 0.3f;
			player_->teleport(p, std::atan2(g.x - p.x, g.z - p.z));
			camera_->face_direction(g - p);
			camera_->snap();
			player_->set_input_enabled(false);
		}
	} else if (capture_view_.begins_with("at:") || capture_view_.begins_with("atover:")) {
		// at:x,z / atover:x,z: stand at (or look down on) a world point (review aid for terrain,
		// roads, woods and towns).
		const bool over = capture_view_.begins_with("atover:");
		PackedStringArray xz = capture_view_.substr(over ? 7 : 3).split(",");
		if (xz.size() == 2) {
			const float x = xz[0].to_float(), z = xz[1].to_float();
			streamer_->update(Vector3(x, 0, z), true);
			const Vector3 p(x, streamer_->ground(x, z) + 0.3f, z);
			player_->teleport(p, home.spawn_yaw);
			camera_->set_overview(over);
			camera_->snap();
		}
	}
}

// --fx-test: frames between firing and the capture (--fire-lead=N, 7 with --fire-only, else 18).
int FestivalGame::fire_lead() const {
	for (const String &a : OS::get_singleton()->get_cmdline_user_args()) {
		if (a.begins_with("--fire-lead=")) {
			return a.substr(12).to_int();
		}
	}
	return OS::get_singleton()->get_cmdline_user_args().has("--fire-only") ? 7 : 18;
}

void FestivalGame::call_ui(const char *method, const Array &args) {
	if (hud_ && hud_->has_method(method)) {
		hud_->callv(method, args);
	}
}

void FestivalGame::call_audio(const char *method, const Array &args) {
	if (audio_ && audio_->has_method(method)) {
		audio_->callv(method, args);
	}
}

// ---------------------------------------------------------------- areas
void FestivalGame::area_loaded(Area &a) {
	const int64_t key = int64_t(a.key);
	if (CandyField *cf = a.gen->candy_field()) {
		cf->connect("candy_collected", Callable(this, "_on_candy").bind(key));
		cf->connect("house_cleared", Callable(this, "_on_house_cleared").bind(key));
		cf->connect("sequence_completed", Callable(this, "_on_sequence").bind(key));
	}
	a.extras = memnew(Node3D);
	a.extras->set_name(String(a.plan.name.c_str()) + " Infestation");
	streamer_->add_child(a.extras);
	auto inf = std::make_unique<Infestation>();
	const uint64_t k = a.key;
	AreaState *st = a.state;
	inf->on_nest_down = [this, st](int i) {
		if (i < int(st->spawner_down.size())) {
			st->spawner_down[i] = 1;
		}
		update_area_hud(true);
	};
	inf->on_vine_down = [this, st](int i) {
		if (i < int(st->vine_down.size())) {
			st->vine_down[i] = 1;
		}
		update_area_hud(true);
	};
	inf->on_heart_down = [this, st](int i) {
		if (i < int(st->heart_down.size())) {
			st->heart_down[i] = 1;
		}
		update_area_hud(true);
	};
	inf->on_freed = [this, k]() { area_freed(k); };
	inf->build(a, &mworld_, a.extras, mode_ == 0);
	if (mode_ == 0) {
		// Trick or Treat: every gate is awake, so every place visited joins the travel network.
		bool known = false;
		for (const FreedArea &f : freed_) {
			known = known || f.key == a.key;
		}
		if (!known) {
			FreedArea f;
			f.key = a.key;
			f.name = a.plan.name;
			f.gate = inf->gate_position();
			freed_.push_back(f);
		}
	}
	infest_[a.key] = std::move(inf);
	place_costume(a);
}

void FestivalGame::area_unloading(Area &a) {
	infest_.erase(a.key);
	for (size_t i = 0; i < costume_pickups_.size();) {
		if (costume_pickups_[i].key == a.key) {
			costume_pickups_[i] = costume_pickups_.back();
			costume_pickups_.pop_back();
		} else {
			++i;
		}
	}
	if (in_area_ && current_area_ == a.key) {
		in_area_ = false;
	}
}

void FestivalGame::area_freed(uint64_t key) {
	Area *a = streamer_->area(key);
	if (!a) {
		return;
	}
	a->state->freed = true;
	Infestation *inf = infest_[key].get();
	FreedArea f;
	f.key = key;
	f.name = a->plan.name;
	f.gate = inf ? inf->gate_position() : a->plan.spawn;
	freed_.push_back(f);
	call_audio("play_freed");
	Array c;
	c.push_back(String(a->plan.name.c_str()) + " is free!");
	c.push_back(3);
	call_ui("callout", c);
	Array t;
	t.push_back(String("The travel gate by the side street is awake."));
	call_ui("toast", t);
	camera_->shake(0.4f);
	for (int k = 0; k < 3; ++k) {
		burst_fx(f.gate + Vector3(k * 1.5f - 1.5f, 6.0f + k, -1.0f + k), true);
	}
	update_area_hud(true);
}

void FestivalGame::update_area_hud(bool force) {
	const Vector3 pp = player_->get_global_position();
	Area *a = streamer_->area_at(pp, 6.0f);
	std::string region;
	if (a) {
		region = a->plan.name;
	} else {
		int i, j;
		WorldPlan::region_of(pp.x, pp.z, i, j);
		const RegionPlan &r = streamer_->plan().region(i, j);
		region = r.settlement() ? "Country Road" : r.name;
	}
	const bool entered = (a && (!in_area_ || current_area_ != a->key));
	in_area_ = a != nullptr;
	if (a) {
		current_area_ = a->key;
	}
	if (!force && !entered && region == current_region_) {
		return;
	}
	const bool changed = region != current_region_;
	current_region_ = region;
	Dictionary d;
	d["name"] = String(region.c_str());
	d["settlement"] = a != nullptr;
	if (a) {
		Infestation *inf = infest_.count(a->key) ? infest_[a->key].get() : nullptr;
		d["nests_left"] = inf ? inf->nests_left() : 0;
		d["nests"] = inf ? inf->nests_total() : 0;
		d["vines_left"] = inf ? inf->vines_left() : 0;
		d["hearts_left"] = inf ? inf->hearts_left() : 0;
		d["hearts"] = inf ? inf->hearts_total() : 0;
		d["vines"] = inf ? inf->vines_total() : 0;
		d["freed"] = a->state->freed;
		// Placed candy pieces picked up here (not candy value: treats and loot aren't part of the area's
		// own candy), and the houses or shops trick-or-treated.
		d["candy"] = int(std::count(a->state->candy_taken.begin(), a->state->candy_taken.end(), 1));
		d["candy_total"] = int(a->state->candy_taken.size());
		d["houses"] = int(std::count(a->state->rung.begin(), a->state->rung.end(), 1));
		d["houses_total"] = int(a->state->rung.size());
		d["town"] = a->plan.kind == RegionKind::Town;
		d["peaceful"] = mode_ == 0;
	}
	Array args;
	args.push_back(d);
	call_ui("set_area", args);
	if (changed && !in_menu_ && cine_mode_.is_empty()) {
		Array b;
		b.push_back(String(region.c_str()));
		String sub;
		if (a && !a->state->freed) {
			sub = "Infested! Destroy the monster nests and the thorn nests (their vines die with them).";
		} else if (a) {
			sub = "Freed. The travel gate glows by the side street.";
		} else {
			sub = "";
		}
		b.push_back(sub);
		call_ui("banner", b);
	}
}

void FestivalGame::gather_targets(std::vector<Damageable *> &out) {
	for (auto &kv : infest_) {
		kv.second->gather(out);
	}
	for (Monster *m : wild_) {
		out.push_back(m);
	}
}

// ---------------------------------------------------------------- candy and houses
// Combo meter: every candy within 1.6 s of the last one extends the streak shown beside the counter.
void FestivalGame::bump_combo() {
	combo_ = combo_t_ > 0.0f ? combo_ + 1 : 1;
	combo_t_ = 1.6f;
	Array a;
	a.push_back(combo_);
	call_ui("set_combo", a);
}

void FestivalGame::on_candy(int kind, int value, int lot, Vector3 pos, int id, int64_t key) {
	since_pickup_ = 0.0f;
	bump_combo();
	wallet_ += value;
	Area *a = streamer_->area(uint64_t(key));
	if (a) {
		a->state->candy_collected += value;
		if (id >= 0 && id < int(a->state->candy_taken.size()) && !a->state->candy_taken[id]) {
			a->state->candy_taken[id] = 1;
			update_area_hud(true);
		}
		pos = a->to_world(pos);
	}
	Array k;
	k.push_back(candy_tier(kind));
	call_audio("play_pickup", k);
	player_->celebrate(kind);
	// Feedback stays proportional to the achievement (Kao 2020; Kao et al. 2024): a plain candy
	// gets its sparkle, chime and bucket bounce; only the big treats get a floating number.
	if (value >= 5) {
		popup(pos + Vector3(0, 0.6f, 0), String("+") + String::num_int64(value), candy_color(kind), value >= 25 ? 1.5f : 1.15f);
	}
	(void)lot;
}

// A whole rhythm group swept up in one flowing run: a chime and a sparkle burst (the combo meter
// already shows the streak).
void FestivalGame::on_sequence(int size, Vector3 pos, int64_t key) {
	wallet_ += size; // a clean run still pays its bonus (quietly; the combo meter shows the streak)
	bonus_ += size;
	if (Area *a = streamer_->area(uint64_t(key))) {
		pos = a->to_world(pos);
	}
	Array l;
	l.push_back(size);
	call_audio("play_combo", l);
	// The same magic sparkles as a pickup, just more of them: a wide shower and a rising plume.
	if (Vfx *v = Vfx::get()) {
		const int n = Math::min(36 + size * 4, 80);
		v->candy_sparkles(pos + Vector3(0, 0.3f, 0), Color(1.0f, 0.75f, 0.3f), n, 4.2f, 1.15f, 1.2f);
		v->candy_sparkles(pos + Vector3(0, 0.6f, 0), Color(1.0f, 0.75f, 0.3f), n / 2, 2.0f, 0.95f, 3.0f);
	} else {
		burst_fx(pos, false);
	}
}

void FestivalGame::on_house_cleared(int lot, int64_t key) {
	Area *a = streamer_->area(uint64_t(key));
	if (!a) {
		return;
	}
	a->gen->light_up_house(lot);
	if (lot >= 0 && lot < int(a->state->lot_cleared.size())) {
		a->state->lot_cleared[lot] = 1;
	}
	call_audio("play_cleared");
	PackedVector3Array bells = a->gen->get_doorbells();
	if (lot >= 0 && lot < bells.size()) {
		const Vector3 b = a->to_world(bells[lot]);
		burst_fx(b + Vector3(0, 6.5f, 0), true);
	}
	Array c;
	c.push_back(String("HOUSE CLEARED!"));
	c.push_back(2);
	call_ui("callout", c);
}

// ---------------------------------------------------------------- combat
float FestivalGame::fire_damage() const {
	static const float dmg[4] = { 1.0f, 1.6f, 2.2f, 3.0f };
	return dmg[Math::clamp(up_.fire, 0, 3)];
}

void FestivalGame::on_fired() {
	if (menu_open_) {
		return;
	}
	const bool driving = car_ && car_->driving();
	const Vector3 facing = driving ? car_->forward() : player_->facing_dir();
	Vector3 base = driving ? car_->get_global_position() + Vector3(0, 1.0f, 0) + facing * 1.4f : player_->get_global_position() + Vector3(0, 0.85f, 0) + facing * 0.45f;
	Vector3 dir = facing;
	// Auto-aim: a close monster from any direction (the kid turns to face it), else what's ahead.
	Damageable *target = cine_aim_on_ ? nullptr : combat_->pick_target(base, facing);
	if (cine_aim_on_) {
		dir = (cine_aim_ - base).normalized(); // a trailer shot aims by hand
		player_->set_facing(std::atan2(dir.x, dir.z));
	}
	if (target) {
		dir = (target->target_point() - base).normalized();
		if (!driving) {
			player_->set_facing(std::atan2(dir.x, dir.z));
		}
	}
	if (!driving) {
		// The shot leaves the wand tip, with a puff of fire there.
		base = player_->cast_from();
		if (target) {
			dir = (target->target_point() - base).normalized();
		}
		if (Vfx *vfx = Vfx::get()) {
			vfx->flames(base, 8, 0.08f, 1.0f);
			vfx->flash(base, Color(1.0f, 0.6f, 0.2f), 1.1f);
			vfx->smoke(base, Color(1.0f, 0.8f, 0.62f), 0.35f, 2);
		}
	}
	combat_->fireball(base, dir, fire_damage(), up_.triple ? 3 : 1);
	call_audio("play_fire");
}

void FestivalGame::on_pounded(Vector3 pos) {
	camera_->shake(0.8f);
	call_audio("play_pound");
	static const float radius[3] = { 3.0f, 4.2f, 5.4f };
	combat_->fire_ring(pos, radius[Math::clamp(up_.blast, 0, 2)], 2.0f + float(up_.blast));
	call_audio("play_blast");
}

void FestivalGame::on_spun() {
	// The tail spin swats anything right next to the kid.
	std::vector<Damageable *> targets;
	gather_targets(targets);
	const Vector3 p = player_->get_global_position() + Vector3(0, 0.6f, 0);
	for (Damageable *t : targets) {
		if (t->target_alive() && t->target_point().distance_to(p) < 1.6f + t->target_radius()) {
			t->take_hit(1.0f, p);
		}
	}
}

void FestivalGame::hurt_player(const Vector3 &from) {
	if (invuln_ > 0.0f || menu_open_ || teleport_t_ >= 0.0f || cine_i_ >= 0) {
		return;
	}
	invuln_ = 1.5f;
	static const float lid[3] = { 1.0f, 0.6f, 0.35f };
	int lost = Math::min(wallet_, Math::max(5, int(std::round(float(wallet_) * 0.15f))));
	lost = Math::min(60, int(std::ceil(float(lost) * lid[Math::clamp(up_.lid, 0, 2)])));
	const bool driving = car_ && car_->driving();
	if (driving) {
		lost /= 2; // the car takes some of the bump
	}
	wallet_ -= lost;
	const Vector3 pp = player_->get_global_position();
	if (lost > 0) {
		loose_->spill(pp, lost, true);
		popup(pp + Vector3(0, 1.6f, 0), String("-") + String::num_int64(lost), Color(1.0f, 0.45f, 0.45f), 1.3f, 1.0f);
		call_audio("play_spill");
	}
	if (!driving) {
		player_->knockback(from, 6.5f);
	}
	camera_->shake(0.5f);
	call_audio("play_hurt");
	call_ui("hurt");
	Array w;
	w.push_back(wallet_);
	call_ui("set_candy", w);
	++bot_hits_;
}

void FestivalGame::monster_event(const char *event, const Vector3 &pos) {
	const String e(event);
	if (e == "telegraph") {
		call_audio("play_telegraph");
	} else if (e == "monster_hit" || e == "nest_hit" || e == "vine_hit") {
		// The fire impact sound covers these.
	} else if (e == "monster_die") {
		call_audio("play_monster_pop");
		++bot_kills_;
	} else if (e == "nest_break") {
		call_audio("play_nest_break");
		camera_->shake(0.35f);
		popup(pos + Vector3(0, 1.5f, 0), "Nest destroyed!", Color(1.0f, 0.8f, 0.4f), 1.3f, 1.4f);
	} else if (e == "heart_burn") {
		call_audio("play_nest_break");
		camera_->shake(0.45f);
		popup(pos + Vector3(0, 2.5f, 0), "Thorn nest burned!", Color(0.85f, 0.55f, 1.0f), 1.4f, 1.6f);
	} else if (e == "vine_burn") {
		call_audio("play_vine_burn");
		popup(pos + Vector3(0, 1.8f, 0), "Burned away!", Color(1.0f, 0.6f, 0.3f), 1.2f, 1.2f);
	} else if (e == "throw") {
		call_audio("play_throw");
	} else if (e == "puff") {
		call_audio("play_puff");
	} else if (e == "spawn") {
		call_audio("play_spawn");
	}
}

// ---------------------------------------------------------------- main menu and pause
// The main menu sits over the live world (a peaceful night, the camera slowly circling the kid).
void FestivalGame::show_main_menu() {
	in_menu_ = true;
	menu_open_ = true;
	player_->set_input_enabled(false);
	camera_->set_attract(true);
	call_ui("show_main_menu");
}

void FestivalGame::on_start_mode(int mode) {
	mode = Math::clamp(mode, 0, 1);
	in_menu_ = false;
	menu_open_ = false;
	menu_closed_frame_ = Engine::get_singleton()->get_process_frames();
	camera_->set_attract(false);
	// The menu's backdrop is already a fresh Trick or Treat night; anything else needs a reload.
	if (mode != mode_ || started_) {
		mode_ = mode;
		start_world(started_ ? int64_t(Time::get_singleton()->get_ticks_usec() % 1000000) + 1 : seed_);
	}
	started_ = true;
	player_->set_input_enabled(true);
	Array b;
	b.push_back(String(mode_ == 0 ? "Trick or Treat!" : "Save the Town!"));
	b.push_back(String(mode_ == 0 ? "Ring every doorbell and fill your bucket." : "Monsters have overrun the neighborhood. Destroy their nests!"));
	call_ui("banner", b);
}

void FestivalGame::on_pause_choice(String choice) {
	get_tree()->set_pause(false);
	menu_open_ = false;
	menu_closed_frame_ = Engine::get_singleton()->get_process_frames();
	player_->set_input_enabled(true);
	if (choice == "new_night") {
		start_world(int64_t(Time::get_singleton()->get_ticks_usec() % 1000000) + 1);
	} else if (choice == "main_menu") {
		// Leaving the game (it was autosaved first): the next one starts from nothing.
		reset_progress();
		mode_ = 0;
		started_ = false;
		in_menu_ = true;
		start_world(int64_t(Time::get_singleton()->get_ticks_usec() % 1000000) + 1);
		show_main_menu();
	} else if (choice == "quit") {
		get_tree()->quit();
	}
}

void FestivalGame::reset_progress() {
	wallet_ = 0;
	up_ = Upgrades();
	paint_owned_.assign(RideOnCar::scheme_count(), 0);
	paint_owned_[0] = 1;
	car_paint_ = 0;
	costume_found_.assign(COSTUME_COUNT, 0);
	costume_found_[0] = 1;
	if (costume_ != 0) {
		costume_ = 0;
		player_->set_visual(gen_->instantiate_asset(COSTUMES[0].id));
	}
	told_shop_ = false;
}

// ---------------------------------------------------------------- saved games
namespace {
PackedByteArray flags_to_bytes(const std::vector<char> &v) {
	PackedByteArray b;
	b.resize(int64_t(v.size()));
	for (size_t i = 0; i < v.size(); ++i) {
		b.set(int64_t(i), uint8_t(v[i]));
	}
	return b;
}

// Saved flags into `out` (keeping its size; flag 0 stays set when `keep_first`).
void bytes_to_flags(const Variant &v, std::vector<char> &out, bool keep_first) {
	const PackedByteArray b = v;
	for (size_t i = 0; i < out.size(); ++i) {
		out[i] = int64_t(i) < b.size() ? char(b[int64_t(i)] != 0) : 0;
	}
	if (keep_first && !out.empty()) {
		out[0] = 1;
	}
}

constexpr int SAVE_VERSION = 1;
} // namespace

Dictionary FestivalGame::save_state() {
	Dictionary d;
	if (!streamer_) {
		return d;
	}
	d["version"] = SAVE_VERSION;
	d["seed"] = seed_;
	d["mode"] = mode_;
	d["wallet"] = wallet_;
	d["told_shop"] = told_shop_;
	Dictionary u;
	u["fire"] = up_.fire;
	u["quick"] = up_.quick;
	u["triple"] = up_.triple;
	u["blast"] = up_.blast;
	u["lid"] = up_.lid;
	u["magnet"] = up_.magnet;
	u["car"] = up_.car;
	d["upgrades"] = u;
	d["car_paint"] = car_paint_;
	d["paints"] = flags_to_bytes(paint_owned_);
	d["costume"] = costume_;
	d["costumes"] = flags_to_bytes(costume_found_);
	const bool driving = car_ && car_->driving();
	d["position"] = driving ? car_->get_global_position() : player_->get_global_position();
	const Vector3 f = player_->facing_dir();
	d["yaw"] = std::atan2(f.x, f.z);
	Array freed;
	for (const FreedArea &fa : freed_) {
		Dictionary e;
		e["key_hi"] = int64_t(fa.key >> 32);
		e["key_lo"] = int64_t(fa.key & 0xFFFFFFFFu);
		e["name"] = String(fa.name.c_str());
		e["gate"] = fa.gate;
		freed.push_back(e);
	}
	d["freed"] = freed;
	const Dictionary areas = streamer_->export_states();
	d["areas"] = areas;
	// A summary for the save list.
	int houses = 0, areas_freed = 0, costumes = 0;
	const Array vals = areas.values();
	for (int64_t i = 0; i < vals.size(); ++i) {
		const Dictionary s = vals[i];
		const PackedByteArray rung = s.get("rung", PackedByteArray());
		for (int64_t k = 0; k < rung.size(); ++k) {
			houses += rung[k] ? 1 : 0;
		}
		areas_freed += bool(s.get("freed", false)) ? 1 : 0;
	}
	for (char c : costume_found_) {
		costumes += c ? 1 : 0;
	}
	d["area_name"] = String(current_region_.c_str());
	d["houses"] = houses;
	d["areas_freed"] = areas_freed;
	d["costumes_found"] = costumes;
	return d;
}

bool FestivalGame::load_state(const Dictionary &d) {
	if (!streamer_ || int(d.get("version", 0)) != SAVE_VERSION || !d.has("seed")) {
		return false;
	}
	get_tree()->set_pause(false);
	mode_ = Math::clamp(int(d.get("mode", 1)), 0, 1);
	wallet_ = Math::max(0, int(d.get("wallet", 0)));
	told_shop_ = bool(d.get("told_shop", false));
	const Dictionary u = d.get("upgrades", Dictionary());
	up_ = Upgrades();
	up_.fire = int(u.get("fire", 0));
	up_.quick = int(u.get("quick", 0));
	up_.triple = int(u.get("triple", 0));
	up_.blast = int(u.get("blast", 0));
	up_.lid = int(u.get("lid", 0));
	up_.magnet = int(u.get("magnet", 0));
	up_.car = int(u.get("car", 0));
	paint_owned_.assign(RideOnCar::scheme_count(), 0);
	bytes_to_flags(d.get("paints", PackedByteArray()), paint_owned_, true);
	car_paint_ = Math::clamp(int(d.get("car_paint", 0)), 0, RideOnCar::scheme_count() - 1);
	if (!paint_owned_[car_paint_]) {
		car_paint_ = 0;
	}
	costume_found_.assign(COSTUME_COUNT, 0);
	bytes_to_flags(d.get("costumes", PackedByteArray()), costume_found_, true);
	costume_ = Math::clamp(int(d.get("costume", 0)), 0, COSTUME_COUNT - 1);
	if (!costume_found_[costume_]) {
		costume_ = 0;
	}

	pending_states_ = d.get("areas", Dictionary());
	in_menu_ = false;
	started_ = true;
	camera_->set_attract(false);
	start_world(int64_t(d["seed"]));
	player_->set_visual(gen_->instantiate_asset(COSTUMES[costume_].id));

	// The travel network as it was (Trick or Treat already re-added the areas around the spawn).
	const Array freed = d.get("freed", Array());
	for (int64_t i = 0; i < freed.size(); ++i) {
		const Dictionary e = freed[i];
		FreedArea f;
		f.key = (uint64_t(int64_t(e.get("key_hi", 0))) << 32) | uint64_t(int64_t(e.get("key_lo", 0)));
		f.name = String(e.get("name", "")).utf8().get_data();
		f.gate = e.get("gate", Vector3());
		bool known = false;
		for (const FreedArea &g : freed_) {
			known = known || g.key == f.key;
		}
		if (!known) {
			freed_.push_back(f);
		}
	}

	// Back where the kid was standing.
	const Vector3 p = d.get("position", Vector3());
	if (d.has("position") && p.is_finite()) {
		streamer_->update(p, true);
		const float ground = streamer_->ground(p.x, p.z);
		player_->teleport(Vector3(p.x, Math::max(p.y, ground) + 0.2f, p.z), float(d.get("yaw", 0.0)));
		camera_->set_heading_sign(1.0f);
		camera_->face_direction(player_->facing_dir());
		camera_->snap();
	}
	player_->set_input_enabled(true);
	menu_open_ = false;
	menu_closed_frame_ = Engine::get_singleton()->get_process_frames();
	Array w;
	w.push_back(wallet_);
	call_ui("set_candy", w);
	current_region_.clear();
	update_area_hud(true);
	return true;
}

// ---------------------------------------------------------------- wild monsters
// Out in the woods (away from settlements) a few monsters wander about with no nest: they appear
// out of sight 18-28 m away, keep to roughly where they appeared, and fade away once the player
// has moved on. Defeating them still pays out like a pinata.
void FestivalGame::clear_wild() {
	for (Monster *m : wild_) {
		m->on_gone = nullptr;
		m->queue_free();
	}
	wild_.clear();
}

void FestivalGame::update_wild(float dt) {
	wild_t_ -= dt;
	if (wild_t_ > 0.0f || !wild_root_) {
		return;
	}
	wild_t_ = 1.5f;
	if (mode_ == 0) {
		return; // Trick or Treat: no monsters anywhere
	}
	const Vector3 pp = player_->get_global_position();
	for (Monster *m : std::vector<Monster *>(wild_)) {
		if (m->target_alive() && m->get_global_position().distance_to(pp) > 55.0f) {
			m->vanish();
		}
	}
	if (menu_open_ || teleport_t_ >= 0.0f || in_area_) {
		return;
	}
	WorldPlan &plan = streamer_->plan();
	const float woods = plan.forest_density(pp.x, pp.z);
	if (woods < 0.35f || plan.settlement_mask(pp.x, pp.z) > 0.2f) {
		return;
	}
	int alive = 0;
	for (Monster *m : wild_) {
		alive += m->target_alive() ? 1 : 0;
	}
	const int cap = woods > 0.6f ? 4 : 3;
	if (alive >= cap || !wild_rng_.chance(0.5f)) {
		return;
	}
	for (int tries = 0; tries < 6; ++tries) {
		const float a = wild_rng_.range(0.0f, Math_TAU), r = wild_rng_.range(18.0f, 28.0f);
		Vector3 p = pp + Vector3(std::cos(a) * r, 0, std::sin(a) * r);
		if (plan.forest_density(p.x, p.z) < 0.25f || plan.settlement_mask(p.x, p.z) > 0.1f) {
			continue;
		}
		const MonsterType type = MonsterType(wild_rng_.irange(0, int(MonsterType::Count) - 1));
		p.y = streamer_->ground(p.x, p.z) + (tuning(type).flying ? 1.3f : 0.3f);
		Monster *m = memnew(Monster);
		wild_root_->add_child(m);
		m->set_global_position(p);
		m->init(type, &mworld_, nullptr, wild_rng_.next(), Rect2(p.x - 25.0f, p.z - 25.0f, 50.0f, 50.0f));
		m->on_gone = [this](Monster *gone) {
			wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end());
		};
		wild_.push_back(m);
		break;
	}
}

// ---------------------------------------------------------------- car, shop, travel
void FestivalGame::toggle_car() {
	if (!up_.car) {
		Array t;
		t.push_back(String("The Pumpkin Cruiser is in the shop ({shop})."));
		call_ui("toast", t);
		return;
	}
	if (!car_) {
		car_ = memnew(RideOnCar);
		car_->set_name("PumpkinCruiser");
		add_child(car_);
		car_->build(gen_->get_toon_material());
		car_->set_scheme(car_paint_);
	}
	if (car_->driving()) {
		car_->set_driving(false);
		player_->set_mounted(false);
		const Vector3 side = Vector3(-car_->forward().z, 0, car_->forward().x) * 1.7f;
		Vector3 p = car_->get_global_position() + side;
		p.y = Math::max(p.y, streamer_->ground(p.x, p.z)) + 0.4f;
		player_->teleport(p, car_->yaw());
		call_audio("play_car_off");
		return;
	}
	const Vector3 pp = player_->get_global_position();
	if (car_->get_global_position().distance_to(pp) > 4.0f) {
		// Poof: it appears right here.
		car_->place(pp + Vector3(0, 0.3f, 0), std::atan2(player_->facing_dir().x, player_->facing_dir().z));
		combat_->puff(pp + Vector3(0, 0.8f, 0), Color(0.8f, 0.6f, 1.0f), 30, 4.0f);
		call_audio("play_car_summon");
	}
	car_->set_driving(true);
	player_->set_mounted(true);
	call_audio("play_car_on");
}

void FestivalGame::open_shop() {
	// Each item: id, name, status (right-hand text), desc, enabled; plus an optional 3D preview
	// model per item that the HUD turns in a little stage (car paint and costumes).
	Array items, previews;
	float stage = 0.0f; // preview camera distance (0: no preview stage)
	auto item = [&](const String &id, const String &name, const String &status, const String &desc, bool enabled, Node3D *preview) {
		Dictionary d;
		d["id"] = id;
		d["name"] = name;
		d["status"] = status;
		d["desc"] = desc;
		d["enabled"] = enabled;
		items.push_back(d);
		if (preview) {
			set_instance_param_recursive(preview, "keep_visible", 1.0f); // never cut away (toon occluder dither)
		}
		previews.push_back(preview);
	};
	if (shop_tab_ == 1) {
		stage = 4.4f;
		for (int k = 0; k < RideOnCar::scheme_count(); ++k) {
			const bool owned = paint_owned_[k], equipped = car_paint_ == k;
			const int cost = RideOnCar::scheme_cost(k);
			const String status = equipped ? String("on the car") : owned ? String("owned") : vformat("%d candy", cost);
			const bool enabled = up_.car && !equipped && (owned || wallet_ >= cost);
			item(vformat("paint:%d", k), RideOnCar::scheme_name(k), status, up_.car ? String() : String("Buy the Pumpkin Cruiser first."), enabled,
					RideOnCar::make_preview(k, gen_->get_toon_material()));
		}
	} else if (shop_tab_ == 2) {
		stage = 2.7f;
		for (int k = 0; k < COSTUME_COUNT; ++k) {
			if (!gen_->asset_published(COSTUMES[k].id)) {
				continue;
			}
			const bool found = costume_found_[k], worn = costume_ == k;
			item(vformat("costume:%d", k), found ? String(COSTUMES[k].name) : String("???"), worn ? String("wearing") : found ? String("wear") : String("not found yet"),
					found ? String() : String(COSTUME_HINT), found && !worn, found ? gen_->instantiate_asset(COSTUMES[k].id) : nullptr);
		}
	} else {
		for (const UpgradeDef &u : UPGRADES) {
			const int lvl = *up_.level(u.id);
			const bool done = lvl >= u.max;
			const String steps = u.max > 1 ? vformat("Lv %d/%d   ", lvl, u.max) : String();
			const String status = done ? steps + String("(done)") : steps + vformat("%d candy", u.cost[lvl]);
			item(u.id, u.name, status, u.desc, !done && wallet_ >= u.cost[lvl], nullptr);
		}
	}
	Array tabs;
	tabs.push_back("Upgrades");
	tabs.push_back("Car Paint");
	tabs.push_back("Costumes");
	Array a;
	a.push_back(items);
	a.push_back(wallet_);
	a.push_back(shop_tab_);
	a.push_back(tabs);
	a.push_back(previews);
	a.push_back(stage);
	a.push_back(shop_focus_);
	call_ui("open_shop", a);
	menu_open_ = true;
	player_->set_input_enabled(false);
	call_audio("play_menu");
}

// Hidden costumes: some settlements (never the home one) hide a costume the player hasn't found
// yet, floating high above the peak of a house's rooftop climb, a jump up from the very top.
void FestivalGame::place_costume(Area &a) {
	if (a.key == WorldStreamer::area_key(0, 0) || !a.gen || !a.gen->context() || !a.extras) {
		return;
	}
	std::vector<int> missing;
	for (int k = 1; k < COSTUME_COUNT; ++k) {
		if (!costume_found_[k] && gen_->asset_published(COSTUMES[k].id)) {
			missing.push_back(k);
		}
	}
	if (missing.empty()) {
		return;
	}
	Rng rng(a.key ^ (uint64_t(seed_) * 0x9E3779B97F4A7C15ull), 77);
	if (!rng.chance(0.4f)) {
		return;
	}
	const int k = missing[rng.irange(0, int(missing.size()) - 1)];
	std::vector<Vector3> peaks;
	for (const Lot &lot : a.gen->context()->lots) {
		if (!lot.summit_route.empty()) {
			peaks.push_back(lot.to_world(lot.summit_route.back()));
		}
	}
	if (peaks.empty()) {
		return;
	}
	CostumePickup c;
	c.costume = k;
	c.key = a.key;
	c.pos = a.to_world(peaks[rng.irange(0, int(peaks.size()) - 1)] + Vector3(0, 2.3f, 0));
	c.node = gen_->instantiate_asset(COSTUMES[k].id);
	set_instance_param_recursive(c.node, "keep_visible", 1.0f);
	set_instance_param_recursive(c.node, "rim_strength", 0.45f); // a magic sheen so it reads as special
	OmniLight3D *glow = memnew(OmniLight3D);
	glow->set_color(Color(0.8f, 0.6f, 1.0f));
	glow->set_param(Light3D::PARAM_RANGE, 3.5f);
	glow->set_param(Light3D::PARAM_ENERGY, 1.4f);
	glow->set_position(Vector3(0, 0.6f, 0.6f));
	c.node->add_child(glow);
	a.extras->add_child(c.node);
	c.node->set_global_position(c.pos);
	costume_pickups_.push_back(c);
}

void FestivalGame::update_costume_pickups(float dt) {
	costume_t_ += dt;
	const bool twinkle = int(costume_t_ * 3.0f) != int((costume_t_ - dt) * 3.0f);
	const Vector3 pp = player_->get_global_position() + Vector3(0, 0.6f, 0);
	int found = -1;
	for (CostumePickup &c : costume_pickups_) {
		// Turning slowly on the spot, bobbing, twinkling.
		const Vector3 at = c.pos + Vector3(0, std::sin(costume_t_ * 2.0f) * 0.12f, 0);
		c.node->set_global_transform(Transform3D(Basis(Vector3(0, 1, 0), costume_t_ * 1.6f).scaled(Vector3(0.8f, 0.8f, 0.8f)), at));
		if (twinkle) {
			if (Vfx *v = Vfx::get()) {
				v->candy_sparkles(at + Vector3(0, 0.5f, 0), Color(0.8f, 0.6f, 1.0f), 6, 1.5f, 0.9f, 1.0f);
			}
		}
		if (found < 0 && pp.distance_to(at + Vector3(0, 0.45f, 0)) < 1.3f) {
			found = c.costume;
		}
	}
	if (found < 0) {
		return;
	}
	costume_found_[found] = 1;
	for (size_t i = 0; i < costume_pickups_.size();) {
		CostumePickup &c = costume_pickups_[i];
		if (c.costume == found) {
			if (Vfx *v = Vfx::get()) {
				v->candy_sparkles(c.pos + Vector3(0, 0.5f, 0), Color(0.8f, 0.6f, 1.0f), 70, 4.5f, 1.2f, 1.2f);
			}
			c.node->queue_free();
			costume_pickups_[i] = costume_pickups_.back();
			costume_pickups_.pop_back();
		} else {
			++i;
		}
	}
	call_audio("play_purchase");
	Array co;
	co.push_back(String("COSTUME FOUND!"));
	co.push_back(3);
	call_ui("callout", co);
	Array t;
	t.push_back(vformat("You found the %s costume! Wear it from the shop's Costumes tab ({shop}).", COSTUMES[found].name));
	call_ui("toast", t);
}

void FestivalGame::on_shop_tab(int dir) {
	shop_tab_ = (shop_tab_ + dir + 3) % 3;
	shop_focus_ = String();
	open_shop();
}

void FestivalGame::on_purchase(String id) {
	shop_focus_ = id;
	if (id.begins_with("paint:")) {
		const int k = id.substr(6).to_int();
		if (!up_.car || k < 0 || k >= RideOnCar::scheme_count()) {
			call_audio("play_denied");
			return;
		}
		if (!paint_owned_[k]) {
			if (wallet_ < RideOnCar::scheme_cost(k)) {
				call_audio("play_denied");
				return;
			}
			wallet_ -= RideOnCar::scheme_cost(k);
			paint_owned_[k] = 1;
			Array w;
			w.push_back(wallet_);
			call_ui("set_candy", w);
		}
		car_paint_ = k;
		if (car_) {
			car_->set_scheme(k);
		}
		call_audio("play_purchase");
		open_shop();
		return;
	}
	if (id.begins_with("costume:")) {
		const int k = id.substr(8).to_int();
		if (k < 0 || k >= COSTUME_COUNT || !costume_found_[k]) {
			call_audio("play_denied");
			return;
		}
		costume_ = k;
		player_->set_visual(gen_->instantiate_asset(COSTUMES[k].id));
		call_audio("play_purchase");
		open_shop();
		return;
	}
	for (const UpgradeDef &u : UPGRADES) {
		if (id != String(u.id)) {
			continue;
		}
		int *lvl = up_.level(id);
		if (!lvl || *lvl >= u.max || wallet_ < u.cost[*lvl]) {
			call_audio("play_denied");
			return;
		}
		wallet_ -= u.cost[*lvl];
		*lvl += 1;
		static const float cd[3] = { 0.45f, 0.32f, 0.22f };
		player_->set_fire_cooldown(cd[Math::clamp(up_.quick, 0, 2)]);
		call_audio("play_purchase");
		Array w;
		w.push_back(wallet_);
		call_ui("set_candy", w);
		open_shop(); // refresh
		return;
	}
}

void FestivalGame::on_menu_closed() {
	menu_open_ = false;
	player_->set_input_enabled(true);
	get_tree()->set_pause(false);
	menu_closed_frame_ = Engine::get_singleton()->get_process_frames();
}

void FestivalGame::on_travel(int index) {
	if (index < 0 || index >= int(travel_keys_.size())) {
		return;
	}
	for (const FreedArea &f : freed_) {
		if (f.key == travel_keys_[index]) {
			begin_teleport(f.gate);
			return;
		}
	}
}

void FestivalGame::begin_teleport(const Vector3 &to) {
	teleport_to_ = to;
	teleport_t_ = 0.0f;
	Array a;
	a.push_back(true);
	call_ui("fade", a);
	call_audio("play_teleport");
	menu_open_ = true;
	player_->set_input_enabled(false);
}

Vector3 FestivalGame::player_point() const {
	return player_->get_global_position() + Vector3(0, 0.55f, 0);
}

// ---------------------------------------------------------------- small signals
void FestivalGame::on_stepped(int parity) {
	Array a;
	a.push_back(parity);
	call_audio("play_step", a);
}

void FestivalGame::on_landed(float impact) {
	if (impact > 6.0f) {
		Array a;
		a.push_back(impact);
		call_audio("play_land", a);
	}
}

void FestivalGame::on_jumped() {
	call_audio("play_jump");
}

void FestivalGame::on_flapped() {
	call_audio("play_flap");
}

void FestivalGame::on_bounced() {
	call_audio("play_bounce");
	if (Area *a = streamer_->area_at(player_->get_global_position(), 2.0f)) {
		a->gen->squash_near(a->to_local(player_->get_global_position()));
	}
}

void FestivalGame::on_lightning(float strength) {
	Array a;
	a.push_back(strength);
	call_audio("play_thunder", a);
}

// ---------------------------------------------------------------- juice
void FestivalGame::popup(const Vector3 &pos, const String &text, const Color &color, float size, float life) {
	if (popups_.empty()) {
		for (int i = 0; i < 18; ++i) {
			Popup p;
			p.node = memnew(Label3D);
			p.node->set_font(display_font());
			p.node->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			p.node->set_font_size(72);
			p.node->set_outline_size(18);
			p.node->set_pixel_size(0.0045f);
			p.node->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
			p.node->set_visible(false);
			add_child(p.node);
			popups_.push_back(p);
		}
	}
	Popup &p = popups_[next_popup_];
	next_popup_ = (next_popup_ + 1) % int(popups_.size());
	p.node->set_text(text);
	p.from = pos;
	p.t = 0.0f;
	p.size = size;
	p.life = life;
	p.color = color;
	p.node->set_visible(true);
}

void FestivalGame::update_popups(float dt) {
	for (Popup &p : popups_) {
		if (p.t >= 1.0f) {
			continue;
		}
		p.t = Math::min(1.0f, p.t + dt / p.life);
		const float t = p.t;
		const float rise = 1.0f - (1.0f - t) * (1.0f - t);
		const float pop = t < 0.15f ? Math::lerp(0.2f, 1.35f, t / 0.15f) : Math::lerp(1.35f, 1.0f, Math::min((t - 0.15f) / 0.2f, 1.0f));
		const float alpha = t > 0.65f ? (1.0f - t) / 0.35f : 1.0f;
		p.node->set_global_position(p.from + Vector3(0, rise * 1.1f, 0));
		const float s = pop * p.size;
		p.node->set_scale(Vector3(s, s, s));
		Color c = p.color;
		c.a = alpha;
		p.node->set_modulate(c);
		p.node->set_outline_modulate(Color(0.15f, 0.05f, 0.25f, alpha));
		if (p.t >= 1.0f) {
			p.node->set_visible(false);
		}
	}
}

void FestivalGame::burst_fx(const Vector3 &pos, bool firework) {
	if (Vfx *v = Vfx::get()) {
		v->confetti(pos, firework);
		return;
	}
	if (fx_.empty()) {
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
		mat->set_albedo(Color(0.95f, 0.95f, 0.95f)); // true colours; only a touch of bloom
		Ref<QuadMesh> quad;
		quad.instantiate();
		quad->set_size(Vector2(0.11f, 0.11f));
		quad->set_material(mat);
		Ref<Gradient> rainbow;
		rainbow.instantiate();
		rainbow->set_offsets(PackedFloat32Array({ 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f }));
		rainbow->set_colors(PackedColorArray({ Color(1, 0.45f, 0.75f), Color(1, 0.6f, 0.2f), Color(1, 0.92f, 0.35f), Color(0.45f, 1, 0.75f),
				Color(0.55f, 0.75f, 1), Color(0.8f, 0.5f, 1) }));
		for (int i = 0; i < 6; ++i) {
			CPUParticles3D *p = memnew(CPUParticles3D);
			p->set_emitting(false);
			p->set_one_shot(true);
			p->set_explosiveness_ratio(1.0f);
			p->set_mesh(quad);
			p->set_use_local_coordinates(false);
			p->set_color_initial_ramp(rainbow);
			p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
			p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
			p->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -360.0f);
			p->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 360.0f);
			p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			add_child(p);
			fx_.push_back(p);
		}
	}
	CPUParticles3D *p = fx_[next_fx_];
	next_fx_ = (next_fx_ + 1) % int(fx_.size());
	p->set_global_position(pos);
	if (firework) {
		p->set_amount(110);
		p->set_lifetime(1.8);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(180.0f);
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 5.0f);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 8.0f);
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, 2.5f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, 3.5f);
		p->set_gravity(Vector3(0, -2.5f, 0));
	} else {
		p->set_amount(48);
		p->set_lifetime(1.3);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(55.0f);
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 3.0f);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 5.5f);
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, 1.0f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, 2.0f);
		p->set_gravity(Vector3(0, -7.0f, 0));
	}
	p->restart();
	p->set_emitting(true);
}

void FestivalGame::update_wisps(float dt) {
	for (size_t i = 0; i < wisps_.size();) {
		Wisp &w = wisps_[i];
		w.t += dt / 2.2f;
		if (w.t >= 1.0f) {
			w.node->queue_free();
			wisps_[i] = wisps_.back();
			wisps_.pop_back();
			continue;
		}
		const float t = w.t;
		const Vector3 p = w.from.lerp(w.to, t * 0.85f) + Vector3(std::sin(t * 9.0f) * 0.3f, 0.6f + std::sin(t * Math_PI) * 1.2f, std::cos(t * 7.0f) * 0.3f);
		w.node->set_global_position(p);
		const float s = t < 0.2f ? t / 0.2f : (t > 0.8f ? (1.0f - t) / 0.2f : 1.0f);
		w.node->set_scale(Vector3(s, s, s));
		++i;
	}
}

// ---------------------------------------------------------------- frame
void FestivalGame::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint() || !gen_ || !player_) {
		return;
	}
	const float dt = float(delta);
	Input *in = Input::get_singleton();
	++frame_;
	if (preview_root_) {
		step_preview();
		return;
	}
	if (!streamer_) {
		return;
	}
	if (fx_test_ && frame_ == capture_frames_ - fire_lead()) {
		// Review aid: fire the feedback effects right before a capture.
		const Vector3 p = player_->get_global_position();
		if (Camera3D *cam = get_viewport()->get_camera_3d()) {
			// Fire across the view (to the right of the camera) so the trail is seen side-on.
			const Vector3 r = cam->get_global_transform().basis.get_column(0);
			player_->set_facing(std::atan2(r.x, r.z));
		}
		on_fired();
		if (OS::get_singleton()->get_cmdline_user_args().has("--fire-only")) {
			return; // just the fireball and its trail
		}
		on_sequence(3, p + Vector3(1.0f, 1.0f, 0), -1);
		popup(p + Vector3(2.5f, 2.6f, -2.0f), "Trick or treat!", Color(1.0f, 0.95f, 0.85f), 1.4f, 2.2f);
		loose_->spill(p + Vector3(-1.5f, 0, 0), 30, true);
		combat_->fire_ring(p, 3.0f, 0.0f);
		if (Vfx *v = Vfx::get()) {
			for (int tier = 0; tier < 4; ++tier) {
				v->candy_pickup(p + Vector3(-3.0f + tier * 2.0f, 1.2f, -3.0f), candy_color(candy_kind_of_tier(tier, 0)), tier);
			}
			v->smoke(p + Vector3(3.5f, 0.5f, 0), Color(0.9f, 0.85f, 1.0f), 1.5f, 6);
		}
	}
	if (fly_test_ > 0.0f) {
		// Streaming stress test: glide east along the road at driving speed, logging the worst
		// frame each second.
		if (fly_t_ == 0.0f) {
			player_->set_mounted(true);
		}
		fly_t_ += dt;
		fly_worst_ = Math::max(fly_worst_, dt);
		Vector3 p = player_->get_global_position();
		p.x += 15.0f * dt;
		const float d = streamer_->plan().road_distance(p.x, p.z);
		(void)d;
		p.y = streamer_->ground(p.x, p.z) + 0.5f;
		player_->set_global_position(p);
		if (int(fly_t_) != int(fly_t_ - dt)) {
			UtilityFunctions::print("FLY t=", int(fly_t_), " x=", int(p.x), " worst_ms=", int(fly_worst_ * 1000.0f), " chunks=", streamer_->chunk_count(),
					" areas=", int(streamer_->areas().size()), " fps=", Engine::get_singleton()->get_frames_per_second());
			fly_worst_ = 0.0f;
		}
		if (fly_t_ > fly_test_) {
			get_tree()->quit();
		}
	}
	if (review_free_ && frame_ == 40) {
		// Review aid: burn every nest and vine in the home area, then step onto its gate.
		std::vector<Damageable *> targets;
		infest_[WorldStreamer::area_key(0, 0)]->gather(targets);
		for (Damageable *t : targets) {
			if (!t->is_monster()) {
				t->take_hit(1e6f, t->target_point());
			}
		}
	}
	if (review_travel_ && frame_ == 100) {
		// Pretend the next settlement east is freed too, and travel there through the gate menu.
		for (int i = 1; i < 6; ++i) {
			const RegionPlan &r = streamer_->plan().region(i, 0);
			if (r.settlement()) {
				FreedArea f;
				f.key = WorldStreamer::area_key(i, 0);
				f.name = r.name;
				f.gate = r.spawn;
				freed_.push_back(f);
				break;
			}
		}
		travel_keys_ = { freed_.back().key };
		on_travel(0);
	}
	if (review_free_ && !review_travel_ && frame_ == 120) {
		const Vector3 g = infest_[WorldStreamer::area_key(0, 0)]->gate_position();
		player_->teleport(g + Vector3(0, 0.5f, 0), 0.0f);
		camera_->snap();
	}
	if (review_accept_ && (frame_ == 170 || frame_ == 173)) {
		// Review aid: press and release the gamepad A button while a menu is open.
		Ref<InputEventJoypadButton> ev;
		ev.instantiate();
		ev->set_button_index(JOY_BUTTON_A);
		ev->set_pressed(frame_ == 170);
		Input::get_singleton()->parse_input_event(ev);
	}
	if (review_accept_ && frame_ == 190) {
		UtilityFunctions::print("ACCEPT_TEST menu_open=", menu_open_);
	}
	if (ledge_lot_ >= 0 && frame_ >= 60) {
		// Review aid: run and jump at the edge of a roof or deck just out of reach in the given lot,
		// expect to catch the ledge and hang (screenshot), then climb up onto it.
		Area *a = streamer_->area(WorldStreamer::area_key(0, 0));
		if (frame_ == 60) {
			const GenContext *ctx = a->gen->context();
			const int lot = ledge_lot_ % int(ctx->lots.size());
			float best = 1e9f;
			for (const OBox &b : ctx->boxes) {
				const float top = b.top();
				if (b.lot != lot || !b.walkable || top < 2.5f || top > 3.3f) {
					continue;
				}
				for (int axis : { 0, 2 }) {
					for (float sgn : { -1.0f, 1.0f }) {
						Vector3 nrm = b.xf.basis.get_column(axis).normalized() * sgn;
						nrm.y = 0.0f;
						if (nrm.length() < 0.9f) {
							continue;
						}
						const Vector3 face = a->to_world(b.xf.origin + nrm * b.half[axis]);
						Vector3 start = face + nrm * 2.8f;
						const float score = start.distance_to(ctx->spawn + a->plan.origin);
						if (score < best) {
							best = score;
							start.y = streamer_->ground(start.x, start.z) + 0.1f;
							ledge_target_ = start;
							ledge_dir_ = -nrm;
							ledge_top_ = top + a->plan.origin.y;
						}
					}
				}
			}
			if (best > 1e8f) {
				UtilityFunctions::print("LEDGE_TEST no roof edge in lot ", lot);
				get_tree()->quit();
				return;
			}
			player_->teleport(ledge_target_, std::atan2(ledge_dir_.x, ledge_dir_.z));
			camera_->face_direction(ledge_dir_);
			camera_->snap();
			player_->set_bot(true);
			player_->set_bot_ledges(true);
			invuln_ = 100.0f;
		}
		if (ledge_frame_ < 0) {
			player_->bot_input(ledge_dir_, frame_ == 66, true, false);
			if (OS::get_singleton()->get_cmdline_user_args().has("--ledge-trace") && frame_ % 4 == 0 && frame_ < 130) {
				UtilityFunctions::print("  t=", frame_, " pos=", player_->get_global_position(), " vy=", player_->get_velocity().y, " floor=", player_->is_on_floor(),
						" wall=", player_->is_on_wall());
			}
			if (player_->is_hanging()) {
				ledge_frame_ = frame_;
				player_->set_bot(false);
				player_->set_input_enabled(false);
				if (OS::get_singleton()->get_cmdline_user_args().has("--ledge-side")) {
					camera_->face_direction(ledge_dir_.rotated(Vector3(0, 1, 0), 1.2f));
				}
				UtilityFunctions::print("LEDGE_GRAB lip_y=", ledge_top_, " hang_feet_y=", player_->get_global_position().y);
			} else if (frame_ > 60 + 240) {
				UtilityFunctions::print("LEDGE_TEST no grab; feet_y=", player_->get_global_position().y, " lip_y=", ledge_top_);
				get_tree()->quit();
			}
		} else if (frame_ == ledge_frame_ + 45) {
			if (!capture_path_.is_empty()) {
				Ref<Image> shot = get_viewport()->get_texture()->get_image();
				shot->save_png(capture_path_);
			}
			player_->set_bot(true); // the bot climbs up after a moment
		} else if (frame_ > ledge_frame_ + 45 && frame_ < ledge_frame_ + 110 && frame_ % 6 == 0 && OS::get_singleton()->get_cmdline_user_args().has("--ledge-trace")) {
			UtilityFunctions::print("  climb t=", frame_ - ledge_frame_, " pos=", player_->get_global_position(), " floor=", player_->is_on_floor(), " hanging=", player_->is_hanging());
		} else if (frame_ == ledge_frame_ + 110) {
			UtilityFunctions::print("LEDGE_CLIMB feet_y=", player_->get_global_position().y, " lip_y=", ledge_top_, " hanging=", player_->is_hanging(),
					" on_floor=", player_->is_on_floor());
			get_tree()->quit();
		}
		return;
	}
	if (drop_test_ >= 0.0f) {
		drop_test_ += dt;
		if (OS::get_singleton()->get_cmdline_user_args().has("--drop-super") && !drop_pressed_ && player_->get_velocity().y < 0.0f &&
				player_->get_global_position().y - drop_base_ < 1.25f) {
			player_->set_bot(true);
			player_->bot_input(Vector3(), true, true, false);
			drop_pressed_ = true; // press jump a moment before touching the mat
		}
		drop_peak_ = Math::max(drop_peak_, player_->get_global_position().y - drop_base_);
		if (drop_test_ > 0.3f && drop_test_ < 0.4f) {
			drop_peak_ = 0.0f; // ignore the starting height
		}
		if (drop_test_ > 3.0f) {
			UtilityFunctions::print("DROP_TEST peak_above_ground=", drop_peak_);
			drop_test_ = -1.0f;
		}
	}
	if (mantle_lot_ >= 0 && frame_ >= 60) {
		// Review aid: walk into a lot's side fence (a house in the way also counts as blocked), press jump, and report which side of
		// the fence the kid ends up on.
		Area *a = streamer_->area(WorldStreamer::area_key(0, 0));
		const GenContext *ctx = a->gen->context();
		const Lot &lot = ctx->lots[mantle_lot_ % int(ctx->lots.size())];
		const float fx = lot.width * 0.5f - 0.3f, mz = -lot.depth * 0.5f;
		const Transform3D to_world = Transform3D(Basis(), a->plan.origin) * lot.xf;
		if (frame_ == 60) {
			player_->set_bot(true);
			player_->teleport(to_world.xform(Vector3(fx - 1.5f, 0.3f, mz)), 0.0f);
			invuln_ = 100.0f;
		}
		const Vector3 dir = to_world.basis.xform(Vector3(1, 0, 0)).normalized();
		player_->bot_input(frame_ < 60 + 240 ? dir : Vector3(), frame_ > 60 + 60 && frame_ % 20 == 0, false, false);
		if (frame_ == 60 + 300) {
			const float lx = to_world.affine_inverse().xform(player_->get_global_position()).x;
			UtilityFunctions::print("MANTLE lot=", mantle_lot_, " fence_x=", fx, " end_x=", lx, lx > fx ? " CROSSED" : " BLOCKED");
			get_tree()->quit();
		}
		return;
	}
	if (stairs_lot_ >= 0 && frame_ >= 60) {
		// Review aid: walk (never jump) from the path up a porch's steps to its doorbell.
		Area *a = streamer_->area(WorldStreamer::area_key(0, 0));
		PackedVector3Array bells = a->gen->get_doorbells();
		const Vector3 b = a->to_world(bells[stairs_lot_ % bells.size()]);
		const Lot &slot = a->gen->context()->lots[stairs_lot_ % bells.size()];
		// --stairs-side: start beside the steps where they meet the porch, so the walk climbs from the side.
		const bool side = OS::get_singleton()->get_cmdline_user_args().has("--stairs-side");
		const Vector3 street = a->to_world(slot.to_world(side ? Vector3(slot.house.door_x + 2.0f, 0, slot.house.front_z + slot.house.porch_d + 0.6f)
													   : Vector3(slot.house.door_x, 0, -1.0f)));
		if (frame_ == 60) {
			player_->set_bot(true);
			player_->teleport(street + Vector3(0, 0.3f, 0), 0.0f);
			invuln_ = 100.0f; // ignore thorns for the test
		}
		const Vector3 to = b - player_->get_global_position();
		player_->bot_input(Vector3(to.x, 0, to.z).normalized(), false, false, false);
		stairs_best_ = Math::max(stairs_best_, player_->get_global_position().y - street.y);

		if (frame_ == 60 + 360) {
			const Vector3 el = slot.xf.affine_inverse().xform(a->to_local(player_->get_global_position()));
			UtilityFunctions::print("STAIRS lot=", stairs_lot_, " deck_rise=", b.y - street.y, " best_rise=", stairs_best_, " end_dist=", Vector2(to.x, to.z).length(), " end_local=", el, " door_x=", slot.house.door_x);
			get_tree()->quit();
		}
	}
	if (review_car_ && frame_ == 30) {
		toggle_car();
	}
	if (review_shop_ && frame_ == capture_frames_ - 20) {
		open_shop();
	}
	if (!cine_mode_.is_empty()) {
		cine_step(dt);
	}
	if (autopilot_ > 0.0f) {
		drive_bot(dt);
	} else if (!capture_path_.is_empty() && frame_ == capture_frames_) {
		capture_and_maybe_quit();
	}
	if (in_menu_) {
		return; // the main menu is up: the world just turns behind it
	}
	if (in->is_action_just_pressed("pause") && !menu_open_ && teleport_t_ < 0.0f && Engine::get_singleton()->get_process_frames() > menu_closed_frame_ + 1) {
		menu_open_ = true;
		player_->set_input_enabled(false);
		call_ui("open_pause");
		call_audio("play_menu");
		get_tree()->set_pause(true);
		return;
	}
	if (in->is_action_just_pressed("new_night") && !menu_open_) {
		start_world(int64_t(Time::get_singleton()->get_ticks_usec() % 1000000) + 1);
		return;
	}
	if (in->is_action_just_pressed("toggle_lightning")) {
		set_lightning(!lightning_);
		Array a;
		a.push_back(String(lightning_ ? "Lightning on" : "Lightning off"));
		call_ui("toast", a);
	}
	// Teleport: fade out, stream the destination in, place the kid, fade back in.
	if (teleport_t_ >= 0.0f) {
		teleport_t_ += dt;
		if (teleport_t_ > 0.45f && teleport_t_ - dt <= 0.45f) {
			if (car_ && car_->driving()) {
				toggle_car();
			}
			streamer_->update(teleport_to_, true);
			const Vector3 p = teleport_to_ + Vector3(0, 0.6f, 2.4f);
			player_->teleport(Vector3(p.x, streamer_->ground(p.x, p.z) + 0.4f, p.z), 0.0f);
			camera_->snap();
			gate_armed_ = false;
		}
		if (teleport_t_ > 0.8f) {
			teleport_t_ = -1.0f;
			Array a;
			a.push_back(false);
			call_ui("fade", a);
			menu_open_ = false;
			player_->set_input_enabled(true);
		}
	}

	const Vector3 pp = player_->get_global_position();
	streamer_->update(pp);
	if (sky_) {
		sky_->set_center(Vector3(pp.x, pp.y, pp.z));
	}
	update_wisps(dt);
	update_popups(dt);
	update_costume_pickups(dt);
	update_wild(dt);
	for (auto &kv : infest_) {
		kv.second->update(dt);
	}
	hud_t_ -= dt;
	if (hud_t_ <= 0.0f) {
		hud_t_ = 0.25f;
		update_area_hud(false);
	}

	// Invulnerability blink after a hit.
	if (invuln_ > 0.0f) {
		invuln_ = Math::max(0.0f, invuln_ - dt);
		player_->set_blink(invuln_ > 0.0f && std::fmod(invuln_ * 12.0f, 1.0f) < 0.4f);
	}

	// Inputs that open menus or summon the car.
	if (!menu_open_ && teleport_t_ < 0.0f) {
		if (in->is_action_just_pressed("shop")) {
			open_shop();
		} else if (in->is_action_just_pressed("summon_car")) {
			toggle_car();
		}
	}

	// Riding: the kid sits in the car; the car rams small monsters out of the way.
	const bool driving = car_ && car_->driving();
	camera_->set_follow_body(driving ? car_ : nullptr); // the kid sits still in the seat: follow the car
	if (driving) {
		const Transform3D seat = car_->seat();
		player_->set_global_position(seat.origin);
		player_->set_facing(car_->yaw());
		if (car_->speed() > 6.0f) {
			std::vector<Damageable *> targets;
			gather_targets(targets);
			for (Damageable *t : targets) {
				Monster *m = t->is_monster() ? static_cast<Monster *>(t) : nullptr;
				if (m && m->target_alive() && m->target_point().distance_to(car_->get_global_position() + Vector3(0, 0.6f, 0)) < 1.7f) {
					m->take_hit(3.0f, car_->get_global_position());
					camera_->shake(0.2f);
					call_audio("play_car_bump");
				}
			}
		}
	}

	// Doorbells, candy and gates in every loaded settlement.
	const float magnet = up_.magnet ? 1.5f : 0.0f;
	const float radius = player_->collect_radius() + magnet + (driving ? 0.6f : 0.0f);
	for (Area *a : streamer_->areas()) {
		PackedVector3Array bells = a->gen->get_doorbells();
		for (int i = 0; i < bells.size() && i < int(a->state->rung.size()); ++i) {
			if (a->state->rung[i]) {
				continue;
			}
			const Vector3 b = a->to_world(bells[i]);
			if (Vector2(b.x - pp.x, b.z - pp.z).length() < 0.95f && std::abs(b.y - pp.y) < 1.0f) {
				a->state->rung[i] = 1;
				update_area_hud(true);
				++bot_doorbells_;
				a->gen->open_door(i);
				if (CandyField *cf = a->gen->candy_field()) {
					// Treats spray back toward the street, the way the player came.
					const Vector3 bl = bells[i];
					const Vector3 pl = a->to_local(pp);
					cf->burst(bl, pl - bl + (a->to_local(a->plan.spawn) - bl).normalized() * 0.5f, i, 12, sub_seed(uint64_t(seed_), a->key, uint64_t(i)));
				}
				call_audio("play_doorbell");
				call_audio("play_treats");
				popup(b + Vector3(0, 2.5f, 0), "Trick or treat!", Color(1.0f, 0.95f, 0.85f), 1.4f, 2.2f);
				if (Vfx *v = Vfx::get()) {
					v->candy_sparkles(b + Vector3(0, 1.3f, 0), Color(1.0f, 0.75f, 0.3f), 40, 3.5f, 1.1f, 1.5f);
				}
				camera_->shake(0.25f);
			}
		}
		if (CandyField *cf = a->gen->candy_field()) {
			const int got = cf->collect(a->to_local(pp) + Vector3(0, 0.55f, 0), radius, 1.15f, dt);
			if (got > 0) {
				Array w;
				w.push_back(wallet_);
				call_ui("set_candy", w);
			}
		}
		// Travel gate: step onto a freed gate's pad to pick a destination.
		auto it = infest_.find(a->key);
		if (it != infest_.end()) {
			const Vector3 g = it->second->gate_position();
			const bool on_pad = Vector2(g.x - pp.x, g.z - pp.z).length() < 1.3f && std::abs(g.y - pp.y) < 1.2f && !driving;
			if (on_pad && gate_armed_ && !menu_open_ && teleport_t_ < 0.0f) {
				gate_armed_ = false;
				if (!it->second->freed()) {
					Array t;
					t.push_back(String("The gate is asleep. Free ") + String(a->plan.name.c_str()) + " to wake it.");
					call_ui("toast", t);
				} else {
					Array list;
					travel_keys_.clear();
					for (const FreedArea &f : freed_) {
						if (f.key == a->key) {
							continue;
						}
						Dictionary d;
						d["name"] = String(f.name.c_str());
						d["distance"] = int(f.gate.distance_to(pp));
						list.push_back(d);
						travel_keys_.push_back(f.key);
					}
					Array args;
					args.push_back(list);
					call_ui("open_travel", args);
					menu_open_ = true;
					player_->set_input_enabled(false);
					call_audio("play_menu");
				}
			}
			if (!on_pad && Vector2(g.x - pp.x, g.z - pp.z).length() > 2.2f && a->key == current_area_) {
				gate_armed_ = true;
			}
			// Candy shop cart: step onto its doormat.
			const Vector3 sp = it->second->shop_position();
			const float sd = Vector2(sp.x - pp.x, sp.z - pp.z).length();
			if (sd < 0.9f && std::abs(sp.y - pp.y) < 1.2f && !driving && shop_armed_ && !menu_open_ && teleport_t_ < 0.0f) {
				shop_armed_ = false;
				open_shop();
			}
			if (sd > 2.0f && a->key == current_area_) {
				shop_armed_ = true;
			}
		}
	}
	// The first time an upgrade becomes affordable, point at the shop.
	if (!told_shop_ && wallet_ >= 60) {
		told_shop_ = true;
		Array t;
		t.push_back(String("You can afford an upgrade! Press {shop}, or visit a candy cart."));
		call_ui("toast", t);
	}
	const int loose = loose_->collect(player_point(), radius, dt);
	if (loose > 0) {
		wallet_ += loose;
		since_pickup_ = 0.0f;
		Array w;
		w.push_back(wallet_);
		call_ui("set_candy", w);
	}
	since_pickup_ += dt;
	if (combo_t_ > 0.0f) {
		combo_t_ -= dt;
		if (combo_t_ <= 0.0f) {
			combo_ = 0;
			Array a;
			a.push_back(0);
			call_ui("set_combo", a);
		}
	}
	// Idle nudge: after 20 s without a pickup, firefly wisps drift toward the nearest candy.
	nudge_cooldown_ -= dt;
	if (since_pickup_ > 20.0f && nudge_cooldown_ <= 0.0f) {
		nudge_cooldown_ = 6.0f;
		Vector3 target;
		float best = 1e18f;
		for (Area *a : streamer_->areas()) {
			if (CandyField *cf = a->gen->candy_field()) {
				if (cf->collected() < cf->total()) {
					const Vector3 c = a->to_world(cf->nearest_remaining(a->to_local(pp)));
					if (c.distance_squared_to(pp) < best) {
						best = c.distance_squared_to(pp);
						target = c;
					}
				}
			}
		}
		if (best < 1e17f) {
			Ref<SphereMesh> sm;
			sm.instantiate();
			sm->set_radius(0.09f);
			sm->set_height(0.18f);
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			mat->set_albedo(Color(0.9f, 1.0f, 0.5f));
			mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
			mat->set_emission(Color(0.9f, 1.0f, 0.45f));
			mat->set_emission_energy_multiplier(4.0f);
			sm->set_material(mat);
			for (int k = 0; k < 3; ++k) {
				Wisp w;
				w.node = memnew(MeshInstance3D);
				w.node->set_mesh(sm);
				add_child(w.node);
				w.from = pp + Vector3(0.3f * k, 0, -0.2f * k);
				w.to = target;
				w.t = -0.15f * k;
				w.node->set_scale(Vector3(0, 0, 0));
				wisps_.push_back(w);
			}
		}
	}
}

// ---------------------------------------------------------------- preview mode
static void merge_bounds(Node *n, const Transform3D &parent, AABB &box, bool &first) {
	Transform3D xf = parent;
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		xf = parent * n3->get_transform();
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
		const AABB b = xf.xform(mi->get_aabb());
		box = first ? b : box.merge(b);
		first = false;
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		merge_bounds(n->get_child(i), xf, box, first);
	}
}

void FestivalGame::setup_preview() {
	player_->set_visible(false);
	player_->set_physics_process(false);
	player_->set_process(false);
	camera_->set_process(false);
	RenderingServer::get_singleton()->global_shader_parameter_set("cutout_radius", 0.0f);
	if (env_) {
		env_->set_camera_attributes(Ref<CameraAttributes>()); // no toy-lens blur on close-ups
	}
	preview_root_ = memnew(Node3D);
	preview_root_->set_name("Preview");
	add_child(preview_root_);
	// Lawn plot with a paver path so materials are judged against the real ground.
	MeshBuilder g;
	g.quad_out(Vector3(-7, 0, -7), Vector3(7, 0, -7), Vector3(7, 0, 7), Vector3(-7, 0, 7), mat_cell(pal::LAWN, mat::GRASS), Vector3(0, -1, 0));
	g.quad_out(Vector3(-0.8f, 0.01f, 2.2f), Vector3(0.8f, 0.01f, 2.2f), Vector3(0.8f, 0.01f, 7), Vector3(-0.8f, 0.01f, 7), mat_cell(pal::STONE_LIGHT, mat::PAVERS), Vector3(0, -1, 4));
	MeshInstance3D *ground = memnew(MeshInstance3D);
	ground->set_mesh(g.build());
	ground->set_material_override(gen_->get_toon_material());
	preview_root_->add_child(ground);
	Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(preview_path_);
	if (scene.is_null()) {
		UtilityFunctions::push_error("Preview: cannot load ", preview_path_);
		get_tree()->quit(1);
		return;
	}
	Node3D *asset = Object::cast_to<Node3D>(scene->instantiate());
	apply_material_recursive(asset, gen_->get_toon_material());
	preview_root_->add_child(asset);
	bool first = true;
	merge_bounds(asset, Transform3D(), preview_box_, first);
	// Floating assets (candy, clouds, hanging lanterns) have their origin at the centre or top:
	// lift them so they sit just above the lawn.
	if (preview_box_.position.y < -0.01f) {
		const float lift = -preview_box_.position.y + 0.25f;
		asset->set_position(Vector3(0, lift, 0));
		preview_box_.position.y += lift;
	}
	// A warm porch-style light for the night views; a neutral studio key for texture checks.
	OmniLight3D *warm = memnew(OmniLight3D);
	warm->set_color(Color(1.0f, 0.58f, 0.26f));
	warm->set_param(Light3D::PARAM_ENERGY, 1.6f);
	warm->set_param(Light3D::PARAM_RANGE, 6.0f + preview_box_.size.length());
	warm->set_position(preview_box_.get_center() + Vector3(-1.5f, preview_box_.size.y * 0.6f + 1.0f, 2.0f + preview_box_.size.z));
	preview_root_->add_child(warm);
	studio_ = memnew(DirectionalLight3D);
	studio_->set_color(Color(1.0f, 0.97f, 0.92f));
	studio_->set_param(Light3D::PARAM_ENERGY, 0.0f);
	studio_->set_shadow(true);
	studio_->set_global_transform(Transform3D(Basis::looking_at(Vector3(0.4f, -0.8f, -0.5f).normalized(), Vector3(0, 1, 0)), Vector3()));
	preview_root_->add_child(studio_);
	if (moon_) {
		moon_->set_global_transform(Transform3D(Basis::looking_at(Vector3(-0.3f, -0.75f, -0.6f).normalized(), Vector3(0, 1, 0)), Vector3()));
	}
	preview_sheet_ = Image::create(1600, 900, false, Image::FORMAT_RGBA8);
	preview_view_ = 0;
	preview_wait_ = 8;
	if (capture_path_.is_empty()) {
		capture_path_ = "user://preview.png";
	}
}

void FestivalGame::step_preview() {
	struct View {
		float yaw, pitch;
		bool studio;
	};
	static const View views[4] = { { -0.6f, 0.38f, false }, { 2.5f, 0.38f, false }, { -0.6f, 0.45f, true }, { -1.57f, 0.12f, true } };
	Camera3D *cam = camera_->camera();
	if (!cam || preview_view_ >= 4) {
		return;
	}
	const View &v = views[preview_view_];
	const Vector3 c = preview_box_.get_center();
	const float radius = Math::max(preview_box_.size.length() * 0.5f, 0.5f);
	const float dist = radius / std::tan(Math::deg_to_rad(cam->get_fov() * 0.5f)) * 1.25f;
	const Vector3 dir(std::sin(v.yaw) * std::cos(v.pitch), std::sin(v.pitch), std::cos(v.yaw) * std::cos(v.pitch));
	cam->set_global_position(c + dir * dist);
	cam->look_at(c, Vector3(0, 1, 0));
	studio_->set_param(Light3D::PARAM_ENERGY, v.studio ? 1.3f : 0.0f);
	if (moon_) {
		moon_->set_param(Light3D::PARAM_ENERGY, v.studio ? 0.25f : 0.62f);
	}
	if (--preview_wait_ > 0) {
		return;
	}
	Ref<Image> shot = get_viewport()->get_texture()->get_image();
	shot->convert(Image::FORMAT_RGBA8);
	shot->resize(800, 450, Image::INTERPOLATE_LANCZOS);
	preview_sheet_->blit_rect(shot, Rect2i(0, 0, 800, 450), Vector2i((preview_view_ % 2) * 800, (preview_view_ / 2) * 450));
	++preview_view_;
	preview_wait_ = 6;
	if (preview_view_ == 4) {
		preview_sheet_->save_png(capture_path_);
		UtilityFunctions::print("CAPTURED ", capture_path_);
		get_tree()->quit();
	}
}

// ---------------------------------------------------------------- review bot
void FestivalGame::drive_bot(float dt) {
	if (bot_time_ == 0.0f) {
		player_->set_bot(true);
		Engine::get_singleton()->set_time_scale(2.0);
	}
	bot_time_ += dt;
	const Vector3 pos = player_->get_global_position();
	Area *area = streamer_->area_at(pos, 25.0f);
	CandyField *cf = area ? area->gen->candy_field() : nullptr;
	bot_retarget_ -= dt;
	for (size_t i = 0; i < bot_blacklist_.size();) {
		bot_blacklist_[i].second -= dt;
		if (bot_blacklist_[i].second <= 0.0f) {
			bot_blacklist_[i] = bot_blacklist_.back();
			bot_blacklist_.pop_back();
		} else {
			++i;
		}
	}
	if (cf && (bot_retarget_ <= 0.0f || !bot_has_target_ || pos.distance_to(bot_target_) < 0.6f)) {
		const Vector3 prev = bot_target_;
		Vector3 local;
		std::vector<std::pair<Vector3, float>> avoid;
		for (const auto &b : bot_blacklist_) {
			avoid.push_back({ area->to_local(b.first), b.second });
		}
		bot_has_target_ = cf->nearest_within(area->to_local(pos), 3.0f, local, &avoid);
		bot_target_ = area->to_world(local);
		bot_retarget_ = 0.35f;
		bot_target_age_ = prev.distance_to(bot_target_) < 0.1f ? bot_target_age_ : 0.0f;
	}
	bot_target_age_ += dt;
	if (cf && cf->collected() != bot_last_count_) {
		bot_last_count_ = cf->collected();
		bot_last_pickup_ = bot_time_;
	}
	if (bot_has_target_ && (bot_target_age_ > 6.0f || bot_time_ - bot_last_pickup_ > 8.0f)) {
		// Give up on candy we can't reach (and its neighbours) for a while.
		bot_blacklist_.push_back({ bot_target_, 30.0f });
		bot_target_age_ = 0.0f;
		bot_retarget_ = 0.0f;
		bot_last_pickup_ = bot_time_;
	}
	Vector3 wish;
	bool jump = false, hold = false, fire = false;
	if (bot_has_target_) {
		const Vector3 to = bot_target_ - pos;
		const Vector3 flat(to.x, 0, to.z);
		const float dist = flat.length();
		if (dist > 0.25f) {
			wish = flat / dist;
		}
		const float vy = player_->get_velocity().y;
		if (player_->is_on_floor()) {
			jump = (to.y > 0.9f && dist < 2.8f);
		} else if (vy < 0.5f && to.y > 0.2f) {
			jump = true; // wing flap
		}
		hold = to.y > 0.0f;
	}
	// Fight back: breathe fire at anything close in front.
	if (combat_->aim_assist(pos + Vector3(0, 0.8f, 0), player_->facing_dir(), 12.0f, 0.5f)) {
		fire = true;
	}
	if (player_->get_speed() < 0.6f && wish.length() > 0.1f) {
		bot_stuck_ += dt;
		bot_stuck_total_ += dt;
	} else {
		bot_stuck_ = 0.0f;
	}
	if (bot_stuck_ > 0.4f && player_->is_on_floor()) {
		jump = true;
	}
	if (bot_stuck_ > 1.5f) {
		bot_escape_ = 0.6f;
		bot_stuck_ = 0.0f;
		bot_retarget_ = 0.0f;
	}
	if (bot_escape_ > 0.0f) {
		bot_escape_ -= dt;
		wish = wish.rotated(Vector3(0, 1, 0), Math_PI * 0.5f);
	}
	player_->bot_input(wish, jump, hold, false, fire);
	bot_log_ += dt;
	const int collected = cf ? cf->collected() : 0, total = cf ? cf->total() : 0;
	if (bot_log_ > 10.0f) {
		bot_log_ = 0.0f;
		UtilityFunctions::print("BOT t=", bot_time_, " collected=", collected, "/", total, " wallet=", wallet_, " doorbells=", bot_doorbells_,
				" hits=", bot_hits_, " kills=", bot_kills_, " chunks=", streamer_->chunk_count(), " stuck=", bot_stuck_total_, " fps=",
				Engine::get_singleton()->get_frames_per_second(), " pos=", pos);
	}
	if (bot_time_ >= autopilot_) {
		UtilityFunctions::print("BOT_DONE seconds=", bot_time_, " collected=", collected, "/", total, " wallet=", wallet_, " perfect_bonus=", bonus_,
				" doorbells=", bot_doorbells_, " hits=", bot_hits_, " kills=", bot_kills_, " stuck_seconds=", bot_stuck_total_, " ledge_grabs=", player_->grab_count());
		autopilot_ = 0.0f;
		Engine::get_singleton()->set_time_scale(1.0);
		if (!capture_path_.is_empty()) {
			capture_and_maybe_quit();
		} else {
			get_tree()->quit();
		}
	}
}

void FestivalGame::capture_and_maybe_quit() {
	Ref<Image> img = get_viewport()->get_texture()->get_image();
	if (img.is_valid()) {
		img->save_png(capture_path_);
		UtilityFunctions::print("CAPTURED ", capture_path_);
	}
	if (quit_after_capture_) {
		get_tree()->quit();
	}
}

} // namespace tot
