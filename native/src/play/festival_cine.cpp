// Cinematic director: scripted showcase shots of the home neighborhood (and a town) for
// screenshots and the trailer. Each shot places the kid, drives them like the autopilot does
// (run, jump, glide, cast, drive), and flies the camera on its own path; the camera rig, the HUD
// and the occluder cutout are out of the way. Run with the movie writer for the trailer:
//   godot --path game --resolution 1920x1080 --fixed-fps 30 --write-movie trailer.avi -- --cine=reel
// or for stills (one per shot, saved at the shot's best moment):
//   godot --path game --resolution 2560x1440 -- --cine=reel --cine-out=C:/shots --cine-clean

#include "play/festival_game.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "gen/candy_kinds.h"

#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "play/candy_field.h"
#include "play/combat.h"
#include "play/dragon_controller.h"
#include "play/festival_camera.h"
#include "play/loose_candy.h"
#include "play/ride_on_car.h"
#include "play/vfx.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

namespace {

Transform3D look(const Vector3 &from, const Vector3 &at) {
	Vector3 d = at - from;
	if (d.length() < 0.01f) {
		d = Vector3(0, 0, -1);
	}
	return Transform3D(Basis::looking_at(d.normalized(), Vector3(0, 1, 0)), from);
}

Vector3 flat(Vector3 v) {
	v.y = 0.0f;
	return v.length() > 1e-4f ? v.normalized() : Vector3(0, 0, 1);
}

Vector3 side_of(const Vector3 &d) {
	return Vector3(d.z, 0, -d.x);
}

float ease(float x) {
	x = Math::clamp(x, 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

} // namespace

// The kid back on their feet, out of the car, ready for the next shot.
void FestivalGame::cine_reset_player() {
	if (car_) {
		car_->set_auto(false);
		if (car_->driving()) {
			toggle_car();
		}
	}
	player_->set_bot(true);
	player_->set_input_enabled(false);
	up_.magnet = 0;
	loose_->set_reach(1.6f);
	cine_aim_on_ = false;
	cine_cut_at_ = -1.0f;
	cine_aggro_ = false;
	player_->bot_input(Vector3(), false, false, false, false);
}

bool FestivalGame::cine_clear(const Vector3 &a, const Vector3 &b) {
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (!space) {
		return true;
	}
	// Both ways, so a camera that starts inside something (a post, a hedge) still counts as blocked.
	Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(a, b, 1 | 2 | 4);
	Ref<PhysicsRayQueryParameters3D> r = PhysicsRayQueryParameters3D::create(b, a, 1 | 2 | 4);
	return space->intersect_ray(q).is_empty() && space->intersect_ray(r).is_empty();
}

Vector2 FestivalGame::cine_lane(const Vector3 &anchor, const Vector3 &back_dir, std::initializer_list<float> sides, std::initializer_list<float> backs, float push, float height,
		std::initializer_list<Vector3> targets) {
	const Vector3 side_dir = side_of(back_dir);
	for (float back : backs) {
		for (float side : sides) {
			bool ok = true;
			for (float p : { 0.0f, push * 0.5f, push }) {
				Vector3 cam = anchor + back_dir * (back - p) + side_dir * side;
				cam.y = streamer_->ground(cam.x, cam.z) + height;
				for (const Vector3 &tg : targets) {
					ok = ok && cine_clear(cam, tg);
				}
			}
			if (ok) {
				return Vector2(side, back);
			}
		}
	}
	return Vector2(*sides.begin(), *backs.begin());
}

void FestivalGame::cine_home(Vector3 &spawn, Vector3 &street) {
	spawn = streamer_->plan().region(0, 0).spawn;
	street = Vector3(0, 0, -1);
	if (Area *a = streamer_->area(WorldStreamer::area_key(0, 0))) {
		const float y = a->gen->street_yaw_at(a->to_local(spawn));
		street = Vector3(-std::sin(y), 0, -std::cos(y));
	}
}

void FestivalGame::cine_build() {
	cine_.clear();
	cine_base_seed_ = seed_;
	if (cine_bounce_seed_ == 0) {
		cine_bounce_seed_ = 42; // a big trampoline in an open yard, under the moon
	}
	Area *home = streamer_->area(WorldStreamer::area_key(0, 0));
	if (!home) {
		return;
	}
	const RegionPlan &hp = streamer_->plan().region(0, 0);
	const Vector3 spawn = hp.spawn;
	auto ground = [this](Vector3 p, float lift) {
		p.y = streamer_->ground(p.x, p.z) + lift;
		return p;
	};
	// Along the street from the spawn (the direction the camera looks down it).
	const float sy = home->gen->street_yaw_at(home->to_local(spawn));
	const Vector3 street(-std::sin(sy), 0, -std::cos(sy));
	Infestation *inf = infest_.count(home->key) ? infest_[home->key].get() : nullptr;
	// Nests nearest the spawn first (out on the open street rather than behind a house).
	std::vector<int> near_nests;
	if (inf) {
		for (int i = 0; i < inf->nests_total(); ++i) {
			near_nests.push_back(i);
		}
		std::sort(near_nests.begin(), near_nests.end(), [&](int a, int b) { return inf->nest_position(a).distance_to(spawn) < inf->nest_position(b).distance_to(spawn); });
	}
	PackedVector3Array bells = home->gen->get_doorbells();
	auto kid = [this]() { return player_->get_global_position(); };
	int doorbell_lot = 0; // the house the doorbell shot walks up to
	// Houses ranked by how well dressed they are: Halloween props near the front door, plus the
	// candy around it (houses right at the spawn are left for the street shots).
	std::vector<int> decorated;
	{
		static const char *DECOR[] = { "jack", "pumpkin", "lantern", "skeleton", "ghost", "inflatable", "scarecrow", "hay", "mum", "corn",
			"tomb", "grave", "witch", "spider", "bat", "banner", "wreath", "candle", "cauldron", "gourd" };
		const GenContext &gctx = *home->gen->context();
		std::vector<std::pair<float, int>> ranked;
		for (int i = 0; i < bells.size(); ++i) {
			const Vector3 bell = bells[i];
			if (home->to_world(bell).distance_to(spawn) < 10.0f) {
				continue;
			}
			float score = 0.0f;
			for (const PropPlacement &pp : gctx.props) {
				if (pp.lot != i || pp.xf.origin.distance_to(bell) > 12.0f) {
					continue;
				}
				bool dressed = false;
				for (const char *k : DECOR) {
					dressed = dressed || pp.id.find(k) != std::string::npos;
				}
				score += dressed ? 1.5f : 0.2f;
			}
			for (const CandySpec &c : gctx.candy) {
				if (c.pos.distance_to(bell) < 10.0f) {
					score += 0.5f;
				}
			}
			ranked.push_back({ -score, i });
		}
		std::sort(ranked.begin(), ranked.end());
		for (const auto &r : ranked) {
			decorated.push_back(r.second);
		}
		if (decorated.empty() && bells.size() > 0) {
			decorated.push_back(0);
		}
	}
	// 1. Aerial: a crane shot down the street, the kid running along the magic trail.
	{
		CineShot s;
		s.name = "01_aerial";
		s.caption = "But monsters have moved in...";
		s.duration = 6.0f;
		s.still_at = 0.35f;
		s.avoid = false;
		s.setup = [=, this]() {
			cine_a_ = street;
			player_->teleport(ground(spawn - street * 4.0f, 0.2f), std::atan2(street.x, street.z));
		};
		s.tick = [this](float t) { player_->bot_input(cine_a_ * (t > 0.6f ? 1.0f : 0.0f), false, false, false); };
		s.cam = [=, this](float t, float &fov) {
			fov = 50.0f;
			const Vector3 k = kid();
			const float u = ease(t / 6.0f);
			const Vector3 from = k - cine_a_ * (16.0f - 6.0f * u) + side_of(cine_a_) * 5.0f + Vector3(0, 13.0f - 6.0f * u, 0);
			return look(from, k + cine_a_ * 10.0f + Vector3(0, 1.0f, 0));
		};
		cine_.push_back(s);
	}
	// 2. Porch: low behind the kid, walking up to a glowing house to ring the bell.
	if (bells.size() > 0) {
		CineShot s;
		s.name = "02_doorbell";
		s.caption = "One Halloween night, ring every doorbell";
		s.duration = 8.0f; // cut short once the bell rings
		s.still_at = 0.3f;
		s.avoid = false;
		// The best-dressed house: the most Halloween decorations and candy around its front door.
		const int best = decorated[0];
		doorbell_lot = best;
		const Vector3 door = home->to_world(bells[best]);
		// The front path: from the sidewalk end, straight up to the porch steps.
		const Lot &plot = home->gen->context()->lots[best];
		const Vector3 path_start = home->to_world(plot.to_world(Vector3(plot.house.door_x, 0, -0.6f)));
		s.setup = [=, this]() {
			cine_a_ = door;
			cine_b_ = path_start + flat(path_start - door) * 2.5f; // out on the sidewalk, so the walk up reads
			cine_c_ = flat(path_start - door); // back down the path, toward the street
			loose_->set_reach(0.0f); // candy on the path is picked up on touch, without magnet streaks
			player_->teleport(ground(cine_b_, 0.2f), std::atan2(-cine_c_.x, -cine_c_.z));
			// Find a camera spot with a clear view up the whole path to the door, from both ends of its
			// gentle push-in, so it never passes through a gate post, mailbox, fence or hedge.
			cine_d_ = Vector3(1.4f, 3.2f, 0);
			if (PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state()) {
				auto clear = [&](const Vector3 &a, const Vector3 &b) {
					Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(a, b, 1 | 2 | 4);
					Ref<PhysicsRayQueryParameters3D> r = PhysicsRayQueryParameters3D::create(b, a, 1 | 2 | 4);
					return space->intersect_ray(q).is_empty() && space->intersect_ray(r).is_empty();
				};
				const Vector3 up(0, 1.7f, 0);
				const Vector3 targets[3] = { door + cine_c_ * 0.8f + Vector3(0, 0.6f, 0), cine_b_.lerp(door, 0.5f) + Vector3(0, 0.9f, 0), cine_b_ + Vector3(0, 0.9f, 0) };
				bool found = false;
				for (float back : { 3.2f, 4.2f, 2.4f }) {
					for (float side : { 1.4f, -1.4f, 0.7f, -0.7f, 2.2f, -2.2f, 0.0f }) {
						bool ok = true;
						for (float push : { 0.0f, 0.8f }) {
							const Vector3 cam = ground(cine_b_ + cine_c_ * (back - push) + side_of(cine_c_) * side, 0.0f) + up;
							for (const Vector3 &tg : targets) {
								ok = ok && clear(cam, tg);
							}
						}
						if (ok) {
							cine_d_ = Vector3(side, back, 0);
							found = true;
							break;
						}
					}
					if (found) {
						break;
					}
				}
			}
			// Nothing hanging in front of the lens: clear the yard candy close to the camera's line (the
			// rest of the yard's candy stays to frame the house).
			if (Area *ha = streamer_->area(WorldStreamer::area_key(0, 0))) {
				if (CandyField *cf = ha->gen->candy_field()) {
					const Vector3 cam = cine_b_ + cine_c_ * cine_d_.y + side_of(cine_c_) * cine_d_.x;
					for (float u : { 0.0f, 0.15f, 0.3f, 0.45f }) {
						cf->hide_near(ha->to_local(cam.lerp(door, u) + Vector3(0, 1.5f, 0)), 3.0f);
					}
				}
			}
		};
		s.tick = [this](float t) {
			const Vector3 d = cine_a_ - player_->get_global_position();
			const bool there = Vector2(d.x, d.z).length() < 1.3f; // stand still at the door (no dithering)
			player_->bot_input(t > 0.6f && !there ? flat(d) * 0.42f : Vector3(), false, false, false);
			if (there && cine_cut_at_ < 0.0f) {
				cine_cut_at_ = t + 1.1f; // the bell rings and the door opens: cut soon after, don't linger
			}
		};
		s.cam = [this](float t, float &fov) {
			fov = 48.0f;
			// Low behind the kid at the foot of the path, the dressed-up house ahead.
			const Vector3 k = player_->get_global_position();
			const Vector3 from = cine_b_ + cine_c_ * (cine_d_.y - 0.8f * ease(t / 4.0f)) + side_of(cine_c_) * cine_d_.x;
			return look(Vector3(from.x, streamer_->ground(from.x, from.z) + 1.7f, from.z), cine_a_.lerp(k, 0.35f) + Vector3(0, 1.6f, 0));
		};
		cine_.push_back(s);
	}
	// 2b. Trick or treat, up close: the door opens and treats spray out past the camera.
	if (bells.size() > 1) {
		CineShot s;
		s.name = "door";
		s.caption = "Trick or treat!";
		s.duration = 5.0f;
		s.still_at = 0.62f;
		s.avoid = false;
		const int pick = decorated.size() > 1 ? decorated[1] : -1; // the next best-dressed house
		if (pick >= 0) {
			const Vector3 door = home->to_world(bells[pick]);
			// Square on to the porch: the way the house's own front path runs (not toward the spawn).
			const Lot &plot = home->gen->context()->lots[pick];
			const Vector3 path_start = home->to_world(plot.to_world(Vector3(plot.house.door_x, 0, -0.6f)));
			s.setup = [=, this]() {
				cine_a_ = door;
				cine_c_ = flat(path_start - door);
				player_->teleport(ground(door + cine_c_ * 3.4f, 0.2f), std::atan2(-cine_c_.x, -cine_c_.z));
			};
			s.tick = [this](float t) {
				const Vector3 d = cine_a_ - player_->get_global_position();
				const bool there = Vector2(d.x, d.z).length() < 1.0f;
				player_->bot_input(t > 0.4f && !there ? flat(d) * 0.5f : Vector3(), false, false, false);
			};
			s.cam = [this](float t, float &fov) {
				fov = 50.0f;
				// Straight up the steps (the one gap in the porch railing).
				const Vector3 from = cine_a_ + cine_c_ * (3.0f + 0.2f * t) - side_of(cine_c_) * 0.7f + Vector3(0, 1.2f, 0);
				return look(from, cine_a_ + Vector3(0, 1.0f, 0));
			};
			cine_.push_back(s);
		}
	}
	// 2c. A house lights up: every treat collected, its porch glows and fireworks go up.
	if (bells.size() > 0) {
		CineShot s;
		s.name = "lights";
		s.caption = "Light up every house";
		s.duration = 5.0f;
		s.still_at = 0.45f;
		const Vector3 door = home->to_world(bells[doorbell_lot]);
		const uint64_t key = home->key;
		const int lot = doorbell_lot;
		s.avoid = false; // a lane checked clear in setup instead (no snapping through lamp posts)
		s.setup = [=, this]() {
			cine_a_ = door;
			cine_c_ = flat(spawn - door);
			cine_d_ = Vector3();
			const Vector3 kid_at = ground(door + cine_c_ * 7.0f, 0.2f);
			player_->teleport(kid_at, std::atan2(-cine_c_.x, -cine_c_.z));
			// A clear line to the porch, the roof and the kid over the whole dolly (lamp posts, trees).
			const Vector2 lane = cine_lane(door, cine_c_, { 3.0f, -3.0f, 1.5f, -1.5f, 4.5f, -4.5f, 0.0f }, { 15.0f, 13.0f, 17.0f, 11.0f }, 4.0f, 2.4f,
					{ door + cine_c_ * 1.2f + Vector3(0, 0.5f, 0), door + cine_c_ * 1.2f + Vector3(0, 3.5f, 0), kid_at + Vector3(0, 0.8f, 0) });
			cine_b_ = Vector3(lane.x, lane.y, 0);
		};
		s.tick = [=, this](float t) {
			if (t > 0.9f && cine_d_.x == 0.0f) {
				cine_d_.x = 1.0f;
				on_house_cleared(lot, int64_t(key));
			}
			player_->bot_input(Vector3(), false, false, t > 1.4f && t < 1.45f);
		};
		s.cam = [this](float t, float &fov) {
			fov = 50.0f;
			Vector3 from = cine_a_ + cine_c_ * (cine_b_.y - 0.8f * t) + side_of(cine_c_) * cine_b_.x;
			from.y = streamer_->ground(from.x, from.z) + 2.4f;
			return look(from, cine_a_ + Vector3(0, 3.2f, 0));
		};
		cine_.push_back(s);
	}
	// 2d. A trail of candy down the sidewalk, scooped up at a run.
	{
		CineShot s;
		s.name = "candy";
		s.caption = "Scoop up every candy";
		s.duration = 4.5f;
		s.still_at = 0.3f;
		s.setup = [=, this]() {
			cine_a_ = street;
			loose_->set_reach(0.0f); // candy waits to be touched (no magnet streaks)
			const Vector3 start = spawn - street * 9.0f;
			player_->teleport(ground(start, 0.2f), std::atan2(street.x, street.z));
			for (int i = 0; i < 14; ++i) {
				const int tier = i == 13 ? 3 : (i == 9 ? 2 : (i % 4 == 3 ? 1 : 0));
				const Vector3 p = start + street * (2.5f + i * 1.15f);
				loose_->hover(ground(p, 0.85f), candy_kind_of_tier(tier, uint64_t(i * 7 + 3)), 30.0f);
			}
		};
		s.tick = [this](float t) { player_->bot_input(t > 0.3f ? cine_a_ * 0.7f : Vector3(), false, false, false); };
		s.cam = [this](float t, float &fov) {
			fov = 48.0f;
			const Vector3 k = player_->get_global_position();
			return look(k + cine_a_ * 4.8f + side_of(cine_a_) * 1.8f + Vector3(0, 0.7f, 0), k + cine_a_ * 1.2f + Vector3(0, 0.75f, 0));
		};
		cine_.push_back(s);
	}
	// 2e. A jump through an arc of candy, side-on like a platformer.
	{
		CineShot s;
		s.name = "jump";
		s.caption = "Jump for the sweet stuff";
		s.duration = 4.5f;
		s.still_at = 0.27f;
		s.setup = [=, this]() {
			cine_a_ = street;
			cine_b_ = spawn + street * 6.0f; // where the jump starts
			loose_->set_reach(0.0f);
			player_->teleport(ground(cine_b_ - street * 4.5f, 0.2f), std::atan2(street.x, street.z));
			const float ground_y = streamer_->ground(cine_b_.x, cine_b_.z);
			for (int i = 0; i <= 8; ++i) {
				const float u = float(i) / 8.0f;
				Vector3 p = cine_b_ + street * (0.4f + u * 5.2f);
				p.y = ground_y + 0.9f + 2.1f * std::sin(u * float(Math_PI));
				loose_->hover(p, candy_kind_of_tier(i == 4 ? 2 : (i % 3 == 1 ? 1 : 0), uint64_t(i * 5 + 1)), 30.0f);
			}
			cine_d_ = Vector3();
		};
		s.tick = [this](float t) {
			const float along = (player_->get_global_position() - cine_b_).dot(cine_a_);
			const bool jump = along > -0.6f && cine_d_.x == 0.0f;
			if (jump) {
				cine_d_.x = 1.0f;
			}
			player_->bot_input(t > 0.2f ? cine_a_ * 0.75f : Vector3(), jump, along > -0.6f && along < 3.0f, false);
		};
		s.cam = [this](float t, float &fov) {
			fov = 44.0f;
			// Side-on, sliding along with the kid.
			const float along = Math::clamp((player_->get_global_position() - cine_b_).dot(cine_a_), -2.0f, 7.0f);
			const Vector3 mid = cine_b_ + cine_a_ * (along + 0.8f);
			return look(mid - side_of(cine_a_) * 6.5f + Vector3(0, 1.5f, 0), mid + Vector3(0, 1.4f, 0));
		};
		cine_.push_back(s);
	}
	// 2f. Close on the bucket: a shower of candy streaming in.
	{
		CineShot s;
		s.name = "bucket";
		s.caption = "Fill your bucket";
		s.duration = 4.5f;
		s.still_at = 0.3f;
		s.setup = [=, this]() {
			cine_a_ = street;
			cine_d_ = Vector3();
			loose_->set_reach(0.0f);
			player_->teleport(ground(spawn + street * 3.0f + side_of(street) * 2.0f, 0.2f), std::atan2(-street.x, -street.z));
		};
		s.tick = [this](float t) {
			if (t > 0.3f && cine_d_.x == 0.0f) {
				cine_d_.x = 1.0f;
				loose_->fountain(player_->get_global_position() + Vector3(0, 0.4f, 0) + cine_a_ * 1.2f, 18);
			}
			player_->bot_input(Vector3(), false, false, false);
		};
		s.cam = [this](float t, float &fov) {
			fov = 40.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 front = -cine_a_; // the way the kid faces
			return look(k + front * (2.4f - 0.2f * t) + side_of(front) * 1.0f + Vector3(0, 0.9f, 0), k + Vector3(0, 0.7f, 0));
		};
		cine_.push_back(s);
	}
	// Trampoline: dropped onto a backyard trampoline holding jump, rocketing up past the roofs. It
	// has a world of its own (--cine-bounce-seed) picked for a big trampoline in an open yard.
	{
		CineShot s;
		s.name = "bounce";
		s.caption = "Bounce higher than the houses";
		s.duration = 5.0f;
		s.still_at = 0.14f;
		s.seed = cine_bounce_seed_;
		s.setup = [this]() {
			Vector3 home_spawn, street;
			cine_home(home_spawn, street);
			Area *a = streamer_->area(WorldStreamer::area_key(0, 0));
			Vector3 tramp = home_spawn;
			bool found = false;
			for (const char *id : { "trampoline_large", "trampoline_mini" }) {
				for (const PropPlacement &pp : a ? a->gen->context()->props : std::vector<PropPlacement>()) {
					if (pp.id == id) {
						tramp = a->to_world(pp.xf.origin);
						found = true;
						break;
					}
				}
				if (found) {
					break;
				}
			}
			cine_a_ = tramp;
			cine_b_ = flat(home_spawn - tramp); // toward the street
			player_->teleport(tramp + Vector3(0, 3.5f, 0), std::atan2(cine_b_.x, cine_b_.z));
		};
		s.tick = [this](float) { player_->bot_input(Vector3(), false, true, false); }; // held: the super bounce
		s.cam = [this](float t, float &fov) {
			// From the backyard side, wide enough to keep the trampoline and the kid in frame as the
			// kid shoots up past the roofs toward the street.
			fov = 56.0f;
			const Vector3 k = player_->get_global_position();
			const float h = Math::max(0.0f, k.y - cine_a_.y);
			// From the side, across the yard (the back of the lot is hedged).
			const Vector3 from = cine_a_ + side_of(cine_b_) * (5.5f + 0.35f * h) - cine_b_ * 1.5f + Vector3(0, 1.2f + 0.5f * h, 0);
			return look(from, (cine_a_ + Vector3(0, 0.4f, 0)).lerp(k, 0.6f));
		};
		cine_.push_back(s);
	}	// 4. Glide: wings out over the street.
	{
		CineShot s;
		s.name = "04_glide";
		s.caption = "Glide on dragon wings";
		s.duration = 5.0f;
		s.still_at = 0.5f;
		s.setup = [=, this]() {
			cine_a_ = street;
			player_->teleport(ground(spawn - street * 6.0f, 9.0f), std::atan2(street.x, street.z));
		};
		s.tick = [this](float t) { player_->bot_input(cine_a_, t > 0.15f && t < 0.25f, t > 0.3f, false); };
		s.cam = [this](float t, float &fov) {
			fov = 50.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 from = k - cine_a_ * 4.5f + side_of(cine_a_) * (3.5f - 0.6f * t) + Vector3(0, 1.4f, 0);
			return look(from, k + cine_a_ * 5.0f - Vector3(0, 1.2f, 0));
		};
		cine_.push_back(s);
	}
	// 5. Fireball: casting at a monster nest, side-on.
	if (inf && inf->nests_total() > 1) {
		CineShot s;
		s.name = "05_fireball";
		s.caption = "Cast fire with your wand";
		s.duration = 5.0f;
		s.still_at = 0.4f; // 2.0 s: a fireball in flight, 0.1 s after the cast at 1.9 s
		const Vector3 nest = inf->nest_position(near_nests[1]);
		s.setup = [=, this]() {
			cine_a_ = nest;
			cine_c_ = flat(spawn - nest);
			player_->teleport(ground(nest + cine_c_ * 7.0f, 0.3f), std::atan2(-cine_c_.x, -cine_c_.z));
			cine_fire_ = 0.4f;
			cine_aim_on_ = true;
			cine_aim_ = nest + Vector3(0, 0.8f, 0);
		};
		s.tick = [this](float t) {
			bool fire = false;
			if (t >= cine_fire_) {
				cine_fire_ += 0.5f;
				fire = true;
			}
			player_->bot_input(Vector3(), false, false, false, fire);
		};
		s.cam = [this](float t, float &fov) {
			fov = 42.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 from = k + cine_c_ * 2.6f + side_of(cine_c_) * (3.2f - 0.15f * t) + Vector3(0, 1.3f, 0);
			return look(from, k.lerp(cine_a_, 0.6f) + Vector3(0, 1.0f, 0));
		};
		cine_.push_back(s);
	}
	// 6. Thorn nest: the root mass with its glowing heart, the kid burning it back.
	if (inf && inf->hearts_total() > 0) {
		CineShot s;
		s.name = "06_thorns";
		s.caption = "Burn back the thorn nests";
		s.duration = 5.0f;
		s.still_at = 0.55f;
		const Vector3 heart = inf->heart_position(0);
		s.setup = [=, this]() {
			cine_a_ = heart;
			cine_c_ = flat(spawn - heart);
			player_->teleport(ground(heart + cine_c_ * 8.0f, 0.3f), std::atan2(-cine_c_.x, -cine_c_.z));
			cine_fire_ = 0.8f;
			cine_aim_on_ = true; // at the thorn nest, not whatever the auto-aim fancies
			cine_aim_ = heart + Vector3(0, 1.0f, 0);
		};
		s.tick = [this](float t) {
			bool fire = false;
			if (t >= cine_fire_) {
				cine_fire_ += 0.55f;
				fire = true;
			}
			player_->bot_input(Vector3(), false, false, false, fire);
		};
		s.cam = [this](float t, float &fov) {
			fov = 46.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 from = k + cine_c_ * 3.2f + side_of(cine_c_) * (2.2f + 0.2f * t) + Vector3(0, 0.9f, 0);
			return look(from, cine_a_ + Vector3(0, 1.1f, 0));
		};
		cine_.push_back(s);
	}
	// 7. Candy fountain: a nest bursts and rains candy around the kid.
	if (inf && inf->nests_total() > 0) {
		CineShot s;
		s.name = "07_fountain";
		s.caption = "Free the neighborhood";
		s.duration = 5.5f;
		s.still_at = 0.42f;
		const Vector3 nest = inf->nest_position(near_nests[0]);
		s.setup = [=, this]() {
			cine_a_ = nest;
			cine_c_ = flat(spawn - nest);
			cine_d_ = Vector3(); // 0: not burst yet
			player_->teleport(ground(nest + cine_c_ * 5.0f, 0.3f), std::atan2(-cine_c_.x, -cine_c_.z));
		};
		s.tick = [this](float t) {
			if (t > 0.8f && cine_d_.x == 0.0f) {
				cine_d_.x = 1.0f;
				std::vector<Damageable *> targets;
				gather_targets(targets);
				Damageable *best = nullptr;
				float best_d = 3.0f;
				for (Damageable *d : targets) {
					const float dd = d->target_point().distance_to(cine_a_ + Vector3(0, 0.8f, 0));
					if (!d->is_monster() && dd < best_d) {
						best_d = dd;
						best = d;
					}
				}
				if (best) {
					best->take_hit(999.0f, player_->get_global_position());
				}
			}
			if (t > 3.2f && cine_d_.y == 0.0f) {
				// Then every other nest and thorn nest goes too: the vines wither and the area is free, so
				// the candy shots that follow have clear streets.
				cine_d_.y = 1.0f;
				std::vector<Damageable *> targets;
				gather_targets(targets);
				for (Damageable *d : targets) {
					if (!d->is_monster() && d->target_alive()) {
						d->take_hit(999.0f, d->target_point() + Vector3(0, 1.0f, 0));
					}
				}
			}
			const Vector3 d = cine_a_ - player_->get_global_position();
			const bool there = Vector2(d.x, d.z).length() < 2.2f;
			player_->bot_input(t > 1.8f && !there ? flat(d) * 0.6f : Vector3(), false, false, false);
		};
		s.cam = [this](float t, float &fov) {
			fov = 50.0f;
			const Vector3 from = cine_a_ + cine_c_ * 7.5f + side_of(cine_c_) * 2.0f + Vector3(0, 1.6f + 0.2f * t, 0);
			return look(from, cine_a_ + cine_c_ * 2.5f + Vector3(0, 2.2f, 0));
		};
		cine_.push_back(s);
	}
	// 9. A town: running down the high street between the lit shop fronts.
	{
		Vector3 town_spawn;
		float town_yaw = 0.0f;
		bool found = false;
		for (int r = 1; r <= 3 && !found; ++r) {
			for (int i = -r; i <= r && !found; ++i) {
				for (int j = -r; j <= r && !found; ++j) {
					const RegionPlan &rp = streamer_->plan().region(i, j);
					if (rp.kind == RegionKind::Town) {
						town_spawn = rp.spawn;
						town_yaw = rp.spawn_yaw;
						found = true;
					}
				}
			}
		}
		cine_town_found_ = found;
		if (found) {
			CineShot s;
			s.name = "08_town";
			s.caption = "Explore towns, neighborhoods, and woods";
			s.duration = 5.0f;
			s.still_at = 0.3f;
			s.setup = [=, this]() {
				streamer_->update(town_spawn, true);
				Vector3 d(std::sin(town_yaw), 0, std::cos(town_yaw));
				if (Area *a = streamer_->area_at(town_spawn, 4.0f)) {
					const float y = a->gen->street_yaw_at(a->to_local(town_spawn));
					d = Vector3(-std::sin(y), 0, -std::cos(y));
				}
				cine_a_ = d;
				cine_town_ = town_spawn;
				cine_town_dir_ = d;
				player_->teleport(ground(town_spawn - d * 5.0f, 0.2f), std::atan2(d.x, d.z));
			};
			s.tick = [this](float t) { player_->bot_input(cine_a_ * (t > 0.3f ? 1.0f : 0.0f), false, false, false); };
			s.cam = [this](float t, float &fov) {
				fov = 46.0f;
				const Vector3 k = player_->get_global_position();
				const Vector3 from = k + cine_a_ * (6.0f - 0.5f * t) + side_of(cine_a_) * 1.2f + Vector3(0, 1.1f, 0);
				return look(from, k + Vector3(0, 1.3f, 0) - cine_a_ * 3.0f);
			};
			cine_.push_back(s);
		}
	}
	// 9. The Pumpkin Cruiser, headlights on, coming down the town's high street at the camera.
	if (cine_town_found_) {
		CineShot s;
		s.name = "09_car";
		s.caption = "Cruise the night";
		s.duration = 5.0f;
		s.still_at = 0.55f;
		s.setup = [=, this]() {
			cine_a_ = cine_town_dir_;
			const Vector3 start = ground(cine_town_ - cine_town_dir_ * 16.0f, 0.3f);
			up_.car = 1;
			player_->teleport(start, std::atan2(cine_a_.x, cine_a_.z));
			toggle_car();
			if (car_) {
				car_->place(start + Vector3(0, 0.1f, 0), std::atan2(cine_a_.x, cine_a_.z));
				car_->set_auto(true, cine_a_);
			}
		};
		s.tick = [this](float) {
			if (car_) {
				car_->set_auto(true, cine_a_);
			}
		};
		s.cam = [this](float t, float &fov) {
			fov = 44.0f;
			const Vector3 c = car_ ? car_->get_global_position() : player_->get_global_position();
			const Vector3 from = c + cine_a_ * (7.0f - 1.2f * t) + side_of(cine_a_) * 2.2f + Vector3(0, 0.6f, 0);
			return look(from, c + Vector3(0, 0.8f, 0));
		};
		cine_.push_back(s);
	}
	// 10. Finale: a wand spin in the street, the camera rising to the moon and the purple sky.
	{
		CineShot s;
		s.name = "10_finale";
		s.caption = "Halloween: Trick or Treat!"; // shown as the logo
		s.sub = "A Halloween night adventure";
		s.duration = 7.0f;
		s.still_at = 0.22f;
		s.setup = [=, this]() {
			Vector3 sp, st;
			cine_home(sp, st);
			streamer_->update(sp, true);
			cine_a_ = st;
			player_->teleport(ground(sp, 0.2f), std::atan2(st.x, st.z));
		};
		s.tick = [this](float t) {
			const bool spin = (t > 0.5f && t < 0.55f) || (t > 2.2f && t < 2.25f);
			player_->bot_input(Vector3(), false, false, spin);
		};
		s.cam = [this](float t, float &fov) {
			fov = 45.0f;
			const Vector3 k = player_->get_global_position();
			const float u = ease(t / 7.0f);
			const Vector3 from = k + cine_a_ * (2.6f + 9.0f * u) + side_of(cine_a_) * (0.8f + 2.0f * u) + Vector3(0, 0.9f + 6.0f * u, 0);
			return look(from, k + Vector3(0, 0.75f + 5.0f * u, 0) - cine_a_ * (4.0f * u));
		};
		cine_.push_back(s);
	}
	// Monsters: a pumpkin hopper, a ghost, a spider and a candy-corn gremlin out in the street.
	{
		CineShot s;
		s.name = "monsters";
		s.caption = "Monsters roam the night";
		s.duration = 5.5f;
		s.still_at = 0.42f;
		s.setup = [=, this]() {
			cine_a_ = street;
			const Vector3 k = ground(spawn + street * 1.0f, 0.2f);
			player_->teleport(k, std::atan2(street.x, street.z));
			const MonsterType types[4] = { MonsterType::PumpkinHopper, MonsterType::Ghost, MonsterType::Spider, MonsterType::Bat };
			const float across[4] = { -3.2f, -1.0f, 1.2f, 3.3f };
			const float ahead[4] = { 7.0f, 8.5f, 7.6f, 6.4f };
			for (int i = 0; i < 4; ++i) {
				Vector3 p = k + street * ahead[i] + side_of(street) * across[i];
				p.y = streamer_->ground(p.x, p.z) + (tuning(types[i]).flying ? 1.3f : 0.3f);
				Monster *m = memnew(Monster);
				wild_root_->add_child(m);
				m->set_global_position(p);
				// Fully active (hopping, flapping, scuttling about), just not after the kid this shot.
				m->init(types[i], &mworld_, nullptr, uint64_t(1000 + i), Rect2(p.x - 3.5f, p.z - 3.5f, 7.0f, 7.0f));
				m->on_gone = [this](Monster *gone) { wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end()); };
				wild_.push_back(m);
			}
			cine_fire_ = 2.7f;
		};
		s.tick = [this](float t) {
			bool fire = false;
			if (t >= cine_fire_) {
				cine_fire_ += 0.5f;
				fire = true;
			}
			player_->bot_input(Vector3(), false, false, false, fire);
		};
		s.cam = [this](float t, float &fov) {
			fov = 50.0f;
			const Vector3 k = player_->get_global_position();
			return look(k - cine_a_ * (3.4f - 0.2f * t) + side_of(cine_a_) * 1.6f + Vector3(0, 1.2f, 0), k + cine_a_ * 7.0f + Vector3(0, 0.8f, 0));
		};
		cine_.push_back(s);
	}
	// Stretches of plain play between the scripted shots: the normal camera and HUD, the kid
	// steering like a player would.
	{
		CineShot s;
		s.name = "play_candy";
		s.gameplay = true;
		s.trailer = false; // cut from the trailer (it sat just before the monsters reveal); still a gameplay screenshot
		s.duration = 5.0f;
		s.still_at = 0.6f;
		s.setup = [=, this]() {
			cine_a_ = street;
			cine_d_ = Vector3(-10.0f, 0, 0); // x: when the kid last jumped
			player_->teleport(ground(spawn - street * 2.0f + side_of(street) * 1.5f, 0.2f), std::atan2(street.x, street.z));
		};
		s.tick = [this](float t) {
			// Keep running on down the street, drifting toward candy that's ahead and hopping for the
			// raised pieces. Never doubles back (chasing whatever is nearest made the kid dither).
			const Vector3 k = player_->get_global_position();
			Vector3 wish = cine_a_;
			bool jump = false;
			if (Area *a = streamer_->area_at(k, 4.0f)) {
				if (CandyField *cf = a->gen->candy_field()) {
					Vector3 local;
					if (cf->nearest_within(a->to_local(k), 3.0f, local, nullptr)) {
						const Vector3 d = a->to_world(local) - k;
						const float ahead = Vector2(d.x, d.z).normalized().dot(Vector2(cine_a_.x, cine_a_.z));
						const float dist = Vector2(d.x, d.z).length();
						if (ahead > 0.55f && dist < 9.0f) {
							wish = (cine_a_ + flat(d) * 0.8f).normalized(); // steer, still forward
							jump = d.y > 1.1f && dist < 2.4f && cine_d_.x < t - 0.9f;
							if (jump) {
								cine_d_.x = t;
							}
						}
					}
				}
			}
			player_->bot_input(wish * 0.85f, jump, jump, false);
		};
		s.cam = [](float, float &fov) { return Transform3D(); };
		cine_.push_back(s);
	}
	{
		CineShot s;
		s.name = "play_fight";
		s.gameplay = true;
		s.duration = 5.0f;
		s.still_at = 0.55f;
		s.setup = [=, this]() {
			cine_a_ = street;
			cine_aggro_ = true; // the monsters come at the kid like they do in play
			cine_fire_ = 0.5f;
			const Vector3 k = ground(spawn + street * 4.0f, 0.2f);
			cine_b_ = k + street * 8.0f; // where the monsters gather
			player_->teleport(k, std::atan2(street.x, street.z));
			const MonsterType types[3] = { MonsterType::PumpkinHopper, MonsterType::Bat, MonsterType::Spider };
			for (int i = 0; i < 3; ++i) {
				Vector3 p = k + street * (7.0f + i) + side_of(street) * (float(i) - 1.0f) * 2.5f;
				p.y = streamer_->ground(p.x, p.z) + (tuning(types[i]).flying ? 1.3f : 0.3f);
				Monster *m = memnew(Monster);
				wild_root_->add_child(m);
				m->set_global_position(p);
				m->init(types[i], &mworld_, nullptr, uint64_t(2000 + i), Rect2(p.x - 10.0f, p.z - 10.0f, 20.0f, 20.0f));
				m->on_gone = [this](Monster *gone) { wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end()); };
				wild_.push_back(m);
			}
		};
		s.tick = [this](float t) {
			bool fire = false;
			if (t >= cine_fire_) {
				cine_fire_ += 0.55f;
				fire = true;
			}
			// Advance on the monsters while casting, then hold ground (no back-and-forth strafing).
			const Vector3 to = cine_b_ - player_->get_global_position();
			const bool close = Vector2(to.x, to.z).length() < 4.5f;
			player_->bot_input(close ? Vector3() : cine_a_ * 0.45f, false, false, false, fire);
		};
		s.cam = [](float, float &fov) { return Transform3D(); };
		cine_.push_back(s);
	}
	{
		CineShot s;
		s.name = "play_drive";
		s.gameplay = true;
		s.duration = 4.5f;
		s.still_at = 0.5f;
		s.setup = [this]() {
			cine_a_ = cine_town_dir_;
			const Vector3 start = cine_town_ + cine_town_dir_ * 4.0f;
			Vector3 p = start;
			p.y = streamer_->ground(p.x, p.z) + 0.3f;
			player_->teleport(p, std::atan2(cine_a_.x, cine_a_.z));
			up_.car = 1;
			toggle_car();
			if (car_) {
				car_->place(p + Vector3(0, 0.1f, 0), std::atan2(cine_a_.x, cine_a_.z));
				car_->set_auto(true, cine_a_);
			}
		};
		s.tick = [this](float t) {
			if (car_) {
				// Straight on down the high street (a left-right weave read as dithering).
				car_->set_auto(true, cine_a_);
			}
		};
		s.cam = [](float, float &fov) { return Transform3D(); };
		cine_.push_back(s);
	}
	// Menus (gameplay screenshots only): the title screen, and the shop's Car Paint tab.
	{
		CineShot s;
		s.name = "menu";
		s.play_only = true;
		s.duration = 3.0f;
		s.still_at = 0.8f;
		s.setup = [this]() { show_main_menu(); };
		s.tick = [this](float t) {
			if (t > 2.9f && in_menu_) {
				in_menu_ = false;
				menu_open_ = false;
				camera_->set_attract(false);
				call_ui("_close_menu", Array::make(false));
				call_ui("cine_mode", Array::make(false));
			}
		};
		s.cam = [](float, float &fov) { return Transform3D(); };
		cine_.push_back(s);
	}
	{
		CineShot s;
		s.name = "shop";
		s.play_only = true;
		s.duration = 3.0f;
		s.still_at = 0.7f;
		s.setup = [this]() {
			up_.car = 1;
			wallet_ = Math::max(wallet_, 480);
			shop_tab_ = 1;
			shop_focus_ = "paint:4";
			open_shop();
		};
		s.tick = [this](float t) {
			if (t > 2.9f && menu_open_) {
				call_ui("_close_menu", Array::make(false));
				menu_open_ = false;
				shop_tab_ = 0;
			}
		};
		s.cam = [](float, float &fov) { return Transform3D(); };
		cine_.push_back(s);
	}
	// The order they play in: houses, then the fight that frees the neighborhood, then candy fun in
	// the cleared streets, then the wider world.
	{
		static const char *ORDER[] = { "menu", "doorbell", "door", "lights", "play_candy", "aerial", "fireball", "monsters", "play_fight", "thorns", "fountain", "candy", "jump", "bucket", "shop", "glide", "town", "car", "play_drive", "bounce", "finale" };
		auto rank = [](const std::string &n) {
			const size_t u = n.find('_');
			const std::string base = (u != std::string::npos && u <= 3 && std::isdigit((unsigned char)n[0])) ? n.substr(u + 1) : n;
			for (int i = 0; i < 21; ++i) {
				if (base == ORDER[i]) {
					return i;
				}
			}
			return 99;
		};
		std::stable_sort(cine_.begin(), cine_.end(), [&](const CineShot &a, const CineShot &b) { return rank(a.name) < rank(b.name); });
	}
	if (!cine_play_) {
		cine_.erase(std::remove_if(cine_.begin(), cine_.end(), [](const CineShot &s) { return s.play_only || !s.trailer; }), cine_.end());
	}
	// Number the shots in playing order (stills are saved under these names).
	for (size_t i = 0; i < cine_.size(); ++i) {
		std::string base = cine_[i].name;
		const size_t u = base.find('_');
		if (u != std::string::npos && u <= 3 && std::isdigit((unsigned char)base[0])) {
			base = base.substr(u + 1);
		}
		char buf[8];
		snprintf(buf, sizeof(buf), "%02d_", int(i + 1));
		cine_[i].name = buf + base;
	}
	if (cine_mode_ != "reel") {
		// A single named shot (by its name or number).
		std::vector<CineShot> keep;
		for (const CineShot &s : cine_) {
			const String n(s.name.c_str());
			if (n == cine_mode_ || n.begins_with(cine_mode_ + "_") || n.ends_with("_" + cine_mode_)) {
				keep.push_back(s);
			}
		}
		cine_ = keep;
	}
	UtilityFunctions::print("CINE shots=", int(cine_.size()));
}

