// Character showcase recorder: one character per run (a player costume or a monster) walked through a
// fixed timeline of beats on a clear stretch of the home street, for Godot's movie writer:
//   godot --path game --resolution 960x1080 --fixed-fps 60 --write-movie x.avi -- --showcase=kid_cat --showcase-out=DIR
// tools/showcase.ps1 runs it for every character, trims the lead-in, and makes the mp4, the beat list
// (<id>.beats.json) and the stills. It exists to be run twice, before and after an art or animation
// change, so the two recordings line up frame for frame. That is why:
//   - every beat has a fixed frame count, and every input lands on a fixed frame;
//   - the camera is a pure function of the beat, the frame and the character's position (never of
//     mesh bounds: the framing sizes below are constants per character);
//   - it only drives gameplay: the real DragonController through its bot input, the game's own costume
//     switch, hurt and pickup events, and real Monsters through their AI plus four small hooks
//     (Monster::showcase_*). It never touches part nodes, meshes or animation state, so whatever the
//     game plays for a gameplay event is what gets recorded.

#include "play/festival_game.h"

#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "gen/candy_kinds.h"
#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "play/candy_field.h"
#include "play/combat.h"
#include "play/dragon_controller.h"
#include "play/festival_camera.h"
#include "play/loose_candy.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

namespace {

struct Beat {
	const char *name;
	int frames; // at 60 fps
};

// Player costumes: 2030 frames (33.8 s). Inputs are timed so the action is in full swing at the
// middle frame of its beat (the frame the review sheets show).
enum { P_TURN, P_FACE, P_WALK, P_RUN, P_STOP, P_JUMP, P_DOUBLE, P_SPIN, P_ROLL, P_POUND, P_FIRE, P_HURT, P_CANDY, P_IDLE };
const Beat PLAYER_BEATS[] = {
	{ "turntable", 360 }, { "face", 150 }, { "walk", 120 }, { "run", 180 }, { "stop_idle", 90 }, { "jump", 90 }, { "double_jump_glide", 240 },
	{ "spin", 90 }, { "roll", 90 }, { "ground_pound", 120 }, { "fire_cast", 120 }, { "hurt", 170 }, { "candy_celebrate", 120 }, { "idle", 90 },
};
// Monsters: 1750 frames (29.2 s).
enum { M_TURN, M_FACE, M_WANDER, M_CHASE, M_ATTACK_1, M_ATTACK_2, M_HIT, M_DIE, M_RISE };
const Beat MONSTER_BEATS[] = {
	{ "turntable", 360 }, { "face", 150 }, { "wander", 180 }, { "chase", 180 }, { "attack_1", 240 }, { "attack_2", 240 }, { "hit", 120 }, { "die", 120 }, { "rise_idle", 160 },
};

constexpr int FPS = 60;
constexpr int SETTLE = 20; // game frames before the stage is set
constexpr int PREROLL = 150; // frames on stage before the first beat (cut from the mp4): the costume switch's sparkles and the monster's spawn are over
constexpr int MARKER = 12; // the last frames of the lead-in are the magenta sync marker
constexpr float FOV = 38.0f; // vertical
const float LENS = 1.0f / (2.0f * std::tan(Math::deg_to_rad(FOV) * 0.5f)); // camera distance per metre of frame height
constexpr float RUN_BACK = 14.5f; // the walk and run start this far behind the stage point and end about as far beyond it

// Fixed framing per character: nominal height and width (metres), the height of the face above the
// feet and how tall a frame the face close-up shows. Never measured from the meshes: an upgraded
// model is framed exactly like the one before it.
struct Framing {
	const char *id;
	float h, w, face_y, face_h;
	// The least angle (radians) the action cameras keep from the stage front, toward the street's
	// length. The bat flies at the height of the street trees' crowns: its cameras stay out over the
	// road, up and down the street from it, instead of out toward the kerb.
	float az_min = 0.0f;
};
const Framing FRAMING[] = {
	{ "dragon_kid", 1.2f, 1.0f, 0.82f, 1.0f },
	{ "kid_ghost", 1.2f, 1.0f, 0.82f, 1.0f },
	{ "kid_cat", 1.25f, 1.0f, 0.82f, 1.0f },
	{ "kid_witch", 1.45f, 1.0f, 0.85f, 1.1f },
	{ "kid_skeleton", 1.2f, 1.0f, 0.82f, 1.0f },
	{ "kid_pumpkin", 1.2f, 1.1f, 0.82f, 1.0f },
	{ "monster_pumpkin", 1.25f, 1.6f, 0.55f, 1.5f },
	{ "monster_bat", 0.8f, 1.7f, 0.3f, 1.0f, 1.05f },
	{ "monster_skeleton", 1.35f, 0.9f, 1.0f, 0.95f },
	{ "monster_ghost", 1.75f, 1.2f, 0.95f, 1.4f },
	{ "monster_gremlin", 1.05f, 0.9f, 0.55f, 1.1f },
	{ "monster_mummy", 1.55f, 1.0f, 1.15f, 1.0f },
	{ "monster_slime", 1.15f, 1.5f, 0.55f, 1.4f },
	{ "monster_scarecrow", 2.0f, 1.3f, 1.4f, 1.15f },
	{ "monster_spider", 0.85f, 1.7f, 0.45f, 1.3f },
	{ "monster_toadstool", 1.25f, 1.2f, 0.6f, 1.25f },
};
// In FestivalGame's costume order, and in MonsterType order.
const char *COSTUME_IDS[] = { "dragon_kid", "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin" };
const char *MONSTER_IDS[] = { "monster_pumpkin", "monster_bat", "monster_skeleton", "monster_ghost", "monster_gremlin", "monster_mummy", "monster_slime",
	"monster_scarecrow", "monster_spider", "monster_toadstool" };

// How far off the target stands for each kind of attack.
float attack_distance(const MonsterTuning &t) {
	switch (t.attack) {
		case MonsterTuning::HOP: return 3.0f;
		case MonsterTuning::SWOOP: return 4.0f;
		case MonsterTuning::THROW: return 6.0f;
		case MonsterTuning::LUNGE: return t.reach + 0.9f;
		case MonsterTuning::GRAB: return 2.6f;
		case MonsterTuning::CHARGE: return 5.0f;
		case MonsterTuning::POUNCE: return 4.5f;
		case MonsterTuning::PUFF: return 1.6f;
	}
	return 3.0f;
}

// Frames from the end of the wind-up to the attack's most telling moment (in the air, mid-charge,
// the bone just thrown).
int attack_lead(const MonsterTuning &t) {
	switch (t.attack) {
		case MonsterTuning::HOP: return 14;
		case MonsterTuning::SWOOP: return 16;
		case MonsterTuning::THROW: return 8;
		case MonsterTuning::LUNGE: return 8;
		case MonsterTuning::GRAB: return 10;
		case MonsterTuning::CHARGE: return 30;
		case MonsterTuning::POUNCE: return 16;
		case MonsterTuning::PUFF: return 8;
	}
	return 10;
}

Transform3D look(const Vector3 &from, const Vector3 &at) {
	return Transform3D(Basis::looking_at((at - from).normalized(), Vector3(0, 1, 0)), from);
}

} // namespace

