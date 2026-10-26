#include "play/monsters.h"
#include "core/fonts.h"

#include "play/vfx.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <functional>
#include <cmath>
#include <queue>

#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

const MonsterTuning &tuning(MonsterType t) {
	using A = MonsterTuning;
	static const MonsterTuning table[] = {
		// speed hp reach aggro windup cooldown attack flying phase hops hover max nest_hp colour
		// Tame on purpose (a kids' game): unhurried walkers, long readable wind-ups, gaps between attacks.
		{ 3.2f, 2, 1.1f, 16, 0.45f, 1.6f, A::HOP, false, false, true, 0.0f, 3, 8, Color(1.0f, 0.55f, 0.2f) }, // pumpkin hopper (as it was)
		{ 3.6f, 1, 0.9f, 15, 0.7f, 2.6f, A::SWOOP, true, false, false, 2.6f, 3, 6, Color(0.7f, 0.45f, 1.0f) }, // bat
		{ 1.9f, 3, 1.0f, 15, 0.8f, 2.8f, A::THROW, false, false, false, 0.0f, 2, 9, Color(0.95f, 0.93f, 0.85f) }, // skeleton
		{ 1.6f, 2, 1.3f, 13, 0.75f, 2.4f, A::LUNGE, true, true, false, 0.35f, 2, 7, Color(0.9f, 0.95f, 1.0f) }, // ghost
		{ 2.8f, 1, 0.9f, 14, 0.6f, 2.0f, A::LUNGE, false, false, false, 0.0f, 3, 6, Color(1.0f, 0.8f, 0.3f) }, // gremlin
		{ 1.3f, 5, 1.8f, 12, 0.9f, 2.6f, A::GRAB, false, false, false, 0.0f, 2, 11, Color(1.0f, 0.95f, 0.8f) }, // mummy
		{ 1.7f, 3, 1.0f, 13, 0.8f, 3.0f, A::THROW, false, false, true, 0.0f, 2, 8, Color(0.5f, 1.0f, 0.4f) }, // slime
		{ 1.4f, 4, 1.1f, 13, 1.0f, 3.2f, A::CHARGE, false, false, false, 0.0f, 2, 9, Color(1.0f, 0.85f, 0.45f) }, // scarecrow
		{ 2.4f, 2, 1.0f, 14, 0.65f, 2.4f, A::POUNCE, false, false, false, 0.0f, 3, 7, Color(0.75f, 0.4f, 1.0f) }, // spider
		{ 1.5f, 3, 2.4f, 12, 1.1f, 3.2f, A::PUFF, false, false, false, 0.0f, 2, 8, Color(0.85f, 0.5f, 1.0f) }, // toadstool
	};
	return table[int(t)];
}

namespace {
constexpr float GRAVITY = 22.0f;
const Color TELEGRAPH(1.0f, 0.75f, 0.35f);

float wrap_angle(float a) {
	return Math::fmod(a + Math_PI * 3.0f, Math_TAU) - Math_PI;
}

// A soft additive column of light (fading toward the top) that marks something to destroy.
MeshInstance3D *make_beacon(const Color &color, float y) {
	Ref<CylinderMesh> c;
	c.instantiate();
	c->set_top_radius(0.12f);
	c->set_bottom_radius(0.45f);
	c->set_height(18.0f);
	c->set_radial_segments(10);
	c->set_rings(1);
	c->set_cap_top(false);
	c->set_cap_bottom(false);
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(PackedFloat32Array({ 0.0f, 0.75f, 1.0f }));
	g->set_colors(PackedColorArray({ Color(1, 1, 1, 0.0f), Color(1, 1, 1, 0.13f), Color(1, 1, 1, 0.3f) }));
	Ref<GradientTexture2D> tex;
	tex.instantiate();
	tex->set_gradient(g);
	tex->set_width(4);
	tex->set_height(64);
	tex->set_fill_from(Vector2(0, 0));
	tex->set_fill_to(Vector2(0, 1));
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
	m->set_albedo(color * 0.9f);
	m->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	c->set_material(m);
	MeshInstance3D *beacon = memnew(MeshInstance3D);
	beacon->set_mesh(c);
	beacon->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	beacon->set_position(Vector3(0, y, 0));
	return beacon;
}

MeshInstance3D *mesh_node(const Ref<ArrayMesh> &mesh, const Ref<Material> &mat) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_material_override(mat);
	return mi;
}
} // namespace

// ================================================================ Monster
void Monster::_bind_methods() {
}

void Monster::init(MonsterType type, MonsterWorld *world, MonsterNest *home, uint64_t seed, const Rect2 &leash) {
	type_ = type;
	world_ = world;
	home_ = home;
	leash_ = leash;
	rng_ = Rng(seed, 3);
	const MonsterTuning &t = tuning(type);
	const MonsterArt &art = (*world->art)[type];
	hp_ = t.hp;
	cooldown_ = rng_.range(0.8f, 2.0f);
	orbit_ = rng_.range(0.0f, Math_TAU);
	set_collision_layer(16);
	set_collision_mask(1 | 2); // nobody walks through houses (ghosts included)
	set_floor_max_angle(Math::deg_to_rad(50.0f));
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<CapsuleShape3D> cap;
	cap.instantiate();
	cap->set_radius(art.radius * 0.8f);
	cap->set_height(Math::max(art.height, art.radius * 2.0f));
	cs->set_shape(cap);
	cs->set_position(Vector3(0, Math::max(art.height, art.radius * 2.0f) * 0.5f, 0));
	add_child(cs);
	visual_ = memnew(Node3D);
	add_child(visual_);
	body_ = mesh_node(art.body, world->toon);
	visual_->add_child(body_);
	if (art.part_a.is_valid()) {
		part_a_ = mesh_node(art.part_a, world->toon);
		part_a_->set_position(art.pivot_a);
		visual_->add_child(part_a_);
	}
	if (art.part_b.is_valid()) {
		part_b_ = mesh_node(art.part_b, world->toon);
		part_b_->set_position(art.pivot_b);
		visual_->add_child(part_b_);
	}
	for (MeshInstance3D *m : { body_, part_a_, part_b_ }) {
		if (m) {
			m->set_instance_shader_parameter("rim_strength", 0.35f);
			m->set_instance_shader_parameter("rim_color", t.color);
			m->set_instance_shader_parameter("keep_visible", 1.0f); // never dithered away by the player cutout
		}
	}
	visual_->set_scale(Vector3(0.01f, 0.01f, 0.01f));
	set_state(State::Rise);
	set_physics_process(true);
	set_process(true);
}

Vector3 Monster::target_point() const {
	const MonsterArt &art = (*world_->art)[type_];
	return get_global_position() + Vector3(0, art.height * 0.5f, 0);
}

float Monster::target_radius() const {
	return (*world_->art)[type_].radius + 0.15f;
}

void Monster::set_state(State s) {
	state_ = s;
	state_t_ = 0.0f;
}

void Monster::flash(const Color &c) {
	flash_color_ = c;
	flash_t_ = 1.0f;
}

Vector3 Monster::pick_goal(const Vector3 &player, bool chasing) {
	const Vector3 pos = get_global_position();
	if (chasing) {
		// Wobble toward the player: a random offset that shrinks as they get close.
		const float d = Vector2(player.x - pos.x, player.z - pos.z).length();
		const float off = Math::clamp(d * 0.35f, 0.0f, 4.5f);
		const float a = rng_.range(0.0f, Math_TAU);
		return player + Vector3(std::cos(a) * off, 0, std::sin(a) * off);
	}
	const Vector3 home = home_ ? home_->get_global_position() : pos;
	const float a = rng_.range(0.0f, Math_TAU), r = rng_.range(2.0f, 8.0f);
	return home + Vector3(std::cos(a) * r, 0, std::sin(a) * r);
}

void Monster::begin_attack() {
	const MonsterTuning &t = tuning(type_);
	const Vector3 pos = get_global_position();
	const Vector3 player = world_->player_pos();
	Vector3 to = player - pos;
	to.y = 0.0f;
	attack_dir_ = to.length() > 0.01f ? to.normalized() : Vector3(std::sin(yaw_), 0, std::cos(yaw_));
	hit_landed_ = false;
	airborne_attack_ = false;
	Vector3 v = get_velocity();
	switch (t.attack) {
		case MonsterTuning::HOP:
		case MonsterTuning::POUNCE: {
			const float flat = Vector2(to.x, to.z).length();
			const float hs = Math::clamp(flat / 0.62f, 2.0f, t.attack == MonsterTuning::POUNCE ? 9.0f : 7.0f);
			v = attack_dir_ * hs;
			v.y = t.attack == MonsterTuning::POUNCE ? 5.8f : 7.2f;
			airborne_attack_ = true;
			break;
		}
		case MonsterTuning::THROW: {
			const Vector3 origin = pos + Vector3(0, (*world_->art)[type_].height * 0.9f, 0);
			const Vector3 aim = player + Vector3(0, 0.6f, 0);
			const float g = type_ == MonsterType::Slime ? 10.0f : 14.0f;
			const float tt = Math::clamp(origin.distance_to(aim) / 9.0f, 0.55f, 1.3f);
			Vector3 vel = (aim - origin) / tt;
			vel.y += 0.5f * g * tt;
			world_->combat->throw_projectile(origin, vel, type_ == MonsterType::Slime ? Combat::GOO : Combat::BONE);
			if (world_->event) {
				world_->event("throw", origin);
			}
			break;
		}
		case MonsterTuning::PUFF: {
			world_->combat->puff(pos + Vector3(0, 0.5f, 0), Color(0.75f, 0.45f, 1.0f), 40, 4.0f);
			world_->combat->puff(pos + Vector3(0, 0.3f, 0), Color(0.5f, 1.0f, 0.5f), 30, 3.0f);
			if (world_->event) {
				world_->event("puff", pos);
			}
			if (Vector2(player.x - pos.x, player.z - pos.z).length() < t.reach && std::abs(player.y - pos.y) < 2.0f) {
				world_->hurt_player(pos);
			}
			hit_landed_ = true;
			break;
		}
		default:
			break;
	}
	set_velocity(v);
	set_state(State::Attack);
}