void FestivalGame::cine_begin(int i) {
	cine_i_ = i;
	cine_t_ = 0.0f;
	cine_still_ = false;
	cine_fading_ = false;
	if (i >= int(cine_.size())) {
		UtilityFunctions::print("CINE done");
		get_tree()->quit();
		return;
	}
	CineShot &s = cine_[i];
	const int64_t want = s.seed ? s.seed : cine_base_seed_;
	if (want != seed_) {
		start_world(want); // this shot has a world of its own (or the reel's world comes back)
	}
	clear_wild(); // showcase monsters from an earlier shot
	cine_reset_player();
	if (s.setup) {
		s.setup();
	}
	if (cine_play_ || s.gameplay) {
		if (s.gameplay && !cine_play_) {
			call_ui("cine_mode", Array::make(false)); // the HUD is back for a stretch of plain play
			call_ui("fade", Array::make(false));
		}
		// Gameplay screenshots: the normal camera rig, behind the kid the way they face, and the HUD.
		camera_->set_cinematic(false);
		camera_->face_direction(player_->facing_dir());
		camera_->snap();
		UtilityFunctions::print("CINE shot ", String(s.name.c_str()), " (gameplay camera)");
		return;
	}
	camera_->set_cinematic(true);
	float fov = 45.0f;
	camera_->set_cinematic_pose(s.cam(0.0f, fov), fov);
	call_ui("cine_mode", Array::make(true));
	if (!cine_clean_) {
		call_ui("fade", Array::make(false));
		if (!s.caption.empty()) {
			call_ui("cine_caption", Array::make(String(s.caption.c_str()), String(s.sub.c_str()), s.duration));
		}
	}
	UtilityFunctions::print("CINE shot ", String(s.name.c_str()));
}