struct FestivalGame::Showcase {
	bool player = true;
	int index = 0; // costume, or MonsterType
	Framing fr = FRAMING[0];
	const Beat *beats = nullptr;
	int beat_count = 0, total = 0;
	int n = -PREROLL - 1; // frames since the first beat began
	// The stage: a point on the home street, its front (the way the character faces at rest, toward
	// the turntable's first camera) and its right as seen from that camera (along the street).
	Vector3 S, F, R;
	float yaw_f = 0.0f, yaw_r = 0.0f;
	float hero = 3.0f; // camera distance that shows the whole character at about 60% of the frame height
	Vector3 feet; // where the character stands (for a monster that has died: where it last was)
	Monster *mon = nullptr;
	Vector3 lure; // what monsters take for the player's position
	Vector3 away; // a lure nobody notices
	Vector3 anchor; // per beat
	OmniLight3D *key = nullptr, *fill = nullptr;
	int64_t movie_start = 0; // the engine's process frame at the first beat's first frame
	// Sync marker: the whole picture is plain magenta for the last frames of the lead-in. The script
	// finds the last magenta frame in the movie and starts the clip on the one after it, so the cut
	// never depends on counting frames from the start of the run.
	ColorRect *marker = nullptr;
	Vector2i size = Vector2i(960, 1080); // the recording (--showcase-size=WxH)
	bool borderless = false;
};

void FestivalGame::showcase_free() {
	delete showcase_;
	showcase_ = nullptr;
}

// A fresh monster on the stage point, facing the front, standing where it is.
void FestivalGame::showcase_spawn(uint64_t seed) {
	Showcase &s = *showcase_;
	const MonsterType type = MonsterType(s.index);
	Vector3 at = s.S;
	at.y = streamer_->ground(at.x, at.z) + (tuning(type).flying ? 1.3f : 0.3f); // as the nests and the wild spawns place them
	Monster *m = memnew(Monster);
	wild_root_->add_child(m);
	m->set_global_position(at);
	m->init(type, &mworld_, nullptr, seed, Rect2()); // no leash: it stays put until a beat sends it somewhere
	m->showcase_face(s.yaw_f);
	m->on_gone = [this](Monster *gone) {
		wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end());
		if (showcase_ && showcase_->mon == gone) {
			showcase_->mon = nullptr;
		}
	};
	wild_.push_back(m);
	s.mon = m;
	s.feet = at;
}