// Per-attack motion while the attack plays out; returns to Recover when done.
void Monster::attack_step(float dt, const Vector3 &player) {
	const MonsterTuning &t = tuning(type_);
	const Vector3 pos = get_global_position();
	Vector3 v = get_velocity();
	const float d = Vector2(player.x - pos.x, player.z - pos.z).length();
	const bool close = d < t.reach && std::abs(player.y - pos.y) < 1.8f;
	bool done = false;
	switch (t.attack) {
		case MonsterTuning::LUNGE:
		case MonsterTuning::GRAB: {
			const float dur = t.attack == MonsterTuning::GRAB ? 0.35f : 0.25f;
			const float sp = t.attack == MonsterTuning::GRAB ? 3.6f : t.speed * 2.0f;
			v.x = attack_dir_.x * sp;
			v.z = attack_dir_.z * sp;
			done = state_t_ > dur;
			break;
		}
		case MonsterTuning::HOP:
		case MonsterTuning::POUNCE:
			done = state_t_ > 0.15f && is_on_floor();
			if (done) {
				world_->combat->puff(pos + Vector3(0, 0.1f, 0), Color(0.8f, 0.82f, 0.95f), 10, 2.0f);
				if (d < t.reach + 0.4f && !hit_landed_) {
					world_->hurt_player(pos);
					hit_landed_ = true;
				}
			}
			break;
		case MonsterTuning::SWOOP: {
			const Vector3 to = (player + Vector3(0, 0.7f, 0)) - pos;
			const Vector3 dir = to.length() > 0.01f ? to.normalized() : attack_dir_;
			v = dir * 6.5f;
			done = state_t_ > 0.8f || hit_landed_;
			break;
		}
		case MonsterTuning::CHARGE:
			v.x = attack_dir_.x * 6.0f;
			v.z = attack_dir_.z * 6.0f;
			done = state_t_ > 1.1f || (state_t_ > 0.2f && get_real_velocity().length() < 2.0f);
			if (state_t_ > 0.05f && int(state_t_ * 10.0f) != int((state_t_ - dt) * 10.0f)) {
				world_->combat->puff(pos + Vector3(0, 0.1f, 0), Color(0.85f, 0.8f, 0.6f), 4, 1.0f);
			}
			break;
		default:
			done = state_t_ > 0.3f;
	}
	if (close && !hit_landed_ && (t.attack == MonsterTuning::LUNGE || t.attack == MonsterTuning::GRAB || t.attack == MonsterTuning::SWOOP ||
										 t.attack == MonsterTuning::CHARGE || (airborne_attack_ && !is_on_floor()))) {
		world_->hurt_player(pos);
		hit_landed_ = true;
	}
	set_velocity(v);
	if (done) {
		cooldown_ = t.cooldown + rng_.range(0.0f, 0.8f) + (t.attack == MonsterTuning::CHARGE ? 1.2f : 0.0f);
		set_state(State::Recover);
	}
}

void Monster::_physics_process(double delta) {
	const float dt = float(delta);
	if (!world_) {
		return;
	}
	const MonsterTuning &t = tuning(type_);
	state_t_ += dt;
	cooldown_ = Math::max(0.0f, cooldown_ - dt);
	const Vector3 pos = get_global_position();
	const Vector3 player = world_->player_pos();
	Vector3 to = player - pos;
	const float dist = Vector2(to.x, to.z).length();
	Vector3 v = get_velocity();
	Vector3 desired;
	float accel = 18.0f;
	const bool chasing = dist < t.aggro;

	switch (state_) {
		case State::Dying:
			if (state_t_ > 0.28f) {
				if (on_gone) {
					on_gone(this);
				}
				queue_free();
				set_physics_process(false);
			}
			return;
		case State::Rise:
			if (state_t_ > 0.55f) {
				set_state(State::Roam);
			}
			break;
		case State::Hurt:
			desired = Vector3();
			accel = 6.0f;
			if (state_t_ > 0.28f) {
				set_state(State::Roam);
			}
			break;
		case State::Windup:
			desired = Vector3();
			accel = 25.0f;
			if (int(state_t_ * 8.0f) != int((state_t_ - dt) * 8.0f)) {
				flash(TELEGRAPH);
			}
			if (to.length() > 0.01f && t.attack != MonsterTuning::CHARGE) {
				attack_dir_ = Vector3(to.x, 0, to.z).normalized();
			}
			if (state_t_ > t.windup) {
				begin_attack();
			}
			break;
		case State::Attack:
			attack_step(dt, player);
			v = get_velocity();
			break;
		case State::Recover:
		case State::Roam: {
			goal_t_ -= dt;
			if (goal_t_ <= 0.0f || Vector2(goal_.x - pos.x, goal_.z - pos.z).length() < 0.8f) {
				goal_ = pick_goal(player, chasing);
				goal_t_ = chasing ? rng_.range(0.8f, 1.6f) : rng_.range(2.5f, 4.5f);
			}
			Vector3 target = goal_;
			// Stay near home turf.
			if (!leash_.has_point(Vector2(pos.x, pos.z))) {
				target = home_ ? home_->get_global_position() : pos;
			}
			if (t.attack == MonsterTuning::THROW && chasing) {
				// Keep a throwing distance: back off when crowded, close in when far.
				if (dist < 4.5f) {
					target = pos - Vector3(to.x, 0, to.z).normalized() * 3.0f;
				} else if (dist < 9.0f) {
					target = pos + Vector3(-to.z, 0, to.x).normalized() * 2.0f;
				}
			}
			if (t.attack == MonsterTuning::SWOOP && chasing) {
				orbit_ += dt * 1.3f;
				target = player + Vector3(std::cos(orbit_) * 3.8f, 0, std::sin(orbit_) * 3.8f);
			}
			Vector3 go = steer_toward(pos, target, dt) - pos;
			go.y = 0.0f;
			const float speed = (state_ == State::Recover ? 0.4f : 1.0f) * t.speed * (chasing ? 1.0f : 0.5f);
			desired = go.length() > 0.3f ? go.normalized() * speed : Vector3();
			// Attack when close enough and rested.
			if (state_ == State::Recover && cooldown_ <= 0.0f) {
				set_state(State::Roam);
			}
			bool in_range = false;
			switch (t.attack) {
				case MonsterTuning::LUNGE:
				case MonsterTuning::GRAB:
					in_range = dist < t.reach + 1.3f;
					break;
				case MonsterTuning::HOP:
					in_range = dist < 4.2f && dist > 0.8f;
					break;
				case MonsterTuning::POUNCE:
					in_range = dist < 7.0f && dist > 2.2f;
					break;
				case MonsterTuning::SWOOP:
					in_range = dist < 6.0f;
					break;
				case MonsterTuning::THROW:
					in_range = dist > 3.5f && dist < 12.0f;
					break;
				case MonsterTuning::CHARGE:
					in_range = dist < 12.0f && dist > 2.0f;
					break;
				case MonsterTuning::PUFF:
					in_range = dist < 2.4f;
					break;
			}
			if (state_ == State::Roam && cooldown_ <= 0.0f && in_range && std::abs(to.y) < 3.0f) {
				set_state(State::Windup);
				if (world_->event) {
					world_->event("telegraph", pos);
				}
			}
			break;
		}
	}

	// Locomotion.
	if (t.flying) {
		const float hover = t.hover + (t.attack == MonsterTuning::SWOOP && state_ != State::Attack ? 0.0f : 0.0f);
		const float ty = world_->ground(pos.x, pos.z) + hover + std::sin(anim_t_ * 2.2f) * 0.25f;
		if (state_ != State::Attack) {
			Vector3 h(v.x, 0, v.z);
			h = h.move_toward(desired, accel * dt);
			v.x = h.x;
			v.z = h.z;
			v.y = (ty - pos.y) * 3.0f;
		}
	} else {
		if (state_ != State::Attack || t.attack == MonsterTuning::LUNGE || t.attack == MonsterTuning::GRAB || t.attack == MonsterTuning::CHARGE) {
			if (state_ != State::Attack) {
				Vector3 h(v.x, 0, v.z);
				if (t.hops && state_ == State::Roam) {
					// Hoppers only steer when they leave the ground.
					hop_t_ -= dt;
					if (is_on_floor()) {
						h = h.move_toward(Vector3(), 30.0f * dt);
						if (hop_t_ <= 0.0f && desired.length() > 0.1f) {
							h = desired * 1.4f;
							v.y = 5.0f;
							hop_t_ = rng_.range(0.45f, 0.75f);
						}
					}
				} else {
					h = h.move_toward(desired, accel * dt);
				}
				v.x = h.x;
				v.z = h.z;
			}
		}
		v.y -= GRAVITY * dt;
	}
	v += knock_;
	knock_ = Vector3();
	set_velocity(v);
	move_and_slide();
	// Stuck against something while trying to move: hop, and pick a new way round.
	if (!t.flying && state_ == State::Roam && desired.length() > 0.5f && get_real_velocity().length() < 0.4f) {
		stuck_t_ += dt;
		if (stuck_t_ > 0.5f && is_on_floor()) {
			Vector3 vv = get_velocity();
			vv.y = 5.5f;
			set_velocity(vv);
			goal_ = pos + Vector3(-desired.z, 0, desired.x).normalized() * rng_.range(-4.0f, 4.0f);
			goal_t_ = 1.2f;
			stuck_t_ = 0.0f;
		}
	} else {
		stuck_t_ = 0.0f;
	}
	// Never fall out of the world.
	const float gy = world_->ground(pos.x, pos.z);
	if (pos.y < gy - 6.0f) {
		set_global_position(Vector3(pos.x, gy + 1.0f, pos.z));
		set_velocity(Vector3());
	}
	// Face where we are going (or the player while winding up).
	Vector3 face = state_ == State::Windup || state_ == State::Attack ? attack_dir_ : Vector3(v.x, 0, v.z);
	if (face.length() > 0.2f) {
		const float want = std::atan2(face.x, face.z);
		yaw_ += wrap_angle(want - yaw_) * (1.0f - std::exp(-dt * 10.0f));
	}
}

