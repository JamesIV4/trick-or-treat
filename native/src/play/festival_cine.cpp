// Cinematic director: scripted showcase shots of the home neighborhood (and a town) for
// screenshots and the trailer. Each shot places the kid, drives them like the autopilot does
// (run, jump, glide, cast, drive), and flies the camera on its own path; the camera rig, the HUD
// and the occluder cutout are out of the way. Run with the movie writer for the trailer:
//   godot --path game --resolution 1920x1080 --fixed-fps 30 --write-movie trailer.avi -- --cine=reel
// or for stills (one per shot, saved at the shot's best moment):
//   godot --path game --resolution 2560x1440 -- --cine=reel --cine-out=C:/shots --cine-clean

#include "play/festival_game.h"

#include <godot_cpp/classes/camera_attributes_practical.hpp>

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
	up_.fire = 0;
	up_.triple = 0;
	loose_->set_reach(1.6f);
	cine_aim_on_ = false;
	cine_cut_at_ = -1.0f;
	cine_aggro_ = false;
	player_->bot_input(Vector3(), false, false, false, false);
}

// The area's houses ranked by how well dressed they are: Halloween props near the front door, plus the
// candy around it (houses right at the spawn are left for the street shots).
static std::vector<int> dressed_houses(Area *home, const Vector3 &spawn) {
	static const char *DECOR[] = { "jack", "pumpkin", "lantern", "skeleton", "ghost", "inflatable", "scarecrow", "hay", "mum", "corn",
		"tomb", "grave", "witch", "spider", "bat", "banner", "wreath", "candle", "cauldron", "gourd" };
	const PackedVector3Array bells = home->gen->get_doorbells();
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
	std::vector<int> decorated;
	for (const auto &r : ranked) {
		decorated.push_back(r.second);
	}
	if (decorated.empty() && bells.size() > 0) {
		decorated.push_back(0);
	}
	return decorated;
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
		cine_bounce_seed_ = 69; // a big trampoline in an open backyard, clear of the hedge (re-picked after the 2026-10-26 trampoline clearance fix)
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
	const std::vector<int> decorated = dressed_houses(home, spawn);
	// 1. Aerial: a crane shot down the street, the kid running along the magic trail.
	{
		CineShot s;
		s.name = "01_aerial";
		s.caption = "But monsters have moved in...";
		s.duration = 6.0f;
		s.still_at = 0.35f;
		s.avoid = false;
		s.seed = 103; // picked from rendered options: a bending street with the nests strung along it
		// The way the street runs at p, turned to carry on along `along` (streets bend; the run follows them).
		auto street_on = [this](const Vector3 &p, const Vector3 &along) {
			Area *a = streamer_->area_at(p, 4.0f);
			if (!a) {
				return along;
			}
			const float y = a->gen->street_yaw_at(a->to_local(p));
			const Vector3 d(-std::sin(y), 0, -std::cos(y));
			return d.dot(along) < 0.0f ? -d : d;
		};
		s.setup = [=, this]() {
			Vector3 sp, st;
			cine_home(sp, st);
			cine_a_ = st;
			Vector3 p = sp - st * 4.0f;
			p.y = streamer_->ground(p.x, p.z) + 0.2f;
			player_->teleport(p, std::atan2(st.x, st.z));
			// Four monster nests down the street ahead, either side of the kid's run, each a different
			// kind with three of its monsters about it: the neighborhood has been moved in on. Placed along
			// the street as it bends, at the road's edges.
			const MonsterType kinds[4] = { MonsterType::PumpkinHopper, MonsterType::Ghost, MonsterType::Spider, MonsterType::Skeleton };
			const float ahead[4] = { 12.0f, 18.0f, 24.0f, 30.0f };
			const float across[4] = { 3.4f, -3.4f, 3.4f, -3.4f };
			std::vector<std::pair<Vector3, Vector3>> path; // point and street direction, every metre
			Vector3 q = p, d = st;
			for (int m = 0; m <= 32; ++m) {
				d = street_on(q, d);
				path.push_back({ q, d });
				q += d;
			}
			for (int i = 0; i < 4; ++i) {
				const auto &[on, dir] = path[int(ahead[i])];
				Vector3 at = on + side_of(dir) * across[i];
				at.y = streamer_->ground(at.x, at.z);
				MonsterNest *n = memnew(MonsterNest);
				wild_root_->add_child(n);
				n->set_global_position(at);
				n->init(kinds[i], &mworld_, uint64_t(2000 + i)); // no spawn callback: just these three
				cine_props_.push_back(n);
				for (int j = 0; j < 3; ++j) {
					const float a = float(Math_TAU) * (float(j) / 3.0f + 0.1f * float(i));
					Vector3 m_at = at + Vector3(std::cos(a), 0, std::sin(a)) * 2.0f;
					m_at.y = streamer_->ground(m_at.x, m_at.z) + (tuning(kinds[i]).flying ? 1.3f : 0.3f);
					Monster *m = memnew(Monster);
					wild_root_->add_child(m);
					m->set_global_position(m_at);
					m->init(kinds[i], &mworld_, nullptr, uint64_t(3000 + i * 3 + j), Rect2(at.x - 2.6f, at.z - 2.6f, 5.2f, 5.2f));
					m->on_gone = [this](Monster *gone) { wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end()); };
					wild_.push_back(m);
				}
			}
		};
		s.tick = [=, this](float t) {
			// Along the street as it bends (the camera swings round with it), past the nests.
			cine_a_ = cine_a_.lerp(street_on(kid(), cine_a_), 0.08f).normalized();
			player_->bot_input(cine_a_ * (t > 0.6f ? 0.75f : 0.0f), false, false, false);
		};
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
		s.vines = false;
		s.caption = "On Halloween night, ring every doorbell";
		s.duration = 8.0f; // cut short once the bell rings
		s.still_at = 0.3f;
		s.avoid = false;
		s.seed = 109; // a world with a well-dressed house and an open yard (picked from rendered options)
		doorbell_lot = decorated[0];
		s.setup = [=, this]() {
			// The best-dressed houses of this shot's world (the most Halloween decorations and candy around
			// the front door), each with its front path from the sidewalk end straight up to the porch steps.
			// The next best-dressed house is left for the door close-up.
			Vector3 sp, st;
			cine_home(sp, st);
			Area *ha = streamer_->area(WorldStreamer::area_key(0, 0));
			if (!ha) {
				return;
			}
			const PackedVector3Array hb = ha->gen->get_doorbells();
			const std::vector<int> ranked = dressed_houses(ha, sp);
			std::vector<std::pair<Vector3, Vector3>> houses;
			for (int i = 0; i < int(ranked.size()) && houses.size() < 5; ++i) {
				if (i == 1 && ranked.size() > 2) {
					continue;
				}
				const Lot &plot = ha->gen->context()->lots[ranked[i]];
				houses.push_back({ ha->to_world(hb[ranked[i]]), ha->to_world(plot.to_world(Vector3(plot.house.door_x, 0, -0.6f))) });
			}
			// The first house with a camera spot that sees up the whole path to the door, from both ends of
			// its gentle push-in, and has the frame's edges open too: no gate post, mailbox, fence or hedge
			// in the way, and no tree trunk or parked car filling a side of the picture.
			const Vector3 up(0, 1.7f, 0);
			bool found = false;
			for (const auto &[door, path_start] : houses) {
				cine_a_ = door;
				cine_b_ = path_start + flat(path_start - door) * 2.5f; // out on the sidewalk, so the walk up reads
				cine_c_ = flat(path_start - door); // back down the path, toward the street
				cine_d_ = Vector3(1.4f, 4.2f, 0);
				const Vector3 sd = side_of(cine_c_);
				const Vector3 mid = cine_b_.lerp(door, 0.5f);
				const Vector3 targets[] = { door + cine_c_ * 0.8f + Vector3(0, 0.6f, 0), mid + Vector3(0, 0.9f, 0), cine_b_ + Vector3(0, 0.9f, 0),
					door + cine_c_ * 1.5f + sd * 4.0f + Vector3(0, 1.2f, 0), door + cine_c_ * 1.5f - sd * 4.0f + Vector3(0, 1.2f, 0),
					door + Vector3(0, 3.5f, 0), mid + sd * 2.5f + Vector3(0, 0.6f, 0), mid - sd * 2.5f + Vector3(0, 0.6f, 0) };
				for (float back : { 4.2f, 5.0f, 3.4f }) {
					for (float side : { 1.4f, -1.4f, 0.7f, -0.7f, 2.2f, -2.2f, 0.0f }) {
						bool ok = true;
						for (float push : { 0.0f, 0.8f }) {
							const Vector3 cam = ground(cine_b_ + cine_c_ * (back - push) + sd * side, 0.0f) + up;
							for (const Vector3 &tg : targets) {
								ok = ok && cine_clear(cam, tg);
							}
							// Nothing standing just off to either side of the lens either (a lamp post or a tree
							// trunk right at the edge of the picture).
							for (float sx : { -1.8f, 1.8f }) {
								for (float h : { 0.0f, 1.6f }) {
									ok = ok && cine_clear(cam, cam - cine_c_ * 2.6f + sd * sx + Vector3(0, h, 0));
								}
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
				if (found) {
					break;
				}
			}
			UtilityFunctions::print("CINE doorbell: house at ", cine_a_, found ? " (clear)" : " (none clear)", " cam ", cine_d_);
			loose_->set_reach(0.0f); // candy on the path is picked up on touch, without magnet streaks
			player_->teleport(ground(cine_b_, 0.2f), std::atan2(-cine_c_.x, -cine_c_.z));
			// Nothing hanging in front of the lens: clear the yard candy close to the camera's line (the
			// rest of the yard's candy stays to frame the house).
			if (Area *ha = streamer_->area(WorldStreamer::area_key(0, 0))) {
				if (CandyField *cf = ha->gen->candy_field()) {
					const Vector3 cam = cine_b_ + cine_c_ * cine_d_.y + side_of(cine_c_) * cine_d_.x;
					for (float u : { 0.0f, 0.15f, 0.3f, 0.45f }) {
						cf->hide_near(ha->to_local(cam.lerp(cine_a_, u) + Vector3(0, 1.5f, 0)), 3.0f);
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
		s.vines = false;
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
		s.vines = false;
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
		s.vines = false;
		s.caption = "Scoop up every candy";
		s.duration = 4.5f;
		s.still_at = 0.3f;
		s.setup = [=, this]() {
			cine_a_ = street;
			loose_->set_reach(0.0f); // candy waits to be touched (no magnet streaks)
			const Vector3 start = spawn - street * 9.0f;
			// The camera leads the kid down the street: pick the side offset whose whole path (about 22 m)
			// misses lamp posts and trees, with the kid in clear view the whole way.
			cine_d_ = Vector3(1.8f, 0, 0);
			for (float side : { 1.8f, -1.8f, 1.0f, -1.0f, 2.6f, -2.6f, 0.0f }) {
				bool clear = true;
				Vector3 prev;
				for (int u = 0; u <= 24 && clear; u += 2) {
					const Vector3 kid = ground(start + street * float(u), 0.75f);
					const Vector3 cam = kid + street * 4.8f + side_of(street) * side;
					clear = cine_clear(cam, kid) && (u == 0 || cine_clear(prev, cam));
					prev = cam;
				}
				if (clear) {
					cine_d_.x = side;
					break;
				}
			}
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
			return look(k + cine_a_ * 4.8f + side_of(cine_a_) * cine_d_.x + Vector3(0, 0.7f, 0), k + cine_a_ * 1.2f + Vector3(0, 0.75f, 0));
		};
		cine_.push_back(s);
	}
	// 2e. A jump through an arc of candy, side-on like a platformer.
	{
		CineShot s;
		s.name = "jump";
		s.vines = false;
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
		s.vines = false;
		s.caption = "Fill your bucket";
		s.duration = 4.5f;
		s.still_at = 0.3f;
		s.seed = 104; // picked from rendered options
		// The camera holds its path whatever passes it, and anything between it and the kid (a lamp
		// post, a tree) dithers away as in play.
		s.avoid = false;
		s.cutout = true;
		// A real pickup: the kid walks toward the camera through a cluster of the street's own candy,
		// which flies into the bucket (no candy spawned for the shot).
		s.setup = [=, this]() {
			Vector3 spawn, street;
			cine_home(spawn, street); // at run time: the shot can have a world of its own
			cine_a_ = street;
			Vector3 best = spawn + street * 3.0f;
			float best_score = -1e9f;
			// The home area as it is now: shots with their own worlds reload it, so a pointer taken
			// when the reel was built would be stale.
			Area *ha = streamer_->area(WorldStreamer::area_key(0, 0));
			if (!ha) {
				player_->teleport(ground(best, 0.2f), std::atan2(-street.x, -street.z));
				return;
			}
			const GenContext &gctx = *ha->gen->context();
			for (const CandySpec &c : gctx.candy) {
				const Vector3 w = ha->to_world(c.pos);
				if (w.y - streamer_->ground(w.x, w.z) > 1.0f || w.distance_to(spawn) > 40.0f) {
					continue; // only candy a kid can walk through, near home
				}
				int near = 0;
				for (const CandySpec &o : gctx.candy) {
					const Vector3 ow = ha->to_world(o.pos);
					near += ow.distance_to(w) < 2.5f && ow.y - streamer_->ground(ow.x, ow.z) < 1.0f ? 1 : 0;
				}
				const float score = float(near) - w.distance_to(spawn) * 0.02f;
				if (score > best_score) {
					best_score = score;
					best = w;
				}
			}
			cine_b_ = best;
			const Vector3 kid_at = ground(best + street * 2.8f, 0.2f);
			player_->teleport(kid_at, std::atan2(-street.x, -street.z));
			// The camera's side: one with nothing (a lamp post, a tree trunk, a parked car) between it and
			// the kid over the whole walk.
			const Vector3 front = -street;
			const Vector3 kid_end = kid_at + front * 2.6f * 0.45f * 4.5f; // where the walk ends (see tick)
			cine_d_ = Vector3(1.8f, 0, 0);
			for (float side : { 1.8f, -1.8f, 1.1f, -1.1f, 2.6f, -2.6f }) {
				bool ok = true;
				for (const Vector3 &k : { kid_at, kid_at.lerp(kid_end, 0.5f), kid_end }) {
					const Vector3 cam = k + front * 4.6f + side_of(front) * side + Vector3(0, 1.4f, 0);
					ok = ok && cine_clear(cam, k + Vector3(0, 0.7f, 0)) && cine_clear(cam, k + Vector3(0, 1.3f, 0));
				}
				if (ok) {
					cine_d_.x = side;
					break;
				}
			}
		};
		s.tick = [this](float t) {
			// Walk on through the candy toward the camera, then stop with the bucket full of it.
			player_->bot_input(t < 2.6f ? -cine_a_ * 0.45f : Vector3(), false, false, false);
		};
		s.cam = [this](float t, float &fov) {
			fov = 40.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 front = -cine_a_; // the way the kid walks
			fov = 30.0f; // from beyond the toy lens's near blur (4 m), framed as tight as before
			return look(k + front * 4.6f + side_of(front) * cine_d_.x + Vector3(0, 1.4f, 0), k + Vector3(0, 0.7f, 0));
		};
		cine_.push_back(s);
	}
	// Trampoline: dropped onto a backyard trampoline holding jump, rocketing up past the roofs. It
	// has a world of its own (--cine-bounce-seed) picked for a big trampoline in an open yard.
	{
		CineShot s;
		s.name = "bounce";
		s.vines = false;
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
			// A camera spot with a clear view of the mat and of the kid's flight up past the roofs:
			// either side of the trampoline, a little back or forward (x: side offset, y: back offset).
			const Vector2 lane = cine_lane(tramp, -cine_b_, { -5.5f, 5.5f, -7.0f, 7.0f, -4.0f, 4.0f }, { 1.5f, 0.0f, 3.0f, -1.5f }, 0.0f, 1.2f,
					{ tramp + Vector3(0, 0.4f, 0), tramp + Vector3(0, 5.0f, 0), tramp + Vector3(0, 10.0f, 0) });
			cine_d_ = Vector3(lane.x, 0.0f, lane.y);
			player_->teleport(tramp + Vector3(0, 3.5f, 0), std::atan2(cine_b_.x, cine_b_.z));
		};
		s.tick = [this](float) { player_->bot_input(Vector3(), false, true, false); }; // held: the super bounce
		s.cam = [this](float t, float &fov) {
			// From the backyard side, wide enough to keep the trampoline and the kid in frame as the
			// kid shoots up past the roofs toward the street.
			fov = 56.0f;
			const Vector3 k = player_->get_global_position();
			const float h = Math::max(0.0f, k.y - cine_a_.y);
			// From the clear side found at setup, pulling back and rising as the kid climbs.
			const Vector3 back = -cine_b_;
			const float side = cine_d_.x + (cine_d_.x >= 0.0f ? 1.0f : -1.0f) * 0.35f * h;
			const Vector3 from = cine_a_ + side_of(back) * side + back * cine_d_.z + Vector3(0, 1.2f + 0.5f * h, 0);
			return look(from, (cine_a_ + Vector3(0, 0.4f, 0)).lerp(k, 0.6f));
		};
		s.trailer = false; // a screenshot: the trailer uses the gameplay angle below, which reads better
		cine_.push_back(s);
		s.trailer = true;
		// The same jump through the player's own camera (with the HUD): the gameplay screenshot of it
		// reads so well that it plays in the trailer too.
		s.name = "play_bounce";
		s.caption.clear();
		s.gameplay = true;
		cine_.push_back(s);
	}	// 4. Glide: wings out over the street.
	{
		CineShot s;
		s.name = "04_glide";
		s.vines = false;
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
		s.avoid = false; // the lane above is already clear
		s.caption = "Cast fire with your wand";
		s.duration = 5.0f;
		s.still_at = 0.4f; // 2.0 s: a fireball in flight, 0.1 s after the cast at 1.9 s
		s.seed = 106; // the nest out in the open by the street (other worlds hid it behind a hedge or a house)
		s.setup = [this]() {
			// This shot has its own world: find its nests now (the second nearest the spawn).
			Vector3 sp, st;
			cine_home(sp, st);
			Infestation *in = infest_.count(WorldStreamer::area_key(0, 0)) ? infest_[WorldStreamer::area_key(0, 0)].get() : nullptr;
			std::vector<int> order;
			for (int i = 0; in && i < in->nests_total(); ++i) {
				order.push_back(i);
			}
			std::sort(order.begin(), order.end(), [&](int a, int b) { return in->nest_position(a).distance_to(sp) < in->nest_position(b).distance_to(sp); });
			const Vector3 nest = order.empty() ? sp + st * 8.0f : in->nest_position(order[std::min<size_t>(1, order.size() - 1)]);
			cine_a_ = nest;
			cine_c_ = flat(sp - nest);
			if (in) {
				in->fill_nest_near(nest); // its monsters around it, for context
			}
			Vector3 kid_at = nest + cine_c_ * 7.0f;
			kid_at.y = streamer_->ground(kid_at.x, kid_at.z) + 0.3f;
			player_->teleport(kid_at, std::atan2(-cine_c_.x, -cine_c_.z));
			cine_fire_ = 0.4f;
			cine_aim_on_ = true;
			cine_aim_ = nest + Vector3(0, 0.8f, 0);
			// A camera spot beside the kid with a clear view of the kid and the nest (not pulled into
			// the kid's head by a hedge or fence in between).
			const Vector2 lane = cine_lane(kid_at, cine_c_, { 3.2f, -3.2f, 4.5f, -4.5f, 2.2f, -2.2f }, { 2.6f, 3.6f, 1.6f }, 0.6f, 1.3f,
					{ kid_at + Vector3(0, 0.9f, 0), nest + Vector3(0, 0.8f, 0) });
			cine_d_ = Vector3(lane.x, 0.0f, lane.y);
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
			const float side = cine_d_.x - (cine_d_.x >= 0.0f ? 0.15f : -0.15f) * t;
			const Vector3 from = k + cine_c_ * cine_d_.z + side_of(cine_c_) * side + Vector3(0, 1.3f, 0);
			return look(from, k.lerp(cine_a_, 0.6f) + Vector3(0, 1.0f, 0));
		};
		cine_.push_back(s);
	}
	// 6. Thorn nest: the root mass with its glowing heart, the kid burning it back.
	if (inf && inf->hearts_total() > 0) {
		CineShot s;
		s.name = "06_thorns";
		s.avoid = false; // the lane above is checked clear
		s.caption = "Burn back the thorn nests";
		s.duration = 5.0f;
		s.still_at = 0.55f;
		const Vector3 heart = inf->heart_position(0);
		s.setup = [=, this]() {
			cine_a_ = heart;
			// The kid stands out on the street side of the nest with a clear line of fire to the heart (rays end
			// short of it: its own body would block them), trying directions around the street side first.
			Infestation *in = infest_.count(WorldStreamer::area_key(0, 0)) ? infest_[WorldStreamer::area_key(0, 0)].get() : nullptr;
			const Vector3 toward = flat(spawn - heart);
			const Vector3 aim = heart + Vector3(0, 1.0f, 0);
			Vector3 dir = toward;
			Vector3 kid_at = ground(heart + toward * 8.0f, 0.3f);
			for (int a = 0; a < 16; ++a) {
				const float ang = (a % 2 ? 1.0f : -1.0f) * float((a + 1) / 2) * float(Math_TAU) / 16.0f;
				const Vector3 d = toward.rotated(Vector3(0, 1, 0), ang);
				const Vector3 p = ground(heart + d * 8.0f, 0.3f);
				if (cine_clear(p + Vector3(0, 0.9f, 0), aim + d * 2.5f)) {
					dir = d;
					kid_at = p;
					break;
				}
			}
			cine_c_ = dir;
			player_->teleport(kid_at, std::atan2(-cine_c_.x, -cine_c_.z));
			const Vector2 lane = cine_lane(kid_at, cine_c_, { 2.6f, -2.6f, 3.4f, -3.4f, 1.8f, -1.8f }, { 5.0f, 6.0f, 4.2f }, 0.0f, 1.5f,
					{ kid_at + Vector3(0, 0.9f, 0), aim + cine_c_ * 2.5f });
			cine_d_ = Vector3(lane.x, 0.0f, lane.y);
			// The tendrils creep out along the ground with no physics bodies (rays pass through them), so any
			// that would stand in the kid's spot, the camera's slide or the sightline between them are put away
			// for the shot (hidden thorns are inert; the nest itself always stays).
			if (in) {
				const float slide = lane.x + (lane.x >= 0.0f ? 1.0f : -1.0f); // where the cam ends up (see cam)
				Vector3 c0 = kid_at + cine_c_ * lane.y + side_of(cine_c_) * lane.x;
				Vector3 c1 = kid_at + cine_c_ * lane.y + side_of(cine_c_) * slide;
				c0.y = c1.y = kid_at.y + 1.5f;
				const int hidden = in->hide_thorns_near(kid_at, kid_at, 2.8f) + in->hide_thorns_near(c0, c1, 2.0f) +
						in->hide_thorns_near(c0, kid_at, 1.2f) + in->hide_thorns_near(c1, kid_at, 1.2f);
				UtilityFunctions::print("CINE thorns: kid ", kid_at, " lane ", lane, " hid ", hidden, " thorns, nearest now ", in->thorn_distance(kid_at), " m");
			}
			cine_fire_ = 0.8f;
			cine_aim_on_ = true; // at the thorn nest, not whatever the auto-aim fancies
			cine_aim_ = aim;
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
			const float side = cine_d_.x + (cine_d_.x >= 0.0f ? 0.2f : -0.2f) * t;
			const Vector3 from = k + cine_c_ * cine_d_.z + side_of(cine_c_) * side + Vector3(0, 1.5f, 0);
			// Between the kid and the heart, so both share the frame.
			return look(from, k.lerp(cine_a_, 0.45f) + Vector3(0, 1.0f, 0));
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
			if (Infestation *in = infest_.count(WorldStreamer::area_key(0, 0)) ? infest_[WorldStreamer::area_key(0, 0)].get() : nullptr) {
				in->fill_nest_near(nest); // its monsters around it, for context
			}
			// The kid blasts the nest for real (no instant kills): hotter, triple flames (shop upgrades)
			// so it bursts within the shot, aimed at the nest itself.
			up_.fire = 3;
			up_.triple = 1;
			cine_fire_ = 0.6f;
			cine_aim_on_ = true;
			cine_aim_ = nest + Vector3(0, 0.8f, 0);
			player_->teleport(ground(nest + cine_c_ * 5.5f, 0.3f), std::atan2(-cine_c_.x, -cine_c_.z));
		};
		s.tick = [this](float t) {
			bool fire = false;
			bool standing = false;
			std::vector<Damageable *> targets;
			gather_targets(targets);
			for (Damageable *d : targets) {
				if (d->is_nest() && d->target_alive() && d->target_point().distance_to(cine_a_ + Vector3(0, 0.8f, 0)) < 3.0f) {
					standing = true;
				}
			}
			if (standing && t >= cine_fire_) {
				cine_fire_ += 0.45f;
				fire = true;
			}
			player_->bot_input(Vector3(), false, false, false, fire);
		};
		s.cam = [this](float t, float &fov) {
			fov = 50.0f;
			// Off to the side of the kid (who stands 5.5 m out, casting), the nest and the kid both in frame.
			const Vector3 from = cine_a_ + cine_c_ * 6.0f + side_of(cine_c_) * 4.8f + Vector3(0, 2.0f + 0.15f * t, 0);
			return look(from, cine_a_ + cine_c_ * 2.4f + Vector3(0, 1.6f, 0));
		};
		cine_.push_back(s);
	}
	// 9. A town: running down the high street between the lit shop fronts.
	Vector3 town_spawn;
	float town_yaw = 0.0f;
	bool town_found = false;
	for (int r = 1; r <= 3 && !town_found; ++r) {
		for (int i = -r; i <= r && !town_found; ++i) {
			for (int j = -r; j <= r && !town_found; ++j) {
				const RegionPlan &rp = streamer_->plan().region(i, j);
				if (rp.kind == RegionKind::Town) {
					town_spawn = rp.spawn;
					town_yaw = rp.spawn_yaw;
					town_found = true;
				}
			}
		}
	}
	cine_town_found_ = town_found;
	// A straight, clear stretch of street at least len long from near `from` (each shot finds its own, so
	// it works recorded on its own too): the direction from the street layout, checked along its length
	// against the street bending away and against anything standing in the road at knee and head height.
	auto town_road = [=, this](const Vector3 &from, float from_yaw, float len, Vector3 &start, Vector3 &dir) {
		const Vector3 town_spawn = from;
		const float town_yaw = from_yaw;
		streamer_->update(town_spawn, true);
		auto street_dir = [this](const Vector3 &p, Vector3 &d) {
			Area *a = streamer_->area_at(p, 4.0f);
			if (!a) {
				return false;
			}
			const float y = a->gen->street_yaw_at(a->to_local(p));
			d = Vector3(-std::sin(y), 0, -std::cos(y));
			return true;
		};
		Vector3 d0(std::sin(town_yaw), 0, std::cos(town_yaw));
		street_dir(town_spawn, d0);
		start = ground(town_spawn, 0.2f);
		dir = d0;
		for (float off : { 0.0f, -8.0f, 8.0f, -16.0f, 16.0f, -24.0f, 24.0f, -32.0f, 32.0f }) {
			for (float sign : { 1.0f, -1.0f }) {
				const Vector3 o = ground(town_spawn + d0 * off, 0.0f);
				const Vector3 d = d0 * sign;
				bool ok = true;
				Vector3 prev = o;
				for (float u = 2.5f; u <= len && ok; u += 2.5f) {
					const Vector3 p = ground(o + d * u, 0.0f);
					Vector3 here;
					ok = street_dir(p, here) && std::abs(here.dot(d)) > 0.985f && std::abs(p.y - prev.y) < 0.6f &&
							cine_clear(prev + Vector3(0, 0.5f, 0), p + Vector3(0, 0.5f, 0)) && cine_clear(prev + Vector3(0, 1.6f, 0), p + Vector3(0, 1.6f, 0));
					prev = p;
				}
				if (ok) {
					start = o + Vector3(0, 0.2f, 0);
					dir = d;
					return true;
				}
			}
		}
		return false;
	};
	// A straight stretch of the high street in the town of the current world (a shot can have a world of
	// its own, so this is looked up at run time).
	auto high_street = [=, this](float len, Vector3 &start, Vector3 &dir) {
		Vector3 tsp = town_spawn;
		float tyaw = town_yaw;
		for (int r = 1, done = 0; r <= 3 && !done; ++r) {
			for (int i = -r; i <= r && !done; ++i) {
				for (int j = -r; j <= r && !done; ++j) {
					const RegionPlan &rp = streamer_->plan().region(i, j);
					if (rp.kind == RegionKind::Town) {
						tsp = rp.spawn;
						tyaw = rp.spawn_yaw;
						done = 1;
					}
				}
			}
		}
		return town_road(tsp, tyaw, len, start, dir);
	};
	// Candy floating in the camera's path would fill the lens as it passes: cleared along the line
	// (the rest of the street's candy stays).
	auto clear_candy_line = [this](const Vector3 &a, const Vector3 &b, float r) {
		for (int i = 0; i <= 12; ++i) {
			const Vector3 p = a.lerp(b, float(i) / 12.0f);
			if (Area *ar = streamer_->area_at(p, 4.0f)) {
				if (CandyField *cf = ar->gen->candy_field()) {
					cf->hide_near(ar->to_local(p), r);
				}
			}
		}
	};
	if (town_found) {
		CineShot s;
		s.name = "08_town";
		s.vines = false;
		s.caption = "Explore towns, neighborhoods, and woods";
		s.duration = 5.0f;
		s.still_at = 0.3f;
		s.avoid = false; // the street ahead is checked clear in setup
		// Picked from rendered options: the spot is the reel world's town square, which in world 103 is a
		// leafy neighborhood street by the candy shop.
		s.seed = 103;
		s.setup = [=, this]() {
			Vector3 start, d;
			const bool ok = town_road(town_spawn, town_yaw, 30.0f, start, d);
			UtilityFunctions::print("CINE town: straight street ", ok, " from ", start, " dir ", d);
			cine_a_ = d;
			cine_town_ = start;
			cine_town_dir_ = d;
			player_->teleport(start, std::atan2(d.x, d.z));
			clear_candy_line(start + d * 2.0f + side_of(d) * 1.2f + Vector3(0, 1.3f, 0), start + d * 30.0f + side_of(d) * 1.2f + Vector3(0, 1.3f, 0), 2.2f);
		};
		s.tick = [this](float t) { player_->bot_input(cine_a_ * (t > 0.3f ? 0.8f : 0.0f), false, false, false); };
		s.cam = [this](float t, float &fov) {
			fov = 46.0f;
			// Backing away down the street ahead of the kid, the shop fronts sliding by on both sides.
			const Vector3 k = player_->get_global_position();
			const Vector3 from = k + cine_a_ * (6.0f - 0.5f * t) + side_of(cine_a_) * 1.2f + Vector3(0, 1.3f, 0);
			return look(from, k + Vector3(0, 1.2f, 0) - cine_a_ * 3.0f);
		};
		cine_.push_back(s);
	}
	// 9b. The Pumpkin Cruiser, headlights on, coming down the town's high street at the camera.
	if (town_found) {
		CineShot s;
		s.name = "09_car";
		s.vines = false;
		s.caption = "Cruise the night";
		s.duration = 5.0f;
		s.still_at = 0.55f;
		s.avoid = false; // the street ahead is checked clear in setup
		s.seed = 103; // picked from rendered options: down the high street past the hardware store
		s.setup = [=, this]() {
			Vector3 start, d;
			const bool ok = high_street(42.0f, start, d);
			UtilityFunctions::print("CINE car: straight street ", ok, " from ", start, " dir ", d);
			cine_a_ = d;
			up_.car = 1;
			player_->teleport(start, std::atan2(d.x, d.z));
			toggle_car();
			if (car_) {
				car_->place(start + Vector3(0, 0.1f, 0), std::atan2(d.x, d.z));
				car_->set_auto(true, d);
			}
			clear_candy_line(start + side_of(d) * 2.2f + Vector3(0, 0.8f, 0), start + d * 42.0f + side_of(d) * 2.2f + Vector3(0, 0.8f, 0), 2.0f);
			clear_candy_line(start + Vector3(0, 0.8f, 0), start + d * 42.0f + Vector3(0, 0.8f, 0), 1.6f);
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
		s.vines = false; // the monsters alone on an open street, no thorn nest cluttering the road behind them
		s.seed = 110; // a long open street with houses either side (picked from rendered options)
		s.setup = [=, this]() {
			Vector3 sp, st;
			cine_home(sp, st); // at run time: the shot can have a world of its own
			cine_a_ = st;
			const Vector3 k = ground(sp + st * 1.0f, 0.2f);
			player_->teleport(k, std::atan2(st.x, st.z));
			const Vector3 street = st;
			const MonsterType types[4] = { MonsterType::PumpkinHopper, MonsterType::Ghost, MonsterType::Spider, MonsterType::Bat };
			// The walkers on the open side of the frame, clear of the line from the camera through the kid
			// (which runs off to the kid's right down the street); only the bat, flying high, is over that side.
			const float across[4] = { 2.8f, -0.4f, 1.2f, -3.6f };
			const float ahead[4] = { 6.4f, 6.8f, 5.0f, 5.4f };
			for (int i = 0; i < 4; ++i) {
				Vector3 p = k + street * ahead[i] + side_of(street) * across[i];
				p.y = streamer_->ground(p.x, p.z) + (tuning(types[i]).flying ? 1.3f : 0.3f);
				Monster *m = memnew(Monster);
				wild_root_->add_child(m);
				m->set_global_position(p);
				// Fully active (hopping, flapping, scuttling about), just not after the kid this shot, and each
				// kept to its own patch of street so they stay spread out rather than bunching up.
				m->init(types[i], &mworld_, nullptr, uint64_t(1000 + i), Rect2(p.x - 1.2f, p.z - 1.2f, 2.4f, 2.4f));
				m->on_gone = [this](Monster *gone) { wild_.erase(std::remove(wild_.begin(), wild_.end(), gone), wild_.end()); };
				wild_.push_back(m);
			}
			cine_fire_ = 3.2f; // a beat to see them roam first
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
			// Back far enough that the kid sits in the corner of the frame and the monsters fill the middle.
			return look(k - cine_a_ * (4.6f - 0.25f * t) + side_of(cine_a_) * 1.8f + Vector3(0, 1.6f, 0), k + cine_a_ * 5.5f + Vector3(0, 0.9f, 0));
		};
		cine_.push_back(s);
	}
	// Stretches of plain play between the scripted shots: the normal camera and HUD, the kid
	// steering like a player would.
	{
		CineShot s;
		s.name = "play_candy";
		s.vines = false;
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
		s.vines = false;
		s.gameplay = true;
		s.duration = 4.5f;
		s.still_at = 0.5f;
		s.seed = 103; // the same town as the Cruise the night shot
		s.setup = [=, this]() {
			Vector3 start, d;
			high_street(36.0f, start, d);
			cine_a_ = d;
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
	// Costumes: all six kids in a row on the street (the dragon and the five hidden costumes), hopping
	// in a wave with a sparkle each as the camera dollies past.
	{
		CineShot s;
		s.name = "costumes";
		s.face_camera = true;
		s.vines = false;
		s.caption = "Find hidden costumes";
		s.duration = 5.5f;
		s.still_at = 0.55f;
		s.avoid = false;
		s.setup = [=, this]() {
			cine_a_ = street;
			cine_b_ = side_of(street);                                  // the camera's side of the row
			const Vector3 c = ground(spawn + street * 2.0f, 0.0f);
			cine_c_ = c;
			const float face = std::atan2(cine_b_.x, cine_b_.z);
			static const char *IDS[] = { "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin" };
			for (int k = 0; k < 6; ++k) {
				const Vector3 p = ground(c + street * (float(k) - 2.5f) * 0.95f, 0.0f);
				if (k == 0) {
					player_->teleport(p + Vector3(0, 0.2f, 0), face);
					continue;
				}
				if (!gen_->asset_published(IDS[k - 1])) {
					continue;
				}
				Node3D *n = gen_->instantiate_asset(IDS[k - 1]);
				n->set_transform(Transform3D(Basis(Vector3(0, 1, 0), face), p));
				add_child(n);
				n->set_meta("cine_k", k);
				n->set_meta("cine_p", p);
				cine_props_.push_back(n);
			}
		};
		s.tick = [this](float t) {
			const float face = std::atan2(cine_b_.x, cine_b_.z);
			for (Node3D *n : cine_props_) {
				const int k = int(n->get_meta("cine_k"));
				const Vector3 p = n->get_meta("cine_p");
				// A little hop in a wave down the row, and a happy sway.
				const float ph = t * 3.2f - float(k) * 0.7f;
				const float hop = Math::max(0.0f, std::sin(ph)) * 0.12f;
				const float sway = std::sin(t * 2.1f + float(k)) * 0.12f;
				n->set_transform(Transform3D(Basis(Vector3(0, 1, 0), face + sway), p + Vector3(0, hop, 0)));
				const float spark = 0.5f + float(k) * 0.55f;
				if (t >= spark && t - 1.0f / 60.0f < spark) {
					if (Vfx *v = Vfx::get()) {
						v->candy_sparkles(p + Vector3(0, 1.1f, 0), Color(0.85f, 0.6f, 1.0f), 14, 1.6f, 0.9f, 0.6f);
					}
				}
			}
			if (t > 0.5f && t - 1.0f / 60.0f <= 0.5f) {
				if (Vfx *v = Vfx::get()) {
					v->candy_sparkles(player_->get_global_position() + Vector3(0, 1.1f, 0), Color(0.85f, 0.6f, 1.0f), 14, 1.6f, 0.9f, 0.6f);
				}
			}
			player_->bot_input(Vector3(), std::fmod(t, 1.6f) < 0.03f, false, false);
		};
		s.cam = [this](float t, float &fov) {
			fov = 40.0f;
			const float u = ease(t / 5.5f);
			const Vector3 along = cine_a_ * (-3.2f + 6.4f * u);
			const Vector3 from = cine_c_ + cine_b_ * 4.6f + along * 0.7f + Vector3(0, 1.25f, 0);
			return look(from, cine_c_ + along * 0.55f + Vector3(0, 0.75f, 0));
		};
		cine_.push_back(s);
	}
	// Upgrades: at the candy cart the kid opens the shop, buys Triple Flame with the candy they've
	// collected, then turns and casts the new three-way fireball.
	if (inf) {
		CineShot s;
		s.name = "upgrades";
		s.caption_top = true; // the shop panel fills the lower frame
		s.vines = false;
		s.caption = "Spend candy to upgrade your abilities";
		s.duration = 6.0f;
		s.still_at = 0.45f;
		const Vector3 cart = inf->shop_position();
		s.setup = [=, this]() {
			cine_a_ = cart;
			cine_c_ = flat(spawn - cart); // from the cart out toward the street
			cine_d_ = Vector3(); // x: shop opened, y: bought, z: closed
			wallet_ = Math::max(wallet_, 480);
			Array w;
			w.push_back(wallet_);
			call_ui("set_candy", w);
			// In front of the cart (off its doormat, which would open the shop by itself), facing it.
			player_->teleport(ground(cart + cine_c_ * 2.2f, 0.2f), std::atan2(-cine_c_.x, -cine_c_.z));
		};
		s.tick = [this](float t) {
			if (t > 0.5f && cine_d_.x == 0.0f) {
				cine_d_.x = 1.0f;
				shop_tab_ = 0;
				shop_focus_ = "triple";
				menu_open_ = true;
				open_shop();
			}
			if (t > 2.3f && cine_d_.y == 0.0f) {
				cine_d_.y = 1.0f;
				on_purchase("triple");
			}
			if (t > 3.3f && cine_d_.z == 0.0f) {
				cine_d_.z = 1.0f;
				call_ui("_close_menu", Array::make(false));
				menu_open_ = false;
				player_->set_facing(std::atan2(cine_c_.x, cine_c_.z)); // turn to the street
				cine_fire_ = 3.7f;
			}
			bool fire = false;
			if (cine_d_.z != 0.0f && t >= cine_fire_ && t < 5.6f) {
				cine_fire_ += 0.6f;
				fire = true;
			}
			player_->bot_input(Vector3(), false, false, false, fire);
		};
		s.cam = [this](float t, float &fov) {
			fov = 46.0f;
			const Vector3 k = player_->get_global_position();
			const Vector3 from = k + side_of(cine_c_) * 4.5f + cine_c_ * 1.5f + Vector3(0, 1.8f, 0);
			return look(from, k.lerp(cine_a_, 0.35f) + Vector3(0, 1.0f, 0));
		};
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
		static const char *ORDER[] = { "menu", "doorbell", "door", "lights", "play_candy", "aerial", "fireball", "monsters", "play_fight", "thorns", "fountain", "candy", "jump", "bucket", "costumes", "upgrades", "shop", "glide", "town", "car", "play_drive", "bounce", "play_bounce", "finale" };
		auto rank = [](const std::string &n) {
			const size_t u = n.find('_');
			const std::string base = (u != std::string::npos && u <= 3 && std::isdigit((unsigned char)n[0])) ? n.substr(u + 1) : n;
			for (int i = 0; i < int(sizeof(ORDER) / sizeof(ORDER[0])); ++i) {
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
	if (cine_play_) {
		// Every shot is seen through the game camera here already: the gameplay copy of the bounce would repeat it.
		cine_.erase(std::remove_if(cine_.begin(), cine_.end(), [](const CineShot &s) { return s.name == "play_bounce"; }), cine_.end());
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
				if (cine_seed_override_) {
					keep.back().seed = cine_seed_override_;
				}
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
		// The old world's bodies stay in the physics space until they're freed at the end of the frame:
		// set the shot up a few frames on, or its clear-view checks would run against both worlds.
		cine_wait_ = 3;
		return;
	}
	clear_wild(); // showcase monsters from an earlier shot
	for (Node3D *n : cine_props_) {
		n->queue_free();
	}
	cine_props_.clear();
	cine_reset_player();
	for (auto &kv : infest_) {
		if (kv.second) {
			kv.second->set_vines_visible(s.vines); // hidden for shots where someone travels
		}
	}
	if (s.setup) {
		s.setup();
	}
	if (!s.vines) {
		// Again for areas the setup streamed in (a town far from home): hidden thorns are inert too.
		for (auto &kv : infest_) {
			if (kv.second) {
				kv.second->set_vines_visible(false);
			}
		}
	}
	if (cine_play_ || s.gameplay) {
		if (s.gameplay && !cine_play_) {
			call_ui("cine_mode", Array::make(false)); // the HUD is back for a stretch of plain play
			call_ui("fade", Array::make(false));
		}
		// Gameplay screenshots: the normal camera rig, behind the kid the way they face, and the HUD.
		camera_->set_cinematic(false);
		camera_->face_direction(s.face_camera ? -player_->facing_dir() : player_->facing_dir());
		camera_->snap();
		UtilityFunctions::print("CINE shot ", String(s.name.c_str()), " (gameplay camera)");
		return;
	}
	camera_->set_cinematic(true);
	camera_->set_cinematic_cutout(s.cutout);
	float fov = 45.0f;
	camera_->set_cinematic_pose(s.cam(0.0f, fov), fov);
	call_ui("cine_mode", Array::make(true));
	if (!cine_clean_) {
		call_ui("fade", Array::make(false));
		if (!s.caption.empty()) {
			call_ui("cine_caption", Array::make(String(s.caption.c_str()), String(s.sub.c_str()), s.duration, s.caption_top));
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
	if (cine_wait_ > 0) {
		if (--cine_wait_ == 0) {
			cine_begin(cine_i_); // the shot's own world has settled in
		}
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
			// Not the subject itself: the ray starts inside the kid (or the car) and would pull the
			// camera right up against it.
			TypedArray<RID> self;
			self.push_back(player_->get_rid());
			if (car_) {
				self.push_back(car_->get_rid());
			}
			q->set_exclude(self);
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
	// Keep the kid in focus however close the camera gets: no near blur on trailer shots, and the far
	// blur starts well beyond the subject (the gameplay rig keeps the normal toy-lens settings).
	if (env_) {
		Ref<CameraAttributesPractical> lens = env_->get_camera_attributes();
		if (lens.is_valid()) {
			const Vector3 subject = car_ && car_->driving() ? car_->get_global_position() : player_->get_global_position();
			const float dist = rig ? 0.0f : xf.origin.distance_to(subject);
			lens->set_dof_blur_near_enabled(rig);
			lens->set_dof_blur_far_distance(Math::max(32.0f, dist + 12.0f));
		}
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