bool FestivalGame::showcase_setup() {
	const std::string id = showcase_id_.utf8().get_data();
	Showcase *sc = new Showcase();
	showcase_ = sc;
	Showcase &s = *sc;
	bool known = false;
	for (const Framing &f : FRAMING) {
		if (id == f.id) {
			s.fr = f;
			known = true;
		}
	}
	for (int k = 0; k < 6; ++k) {
		if (id == COSTUME_IDS[k]) {
			s.player = true;
			s.index = k;
		}
	}
	for (int k = 0; k < int(MonsterType::Count); ++k) {
		if (id == MONSTER_IDS[k]) {
			s.player = false;
			s.index = k;
		}
	}
	Area *home = streamer_->area(WorldStreamer::area_key(0, 0));
	if (!known || !home || !home->gen->context()) {
		UtilityFunctions::print("SHOWCASE error: unknown id or no home area: ", showcase_id_);
		get_tree()->quit(2);
		return false;
	}
	s.beats = s.player ? PLAYER_BEATS : MONSTER_BEATS;
	s.beat_count = s.player ? int(sizeof(PLAYER_BEATS) / sizeof(Beat)) : int(sizeof(MONSTER_BEATS) / sizeof(Beat));
	for (int i = 0; i < s.beat_count; ++i) {
		s.total += s.beats[i].frames;
	}
	const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	for (int i = 0; i < args.size(); ++i) {
		const String a = args[i];
		if (a.begins_with("--showcase-size=") && a.substr(16).contains("x")) {
			s.size = Vector2i(a.substr(16).get_slice("x", 0).to_int(), a.substr(16).get_slice("x", 1).to_int());
		}
	}
	s.hero = Math::max(s.fr.h / 0.6f, s.fr.w * 1.25f * float(s.size.y) / float(s.size.x)) * LENS;

	// The stage: the straightest stretch of the home street with nothing standing in the run, in the
	// tracking camera's lane or around the turntable. The front faces the side the moonlight comes from.
	const StreetPlan &st = home->gen->context()->street;
	const int count = int(st.center.size());
	auto street_at = [&](int i) {
		Vector3 p = home->to_world(st.center[Math::clamp(i, 0, count - 1)]);
		p.y = streamer_->ground(p.x, p.z);
		return p;
	};
	const Vector3 to_moon = moon_ ? moon_->get_global_transform().basis.get_column(2) : Vector3(0, 1, 0);
	const int reach = int(RUN_BACK) + 2;
	struct Spot {
		float bend;
		int i;
	};
	std::vector<Spot> spots;
	for (int i = reach + 2; i < count - reach - 2; ++i) {
		const Vector3 c = street_at(i);
		Vector3 t = street_at(i + 1) - street_at(i - 1);
		t.y = 0.0f;
		t = t.normalized();
		float bend = 0.0f;
		for (int j = -reach; j <= reach; ++j) {
			Vector3 d = street_at(i + j) - c;
			d.y = 0.0f;
			bend = Math::max(bend, (d - t * d.dot(t)).length());
		}
		spots.push_back({ bend, i });
	}
	std::sort(spots.begin(), spots.end(), [](const Spot &a, const Spot &b) { return a.bend < b.bend || (a.bend == b.bend && a.i < b.i); });
	// Street trees: their crowns hang over the road at the height the flying monsters' cameras work
	// at, so the stage keeps as far from them as the street allows (most of all on the camera's side).
	std::vector<Vector3> trees;
	for (const PropPlacement &pp : home->gen->context()->props) {
		if (pp.id.rfind("tree_", 0) == 0) {
			trees.push_back(home->to_world(pp.xf.origin));
		}
	}
	// How far the nearest tree trunk is from where the cameras work around the stage point: the patch
	// on the front side that the action cameras use, and the turntable's circle.
	auto tree_room = [&](const Vector3 &c, const Vector3 &front, const Vector3 &right) {
		float room = 1.0e9f;
		for (const Vector3 &tr : trees) {
			const Vector3 d = tr - c;
			const float f = d.dot(front), r = d.dot(right);
			const float df = f < -1.0f ? -1.0f - f : Math::max(0.0f, f - 6.5f);
			const float dr = Math::max(0.0f, std::abs(r) - 6.0f);
			room = Math::min(room, std::sqrt(df * df + dr * dr));
			room = Math::min(room, Math::max(0.0f, Vector2(f, r).length() - 4.5f));
		}
		return Math::min(room, 3.5f); // beyond a crown's width, more room is no better
	};
	bool found = false;
	float best = -1.0f;
	for (int pass = 0; pass < 2 && !found; ++pass) { // second pass: the straightest spot, clear or not
		for (const Spot &sp : spots) {
			if (pass == 0 && sp.bend > 1.2f) {
				break; // straight enough for the run to stay in the middle of the road
			}
			const Vector3 c = street_at(sp.i);
			Vector3 t = street_at(sp.i + 1) - street_at(sp.i - 1);
			t.y = 0.0f;
			t = t.normalized();
			for (int side = 0; side < 2; ++side) {
				Vector3 front(-t.z, 0, t.x);
				if ((front.dot(to_moon) < 0.0f) != (side == 1)) {
					front = -front; // the moonlit side first
				}
				const Vector3 right(front.z, 0, -front.x);
				bool clear = true;
				for (float h : { 0.5f, 1.4f }) {
					const Vector3 up(0, h, 0);
					for (int u = -reach; u < reach && clear; u += 2) {
						for (float lane : { 0.0f, 3.4f }) { // the run, and the camera that leads it
							clear = clear && cine_clear(c + right * float(u) + front * lane + up, c + right * float(u + 2) + front * lane + up);
						}
					}
					for (int a = 0; a < 16 && clear; ++a) { // the turntable, and the widest action camera
						const float ang = float(a) * float(Math_TAU) / 16.0f;
						clear = clear && cine_clear(c + up, c + (front * std::cos(ang) + right * std::sin(ang)) * 6.5f + up);
					}
				}
				const float room = tree_room(c, front, right);
				if ((clear && room > best + 0.05f) || (pass == 1 && !found)) {
					s.S = c;
					s.F = front;
					s.R = right;
					found = true;
					best = room;
					UtilityFunctions::print("SHOWCASE stage candidate: street index ", sp.i, " at ", c, " front ", front, " bend ", sp.bend, " tree room ", room,
							clear ? " (clear)" : " (NOT CLEAR)");
				}
			}
			if (pass == 1 && found) {
				break;
			}
		}
	}
	UtilityFunctions::print("SHOWCASE stage: ", s.S, " front ", s.F, " tree room ", best, " (", int(trees.size()), " trees)");
	s.yaw_f = std::atan2(s.F.x, s.F.z);
	s.yaw_r = std::atan2(s.R.x, s.R.z);
	s.away = s.S + Vector3(10000.0f, -1000.0f, 0);
	s.lure = s.away;
	s.feet = s.S;

	// A clean stage: no HUD, no candy or guide trail around it, no lightning, and nothing the keyboard
	// or a gamepad can do to the run.
	if (CanvasLayer *hud = Object::cast_to<CanvasLayer>(hud_)) {
		hud->set_visible(false);
	}
	set_lightning(false);
	InputMap *im = InputMap::get_singleton();
	const TypedArray<StringName> actions = im->get_actions();
	for (int i = 0; i < actions.size(); ++i) {
		im->action_erase_events(actions[i]);
	}
	for (Area *a : streamer_->areas()) {
		if (CandyField *cf = a->gen->candy_field()) {
			cf->hide_near(a->to_local(s.S), 36.0f);
		}
		const TypedArray<Node> magic = a->gen->find_children("Magic*", "MeshInstance3D", true, false);
		for (int i = 0; i < magic.size(); ++i) {
			if (Node3D *n = Object::cast_to<Node3D>(magic[i])) {
				n->set_visible(false);
			}
		}
	}
	loose_->clear();
	loose_->set_reach(0.0f);
	wallet_ = 0;
	// Keep the character sharp however close the camera comes: no near blur (the far blur stays).
	if (env_) {
		Ref<CameraAttributesPractical> lens = env_->get_camera_attributes();
		if (lens.is_valid()) {
			lens->set_dof_blur_near_enabled(false);
		}
	}
	// The showcase's own soft key and fill, carried round with the camera so every side reads.
	s.key = memnew(OmniLight3D);
	s.key->set_color(Color(1.0f, 0.96f, 0.9f));
	s.key->set_param(Light3D::PARAM_RANGE, 9.0f);
	s.key->set_param(Light3D::PARAM_ENERGY, 1.5f);
	s.key->set_param(Light3D::PARAM_ATTENUATION, 0.8f);
	s.key->set_shadow(false);
	add_child(s.key);
	s.fill = memnew(OmniLight3D);
	s.fill->set_color(Color(0.86f, 0.9f, 1.0f));
	s.fill->set_param(Light3D::PARAM_RANGE, 9.0f);
	s.fill->set_param(Light3D::PARAM_ENERGY, 0.6f);
	s.fill->set_param(Light3D::PARAM_ATTENUATION, 0.8f);
	s.fill->set_shadow(false);
	add_child(s.fill);

	CanvasLayer *top = memnew(CanvasLayer);
	top->set_layer(100);
	add_child(top);
	s.marker = memnew(ColorRect);
	s.marker->set_color(Color(1.0f, 0.0f, 1.0f));
	s.marker->set_anchors_preset(Control::PRESET_FULL_RECT);
	s.marker->set_visible(false);
	top->add_child(s.marker);

	camera_->set_cinematic(true);
	camera_->set_cinematic_cutout(false);
	player_->set_bot(true);
	player_->set_input_enabled(false);
	player_->bot_input(Vector3(), false, false, false, false);
	Vector3 stand = s.S;
	stand.y += 0.2f;
	if (s.player) {
		player_->teleport(stand, s.yaw_f);
		if (s.index != 0) {
			// The game's own costume switch: with only this costume found, the next one is it.
			if (!gen_->asset_published(COSTUME_IDS[s.index])) {
				UtilityFunctions::print("SHOWCASE error: costume not published: ", showcase_id_);
				get_tree()->quit(2);
				return false;
			}
			std::fill(costume_found_.begin(), costume_found_.end(), 0);
			costume_found_[0] = 1;
			costume_found_[s.index] = 1;
			costume_ = 0;
			cycle_costume();
			if (costume_ != s.index) {
				UtilityFunctions::print("SHOWCASE error: costume switch failed: ", showcase_id_);
				get_tree()->quit(2);
				return false;
			}
		}
	} else {
		// The kid is the monsters' target: out of sight, standing wherever a beat needs them, and
		// nothing a monster does hurts them. Monsters go for the lure, not for the kid.
		player_->set_visible(false);
		player_->teleport(stand + s.R * 8.0f, s.yaw_f);
		mworld_.player_pos = [this]() { return showcase_ ? showcase_->lure : Vector3(); };
		mworld_.hurt_player = [](const Vector3 &) {};
		combat_->on_player_hit = [](const Vector3 &) {};
		showcase_spawn(7001);
	}
	UtilityFunctions::print("SHOWCASE ", showcase_id_, s.player ? " (player)" : " (monster)", " frames=", s.total, " size=", s.size.x, "x", s.size.y);
	return true;
}