void Monster::_process(double delta) {
	const float dt = float(delta);
	if (!visual_) {
		return;
	}
	anim_t_ += dt;
	const MonsterTuning &t = tuning(type_);
	const float speed = Vector2(get_velocity().x, get_velocity().z).length();
	float s = 1.0f, sy = 1.0f, bob = 0.0f, tilt = 0.0f, spin = 0.0f;
	if (state_ == State::Rise) {
		const float k = Math::clamp(state_t_ / 0.55f, 0.0f, 1.0f);
		s = k < 0.7f ? k / 0.7f * 1.15f : Math::lerp(1.15f, 1.0f, (k - 0.7f) / 0.3f);
		sy = s;
	} else if (state_ == State::Dying) {
		const float k = Math::clamp(state_t_ / 0.28f, 0.0f, 1.0f);
		s = 1.0f + k * 0.4f;
		sy = Math::max(0.01f, 1.0f - k);
		spin = k * 6.0f;
	} else if (state_ == State::Windup) {
		const float k = Math::clamp(state_t_ / t.windup, 0.0f, 1.0f);
		const float shake = std::sin(anim_t_ * 60.0f) * 0.05f * k;
		s = 1.0f + k * (t.attack == MonsterTuning::PUFF ? 0.35f : 0.12f);
		sy = 1.0f - k * 0.18f + shake;
		tilt = t.attack == MonsterTuning::CHARGE ? 0.25f * k : -0.15f * k;
	} else if (state_ == State::Recover && t.attack == MonsterTuning::CHARGE && cooldown_ > t.cooldown) {
		spin = anim_t_ * 8.0f; // dizzy after a charge
	}
	if (!t.flying && state_ != State::Dying) {
		bob = is_on_floor() ? std::abs(std::sin(anim_t_ * (6.0f + speed * 2.0f))) * 0.06f * Math::min(speed, 1.0f) : 0.0f;
		if (t.hops && !is_on_floor()) {
			sy *= 1.12f;
			s *= 0.94f;
		}
	}
	if (type_ == MonsterType::Ghost) {
		tilt += std::sin(anim_t_ * 1.7f) * 0.08f;
	}
	visual_->set_transform(Transform3D(Basis(Vector3(0, 1, 0), yaw_ + spin) * Basis(Vector3(1, 0, 0), tilt).scaled(Vector3(s, sy, s)), Vector3(0, bob, 0)));
	// Limbs.
	const MonsterArt &art = (*world_->art)[type_];
	const float swing = std::sin(anim_t_ * (5.0f + speed * 2.2f));
	auto pose = [&](MeshInstance3D *m, const Vector3 &pivot, const Basis &b) {
		if (m) {
			m->set_transform(Transform3D(b, pivot));
		}
	};
	switch (type_) {
		case MonsterType::Bat: {
			const float flap = std::sin(anim_t_ * (state_ == State::Attack ? 26.0f : 16.0f)) * 0.8f;
			pose(part_a_, art.pivot_a, Basis(Vector3(0, 0, 1), -flap));
			pose(part_b_, art.pivot_b, Basis(Vector3(0, 0, 1), flap));
			break;
		}
		case MonsterType::Spider:
			pose(part_a_, art.pivot_a, Basis(Vector3(0, 1, 0), swing * 0.25f * Math::min(speed, 1.5f)));
			pose(part_b_, art.pivot_b, Basis(Vector3(0, 1, 0), -swing * 0.25f * Math::min(speed, 1.5f)));
			break;
		case MonsterType::Mummy:
		case MonsterType::Scarecrow: {
			const float raise = state_ == State::Windup || state_ == State::Attack ? -0.5f : 0.0f;
			pose(part_a_, art.pivot_a, Basis(Vector3(1, 0, 0), raise + swing * 0.15f));
			pose(part_b_, art.pivot_b, Basis(Vector3(1, 0, 0), raise - swing * 0.15f));
			break;
		}
		default:
			pose(part_a_, art.pivot_a, Basis(Vector3(1, 0, 0), swing * 0.6f * Math::min(speed, 1.0f)));
			pose(part_b_, art.pivot_b, Basis(Vector3(1, 0, 0), -swing * 0.6f * Math::min(speed, 1.0f)));
	}
	// Hit and telegraph flashes.
	if (flash_t_ > 0.0f) {
		flash_t_ = Math::max(0.0f, flash_t_ - dt * 6.0f);
		const Color c = flash_color_ * (flash_t_ * 1.6f);
		for (MeshInstance3D *m : { body_, part_a_, part_b_ }) {
			if (m) {
				m->set_instance_shader_parameter("hit_flash", c);
			}
		}
	}
}

bool Monster::take_hit(float damage, const Vector3 &from) {
	if (dead_) {
		return false;
	}
	hp_ -= damage;
	flash(Color(1.0f, 0.95f, 0.85f));
	Vector3 away = get_global_position() - from;
	away.y = 0.0f;
	away = away.length() > 0.01f ? away.normalized() : Vector3(0, 0, 1);
	if (hp_ <= 0.0f) {
		die(true);
		return true;
	}
	if (state_ != State::Attack || tuning(type_).attack != MonsterTuning::CHARGE) {
		knock_ = away * 5.0f + Vector3(0, tuning(type_).flying ? 0.0f : 3.5f, 0);
		set_state(State::Hurt);
	}
	if (world_->event) {
		world_->event("monster_hit", get_global_position());
	}
	return false;
}

void Monster::die(bool reward) {
	dead_ = true;
	set_state(State::Dying);
	set_collision_layer(0);
	const Vector3 p = target_point();
	world_->combat->puff(p, tuning(type_).color, 26, 4.0f);
	if (world_->event) {
		world_->event(reward ? "monster_die" : "monster_vanish", p);
	}
	// Pinata pop: a handful of candy (3 to 5, now and then a chocolate) bursting out in a shower of the
	// candy magic sparkles.
	if (reward && world_->drop_candy) {
		world_->drop_candy(p, rng_.irange(3, 5) + (rng_.chance(0.15f) ? 5 : 0));
		if (Vfx *v = Vfx::get()) {
			v->candy_sparkles(p, Color(1.0f, 0.75f, 0.3f), 40, 4.0f, 1.1f, 1.2f);
		}
	}
}

void Monster::vanish() {
	if (!dead_) {
		die(false);
	}
}

// ================================================================ MonsterNest
void MonsterNest::_bind_methods() {
}

void MonsterNest::init(MonsterType type, MonsterWorld *world, uint64_t seed) {
	type_ = type;
	world_ = world;
	rng_ = Rng(seed, 5);
	const MonsterArt &art = (*world->art)[type];
	hp_ = tuning(type).nest_hp;
	timer_ = rng_.range(1.0f, 3.0f);
	mesh_ = mesh_node(art.nest, world->toon);
	mesh_->set_instance_shader_parameter("rim_strength", 0.25f);
	mesh_->set_instance_shader_parameter("rim_color", tuning(type).color);
	add_child(mesh_);
	body_ = memnew(StaticBody3D);
	body_->set_collision_layer(1);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<CylinderShape3D> cyl;
	cyl.instantiate();
	cyl->set_radius(art.nest_radius * 0.85f);
	cyl->set_height(art.nest_height);
	cs->set_shape(cyl);
	cs->set_position(Vector3(0, art.nest_height * 0.5f, 0));
	body_->add_child(cs);
	add_child(body_);
	glow_ = memnew(OmniLight3D);
	glow_->set_color(tuning(type).color);
	glow_->set_param(Light3D::PARAM_RANGE, 4.0f);
	glow_->set_param(Light3D::PARAM_ENERGY, 0.6f);
	glow_->set_position(Vector3(0, art.nest_height * 0.6f, 0));
	add_child(glow_);
	// Beacon: a soft column of the monster's colour rising out of the nest, so nests hidden behind
	// houses can still be found from down the street.
	beacon_ = make_beacon(tuning(type).color, art.nest_height * 0.5f + 9.0f);
	add_child(beacon_);	set_rotation(Vector3(0, rng_.range(0.0f, Math_TAU), 0));
	set_process(true);
}

Vector3 MonsterNest::target_point() const {
	return get_global_position() + Vector3(0, Math::min((*world_->art)[type_].nest_height * 0.5f, 1.2f), 0);
}

float MonsterNest::target_radius() const {
	return (*world_->art)[type_].nest_radius;
}

void MonsterNest::_process(double delta) {
	const float dt = float(delta);
	if (destroyed_) {
		sink_ = Math::min(1.0f, sink_ + dt * 1.6f);
		const float s = Math::max(0.01f, 1.0f - sink_);
		mesh_->set_scale(Vector3(1.0f + sink_ * 0.3f, s, 1.0f + sink_ * 0.3f));
		if (sink_ >= 1.0f) {
			mesh_->set_visible(false);
			set_process(false);
		}
		return;
	}
	wobble_ = Math::max(0.0f, wobble_ - dt * 3.0f);
	const float k = std::sin(wobble_ * 25.0f) * wobble_ * 0.12f;
	mesh_->set_scale(Vector3(1.0f + k, 1.0f - k, 1.0f + k));
	if (flash_t_ > 0.0f) {
		flash_t_ = Math::max(0.0f, flash_t_ - dt * 5.0f);
		mesh_->set_instance_shader_parameter("hit_flash", Color(1.0f, 0.9f, 0.75f) * flash_t_ * 1.4f);
	}
	glow_->set_param(Light3D::PARAM_ENERGY, 0.5f + 0.25f * std::sin(timer_ * 3.0f));
	if (dormant_) {
		return;
	}
	if (alive < tuning(type_).max_alive) {
		timer_ -= dt;
		if (timer_ <= 0.0f) {
			timer_ = rng_.range(5.0f, 8.0f);
			wobble_ = 1.0f;
			if (spawn_monster) {
				spawn_monster(this);
			}
		}
	}
}

// Already destroyed on an earlier visit: no effects, no reward.
void MonsterNest::preset_destroyed() {
	destroyed_ = true;
	body_->set_collision_layer(0);
	glow_->set_visible(false);
	beacon_->set_visible(false);
	mesh_->set_visible(false);
	set_process(false);
}