void FestivalGame::cine_step(float dt) {
	if (cine_.empty() && cine_i_ < 0) {
		if (frame_ < 20) {
			return; // let the world settle first
		}
		cine_build();
		if (!cine_out_.is_empty()) {
			DirAccess::make_dir_recursive_absolute(cine_out_);
		}
		cine_begin(0);
		return;
	}
	if (cine_i_ < 0 || cine_i_ >= int(cine_.size())) {
		return;
	}
	CineShot &s = cine_[cine_i_];
	cine_t_ += dt;
	if (s.tick) {
		s.tick(cine_t_);
	}
	float fov = 45.0f;
	const bool rig = cine_play_ || s.gameplay; // the normal game camera
	Transform3D xf = rig ? Transform3D() : s.cam(cine_t_, fov);
	if (s.avoid && !rig) {
		// Keep the camera out of walls, hedges and vines: pull it in front of whatever is between it
		// and the kid (or the car).
		const Vector3 subject = (car_ && car_->driving() ? car_->get_global_position() : player_->get_global_position()) + Vector3(0, 0.9f, 0);
		if (PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state()) {
			Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(subject, xf.origin, 1 | 4);
			const Dictionary hit = space->intersect_ray(q);
			if (!hit.is_empty()) {
				const Vector3 h = hit["position"];
				xf.origin = h + (subject - h).normalized() * 0.35f;
			}
		}
	}
	if (!rig) {
		camera_->set_cinematic_pose(xf, fov);
	}
	if (!cine_still_ && cine_t_ >= s.duration * s.still_at) {
		cine_still_ = true;
		if (!cine_out_.is_empty()) {
			Ref<Image> img = get_viewport()->get_texture()->get_image();
			if (img.is_valid()) {
				const String path = cine_out_.path_join(String(s.name.c_str()) + ".png");
				img->save_png(path);
				UtilityFunctions::print("CINE still ", path);
			}
		}
	}
	if (cine_cut_at_ > 0.0f && cine_cut_at_ < s.duration) {
		s.duration = cine_cut_at_;
	}
	// No candy-corn gremlins on camera (the trailer shows the other monsters).
	{
		std::vector<Damageable *> all;
		gather_targets(all);
		for (Damageable *dm : all) {
			if (dm->is_monster()) {
				Monster *m = static_cast<Monster *>(dm);
				if (m->type() == MonsterType::Gremlin && m->is_visible()) {
					m->set_visible(false);
				}
			}
		}
	}
	if (!cine_clean_ && !cine_fading_ && cine_t_ > s.duration - 0.4f && cine_i_ + 1 < int(cine_.size())) {
		cine_fading_ = true;
		call_ui("fade", Array::make(true));
	}
	if (cine_t_ >= s.duration) {
		cine_begin(cine_i_ + 1);
	}
}

} // namespace tot