// The recording's size. Movie Maker mode records at the project's window size whatever --resolution
// says (and scales every frame to that size), so tools/showcase.ps1 sets the project's window size
// for the run (a temporary override.cfg) and tells the showcase what it should be. The lead-in
// checks that the window really has that size (a tall window can be clamped to the desktop): if
// not it is resized, then tried without a border, and the run gives up loudly rather than record
// a clip of another shape.
bool FestivalGame::showcase_check_size(int attempt) {
	Showcase &s = *showcase_;
	DisplayServer *ds = DisplayServer::get_singleton();
	const Vector2i window = ds->window_get_size();
	if (window == s.size) {
		if (attempt == 2) {
			UtilityFunctions::print("SHOWCASE window ", window.x, "x", window.y, s.borderless ? " (borderless)" : "");
		}
		return true;
	}
	if (attempt == 2) {
		UtilityFunctions::print("SHOWCASE error: wanted ", s.size.x, "x", s.size.y, " but the window is ", window.x, "x", window.y);
		showcase_id_ = String();
		get_tree()->quit(3);
		return false;
	}
	UtilityFunctions::print("SHOWCASE window is ", window.x, "x", window.y, attempt == 0 ? ": resizing" : ": retrying borderless");
	ds->window_set_mode(DisplayServer::WINDOW_MODE_WINDOWED);
	if (attempt == 1) {
		ds->window_set_flag(DisplayServer::WINDOW_FLAG_BORDERLESS, true);
		s.borderless = true;
	}
	ds->window_set_size(s.size);
	ds->window_set_position(ds->screen_get_position(ds->window_get_current_screen()) + (attempt == 1 ? Vector2i() : Vector2i(60, 40)));
	return true;
}