bool MonsterNest::take_hit(float damage, const Vector3 &from) {
	(void)from;
	if (destroyed_) {
		return false;
	}
	hp_ -= damage;
	wobble_ = 1.0f;
	flash_t_ = 1.0f;
	if (hp_ > 0.0f) {
		if (world_->event) {
			world_->event("nest_hit", target_point());
		}
		return false;
	}
	destroyed_ = true;
	body_->set_collision_layer(0);
	glow_->set_visible(false);
	beacon_->set_visible(false);
	const Vector3 p = target_point();
	world_->combat->puff(p, tuning(type_).color, 40, 6.0f);
	world_->combat->puff(p + Vector3(0, 0.6f, 0), Color(1.0f, 0.8f, 0.4f), 30, 5.0f);
	if (world_->event) {
		world_->event("nest_break", p);
	}
	if (world_->candy_fountain) {
		world_->candy_fountain(p + Vector3(0, 0.4f, 0), 18);
	} else if (world_->drop_candy) {
		world_->drop_candy(p, 8);
	}
	if (on_destroyed) {
		on_destroyed(this);
	}
	return true;
}

// ---------------------------------------------------------------- navigation
// A walk grid over the settlement: every solid thing standing on the ground (house bodies, fences,
// hedges, porches, big props) is blocked, grown by a monster's radius, so routes go round them
// instead of shoving into fences or through houses.
void Infestation::build_nav(const Area &area) {
	const GenContext &ctx = *area.gen->context();
	constexpr float PAD = 14.0f, R = 0.45f;
	nav_x0_ = area.plan.x0 - PAD;
	nav_z0_ = area.plan.z0 - PAD;
	nav_y_ = area.plan.origin.y;
	nav_w_ = int((area.plan.x1 - area.plan.x0 + PAD * 2.0f) / NAV_CELL) + 1;
	nav_h_ = int((area.plan.z1 - area.plan.z0 + PAD * 2.0f) / NAV_CELL) + 1;
	nav_.assign(size_t(nav_w_) * nav_h_, 0);
	for (const OBox &b : ctx.boxes) {
		const float bottom = b.xf.origin.y - b.half.y, top = b.xf.origin.y + b.half.y;
		if (top < 0.35f || bottom > 1.3f) {
			continue; // paths, mats and steps; awnings and roofs overhead
		}
		const Transform3D world(b.xf.basis, area.to_world(b.xf.origin));
		const Transform3D inv = world.affine_inverse();
		const float ext = Math::max(b.half.x, b.half.z) * 1.42f + R;
		const int cx0 = int((world.origin.x - ext - nav_x0_) / NAV_CELL), cx1 = int((world.origin.x + ext - nav_x0_) / NAV_CELL);
		const int cz0 = int((world.origin.z - ext - nav_z0_) / NAV_CELL), cz1 = int((world.origin.z + ext - nav_z0_) / NAV_CELL);
		for (int z = std::max(cz0, 0); z <= std::min(cz1, nav_h_ - 1); ++z) {
			for (int x = std::max(cx0, 0); x <= std::min(cx1, nav_w_ - 1); ++x) {
				const Vector3 p(nav_x0_ + (x + 0.5f) * NAV_CELL, world.origin.y, nav_z0_ + (z + 0.5f) * NAV_CELL);
				const Vector3 l = inv.xform(p);
				// The local frame may be scaled; measure in world units.
				const Vector3 sc = b.xf.basis.get_scale();
				const float lx = std::abs(l.x) * sc.x, lz = std::abs(l.z) * sc.z;
				const bool inside = b.cylinder ? Vector2(l.x * sc.x, l.z * sc.z).length() < b.half.x + R
											   : (lx < b.half.x + R && lz < b.half.z + R);
				if (inside) {
					nav_[z * nav_w_ + x] = 1;
				}
			}
		}
	}
}

bool Infestation::nav_line(const Vector3 &a, const Vector3 &b) const {
	const float len = Vector2(b.x - a.x, b.z - a.z).length();
	const int steps = std::max(1, int(len / (NAV_CELL * 0.5f)));
	for (int i = 1; i <= steps; ++i) {
		const Vector3 p = a.lerp(b, float(i) / steps);
		if (nav_blocked(int((p.x - nav_x0_) / NAV_CELL), int((p.z - nav_z0_) / NAV_CELL))) {
			return false;
		}
	}
	return true;
}

bool Infestation::route(const Vector3 &from, const Vector3 &to, std::vector<Vector3> &path) const {
	if (nav_.empty()) {
		return false;
	}
	auto cell = [&](const Vector3 &p, int &x, int &z) {
		x = int(std::floor((p.x - nav_x0_) / NAV_CELL));
		z = int(std::floor((p.z - nav_z0_) / NAV_CELL));
		return x >= 0 && z >= 0 && x < nav_w_ && z < nav_h_;
	};
	int sx, sz, gx, gz;
	if (!cell(from, sx, sz)) {
		return false;
	}
	path.clear();
	const float y = to.y;
	auto centre = [&](int x, int z) { return Vector3(nav_x0_ + (x + 0.5f) * NAV_CELL, y, nav_z0_ + (z + 0.5f) * NAV_CELL); };
	if (!cell(to, gx, gz) || nav_line(from, to)) {
		path.push_back(to);
		return true;
	}
	// Nearest open cell (a monster squeezed against a fence, a goal inside a hedge).
	auto nearest_open = [&](int &x, int &z) {
		for (int r = 0; r <= 4; ++r) {
			for (int dz = -r; dz <= r; ++dz) {
				for (int dx = -r; dx <= r; ++dx) {
					if ((std::abs(dx) == r || std::abs(dz) == r) && !nav_blocked(x + dx, z + dz)) {
						x += dx;
						z += dz;
						return true;
					}
				}
			}
		}
		return false;
	};
	if (!nearest_open(sx, sz) || !nearest_open(gx, gz)) {
		path.push_back(to);
		return true;
	}
	// A* over 8 neighbours (no corner cutting), capped so a hopeless search can't stall a frame.
	const int n = nav_w_ * nav_h_;
	static thread_local std::vector<float> g;
	static thread_local std::vector<int> parent;
	static thread_local std::vector<uint32_t> stamp;
	static thread_local uint32_t gen = 0;
	if (int(g.size()) < n) {
		g.assign(n, 0.0f);
		parent.assign(n, -1);
		stamp.assign(n, 0);
	}
	++gen;
	struct Node {
		float f;
		int i;
		bool operator<(const Node &o) const { return f > o.f; }
	};
	std::priority_queue<Node> open;
	const int start = sz * nav_w_ + sx, goal = gz * nav_w_ + gx;
	auto h = [&](int i) {
		const float dx = float(std::abs(i % nav_w_ - gx)), dz = float(std::abs(i / nav_w_ - gz));
		return (dx + dz) + (1.4142f - 2.0f) * std::min(dx, dz);
	};
	g[start] = 0.0f;
	parent[start] = -1;
	stamp[start] = gen;
	open.push({ h(start), start });
	int best = start, expanded = 0;
	float best_h = h(start);
	while (!open.empty() && expanded < 9000) {
		const Node cur = open.top();
		open.pop();
		if (cur.i == goal) {
			best = goal;
			break;
		}
		const float gc = g[cur.i];
		if (cur.f - h(cur.i) > gc + 1e-3f) {
			continue; // stale entry
		}
		++expanded;
		const int cx = cur.i % nav_w_, cz = cur.i / nav_w_;
		if (h(cur.i) < best_h) {
			best_h = h(cur.i);
			best = cur.i;
		}
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				if ((dx == 0 && dz == 0) || nav_blocked(cx + dx, cz + dz) || (dx && dz && (nav_blocked(cx + dx, cz) || nav_blocked(cx, cz + dz)))) {
					continue;
				}
				const int ni = (cz + dz) * nav_w_ + cx + dx;
				const float ng = gc + ((dx && dz) ? 1.4142f : 1.0f);
				if (stamp[ni] != gen || ng < g[ni]) {
					stamp[ni] = gen;
					g[ni] = ng;
					parent[ni] = cur.i;
					open.push({ ng + h(ni), ni });
				}
			}
		}
	}
	// Walk back (to the goal, or as close as the search got), then pull the string tight.
	std::vector<Vector3> cells;
	for (int i = best; i >= 0 && stamp[i] == gen; i = parent[i]) {
		cells.push_back(centre(i % nav_w_, i / nav_w_));
		if (i == start) {
			break;
		}
	}
	std::reverse(cells.begin(), cells.end());
	if (best == goal) {
		cells.push_back(to);
	}
	Vector3 at = from;
	size_t k = 0;
	while (k < cells.size()) {
		size_t far = k;
		for (size_t j = cells.size(); j-- > k;) {
			if (nav_line(at, cells[j])) {
				far = j;
				break;
			}
		}
		path.push_back(cells[far]);
		at = cells[far];
		k = far + 1;
	}
	if (path.empty()) {
		path.push_back(to);
	}
	return true;
}

// Follow a route toward `target`, re-planning when the target moves or every so often.
Vector3 Monster::steer_toward(const Vector3 &pos, const Vector3 &target, float dt) {
	repath_t_ -= dt;
	if (world_->route && (path_.empty() || repath_t_ <= 0.0f || target.distance_to(path_goal_) > 1.5f)) {
		path_goal_ = target;
		repath_t_ = rng_.range(0.6f, 0.9f);
		if (!world_->route(pos, target, path_)) {
			path_.assign(1, target);
		}
	}
	while (path_.size() > 1 && Vector2(path_.front().x - pos.x, path_.front().z - pos.z).length() < 0.6f) {
		path_.erase(path_.begin());
	}
	return path_.empty() ? target : path_.front();
}
// ================================================================ ThornVine
void ThornVine::_bind_methods() {
}

void ThornVine::init(MonsterWorld *world, int variant, float scale, uint64_t seed) {
	world_ = world;
	scale_ = scale;
	hp_ = 5.0f + scale * 2.0f;
	Rng rng(seed, 9);
	mesh_ = mesh_node(world->art->vine[variant % 3], world->toon);
	mesh_->set_instance_shader_parameter("rim_strength", 0.14f);
	mesh_->set_instance_shader_parameter("rim_color", Color(0.6f, 0.32f, 0.75f));
	rest_ = Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(scale, scale * rng.range(0.9f, 1.15f), scale)), Vector3());
	mesh_->set_transform(rest_);
	add_child(mesh_);
	body_ = memnew(StaticBody3D);
	body_->set_collision_layer(1);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<CylinderShape3D> cyl;
	cyl.instantiate();
	cyl->set_radius(1.15f * scale);
	cyl->set_height(2.4f * scale);
	cs->set_shape(cyl);
	cs->set_position(Vector3(0, 1.2f * scale, 0));
	body_->add_child(cs);
	add_child(body_);
	set_process(true);
}

Vector3 ThornVine::target_point() const {
	return get_global_position() + Vector3(0, 1.0f * scale_, 0);
}

void ThornVine::set_hidden(bool on) {
	hidden_ = on;
	set_visible(!on);
	if (body_ && !burned_) {
		body_->set_collision_layer(on ? 0 : 1);
	}
}

void ThornVine::_process(double delta) {
	const float dt = float(delta);
	if (hidden_) {
		return;
	}
	if (wither_in_ >= 0.0f && burn_t_ < 0.0f) {
		wither_in_ -= dt;
		if (wither_in_ < 0.0f) {
			burned_ = true;
			burn_t_ = 0.0f;
			body_->set_collision_layer(0);
			if (on_burned) {
				on_burned(this);
			}
		}
	}
	if (burn_t_ >= 0.0f) {
		burn_t_ += dt;
		const float flick = 0.6f + 0.4f * std::sin(burn_t_ * 40.0f);
		mesh_->set_instance_shader_parameter("hit_flash", Color(1.0f, 0.45f, 0.1f) * flick * Math::max(0.0f, 1.2f - burn_t_));
		if (int(burn_t_ * 8.0f) != int((burn_t_ - dt) * 8.0f) && burn_t_ < 0.9f) {
			const float a = burn_t_ * 7.0f;
			world_->combat->puff(get_global_position() + Vector3(std::cos(a) * scale_, 0.8f + burn_t_, std::sin(a) * scale_), Color(1.0f, 0.5f, 0.15f), 10, 2.5f);
		}
		const float shrink = Math::clamp((burn_t_ - 0.6f) / 0.6f, 0.0f, 1.0f);
		mesh_->set_transform(Transform3D(rest_.basis.scaled(Vector3(1.0f - shrink * 0.6f, Math::max(0.01f, 1.0f - shrink), 1.0f - shrink * 0.6f)), Vector3()));
		if (burn_t_ > 1.25f) {
			mesh_->set_visible(false);
			set_process(false);
		}
		return;
	}
	if (flash_t_ > 0.0f) {
		flash_t_ = Math::max(0.0f, flash_t_ - dt * 5.0f);
		mesh_->set_instance_shader_parameter("hit_flash", Color(1.0f, 0.55f, 0.2f) * flash_t_ * 1.5f);
	}
	if (heart_) {
		pulse_t_ += dt;
		const float beat = 0.5f + 0.5f * std::pow(std::max(0.0f, std::sin(pulse_t_ * 3.2f)), 6.0f);
		heart_core_->set_scale(Vector3(1, 1, 1) * (0.92f + 0.12f * beat));
		heart_core_->set_instance_shader_parameter("glow_boost", 0.8f + 1.2f * beat);
		heart_light_->set_param(Light3D::PARAM_ENERGY, 0.45f + 0.6f * beat);
	}
	if (shake_ > 0.0f) {
		shake_ = Math::max(0.0f, shake_ - dt * 4.0f);
		const float k = std::sin(shake_ * 30.0f) * shake_ * 0.05f;
		mesh_->set_transform(Transform3D(rest_.basis.scaled(Vector3(1.0f + k, 1.0f - k, 1.0f + k)), Vector3()));
	}
	// Thorns hurt: brushing against a vine knocks the kid back.
	const Vector3 p = world_->player_pos();
	const Vector3 c = get_global_position();
	if (Vector2(p.x - c.x, p.z - c.z).length() < 1.15f * scale_ + 0.5f && p.y - c.y > -0.6f && p.y - c.y < 2.4f * scale_) {
		world_->hurt_player(c);
	}
}

// A thorn nest: the vine mass grown huge around a pulsing purple heart, with a beacon. It takes a
// lot of fire, and every vine growing out of it withers when it burns.
void ThornVine::make_heart() {
	heart_ = true;
	hp_ = 16.0f;
	const MonsterArtKit &kit = *world_->art;
	if (kit.root_mass.is_valid() && kit.root_heart.is_valid()) {
		// The authored root mass is built full size: show it unscaled (collision keeps the nest's size).
		mesh_->set_mesh(kit.root_mass);
		rest_ = Transform3D(rest_.basis.orthonormalized(), Vector3());
		mesh_->set_transform(rest_);
		heart_core_ = mesh_node(kit.root_heart, world_->toon);
		heart_core_->set_position(kit.root_heart_origin);
	} else {
		MeshBuilder m;
		m.blob(Vector3(0, 1.1f, 0), Vector3(0.75f, 0.85f, 0.75f), pal::GLOW_PURPLE, 2);
		m.blob(Vector3(0, 1.1f, 0), Vector3(0.95f, 0.5f, 0.95f), mat_cell(pal::VINE_DARK, mat::LEAVES), 1, 0.15f, 5);
		heart_core_ = mesh_node(m.build(), world_->toon);
	}
	add_child(heart_core_);	heart_light_ = memnew(OmniLight3D);
	heart_light_->set_color(Color(0.75f, 0.35f, 1.0f));
	heart_light_->set_param(Light3D::PARAM_RANGE, 7.0f);
	heart_light_->set_param(Light3D::PARAM_ENERGY, 0.7f);
	heart_light_->set_position(Vector3(0, 1.4f, 0));
	add_child(heart_light_);
	heart_beacon_ = make_beacon(Color(0.75f, 0.35f, 1.0f), 10.0f);
	add_child(heart_beacon_);
}

void ThornVine::wither(float delay) {
	if (!burned_ && wither_in_ < 0.0f) {
		wither_in_ = delay;
	}
}

void ThornVine::preset_burned() {
	burned_ = true;
	body_->set_collision_layer(0);
	mesh_->set_visible(false);
	if (heart_) {
		heart_core_->set_visible(false);
		heart_light_->set_visible(false);
		heart_beacon_->set_visible(false);
	}
	set_process(false);
}

bool ThornVine::take_hit(float damage, const Vector3 &from) {
	(void)from;
	if (burned_ || hidden_) {
		return false;
	}
	hp_ -= damage;
	flash_t_ = 1.0f;
	shake_ = 1.0f;
	if (hp_ > 0.0f) {
		if (world_->event) {
			world_->event("vine_hit", target_point());
		}
		return false;
	}
	burned_ = true;
	burn_t_ = 0.0f;
	body_->set_collision_layer(0);
	if (heart_) {
		heart_core_->set_visible(false);
		heart_light_->set_visible(false);
		heart_beacon_->set_visible(false);
	}
	if (world_->event) {
		world_->event(heart_ ? "heart_burn" : "vine_burn", target_point());
	}
	if (heart_ && world_->candy_fountain) {
		world_->candy_fountain(target_point() + Vector3(0, 0.6f, 0), 22);
	} else if (world_->drop_candy) {
		world_->drop_candy(target_point(), heart_ ? 10 : 3);
	}
	if (on_burned) {
		on_burned(this);
	}
	return true;
}

// ================================================================ VineTendril
void VineTendril::_bind_methods() {
}

void VineTendril::init(MonsterWorld *world, const std::vector<Vector3> &pts, float width, uint64_t seed) {
	world_ = world;
	pts_ = pts;
	width_ = width;
	rng_ = Rng(seed, 13);
	float len = 0.0f;
	for (size_t i = 1; i < pts.size(); ++i) {
		len += pts[i].distance_to(pts[i - 1]);
	}
	hp_ = 2.0f + len * 0.1f;
	set_global_position(pts.front());
	std::vector<Vector3> local;
	for (const Vector3 &p : pts) {
		local.push_back(p - pts.front());
	}
	mesh_ = mesh_node(build_tendril_mesh(local, width, uint32_t(seed)), world->toon);
	mesh_->set_instance_shader_parameter("rim_strength", 0.14f);
	mesh_->set_instance_shader_parameter("rim_color", Color(0.6f, 0.32f, 0.75f));
	add_child(mesh_);
	set_process(true);
}

float VineTendril::nearest(const Vector3 &p, Vector3 &at) const {
	float best = 1e18f;
	at = pts_.front();
	for (size_t i = 0; i + 1 < pts_.size(); ++i) {
		const Vector3 a = pts_[i], b = pts_[i + 1];
		const Vector3 ab = b - a;
		const float l2 = ab.x * ab.x + ab.z * ab.z;
		const float t = l2 > 1e-6f ? Math::clamp(((p.x - a.x) * ab.x + (p.z - a.z) * ab.z) / l2, 0.0f, 1.0f) : 0.0f;
		const Vector3 q = a + ab * t;
		const float d = Vector2(p.x - q.x, p.z - q.z).length_squared();
		if (d < best) {
			best = d;
			at = q;
		}
	}
	return std::sqrt(best);
}

Vector3 VineTendril::target_point() const {
	Vector3 at;
	nearest(world_->player_pos(), at);
	return at + Vector3(0, width_ * 0.6f, 0);
}

float VineTendril::surface_distance(const Vector3 &p) const {
	Vector3 at;
	const float d = nearest(p, at);
	const float dy = p.y - (at.y + width_ * 0.6f);
	return Vector2(d, Math::max(0.0f, std::abs(dy) - width_ * 0.5f)).length() - width_ * 0.6f;
}

void VineTendril::_process(double delta) {
	const float dt = float(delta);
	if (hidden_) {
		return; // no thorns to brush against
	}
	if (wither_in_ >= 0.0f && burn_t_ < 0.0f) {
		wither_in_ -= dt;
		if (wither_in_ < 0.0f) {
			start_burning(false);
		}
	}
	if (burn_t_ >= 0.0f) {
		burn_t_ += dt;
		const float flick = 0.6f + 0.4f * std::sin(burn_t_ * 40.0f);
		mesh_->set_instance_shader_parameter("hit_flash", Color(1.0f, 0.45f, 0.1f) * flick * Math::max(0.0f, 1.3f - burn_t_));
		if (int(burn_t_ * 10.0f) != int((burn_t_ - dt) * 10.0f) && burn_t_ < 1.0f) {
			const Vector3 p = pts_[rng_.next() % pts_.size()];
			world_->combat->puff(p + Vector3(0, 0.4f, 0), Color(1.0f, 0.5f, 0.15f), 8, 2.0f);
		}
		const float shrink = Math::clamp((burn_t_ - 0.5f) / 0.7f, 0.0f, 1.0f);
		mesh_->set_scale(Vector3(1.0f, Math::max(0.01f, 1.0f - shrink), 1.0f));
		if (burn_t_ > 1.25f) {
			mesh_->set_visible(false);
			set_process(false);
		}
		return;
	}
	if (flash_t_ > 0.0f) {
		flash_t_ = Math::max(0.0f, flash_t_ - dt * 5.0f);
		mesh_->set_instance_shader_parameter("hit_flash", Color(1.0f, 0.55f, 0.2f) * flash_t_ * 1.5f);
	}
	// Thorns hurt: brushing against the vine (not jumping clear over it) knocks the kid back.
	const Vector3 p = world_->player_pos();
	Vector3 at;
	const float d = nearest(p, at);
	if (d < width_ * 0.5f + 0.45f && p.y - at.y < width_ + 0.45f && p.y - at.y > -0.8f) {
		world_->hurt_player(at);
	}
}