// The camera on a circle round the character: `azimuth` radians from the stage front toward its
// right, `dist` from a point `look_h` above `feet`, a little above it. The key and fill go with it.
void FestivalGame::showcase_view(const Vector3 &feet, float look_h, float azimuth, float dist) {
	Showcase &s = *showcase_;
	const Vector3 dir = s.F * std::cos(azimuth) + s.R * std::sin(azimuth);
	const Vector3 right(dir.z, 0, -dir.x);
	const Vector3 at = feet + Vector3(0, look_h, 0);
	const Vector3 from = at + dir * dist + Vector3(0, dist * 0.13f, 0);
	camera_->set_cinematic_pose(look(from, at), FOV);
	s.key->set_global_position(at + dir * 2.1f - right * 1.7f + Vector3(0, 1.7f, 0));
	s.fill->set_global_position(at + dir * 1.8f + right * 2.4f + Vector3(0, 0.3f, 0));
}

void FestivalGame::showcase_player(int beat, int f) {
	Showcase &s = *showcase_;
	const float h = s.fr.h;
	Vector3 wish;
	bool jump = false, hold = false, spin = false, fire = false;
	// Back on the stage point, at rest, facing `yaw` (a cut: every beat starts from the same place).
	auto home = [&](float yaw) {
		if (f == 0) {
			player_->teleport(Vector3(s.S.x, s.anchor.y + 0.02f, s.S.z), yaw);
		}
	};
	if (beat == P_TURN && f == 0) {
		s.anchor = player_->get_global_position(); // the floor height the kid settled at
	}
	const float floor_y = s.anchor.y;
	const float three_q = 0.6f; // the action camera: front three-quarter
	const float lead = 0.87f; // the tracking camera: ahead of the run and off to the front side
	bool tall = false; // the jump camera: wider, and rising half as fast as the kid
	float dist = s.hero * 1.35f, az = three_q;
	switch (beat) {
		case P_TURN:
			az = float(Math_TAU) * float(f) / float(s.beats[P_TURN].frames);
			dist = s.hero;
			break;
		case P_FACE:
			break;
		case P_WALK:
			if (f == 0) {
				player_->teleport(Vector3(s.S.x, floor_y + 0.02f, s.S.z) - s.R * RUN_BACK, s.yaw_r);
			}
			wish = s.R * 0.4f;
			az = lead;
			break;
		case P_RUN:
			wish = s.R;
			az = lead;
			break;
		case P_STOP:
			az = lead;
			break;
		case P_JUMP:
			home(s.yaw_f);
			jump = f == 25;
			hold = f >= 25 && f < 62; // a full-height jump
			tall = true;
			break;
		case P_DOUBLE:
			// Jump, flap at the top of it, and keep holding to glide down.
			home(s.yaw_f);
			jump = f == 10 || f == 38;
			hold = f >= 10 && f < 215;
			tall = true;
			break;
		case P_SPIN:
			home(s.yaw_f);
			spin = f == 6 || f == 38; // two spins
			break;
		case P_ROLL:
			home(s.yaw_f);
			if (f == 34) {
				wish = s.R * 0.3f; // the stick picks the roll's direction: along the street
				player_->bot_roll();
			}
			break;
		case P_POUND:
			home(s.yaw_f);
			jump = f == 28;
			hold = f >= 28 && f < 54;
			if (f == 54) {
				player_->bot_roll(); // the roll button in the air: the ground pound
			}
			tall = true;
			break;
		case P_FIRE:
			home(s.yaw_f - 0.45f); // the fireballs cross the frame instead of passing the lens
			fire = f == 20 || f == 54;
			break;
		case P_HURT:
			home(s.yaw_f);
			// Knocked into the air at the beat's middle frame, which is the 7th frame of the game's
			// invulnerability blink: a frame the kid is shown on (the blink hides 2 frames in every 5, and
			// the frame between is a float tie that can fall either way from run to run). The blink
			// ends 1.5 s on, inside this beat.
			if (f == 79) {
				invuln_ = 0.0f;
				hurt_player(player_->get_global_position() + s.F * 1.0f + Vector3(0, 0.5f, 0)); // the game's own hit: knockback, blink
			}
			break;
		case P_CANDY:
			home(s.yaw_f);
			for (int k = 0; k < 3; ++k) {
				static const int tier[3] = { 0, 1, 3 };
				if (f == 20 + k * 35 && loose_->on_pickup) {
					combo_ = 0;
					combo_t_ = 0.0f;
					// The game's own pickup (the bucket bounce and happy squash of DragonController::celebrate,
					// with the pickup sparkles): everyday candy, a chocolate, the golden bar.
					loose_->on_pickup(candy_kind_of_tier(tier[k], uint64_t(k * 7 + 3)), player_->get_global_position() + s.F * 0.25f + Vector3(0, 0.75f, 0));
				}
			}
			break;
		case P_IDLE:
			home(s.yaw_f);
			break;
	}
	player_->bot_input(wish, jump, hold, spin, fire);
	const Vector3 p = player_->get_global_position();
	s.feet = p;
	if (beat == P_FACE) {
		showcase_view(p, s.fr.face_y, 0.5f, s.fr.face_h * LENS);
	} else if (tall) {
		const Vector3 base(p.x, floor_y, p.z);
		showcase_view(base, h * 1.05f + 0.55f * (p.y - floor_y), three_q, s.hero * 1.6f);
	} else {
		showcase_view(p, h * 0.5f, az, dist);
	}
}