void VineTendril::start_burning(bool reward) {
	burned_ = true;
	burn_t_ = 0.0f;
	if (reward) {
		Vector3 at;
		nearest(world_->player_pos(), at);
		if (world_->event) {
			world_->event("vine_burn", at);
		}
		if (world_->drop_candy) {
			world_->drop_candy(at + Vector3(0, 0.5f, 0), 1);
		}
	}
	if (on_burned) {
		on_burned(this);
	}
}

void VineTendril::wither(float delay) {
	if (!burned_ && wither_in_ < 0.0f) {
		wither_in_ = delay;
	}
}

void VineTendril::preset_burned() {
	burned_ = true;
	mesh_->set_visible(false);
	set_process(false);
}

bool VineTendril::take_hit(float damage, const Vector3 &from) {
	(void)from;
	if (burned_ || hidden_) {
		return false;
	}
	hp_ -= damage;
	flash_t_ = 1.0f;
	if (hp_ > 0.0f) {
		return false;
	}
	start_burning(true);
	return true;
}

// ================================================================ Infestation
namespace {

bool clear_of_boxes(const GenContext &ctx, const Vector3 &p, float r) {
	for (const OBox &b : ctx.boxes) {
		if (b.boundary) {
			continue;
		}
		if (b.xf.origin.y + b.half.y < 0.25f) {
			continue;
		}
		const Vector3 l = b.xf.affine_inverse().xform(Vector3(p.x, b.xf.origin.y, p.z));
		if (std::abs(l.x) <= b.half.x + r && std::abs(l.z) <= b.half.z + r) {
			return false;
		}
	}
	return true;
}

// How easy a spot is to shoot at: rays fanned out at fireball height, checked against anything
// tall enough to stop a shot (fences, walls, hedges, houses). 1 means open all round with the
// nearest blocker well away; a nest tucked against a fence or into a corner scores low.
float shot_openness(const GenContext &ctx, const Vector3 &p) {
	constexpr float REACH = 4.5f;
	std::vector<const OBox *> near;
	for (const OBox &b : ctx.boxes) {
		if (b.shot_through || b.top() < 0.55f || b.xf.origin.y - b.half.y > 1.4f) {
			continue; // shots fly through it (yard fences), it's too low to stop one, or it's overhead
		}
		const Vector3 d = b.xf.origin - p;
		if (Vector2(d.x, d.z).length() < REACH + b.half.length()) {
			near.push_back(&b);
		}
	}
	if (near.empty()) {
		return 1.0f;
	}
	auto inside = [&](const Vector3 &q) {
		for (const OBox *b : near) {
			const Vector3 l = b->xf.affine_inverse().xform(Vector3(q.x, b->xf.origin.y, q.z));
			if (std::abs(l.x) <= b->half.x + 0.15f && std::abs(l.z) <= b->half.z + 0.15f) {
				return true;
			}
		}
		return false;
	};
	constexpr int DIRS = 16;
	int open = 0;
	float nearest = REACH;
	for (int k = 0; k < DIRS; ++k) {
		const float a = k * Math_TAU / DIRS;
		const Vector3 dir(std::cos(a), 0, std::sin(a));
		bool hit = false;
		for (float d = 0.5f; d <= REACH; d += 0.4f) {
			if (inside(p + dir * d)) {
				hit = true;
				nearest = Math::min(nearest, d);
				break;
			}
		}
		open += hit ? 0 : 1;
	}
	const float frac = float(open) / DIRS;
	return frac * frac * Math::smoothstep(0.6f, 2.8f, nearest);
}

// Weighted draw: never rules a spot out, but open spots are many times likelier.
float spot_weight(const GenContext &ctx, const Vector3 &p) {
	return 0.05f + shot_openness(ctx, p);
}

} // namespace

Infestation::~Infestation() {
	for (Monster *m : monsters_) {
		m->on_gone = nullptr;
	}
	for (MonsterNest *n : nests_) {
		n->spawn_monster = nullptr;
		n->on_destroyed = nullptr;
	}
	for (ThornVine *v : vines_) {
		v->on_burned = nullptr;
	}
	for (VineTendril *v : tendrils_) {
		v->on_burned = nullptr;
	}
	for (ThornVine *h : hearts_) {
		h->on_burned = nullptr;
	}
}

void Infestation::build(Area &area, MonsterWorld *world, Node3D *root, bool peaceful) {
	world_ = world;
	root_ = root;
	const GenContext &ctx = *area.gen->context();
	AreaState &st = *area.state;
	rng_ = Rng(area.plan.seed ^ 0xBADC0DEull, 11);
	build_nav(area);
	leash_ = Rect2(area.plan.x0 - 12.0f, area.plan.z0 - 12.0f, area.plan.x1 - area.plan.x0 + 24.0f, area.plan.z1 - area.plan.z0 + 24.0f);
	const StreetPlan &s = ctx.street;
	const bool town = ctx.kind == SettlementKind::Town;

	// Travel gate: a pad beside the side street, just inside the lantern arch.
	{
		Vector3 local = s.lane_origin + Vector3(5.6f, 0, float(s.lane_dir) * (s.lane_len - 2.6f));
		if (!clear_of_boxes(ctx, local, 1.3f)) {
			local.x = s.lane_origin.x - 5.6f;
		}
		gate_pos_ = area.to_world(local);
		MeshInstance3D *base = mesh_node(world->art->gate_base, world->toon);
		base->set_position(gate_pos_);
		root->add_child(base);
		gate_lantern_ = mesh_node(world->art->gate_lantern, world->toon);
		gate_lantern_->set_position(gate_pos_);
		gate_lantern_->set_instance_shader_parameter("glow_boost", 0.12f);
		root->add_child(gate_lantern_);
		gate_light_ = memnew(OmniLight3D);
		gate_light_->set_color(Color(0.45f, 1.0f, 0.9f));
		gate_light_->set_param(Light3D::PARAM_RANGE, 9.0f);
		gate_light_->set_param(Light3D::PARAM_ENERGY, 0.0f);
		gate_light_->set_position(gate_pos_ + Vector3(-0.15f, 2.5f, 0));
		root->add_child(gate_light_);
		// The stone pillar and iron crook carrying the lantern are solid.
		StaticBody3D *post = memnew(StaticBody3D);
		post->set_collision_layer(1);
		CollisionShape3D *pcs = memnew(CollisionShape3D);
		Ref<CylinderShape3D> cyl;
		cyl.instantiate();
		cyl->set_radius(0.32f);
		cyl->set_height(3.4f);
		pcs->set_shape(cyl);
		post->add_child(pcs);
		post->set_position(gate_pos_ + Vector3(-1.08f, 1.7f, 0));
		root->add_child(post);
	}

	// Candy shop cart across the side street from the gate: step onto its doormat to shop.
	{
		const float side = area.to_local(gate_pos_).x - s.lane_origin.x > 0.0f ? -1.0f : 1.0f;
		Vector3 local = s.lane_origin + Vector3(side * 6.2f, 0, float(s.lane_dir) * (s.lane_len - 7.5f));
		for (float back : { 7.5f, 10.5f, 4.5f, 13.0f }) {
			const Vector3 cand = s.lane_origin + Vector3(side * 6.2f, 0, float(s.lane_dir) * (s.lane_len - back));
			if (clear_of_boxes(ctx, cand, 1.1f)) {
				local = cand;
				break;
			}
		}
		// The cart's front (+Z) faces the middle of the side street.
		const Transform3D xf(Basis(Vector3(0, 1, 0), -side * Math_PI * 0.5f), area.to_world(local));
		MeshInstance3D *cart = mesh_node(world->art->shop_cart, world->toon);
		cart->set_transform(xf);
		cart->set_instance_shader_parameter("rim_strength", 0.2f);
		root->add_child(cart);
		StaticBody3D *body = memnew(StaticBody3D);
		body->set_collision_layer(1);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		Ref<BoxShape3D> box;
		box.instantiate();
		box->set_size(Vector3(2.2f, 1.25f, 1.3f));
		cs->set_shape(box);
		cs->set_position(Vector3(0, 0.62f, 0));
		body->add_child(cs);
		body->set_transform(xf);
		root->add_child(body);
		Label3D *sign = memnew(Label3D);
		sign->set_font(display_font());
		sign->set_text("CANDY SHOP");
		sign->set_font_size(96);
		sign->set_outline_size(22);
		sign->set_pixel_size(0.0028f);
		sign->set_modulate(Color(1.0f, 0.88f, 0.45f));
		sign->set_outline_modulate(Color(0.15f, 0.05f, 0.2f));
		sign->set_draw_flag(Label3D::FLAG_SHADED, false);
		sign->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
		sign->set_transform(xf * Transform3D(Basis(), Vector3(0, 2.62f, 0.69f)));
		fit_label(sign, 1.5f, 0.3f);
		root->add_child(sign);
		OmniLight3D *lamp = memnew(OmniLight3D);
		lamp->set_color(Color(1.0f, 0.7f, 0.4f));
		lamp->set_param(Light3D::PARAM_RANGE, 5.0f);
		lamp->set_param(Light3D::PARAM_ENERGY, 1.3f);
		lamp->set_position(xf.xform(Vector3(-0.62f, 1.98f, 0.7f))); // the cart's hanging lantern
		root->add_child(lamp);
		shop_pos_ = xf.xform(Vector3(0, 0, 1.25f));
	}

	// Candidate nest spots, scattered at random: anywhere across front yards and backyards (homes),
	// service yards and sidewalks (shops), and open ground off the street.
	std::vector<Vector3> spots;
	std::vector<Vector3> doors;
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		const float zb = h.front_z - h.depth;
		const float hw = lot.width * 0.42f;
		doors.push_back(lot.doorbell);
		if (town) {
			if (lot.depth + zb > 3.6f) {
				for (int k = 0; k < 2; ++k) {
					spots.push_back(lot.to_world(rng_.range(-hw, hw), 0, rng_.range(-lot.depth + 1.2f, zb - 1.2f)));
				}
			}
			for (int k = 0; k < 2; ++k) {
				spots.push_back(lot.to_world(rng_.range(-hw, hw), 0, rng_.range(1.6f, 2.8f)));
			}
		} else {
			for (int k = 0; k < 3; ++k) {
				spots.push_back(lot.to_world(rng_.range(-hw, hw), 0, rng_.range(1.4f, h.front_z + h.porch_d + 0.6f)));
			}
			if (lot.depth + zb > 4.5f) {
				for (int k = 0; k < 3; ++k) {
					spots.push_back(lot.to_world(rng_.range(-hw, hw), 0, rng_.range(-lot.depth + 1.5f, zb - 1.5f)));
				}
			}
		}
	}
	for (int k = 0; k < 10; ++k) {
		const float x = Math::lerp(ctx.bound_x0, ctx.bound_x1, rng_.range(0.06f, 0.94f));
		const float side = rng_.chance(0.5f) ? 1.0f : -1.0f;
		spots.push_back(s.at(x) + s.left_at(x) * side * (s.edge() + (town ? rng_.range(4.5f, 7.5f) : rng_.range(2.0f, 4.5f))));
	}
	std::vector<float> spot_w;
	for (const Vector3 &p : spots) {
		spot_w.push_back(spot_weight(ctx, p));
	}
	const Vector3 spawn_local = ctx.spawn;
	const Vector3 gate_local = area.to_local(gate_pos_);
	auto far_from_key_points = [&](const Vector3 &p, float r) {
		if (p.distance_to(spawn_local) < 7.0f || p.distance_to(gate_local) < 4.5f) {
			return false;
		}
		for (const Vector3 &d : doors) {
			if (p.distance_to(d) < r) {
				return false;
			}
		}
		return true;
	};
	// The area's monster mix: three or four kinds.
	std::vector<int> kinds;
	for (int k = 0; k < int(MonsterType::Count); ++k) {
		kinds.push_back(k);
	}
	rng_.shuffle(kinds);
	kinds.resize(rng_.chance(0.5f) ? 3 : 4);
	const int want_nests = peaceful ? 0 : (town ? rng_.irange(5, 6) : rng_.irange(4, 5));
	// Draw spots at random, weighted toward open ground that's easy to shoot into.
	auto draw = [&](std::vector<Vector3> &cands, std::vector<float> &w, const std::function<bool(const Vector3 &)> &ok) {
		for (;;) {
			float total = 0.0f;
			for (size_t i = 0; i < cands.size(); ++i) {
				if (w[i] > 0.0f && !ok(cands[i])) {
					w[i] = 0.0f; // fails a hard rule (now or for good): drop it
				}
				total += w[i];
			}
			if (total <= 0.0f) {
				return -1;
			}
			float roll = rng_.range(0.0f, total);
			for (size_t i = 0; i < cands.size(); ++i) {
				if (w[i] <= 0.0f) {
					continue;
				}
				roll -= w[i];
				if (roll <= 0.0f) {
					w[i] = 0.0f;
					return int(i);
				}
			}
		}
	};
	std::vector<Vector3> picked;
	while (int(picked.size()) < want_nests) {
		const MonsterType mt = MonsterType(kinds[picked.size() % kinds.size()]);
		const float r = (*world->art)[mt].nest_radius + 0.6f;
		const int i = draw(spots, spot_w, [&](const Vector3 &p) {
			if (!clear_of_boxes(ctx, p, r) || !far_from_key_points(p, 3.0f)) {
				return false;
			}
			for (const Vector3 &q : picked) {
				if (q.distance_to(p) < 14.0f) {
					return false;
				}
			}
			return true;
		});
		if (i < 0) {
			break;
		}
		picked.push_back(spots[i]);
	}
	if (st.spawner_down.size() != picked.size()) {
		st.spawner_down.assign(picked.size(), 0);
	}
	for (size_t i = 0; i < picked.size(); ++i) {
		const MonsterType mt = MonsterType(kinds[i % kinds.size()]);
		MonsterNest *n = memnew(MonsterNest);
		root->add_child(n);
		n->set_global_position(area.to_world(picked[i]));
		n->init(mt, world, rng_.next());
		nests_.push_back(n);
		if (st.spawner_down[i]) {
			n->preset_destroyed();
		}
		const int idx = int(i);
		n->spawn_monster = [this](MonsterNest *nest) { spawn_from(nest); };
		n->on_destroyed = [this, idx](MonsterNest *) {
			if (on_nest_down) {
				on_nest_down(idx);
			}
			check_freed();
		};
	}

	// Thorn nests: two or three huge bramble masses with a glowing heart, away from the monster
	// nests. Every thorn vine in the area grows out of one of them.
	std::vector<Vector3> heart_spots;
	const int want_hearts = peaceful ? 0 : rng_.irange(2, 3);
	// Candidates: open lawns, then the street itself (a bramble mass blocking the road).
	std::vector<Vector3> heart_cands;
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		if (!town) {
			const float hw = lot.width * 0.3f;
			heart_cands.push_back(lot.to_world(rng_.range(-hw, hw), 0, rng_.range(1.8f, h.front_z + h.porch_d)));
			heart_cands.push_back(lot.to_world(h.house_x + rng_.range(-hw, hw), 0, h.front_z - h.depth - rng_.range(2.8f, 4.5f)));
		}
	}
	for (int k = 0; k < 12; ++k) {
		const float x = rng_.range(ctx.bound_x0 + 10.0f, ctx.bound_x1 - 10.0f);
		if (std::abs(x - s.lane_x) > 12.0f) {
			heart_cands.push_back(s.at(x) + s.left_at(x) * rng_.range(-1.2f, 1.2f));
		}
	}
	std::vector<float> heart_w;
	for (const Vector3 &p : heart_cands) {
		heart_w.push_back(spot_weight(ctx, p));
	}
	while (int(heart_spots.size()) < want_hearts) {
		const int i = draw(heart_cands, heart_w, [&](const Vector3 &p) {
			if (!clear_of_boxes(ctx, p, 1.6f) || !far_from_key_points(p, 4.0f) || p.distance_to(spawn_local) < 12.0f) {
				return false;
			}
			for (const Vector3 &q : picked) {
				if (q.distance_to(p) <= 8.0f) {
					return false;
				}
			}
			for (const Vector3 &q : heart_spots) {
				if (q.distance_to(p) <= 15.0f) {
					return false;
				}
			}
			return true;
		});
		if (i < 0) {
			break;
		}
		heart_spots.push_back(heart_cands[i]);
	}
	if (st.heart_down.size() != heart_spots.size()) {
		st.heart_down.assign(heart_spots.size(), 0);
	}
	for (size_t i = 0; i < heart_spots.size(); ++i) {
		ThornVine *h = memnew(ThornVine);
		root->add_child(h);
		h->set_global_position(area.to_world(heart_spots[i]));
		h->rotate_y(rng_.range(0.0f, Math_TAU)); // each root mass sprawls its own way
		h->init(world, int(i) + 1, 1.7f, rng_.next());
		h->make_heart();
		hearts_.push_back(h);
		if (st.heart_down[i]) {
			h->preset_burned();
		}
		const int idx = int(i);
		h->on_burned = [this, idx](ThornVine *heart) {
			if (on_heart_down) {
				on_heart_down(idx);
			}
			// Its vines wither, burning back outward from the thorn nest.
			const Vector3 c = heart->get_global_position();
			for (size_t k = 0; k < tendrils_.size(); ++k) {
				if (tendril_nest_[k] == idx) {
					tendrils_[k]->wither(0.2f + tendrils_[k]->get_global_position().distance_to(c) * 0.05f);
				}
			}
			for (size_t k = 0; k < vines_.size(); ++k) {
				if (vine_nest_[k] == idx) {
					vines_[k]->wither(0.6f + vines_[k]->get_global_position().distance_to(c) * 0.06f);
				}
			}
			check_freed();
		};
	}

	// Thorn vines creep out over the ground from every thorn nest: a few tendrils each, one
	// reaching for the nearest front door, where it knots into a big thorn cluster that blocks the
	// doorbell. They wither and burn away when their thorn nest is destroyed.
	const Vector3 shop_local = area.to_local(shop_pos_);
	auto blocked = [&](const Vector3 &p) {
		if (p.x < ctx.bound_x0 + 1.0f || p.x > ctx.bound_x1 - 1.0f || p.z < ctx.bound_z0 + 1.0f || p.z > ctx.bound_z1 - 1.0f) {
			return true;
		}
		if (p.distance_to(spawn_local) < 5.5f || p.distance_to(gate_local) < 2.6f || p.distance_to(shop_local) < 3.0f) {
			return true;
		}
		return !clear_of_boxes(ctx, p, 0.3f);
	};
	auto grow = [&](Vector3 pos, Vector3 dir, float length, const Vector3 *target) {
		std::vector<Vector3> pts = { pos };
		const float step = 0.7f;
		for (float run = 0.0f; run < length; run += step) {
			if (target) {
				const Vector3 want = Vector3(target->x - pos.x, 0, target->z - pos.z).normalized();
				dir = (dir * 0.55f + want * 0.45f).normalized();
			}
			dir = dir.rotated(Vector3(0, 1, 0), rng_.range(-0.35f, 0.35f));
			bool moved = false;
			for (float turn : { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f }) {
				const Vector3 d = dir.rotated(Vector3(0, 1, 0), turn);
				const Vector3 cand = pos + d * step;
				if (!blocked(cand)) {
					dir = d;
					pos = cand;
					moved = true;
					break;
				}
			}
			if (!moved) {
				break;
			}
			pts.push_back(pos);
			if (target && pos.distance_to(*target) < 0.8f) {
				break;
			}
		}
		return pts;
	};
	std::vector<Vector3> door_paths;
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		door_paths.push_back(town ? lot.to_world(h.door_x, 0, 1.5f) : lot.to_world(h.door_x, 0, h.front_z + h.porch_d + 1.08f + 1.3f));
	}
	std::vector<char> door_taken(door_paths.size(), 0);
	struct Planned {
		std::vector<Vector3> pts;
		int nest;
	};
	std::vector<Planned> planned;
	std::vector<std::pair<Vector3, int>> knots;
	for (size_t ni = 0; ni < heart_spots.size(); ++ni) {
		const Vector3 c = heart_spots[ni];
		const float r = 2.0f;
		// Uneven spokes: a varied count, angles knocked off the even spread, lengths from stubby to long.
		const int n = rng_.irange(4, 8);
		const float a0 = rng_.range(0.0f, Math_TAU);
		for (int k = 0; k < n; ++k) {
			const float a = a0 + (k + rng_.range(-0.35f, 0.35f)) * Math_TAU / n;
			const Vector3 dir = Vector3(std::cos(a), 0, std::sin(a));
			std::vector<Vector3> pts = grow(c + dir * r, dir, rng_.chance(0.3f) ? rng_.range(4.0f, 8.0f) : rng_.range(8.0f, 18.0f), nullptr);
			if (pts.size() >= 4) {
				// A side branch partway along.
				if (pts.size() > 6 && rng_.chance(0.6f)) {
					const size_t at = Math::clamp(size_t(pts.size() * rng_.range(0.3f, 0.7f)), size_t(1), pts.size() - 2);
					const Vector3 bd = (pts[at] - pts[at - 1]).normalized().rotated(Vector3(0, 1, 0), rng_.chance(0.5f) ? 0.9f : -0.9f);
					std::vector<Vector3> branch = grow(pts[at], bd, rng_.range(4.0f, 7.0f), nullptr);
					if (branch.size() >= 4) {
						planned.push_back({ branch, int(ni) });
					}
				}
				planned.push_back({ pts, int(ni) });
			}
		}
		// Reach for the nearest free front door.
		int best = -1;
		float best_d = 26.0f;
		for (size_t d = 0; d < door_paths.size(); ++d) {
			const float dd = door_paths[d].distance_to(c);
			if (!door_taken[d] && dd < best_d && door_paths[d].distance_to(spawn_local) > 7.0f) {
				best_d = dd;
				best = int(d);
			}
		}
		if (best >= 0) {
			const Vector3 target = door_paths[best];
			const Vector3 dir = Vector3(target.x - c.x, 0, target.z - c.z).normalized();
			std::vector<Vector3> pts = grow(c + dir * r, dir, best_d * 1.8f, &target);
			if (pts.size() >= 3 && pts.back().distance_to(target) < 1.5f) {
				door_taken[best] = 1;
				planned.push_back({ pts, int(ni) });
				knots.push_back({ target, int(ni) });
			}
		}
	}
	const size_t vine_count = planned.size() + knots.size();
	if (st.vine_down.size() != vine_count) {
		st.vine_down.assign(vine_count, 0);
	}
	const float lift = 0.02f;
	for (size_t i = 0; i < planned.size(); ++i) {
		std::vector<Vector3> world_pts;
		for (const Vector3 &p : planned[i].pts) {
			world_pts.push_back(area.to_world(p + Vector3(0, lift, 0)));
		}
		VineTendril *v = memnew(VineTendril);
		root->add_child(v);
		v->init(world, world_pts, rng_.range(0.45f, 0.6f), rng_.next());
		tendrils_.push_back(v);
		tendril_nest_.push_back(planned[i].nest);
		if (st.vine_down[i] || st.heart_down[planned[i].nest]) {
			v->preset_burned();
		}
		const int idx = int(i);
		v->on_burned = [this, idx](VineTendril *) {
			if (on_vine_down) {
				on_vine_down(idx);
			}
		};
	}
	for (size_t i = 0; i < knots.size(); ++i) {
		ThornVine *v = memnew(ThornVine);
		root->add_child(v);
		v->set_global_position(area.to_world(knots[i].first));
		v->init(world, int(i), rng_.range(0.85f, 1.05f), rng_.next());
		vines_.push_back(v);
		vine_nest_.push_back(knots[i].second);
		const size_t vi = planned.size() + i;
		if (st.vine_down[vi] || st.heart_down[knots[i].second]) {
			v->preset_burned();
		}
		const int idx = int(vi);
		v->on_burned = [this, idx](ThornVine *) {
			if (on_vine_down) {
				on_vine_down(idx);
			}
		};
	}
	freed_ = false;
	if (peaceful) {
		st.freed = true;
	}
	if (st.freed) {
		set_freed(false);
	}
}