void FestivalGame::showcase_monster(int beat, int f) {
	Showcase &s = *showcase_;
	const MonsterType type = MonsterType(s.index);
	const MonsterTuning &t = tuning(type);
	const float h = s.fr.h;
	Monster *m = s.mon;
	auto ground_at = [&](Vector3 p) {
		p.y = streamer_->ground(p.x, p.z) + (t.flying ? t.hover : 0.1f);
		return p;
	};
	// Put on a spot, at rest, facing `yaw`, staying there (a cut).
	auto place = [&](const Vector3 &at, float yaw) {
		if (m) {
			m->set_global_position(ground_at(at));
			m->set_velocity(Vector3());
			m->showcase_face(yaw);
			m->showcase_leash(Rect2());
		}
		s.lure = s.away;
	};
	const float three_q = 0.6f, lead = 0.87f;
	float az = three_q, dist = s.hero * 1.4f, look_h = h * 0.5f;
	switch (beat) {
		case M_TURN:
			az = float(Math_TAU) * float(f) / float(s.beats[M_TURN].frames);
			dist = s.hero;
			break;
		case M_FACE:
			az = 0.5f;
			dist = s.fr.face_h * LENS;
			look_h = s.fr.face_y;
			break;
		case M_WANDER:
			// Off along the street at its roaming pace (nobody about).
			if (f == 0 && m) {
				m->showcase_leash(Rect2(s.S.x - 80.0f, s.S.z - 80.0f, 160.0f, 160.0f));
				m->showcase_goal(s.S + s.R * 10.0f, 100.0f);
			}
			az = lead;
			break;
		case M_CHASE:
			// It notices someone back down the street and goes for them (too high up to attack).
			if (f == 0 && m) {
				// Throwers stop closing in at 9 m (they keep their distance): theirs stands further off,
				// still inside the range they notice, so they are still coming at the end of the beat.
				const float off = t.attack == MonsterTuning::THROW ? Math::min(t.aggro - 0.5f, 9.0f + t.speed * 3.2f) : 10.5f;
				s.anchor = m->get_global_position() - s.R * off;
				s.anchor.y = streamer_->ground(s.anchor.x, s.anchor.z);
				s.lure = s.anchor + Vector3(0, 40.0f, 0);
				m->showcase_goal(s.anchor, 100.0f);
			}
			az = -lead;
			break;
		case M_ATTACK_1:
		case M_ATTACK_2: {
			// Its target stands a few metres up the street (the kid, unseen and unhurt); the wind-up is
			// started by hand and the monster's own attack follows. First from the side, then from in front.
			const float reach = attack_distance(t);
			const Vector3 from = s.S - s.R * (reach * 0.5f);
			Vector3 target = s.S + s.R * (reach * 0.5f);
			target.y = streamer_->ground(target.x, target.z);
			if (f == 0) {
				place(from, s.yaw_r);
				player_->teleport(target + Vector3(0, 0.1f, 0), s.yaw_f);
			}
			// The wind-up starts so that the attack itself is under way at the middle of the beat.
			const int go = s.beats[beat].frames / 2 - int(std::round(t.windup * float(FPS))) - attack_lead(t);
			if (f == go && m) {
				s.lure = target;
				m->showcase_windup();
			}
			if (f == go + 135) {
				s.lure = s.away; // no second attack of its own
			}
			az = beat == M_ATTACK_1 ? 0.3f : (s.fr.az_min > 0.0f ? float(Math_PI) - s.fr.az_min : 1.1f); // (the bat: from in front on either side)
			dist = s.hero * 1.6f;
			break;
		}
		case M_HIT:
			if (f == 0) {
				place(s.S - s.F * 2.2f, s.yaw_f); // two knocks toward the camera bring it back to the stage point
				player_->teleport(s.S + s.R * 8.0f + Vector3(0, 0.2f, 0), s.yaw_f);
			}
			dist = s.hero * 1.25f;
			// A real hit that cannot kill: flash and knock. From behind, because a knocked monster turns
			// to face the way it is thrown, and thrown toward the camera it keeps its face to it.
			if ((f == 12 || f == 56) && m) {
				m->take_hit(0.01f, m->get_global_position() - s.F * 2.0f);
			}
			break;
		case M_DIE:
			if (f == 52 && m) {
				m->take_hit(1.0e6f, m->get_global_position() - s.F * 2.0f);
			}
			dist = s.hero * 1.25f;
			break;
		case M_RISE:
			if (f == 0) {
				if (m) { // only if the death went wrong
					UtilityFunctions::print("SHOWCASE warning: the monster outlived its death beat");
					m->on_gone = nullptr;
					wild_.erase(std::remove(wild_.begin(), wild_.end(), m), wild_.end());
					m->queue_free();
				}
				loose_->clear(); // the candy it dropped
				showcase_spawn(7002);
			}
			dist = s.hero * 1.2f;
			break;
	}
	if (s.mon) {
		s.feet = s.mon->get_global_position();
	}
	if (beat != M_TURN && beat != M_FACE && std::abs(az) < s.fr.az_min) {
		az = az < 0.0f ? -s.fr.az_min : s.fr.az_min;
	}
	showcase_view(s.feet, look_h, az, dist);
}