void Infestation::spawn_from(MonsterNest *nest) {
	if (freed_ || !root_) {
		return;
	}
	Monster *m = memnew(Monster);
	root_->add_child(m);
	const float a = rng_.range(0.0f, Math_TAU);
	const float r = (*world_->art)[nest->type()].nest_radius + 0.8f;
	Vector3 p = nest->get_global_position() + Vector3(std::cos(a) * r, 0.3f, std::sin(a) * r);
	if (tuning(nest->type()).flying) {
		p.y += 1.0f;
	}
	m->set_global_position(p);
	m->init(nest->type(), world_, nest, rng_.next(), leash_);
	nest->alive++;
	monsters_.push_back(m);
	m->on_gone = [this](Monster *gone) {
		if (gone->home()) {
			gone->home()->alive = Math::max(0, gone->home()->alive - 1);
		}
		monsters_.erase(std::remove(monsters_.begin(), monsters_.end(), gone), monsters_.end());
	};
	world_->combat->puff(p, tuning(nest->type()).color, 16, 2.5f);
	if (world_->event) {
		world_->event("spawn", p);
	}
}

void Infestation::fill_nest_near(const Vector3 &p) {
	MonsterNest *best = nullptr;
	float best_d = 1e9f;
	for (MonsterNest *n : nests_) {
		const float d = n->get_global_position().distance_to(p);
		if (!n->destroyed() && d < best_d) {
			best_d = d;
			best = n;
		}
	}
	if (!best) {
		return;
	}
	for (int guard = 0; best->alive < tuning(best->type()).max_alive && guard < 8; ++guard) {
		spawn_from(best);
	}
}

float Infestation::thorn_distance(const Vector3 &p) const {
	float best = 1e9f;
	for (const VineTendril *t : tendrils_) {
		if (t->target_alive()) {
			best = Math::min(best, t->surface_distance(p));
		}
	}
	for (const std::vector<ThornVine *> *list : { &vines_, &hearts_ }) {
		for (const ThornVine *v : *list) {
			if (v->target_alive()) {
				const Vector3 c = v->get_global_position();
				best = Math::min(best, Vector2(p.x - c.x, p.z - c.z).length() - v->target_radius());
			}
		}
	}
	return best;
}

int Infestation::hide_thorns_near(const Vector3 &a, const Vector3 &b, float r) {
	const auto near = [&](const std::function<float(const Vector3 &)> &dist) {
		for (int i = 0; i <= 8; ++i) {
			if (dist(a.lerp(b, float(i) / 8.0f)) < r) {
				return true;
			}
		}
		return false;
	};
	int n = 0;
	for (VineTendril *t : tendrils_) {
		if (t->target_alive() && near([t](const Vector3 &p) { return t->surface_distance(p); })) {
			t->set_hidden(true);
			++n;
		}
	}
	for (ThornVine *v : vines_) {
		const Vector3 c = v->get_global_position();
		if (v->target_alive() && near([&](const Vector3 &p) { return Vector2(p.x - c.x, p.z - c.z).length() - v->target_radius(); })) {
			v->set_hidden(true);
			++n;
		}
	}
	return n;
}

int Infestation::nests_left() const {
	int n = 0;
	for (const MonsterNest *x : nests_) {
		n += x->destroyed() ? 0 : 1;
	}
	return n;
}

int Infestation::vines_left() const {
	int n = 0;
	for (const ThornVine *x : vines_) {
		n += x->burned() ? 0 : 1;
	}
	for (const VineTendril *x : tendrils_) {
		n += x->burned() ? 0 : 1;
	}
	return n;
}

int Infestation::hearts_left() const {
	int n = 0;
	for (const ThornVine *x : hearts_) {
		n += x->burned() ? 0 : 1;
	}
	return n;
}

// Freed once every monster nest and thorn nest is gone (the vines wither with their thorn nest).
void Infestation::check_freed() {
	if (!freed_ && nests_left() == 0 && hearts_left() == 0) {
		set_freed(true);
		if (on_freed) {
			on_freed();
		}
	}
}

void Infestation::set_freed(bool celebrate) {
	freed_ = true;
	for (MonsterNest *n : nests_) {
		n->set_dormant(true);
	}
	std::vector<Monster *> out = monsters_;
	for (Monster *m : out) {
		m->vanish();
	}
	gate_glow_ = celebrate ? 0.0f : 1.0f;
}

void Infestation::gather(std::vector<Damageable *> &out) {
	for (Monster *m : monsters_) {
		if (m->target_alive()) {
			out.push_back(m);
		}
	}
	for (MonsterNest *n : nests_) {
		if (!n->destroyed()) {
			out.push_back(n);
		}
	}
	for (ThornVine *v : vines_) {
		if (!v->burned()) {
			out.push_back(v);
		}
	}
	for (VineTendril *v : tendrils_) {
		if (!v->burned()) {
			out.push_back(v);
		}
	}
	for (ThornVine *h : hearts_) {
		if (!h->burned()) {
			out.push_back(h);
		}
	}
}

void Infestation::update(float dt) {
	gate_t_ += dt;
	if (freed_) {
		gate_glow_ = Math::min(1.0f, gate_glow_ + dt * 0.8f);
	}
	if (gate_lantern_) {
		const float boost = freed_ ? Math::lerp(0.12f, 1.6f + 0.35f * std::sin(gate_t_ * 2.5f), gate_glow_) : 0.12f + 0.05f * std::sin(gate_t_ * 1.3f);
		gate_lantern_->set_instance_shader_parameter("glow_boost", boost);
		gate_light_->set_param(Light3D::PARAM_ENERGY, freed_ ? gate_glow_ * (1.8f + 0.4f * std::sin(gate_t_ * 2.5f)) : 0.0f);
	}
}

} // namespace tot