void FestivalGame::showcase_finish() {
	Showcase &s = *showcase_;
	if (!showcase_out_.is_empty()) {
		DirAccess::make_dir_recursive_absolute(showcase_out_);
		const Vector2i size = DisplayServer::get_singleton()->window_get_size();
		String js = "{\n";
		js += vformat("  \"id\": \"%s\",\n  \"kind\": \"%s\",\n  \"fps\": %d,\n  \"width\": %d,\n  \"height\": %d,\n  \"borderless\": %s,\n", showcase_id_, s.player ? "player" : "monster", FPS,
				size.x, size.y, s.borderless ? "true" : "false");
		js += vformat("  \"seed\": %d,\n  \"movie_start_hint\": %d,\n  \"sync_marker_frames\": %d,\n  \"total_frames\": %d,\n", seed_, s.movie_start, MARKER, s.total);
		js += vformat("  \"stage\": { \"x\": %.3f, \"y\": %.3f, \"z\": %.3f, \"front_x\": %.4f, \"front_z\": %.4f },\n", s.S.x, s.S.y, s.S.z, s.F.x, s.F.z);
		js += vformat("  \"stills\": { \"front\": %d, \"back\": %d, \"face\": %d },\n", 1, s.beats[0].frames / 2, s.beats[0].frames + s.beats[1].frames / 2);
		js += "  \"beats\": [\n";
		int at = 0;
		for (int i = 0; i < s.beat_count; ++i) {
			js += vformat("    { \"name\": \"%s\", \"start_frame\": %d, \"end_frame\": %d, \"fps\": %d }%s\n", s.beats[i].name, at, at + s.beats[i].frames - 1, FPS,
					i + 1 < s.beat_count ? "," : "");
			at += s.beats[i].frames;
		}
		js += "  ]\n}\n";
		const String path = showcase_out_.path_join(showcase_id_ + ".beats.json");
		Ref<FileAccess> fa = FileAccess::open(path, FileAccess::WRITE);
		if (fa.is_valid()) {
			fa->store_string(js);
			fa->close();
			UtilityFunctions::print("SHOWCASE beats ", path);
		} else {
			UtilityFunctions::print("SHOWCASE error: could not write ", path);
		}
	}
	UtilityFunctions::print("SHOWCASE done ", showcase_id_, " movie_start_frame=", s.movie_start, " frames=", s.total);
	get_tree()->quit();
}

void FestivalGame::showcase_step(float dt) {
	(void)dt;
	if (!showcase_) {
		if (frame_ < SETTLE) {
			return; // let the world settle first
		}
		if (!showcase_setup()) {
			showcase_id_ = String();
		}
		return;
	}
	Showcase &s = *showcase_;
	if (s.n >= s.total + 3) {
		return; // quitting
	}
	++s.n;
	s.marker->set_visible(s.n >= -MARKER && s.n < 0);
	if (s.n < 0) {
		const int attempt = s.n == -PREROLL + 10 ? 0 : (s.n == -PREROLL + 40 ? 1 : (s.n == -20 ? 2 : -1));
		if (attempt >= 0 && !showcase_check_size(attempt)) {
			return;
		}
		// Lead-in: at rest on the stage, the first beat's opening view.
		if (s.player) {
			player_->bot_input(Vector3(), false, false, false, false);
			s.feet = player_->get_global_position();
		} else if (s.mon) {
			s.feet = s.mon->get_global_position();
		}
		showcase_view(s.feet, s.fr.h * 0.5f, 0.0f, s.hero);
		return;
	}
	if (s.n == 0) {
		// Where the first beat should start in the movie (one movie frame per engine frame). Only a
		// hint: the script looks for the sync marker around here and trusts that.
		s.movie_start = int64_t(Engine::get_singleton()->get_process_frames());
	}
	if (s.n >= s.total + 3) {
		showcase_finish(); // a few frames on, so the last beat's last frame is in the movie
		return;
	}
	if (s.n >= s.total) {
		return; // holding the last view
	}
	int beat = 0, f = s.n;
	while (beat + 1 < s.beat_count && f >= s.beats[beat].frames) {
		f -= s.beats[beat].frames;
		++beat;
	}
	if (f == 0) {
		UtilityFunctions::print("SHOWCASE beat ", s.beats[beat].name, " at frame ", s.n);
	}
	if (s.player) {
		showcase_player(beat, f);
	} else {
		showcase_monster(beat, f);
	}
}

} // namespace tot
