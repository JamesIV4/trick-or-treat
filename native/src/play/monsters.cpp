#include "play/monsters.h"
#include "core/fonts.h"

#include "play/vfx.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/viewport.hpp>
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

void monster_review_label(Node *parent, const String &text) {
	static Label *label = nullptr;
	if (!label) {
		if (!parent) {
			return;
		}
		CanvasLayer *layer = memnew(CanvasLayer);
		layer->set_layer(100);
		parent->add_child(layer);
		label = memnew(Label);
		label->set_position(Vector2(16, 12));
		label->add_theme_font_size_override("font_size", 22);
		label->add_theme_color_override("font_color", Color(1, 1, 1));
		label->add_theme_color_override("font_outline_color", Color(0, 0, 0));
		label->add_theme_constant_override("outline_size", 6);
		layer->add_child(label);
	}
	label->set_text(text);
}

namespace {
constexpr float GRAVITY = 22.0f;
const Color TELEGRAPH(1.0f, 0.68f, 0.18f); // (final fixes 2) warmer: at the linear body_flash's strength 0.75 / 0.35 read cream

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
	// One code path for every monster: the art's part list becomes nodes under visual_ (toon material,
	// rim light in the monster's colour, never dithered away by the player cutout), and the animator
	// for this type and rig version poses them.
	rig_.build(visual_, art.parts, &art.sockets, world->toon, t.color);
	anim_.reset(make_animator(type, art.rig_version));
	anim_->bind(rig_);
	rise_time_ = Math::clamp(anim_->rise_time(), 0.1f, MONSTER_ANIM_TIME_MAX);
	death_time_ = Math::clamp(anim_->death_time(), 0.1f, MONSTER_ANIM_TIME_MAX);
	// What a thrower throws looks like what it holds (visual only): the first monster of a kind hands
	// Combat the held part's mesh, recentred so it tumbles about its middle.
	if (t.attack == MonsterTuning::THROW && world->combat) {
		const Combat::ProjectileKind kind = type == MonsterType::Slime ? Combat::GOO : Combat::BONE;
		const int held = anim_->thrown_part(rig_);
		if (held >= 0 && rig_.mesh(held) && rig_.mesh(held)->get_mesh().is_valid() && !world->combat->has_projectile_look(kind)) {
			const Ref<Mesh> src = rig_.mesh(held)->get_mesh();
			const Transform3D recentre(Basis(), -src->get_aabb().get_center());
			Ref<SurfaceTool> st;
			st.instantiate();
			Ref<ArrayMesh> flying;
			for (int s = 0; s < src->get_surface_count(); ++s) {
				st->clear();
				st->append_from(src, s, recentre);
				flying = st->commit(flying);
			}
			world->combat->set_projectile_look(kind, flying, world->toon);
		}
	}
	// Animation variety only; never drawn from rng_, so the AI's rolls are what they always were.
	anim_seed_ = uint32_t(seed ^ (seed >> 32)) * 2654435761u + uint32_t(type) * 97u + 1u;
	lod_frame_ = int(anim_seed_ % 3u);
	lod_check_ = float(anim_seed_ % 7u) * 0.05f;
	visual_->set_scale(Vector3(0.01f, 0.01f, 0.01f));
	anim_root_ = visual_->get_transform();
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

void Monster::flash(const Color &c, bool peak) {
	flash_color_ = c;
	flash_t_ = 1.0f;
	flash_peak_ = peak;
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
			Vector3 origin = pos + Vector3(0, (*world_->art)[type_].height * 0.9f, 0);
			Vector3 muzzle;
			if (anim_ && anim_->muzzle(rig_, muzzle)) {
				origin = muzzle; // the throwing hand or the mouth, when the animator has one
			}
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
	physics_step(delta);
	tick_smooth_.tick(get_global_position()); // where this tick left the body, for drawing between ticks
}

void Monster::physics_step(double delta) {
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
	const Vector3 v_before = v;
	Vector3 desired;
	float accel = 18.0f;
	const bool chasing = dist < t.aggro;
	chasing_ = chasing;

	switch (state_) {
		case State::Dying:
			if (state_t_ > death_time_) {
				if (on_gone) {
					on_gone(this);
				}
				queue_free();
				set_physics_process(false);
			}
			return;
		case State::Rise:
			if (state_t_ > rise_time_) {
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
				flash(TELEGRAPH * (anim_ ? anim_->telegraph_gain() : 1.0f));
			}
			if (to.length() > 0.01f && t.attack != MonsterTuning::CHARGE) {
				attack_dir_ = Vector3(to.x, 0, to.z).normalized();
			}
			if (state_t_ > t.windup) {
				begin_attack();
				// Keep the velocity begin_attack set (the HOP and POUNCE leap). Without this the locomotion below
				// wrote back the velocity from before the attack on this tick, and those attacks never left the ground.
				v = get_velocity();
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
	// For the animator only: how hard it is speeding up, stopping or being knocked (smoothed).
	if (dt > 0.0f) {
		accel_ = anim::damp(accel_, (get_velocity() - v_before) / dt, 0.05f, dt);
	}
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

// Visuals only: gather what the animator needs to know (AnimInput), let it pose the parts and the
// visual root, then fade the hit / telegraph flash. Nothing here feeds back into the AI or physics.
void Monster::_process(double delta) {
	const float dt = float(delta);
	if (!visual_ || !anim_ || !world_) {
		return;
	}
	anim_t_ += dt;
	since_hit_ += dt;
	const MonsterTuning &t = tuning(type_);
	// Far from the camera (checked a few times a second): animators that allow it are posed every
	// third frame and skip their springs, blinks and look-at.
	lod_check_ -= dt;
	if (lod_check_ <= 0.0f) {
		lod_check_ = 0.3f;
		far_ = false;
		if (anim_->distance_lod()) {
			if (const Viewport *vp = get_viewport()) {
				if (const Camera3D *cam = vp->get_camera_3d()) {
					far_ = cam->get_global_position().distance_squared_to(get_global_position()) > MONSTER_LOD_DISTANCE * MONSTER_LOD_DISTANCE;
				}
			}
		}
	}
	lod_dt_ += dt;
	// A state "changes" also when it restarts (hit again while already hurt: its clock goes back).
	const bool state_changed = !anim_started_ || state_ != anim_state_ || state_t_ < anim_state_t_;
	const bool skip = far_ && !state_changed && (++lod_frame_ % 3) != 0;
	if (!skip) {
		if (state_changed) {
			anim_prev_state_ = anim_started_ ? anim_state_ : state_;
			anim_state_ = state_;
			anim_started_ = true;
		}
		anim_state_t_ = state_t_;
		const Vector3 vel = get_velocity();
		const Basis facing(Vector3(0, 1, 0), yaw_);
		AnimInput in;
		in.type = type_;
		in.tuning = &t;
		in.state = MonsterAnimState(int(state_));
		in.prev_state = MonsterAnimState(int(anim_prev_state_));
		in.entered = state_changed;
		in.state_t = state_t_;
		switch (state_) {
			case State::Rise: in.state_len = rise_time_; break;
			case State::Dying: in.state_len = death_time_; break;
			case State::Windup: in.state_len = t.windup; break;
			case State::Hurt: in.state_len = 0.28f; break;
			default: break;
		}
		in.windup = state_ == State::Windup ? Math::clamp(state_t_ / t.windup, 0.0f, 1.0f) : 0.0f;
		in.cooldown = cooldown_;
		in.dizzy = state_ == State::Recover && t.attack == MonsterTuning::CHARGE ? Math::max(0.0f, cooldown_ - t.cooldown) : 0.0f;
		in.since_hit = since_hit_;
		in.dt = lod_dt_;
		in.t = anim_t_;
		in.yaw = yaw_;
		in.speed = Vector2(vel.x, vel.z).length();
		in.vel_local = facing.xform_inv(vel);
		in.accel_local = facing.xform_inv(accel_);
		in.player_local = facing.xform_inv(world_->player_pos() - get_global_position());
		in.attack_dir_local = facing.xform_inv(attack_dir_);
		in.hit_dir_local = facing.xform_inv(hit_dir_);
		in.hop_in = t.hops ? Math::max(0.0f, hop_t_) : 0.0f;
		in.on_floor = is_on_floor();
		in.chasing = chasing_;
		in.origin_world = get_global_position();
		in.ground = world_->ground ? &world_->ground : nullptr;
		if (!surface_) {
			surface_ = [this](float x, float z, float y) -> float {
				const float g = world_->ground ? world_->ground(x, z) : y;
				const Ref<World3D> w3 = get_world_3d();
				if (PhysicsDirectSpaceState3D *space = w3.is_valid() ? w3->get_direct_space_state() : nullptr) {
					// structures (4: kerbs, sidewalks, porches) and the ground mesh (8); never the kid or a monster
					Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(Vector3(x, Math::max(y, g) + 0.7f, z), Vector3(x, Math::min(y, g) - 0.7f, z), 4 | 8);
					const Dictionary hit = space->intersect_ray(q);
					if (!hit.is_empty()) {
						return float(Vector3(hit["position"]).y);
					}
				}
				return g;
			};
		}
		in.surface = &surface_;
		const MonsterArt &art = (*world_->art)[type_];
		in.height = art.height;
		in.radius = art.radius;
		in.seed = anim_seed_;
		in.lod = far_ ? 1 : 0;
		lod_dt_ = 0.0f;
		Transform3D root = anim_root_;
		anim_->update(in, rig_, root);
		anim_root_ = root;
	}
	// Drawn between physics ticks (play/tick_smooth.h), every frame, also on the frames a far-away
	// monster's animator is skipped: otherwise the monster judders against the camera.
	{
		Transform3D drawn = anim_root_;
		drawn.origin += get_global_transform().basis.xform_inv(tick_smooth_.offset(get_global_position()));
		visual_->set_transform(drawn);
	}
	// Review aid (TOT_MONSTER_TEST, festival_game.cpp): where this monster is drawn on screen this frame,
	// for measuring how evenly it moves (art/team/monsters/batch04/real_trace.py reads the lines).
	static const bool review_trace = OS::get_singleton()->has_environment("TOT_MONSTER_TEST");
	if (review_trace) {
		if (const Viewport *vp = get_viewport()) {
			if (const Camera3D *cam = vp->get_camera_3d()) {
				const Vector3 drawn = visual_->get_global_position(), body = get_global_position();
				const Vector2 sd = cam->unproject_position(drawn), sb = cam->unproject_position(body);
				UtilityFunctions::print(vformat("MTRACE id=%d f=%d t=%.5f dt=%.6f st=%d stt=%.3f far=%d floor=%d drawn=%.4f,%.4f,%.4f body=%.4f,%.4f,%.4f sd=%.3f,%.3f sb=%.3f,%.3f vy=%.3f",
						int(get_instance_id() % 100000), int(Engine::get_singleton()->get_process_frames()), anim_t_, dt, int(state_), state_t_, far_ ? 1 : 0, is_on_floor() ? 1 : 0,
						drawn.x, drawn.y, drawn.z, body.x, body.y, body.z, sd.x, sd.y, sb.x, sb.y, get_velocity().y));
			}
		}
	}
	// Hit and telegraph flashes.
	if (flash_t_ > 0.0f) {
		flash_t_ = Math::max(0.0f, flash_t_ - dt * 6.0f);
		// (final fixes 2) A hit starts with the whole body near white for 2 frames (flash_t 0.9, 0.8 at 60 fps; a
		// full-strength silhouette reads solid), then the body_flash fade carries it. The colour is
		// linear now (the sRGB hint squared it away: a 0.3 gain was 0.07 of a flash).
		const float pk = Math::clamp((flash_t_ - 0.68f) / 0.14f, 0.0f, 1.0f);
		const float peak = flash_peak_ ? pk * pk * (3.0f - 2.0f * pk) : 0.0f;
		rig_.hit_flash(flash_color_ * flash_t_, peak);
	}
}

bool Monster::take_hit(float damage, const Vector3 &from) {
	if (dead_) {
		return false;
	}
	hp_ -= damage;
	flash(Color(1.0f, 0.95f, 0.85f) * (anim_ ? anim_->hit_flash_gain() : 1.0f), true);
	Vector3 away = get_global_position() - from;
	away.y = 0.0f;
	away = away.length() > 0.01f ? away.normalized() : Vector3(0, 0, 1);
	hit_dir_ = away; // for the animator's flinch
	since_hit_ = 0.0f;
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
	// Stage 4c: with ~2.5x the nests an area holds, a nest only sends monsters out while the kid is within ~85 m
	// (the ones already out stay), so the live count follows what is around the kid rather than the area's size.
	const Vector3 me = get_global_position(), kid = world_->player_pos();
	const bool near_kid = Vector2(me.x - kid.x, me.z - kid.z).length_squared() < 85.0f * 85.0f;
	if (alive < tuning(type_).max_alive && near_kid) {
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
	build_nav(*area.gen->context(), area.plan);
}

// Plain data from the settlement's plan (area preloading runs it on a worker thread, see prepare()).
void Infestation::build_nav(const GenContext &ctx, const RegionPlan &plan) {
	constexpr float PAD = 14.0f, R = 0.45f;
	nav_x0_ = plan.x0 - PAD;
	nav_z0_ = plan.z0 - PAD;
	nav_y_ = plan.origin.y;
	nav_w_ = int((plan.x1 - plan.x0 + PAD * 2.0f) / NAV_CELL) + 1;
	nav_h_ = int((plan.z1 - plan.z0 + PAD * 2.0f) / NAV_CELL) + 1;
	nav_.assign(size_t(nav_w_) * nav_h_, 0);
	for (const OBox &b : ctx.boxes) {
		const float g = ctx.ground(b.xf.origin.x, b.xf.origin.z); // Stage 3b: heights above the ground under it
		const float bottom = b.xf.origin.y - b.half.y - g, top = b.xf.origin.y + b.half.y - g;
		if (top < 0.35f || bottom > 1.3f) {
			continue; // paths, mats and steps; awnings and roofs overhead
		}
		const Transform3D world(b.xf.basis, b.xf.origin + plan.origin); // Area::to_world
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
	labels_.clear();
	ensure_labels();
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

// Stage 2a: the bigger neighbourhoods need longer searches (the --sweep nav probe: at 9000 about 1 in 30 chases and 1 in 6
// walks home from past the leash between connected cells were cut short; at 40000 none, worst ~6 ms in a debug build).
// A search with no way through (the target in a fenced-off pocket) keeps the old cap: it only wants the nearest approach.
// Stage 2b (2-3x the houses): at 40000 about 1 in 8 walks home from past the leash was still cut short; 100000.
int Infestation::route_cap = 100000;
constexpr int HOPELESS_CAP = 9000;

void Infestation::ensure_labels() const {
	if (labels_.size() != nav_.size()) {
		// Flood fill with route()'s moves (8 neighbours, no corner cutting).
		labels_.assign(nav_.size(), 0);
		int next = 0;
		std::vector<int> stack;
		for (int i = 0; i < int(nav_.size()); ++i) {
			if (nav_[i] || labels_[i]) {
				continue;
			}
			labels_[i] = ++next;
			stack.push_back(i);
			while (!stack.empty()) {
				const int c = stack.back();
				stack.pop_back();
				const int cx = c % nav_w_, cz = c / nav_w_;
				for (int dz = -1; dz <= 1; ++dz) {
					for (int dx = -1; dx <= 1; ++dx) {
						if ((dx == 0 && dz == 0) || nav_blocked(cx + dx, cz + dz) || (dx && dz && (nav_blocked(cx + dx, cz) || nav_blocked(cx, cz + dz)))) {
							continue;
						}
						const int ni = (cz + dz) * nav_w_ + cx + dx;
						if (!labels_[ni]) {
							labels_[ni] = next;
							stack.push_back(ni);
						}
					}
				}
			}
		}
	}
}

int Infestation::nav_label(const Vector3 &p) const {
	if (nav_.empty()) {
		return 0;
	}
	ensure_labels();
	const int x = int(std::floor((p.x - nav_x0_) / NAV_CELL)), z = int(std::floor((p.z - nav_z0_) / NAV_CELL));
	for (int r = 0; r <= 4; ++r) {
		for (int dz = -r; dz <= r; ++dz) {
			for (int dx = -r; dx <= r; ++dx) {
				if ((std::abs(dx) == r || std::abs(dz) == r) && !nav_blocked(x + dx, z + dz)) {
					return labels_[(z + dz) * nav_w_ + x + dx];
				}
			}
		}
	}
	return 0;
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
	route_expanded_ = 0;
	route_capped_ = false;
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
	ensure_labels();
	const int n = nav_w_ * nav_h_;
	const int cap = labels_[sz * nav_w_ + sx] == labels_[gz * nav_w_ + gx] ? route_cap : Math::min(route_cap, HOPELESS_CAP);
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
	while (!open.empty() && expanded < cap) {
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
	route_expanded_ = expanded;
	route_capped_ = best != goal && expanded >= cap;
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

// through_fences: yard fences don't count (thorn vines creep between the pickets and under the rails).
bool clear_of_boxes(const GenContext &ctx, const Vector3 &p, float r, bool through_fences = false) {
	for (const OBox &b : ctx.boxes) {
		if (b.boundary || (through_fences && b.shot_through)) {
			continue;
		}
		if (b.xf.origin.y + b.half.y - ctx.ground(p.x, p.z) < 0.25f) { // Stage 3b: above the ground here
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
	const float g = ctx.ground(p.x, p.z); // Stage 3b: heights above the ground here
	for (const OBox &b : ctx.boxes) {
		if (b.shot_through || b.top() - g < 0.55f || b.xf.origin.y - b.half.y - g > 1.4f) {
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

InfestationLayout plan_infestation(const GenContext &ctx, const MonsterArtKit &art, const Vector3 &origin, bool peaceful, Rng &rng_) {
	InfestationLayout L;
	const std::vector<RoadJunction> mouths = ctx.roads.junctions(0);
	// The gate and the cart sit up the first side road (offsets measured the way the main street runs).
	const Road &lane = mouths.empty() ? ctx.roads.trunk() : ctx.roads.roads[mouths.front().child];
	const float fwd = -float(lane.parent_side); // lateral sign on the lane toward the main street's +s
	const bool town = ctx.kind == SettlementKind::Town;
	const bool legacy = ctx.roads.trunk().x_station; // the pre-Stage-4c town street
	auto to_world = [&](const Vector3 &p) { return p + origin; };
	auto to_local = [&](const Vector3 &p) { return p - origin; };
	// Neighbourhoods (Stage 2a): the gate stands on the island in the middle of the cul-de-sac nearest the
	// spawn, the cart on that turning circle's sidewalk facing it.
	int bulb = -1;
	const bool square = town && ctx.square.valid; // Stage 4c: a town's gate and cart stand on its square
	if (square) {
		L.gate = ctx.square.gate;
		L.cart = ctx.square.cart;
	}
	for (int r = 0; r < int(ctx.roads.roads.size()) && !town; ++r) {
		const Road &R = ctx.roads.roads[r];
		if (R.bulb() && (bulb < 0 || R.bulb_c.distance_to(ctx.spawn) < ctx.roads.roads[bulb].bulb_c.distance_to(ctx.spawn))) {
			bulb = r;
		}
	}
	if (bulb >= 0) {
		const Road &B = ctx.roads.roads[bulb];
		L.gate = B.bulb_c; // the turning circle's own height (Stage 3b: graded roads)
		const Vector3 te = B.tangent_at(B.s_end()), le = B.left_at(B.s_end());
		const float rw = B.bulb_r + B.curb + B.verge + 0.9f; // on the sidewalk ring
		std::vector<Vector3> keep_off; // driveway aprons and front walks round the ring
		for (const Lot &lot : ctx.lots) {
			if (lot.road == bulb) {
				keep_off.push_back(lot.to_world(lot.drive_x, 0.0f, 1.2f));
				keep_off.push_back(lot.to_world(lot.house.door_x, 0.0f, 1.2f));
			}
		}
		Vector3 at = B.bulb_c + te * rw;
		for (float a : { 0.0f, 0.45f, -0.45f, 0.9f, -0.9f, 1.35f, -1.35f, 1.8f, -1.8f, 2.2f, -2.2f }) {
			const Vector3 dir = te * std::cos(a) + le * std::sin(a);
			const Vector3 cand = B.bulb_c + dir * rw;
			bool ok = ctx.roads.zone(cand) == RoadZone::Walk && clear_of_boxes(ctx, cand, 1.1f);
			for (const Vector3 &k : keep_off) {
				ok = ok && Vector2(k.x - cand.x, k.z - cand.z).length() > 2.6f;
			}
			if (ok) {
				at = cand;
				break;
			}
		}
		const Vector3 face = Vector3(B.bulb_c.x - at.x, 0.0f, B.bulb_c.z - at.z).normalized(); // the cart's front faces the gate
		L.cart = Transform3D(Basis(Vector3(0, 1, 0), std::atan2(face.x, face.z)), Vector3(at.x, B.bulb_c.y, at.z));
	}

	// Travel gate: a pad beside the side street, just inside the lantern arch.
	if (bulb < 0 && !square) {
		const float d = lane.s_end() - 2.6f;
		Vector3 local = lane.point(d, fwd * 5.6f);
		if (!clear_of_boxes(ctx, local, 1.3f)) {
			local = lane.point(d, fwd * -5.6f);
		}
		L.gate = local;
	}
	const Vector3 gate_pos = to_world(L.gate);
	// Candy shop cart across the side street from the gate: step onto its doormat to shop.
	if (bulb < 0 && !square) {
		const float side = (mouths.empty() ? ctx.roads.project(0, to_local(gate_pos)) : ctx.roads.project(mouths.front().child, to_local(gate_pos))).lateral * fwd > 0.0f ? -1.0f : 1.0f;
		Vector3 local = lane.point(lane.s_end() - 7.5f, fwd * side * 6.2f);
		for (float back : { 7.5f, 10.5f, 4.5f, 13.0f }) {
			const Vector3 cand = lane.point(lane.s_end() - back, fwd * side * 6.2f);
			if (clear_of_boxes(ctx, cand, 1.1f)) {
				local = cand;
				break;
			}
		}
		// The cart's front (+Z) faces the middle of the side street.
		const Vector3 face = lane.left_at(lane.s_end()) * (-fwd * side);
		L.cart = Transform3D(Basis(Vector3(0, 1, 0), std::atan2(face.x, face.z)), local);
	}

	const Vector3 spawn_local = ctx.spawn;
	const Vector3 gate_local = to_local(gate_pos);
	const Vector3 shop_local = L.cart.xform(Vector3(0, 0, 1.25f)); // the cart's doormat

	// Stage 3c: every nest and thorn nest stands where the walk grid (the raster the monsters walk on, built
	// from the plan alone) connects it to the spawn: in 4c, 6 of 25 hoods had one shut in a fenced backyard.
	Infestation walk;
	{
		RegionPlan rp;
		rp.x0 = ctx.bound_x0 - 3.0f;
		rp.x1 = ctx.bound_x1 + 3.0f;
		rp.z0 = ctx.bound_z0 - 4.0f;
		rp.z1 = ctx.bound_z1 + 4.0f;
		walk.build_nav(ctx, rp);
	}
	const int spawn_label = walk.nav_label(spawn_local);
	auto walk_reachable = [&](const Vector3 &p) { return spawn_label == 0 || walk.nav_label(p) == spawn_label; };

	// Road geometry the rules need: where roads meet, each road's built stretch, the asphalt.
	const std::vector<Road> &roads = ctx.roads.roads;
	std::vector<Vector3> junction_pts;
	for (const Road &R : roads) {
		if (R.parent >= 0) {
			junction_pts.push_back(roads[R.parent].at(R.parent_s));
		}
	}
	auto built = [](const Road &R) {
		return R.lots_s1 > R.lots_s0 ? std::make_pair(R.lots_s0, R.lots_s1) : std::make_pair(R.s_begin(), R.s_end());
	};
	// On some road's asphalt (alongside it, within half a road width of its centreline); `along` gets
	// the fraction along that road's built stretch.
	auto asphalt = [&](const Vector3 &p, float *along) {
		for (int r = 0; r < int(roads.size()); ++r) {
			const RoadHit h = ctx.roads.project(r, p);
			if (h.beyond <= 0.0f && std::abs(h.lateral) < roads[r].half_road) {
				if (along) {
					const auto b = built(roads[r]);
					*along = (h.s - b.first) / Math::max(b.second - b.first, 1.0f);
				}
				return true;
			}
		}
		if (!legacy && ctx.roads.zone(p) == RoadZone::Asphalt) {
			// A turning circle or a junction patch: asphalt, but no "along" a road (nests keep off it).
			if (along) {
				*along = 1.0f;
			}
			return true;
		}
		return false;
	};
	auto near_junction = [&](const Vector3 &p, float d) {
		for (const Vector3 &j : junction_pts) {
			if (Vector2(p.x - j.x, p.z - j.z).length() < d) {
				return true;
			}
		}
		return false;
	};
	auto in_bounds = [&](const Vector3 &p, float m) {
		return p.x > ctx.bound_x0 + m && p.x < ctx.bound_x1 - m && p.z > ctx.bound_z0 + m && p.z < ctx.bound_z1 - m;
	};
	std::vector<Vector3> doors;
	for (const Lot &lot : ctx.lots) {
		doors.push_back(lot.doorbell);
	}
	auto far_from_key_points = [&](const Vector3 &p, float r) {
		if (p.distance_to(spawn_local) < 7.0f || p.distance_to(gate_local) < 4.5f || p.distance_to(shop_local) < 4.5f) {
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
	std::vector<int> &kinds = L.kinds;
	for (int k = 0; k < int(MonsterType::Count); ++k) {
		kinds.push_back(k);
	}
	rng_.shuffle(kinds);
	kinds.resize(rng_.chance(0.5f) ? 3 : 4);
	// Neighbourhoods grew (Stage 2a, 2b): one more nest per ~90 m of built road past Stage 0's ~120 m, one more
	// thorn nest per ~220 m past ~40 m (Stage 2a's 260 m step, carried on), no extra draws.
	float built_len = 0.0f;
	for (const Road &R : ctx.roads.roads) {
		built_len += Math::max(0.0f, R.lots_s1 - R.lots_s0);
	}
	// Stage 4c (user: "a lot more monster spawners and some more thorns"): about 2.5x the nests and +50-75% thorn
	// nests, scaled by the built street length and randomised: hoods 18-24 nests and 6-8 thorn nests, towns (which
	// vary in size now) 12-20 and 4-6.
	int want_nests, want_hearts;
	if (town) {
		want_nests = Math::clamp(12 + int((built_len - 150.0f) / 45.0f), 12, 19) + rng_.irange(0, 1);
		want_hearts = Math::clamp(4 + int((built_len - 200.0f) / 160.0f), 4, 5) + rng_.irange(0, 1);
	} else {
		want_nests = Math::clamp(18 + int((built_len - 400.0f) / 45.0f), 18, 23) + rng_.irange(0, 1);
		want_hearts = Math::clamp(6 + int((built_len - 450.0f) / 150.0f), 6, 7) + rng_.irange(0, 1);
	}
	if (peaceful) {
		want_nests = want_hearts = 0;
	}
	L.want_nests = want_nests;
	L.want_hearts = want_hearts;

	// One spacing rule across every nest; relaxed 15% at a time (at most twice, and it stays relaxed)
	// when the draws run dry, rather than placing fewer.
	struct Placed {
		Vector3 p;
		bool heart;
	};
	std::vector<Placed> placed;
	float scale = 1.0f;
	int level = 0;
	auto spaced = [&](const Vector3 &p, bool heart) {
		for (const Placed &q : placed) {
			const float gap = heart && q.heart ? 18.0f : (heart != q.heart ? 12.0f : 14.0f);
			if (Vector2(p.x - q.p.x, p.z - q.p.z).length() < gap * scale) {
				return false;
			}
		}
		return true;
	};
	// Weighted draw (toward open ground that's easy to shoot into) among the unused candidates that
	// pass `ok`; base weight 0 marks a candidate that fails a fixed rule.
	auto draw = [&](const std::vector<float> &base, std::vector<char> &used, const std::function<bool(int)> &ok, const std::function<float(int)> &mul = nullptr) {
		for (;;) {
			std::vector<float> w(base.size(), 0.0f);
			float total = 0.0f;
			for (size_t i = 0; i < base.size(); ++i) {
				if (base[i] > 0.0f && !used[i] && ok(int(i))) {
					w[i] = base[i] * (mul ? mul(int(i)) : 1.0f);
					total += w[i];
				}
			}
			if (total > 0.0f) {
				float roll = rng_.range(0.0f, total);
				int got = -1;
				for (size_t i = 0; i < w.size(); ++i) {
					if (w[i] <= 0.0f) {
						continue;
					}
					got = int(i);
					roll -= w[i];
					if (roll <= 0.0f) {
						break;
					}
				}
				used[got] = 1;
				return got;
			}
			if (level >= 2) {
				return -1;
			}
			++level;
			scale *= 0.85f;
		}
	};
	// Candidate classes (a spot on the asphalt is ROAD whatever loop made it).
	enum { ROAD, WALK, LOT, OPEN, CLASSES };
	struct Cand {
		Vector3 p;
		bool road = false;
		float along = 0.0f;
		int cls = LOT;
		bool back_row = false; // Stage 4c: a town's open ground out behind its rows (drawn at a fifth of the weight)
	};
	auto add_cand = [&](std::vector<Cand> &out, const Vector3 &p, int cls = LOT) {
		Cand c;
		c.p = p;
		c.road = asphalt(p, &c.along);
		c.cls = c.road ? int(ROAD) : cls;
		out.push_back(c);
	};

	// Thorn nests first (the widest gaps): two or three huge bramble masses with a glowing heart.
	// Every thorn vine in the area grows out of one of them. Anywhere a player can reach: front lawns
	// and backyards, shop service yards, the town square and pocket parks, sidewalks, the street itself
	// (a bramble mass blocking the road), but at most one on a road.
	std::vector<Cand> heart_cands;
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		const float zb = h.front_z - h.depth;
		if (town) {
			const float hw = lot.width * 0.38f;
			if (lot.depth + zb > 4.4f) {
				add_cand(heart_cands, lot.to_world(rng_.range(-hw, hw), 0, rng_.range(-lot.depth + 1.8f, zb - 1.8f)));
			}
		} else {
			// The front lawn between the porch steps and the sidewalk (the mass kept on the grass), and the backyard.
			const float fw = Math::max(0.0f, lot.width * 0.5f - 2.0f);
			const float z0 = h.front_z + h.porch_d + 1.8f, z1 = -1.8f;
			if (z0 < z1) {
				for (int k = 0; k < 2; ++k) {
					add_cand(heart_cands, lot.to_world(rng_.range(-fw, fw), 0, rng_.range(z0, z1)));
				}
			}
			const float hw = lot.width * 0.3f;
			for (int k = 0; k < 2; ++k) {
				add_cand(heart_cands, lot.to_world(h.house_x + rng_.range(-hw, hw), 0, zb - rng_.range(2.8f, 4.5f)));
			}
		}
	}
	for (int r = 0; r < int(roads.size()); ++r) {
		const Road &R = roads[r];
		const auto b = built(R);
		const float len = b.second - b.first;
		// On the asphalt, near the crown.
		for (int k = 0, n = Math::max(2, int(len / 8.0f)); k < n; ++k) {
			add_cand(heart_cands, R.point(Math::lerp(b.first, b.second, rng_.range(0.15f, 0.85f)), rng_.range(-1.2f, 1.2f)));
		}
		for (int side : { 1, -1 }) {
			// On the sidewalk.
			for (int k = 0, n = Math::max(2, int(len / 12.0f + 0.5f)); k < n; ++k) {
				const float lat = R.half_road + R.curb + R.verge + (R.walk - R.verge) * rng_.range(0.3f, 0.7f);
				add_cand(heart_cands, R.point(Math::lerp(b.first, b.second, rng_.range(0.05f, 0.95f)), side * lat), WALK);
			}
			// Open frontage with no lot: the town square, pocket parks.
			for (const auto &g : frontage_gaps(ctx, r, side, 5.0f)) {
				const Transform3D frame = R.frontage_frame((g.first + g.second) * 0.5f, side);
				const float hw = Math::max(0.0f, (g.second - g.first) * 0.5f - 1.6f);
				for (int k = 0; k < 2; ++k) {
					add_cand(heart_cands, frame.xform(Vector3(rng_.range(-hw, hw), 0, -rng_.range(2.0f, town ? 13.0f : 8.0f))), OPEN);
				}
			}
		}
	}
	L.heart_candidates = int(heart_cands.size());
	std::vector<float> heart_w;
	for (const Cand &c : heart_cands) {
		const bool ok = in_bounds(c.p, 2.0f) && clear_of_boxes(ctx, c.p, 1.6f) && far_from_key_points(c.p, 4.0f) &&
				c.p.distance_to(spawn_local) >= 12.0f && !(c.road && near_junction(c.p, 12.0f)) && walk_reachable(c.p);
		heart_w.push_back(ok ? spot_weight(ctx, c.p) : 0.0f);
		L.road_heart_candidates += ok && c.road ? 1 : 0;
	}
	{
		// Each class (road, sidewalk, yards, open ground) gets the same share of the draw, so the most
		// open ground (the road, then the sidewalk) doesn't win every time; openness still picks the
		// spot within a class.
		float total[CLASSES] = {};
		for (size_t i = 0; i < heart_cands.size(); ++i) {
			total[heart_cands[i].cls] += heart_w[i];
		}
		for (size_t i = 0; i < heart_cands.size(); ++i) {
			if (heart_w[i] > 0.0f) {
				heart_w[i] /= total[heart_cands[i].cls];
			}
		}
	}

	// Monster nests: anywhere across front yards and backyards (homes), service yards and sidewalks
	// (shops), open ground off the street, and out on the asphalt of any road.
	std::vector<Cand> spots;
	for (const Lot &lot : ctx.lots) {
		const HouseSpec &h = lot.house;
		const float zb = h.front_z - h.depth;
		const float hw = lot.width * 0.42f;
		if (town) {
			if (lot.depth + zb > 3.6f) {
				for (int k = 0; k < 2; ++k) {
					add_cand(spots, lot.to_world(rng_.range(-hw, hw), 0, rng_.range(-lot.depth + 1.2f, zb - 1.2f)));
				}
			}
			for (int k = 0; k < 2; ++k) {
				add_cand(spots, lot.to_world(rng_.range(-hw, hw), 0, rng_.range(1.6f, 2.8f)));
			}
		} else {
			for (int k = 0; k < 3; ++k) {
				add_cand(spots, lot.to_world(rng_.range(-hw, hw), 0, rng_.range(1.4f, h.front_z + h.porch_d + 0.6f)));
			}
			if (lot.depth + zb > 4.5f) {
				for (int k = 0; k < 3; ++k) {
					add_cand(spots, lot.to_world(rng_.range(-hw, hw), 0, rng_.range(-lot.depth + 1.5f, zb - 1.5f)));
				}
			}
		}
	}
	if (legacy) {
		const Road &s = ctx.roads.trunk();
		for (int k = 0; k < 10; ++k) {
			const float x = Math::lerp(s.lots_s0, s.lots_s1, rng_.range(0.06f, 0.94f));
			const float side = rng_.chance(0.5f) ? 1.0f : -1.0f;
			add_cand(spots, s.at(x) + s.left_at(x) * side * (s.edge() + (town ? rng_.range(4.5f, 7.5f) : rng_.range(2.0f, 4.5f))));
		}
	} else {
		// Off the sidewalk (front lawns' edges, park corners, the wedges between lots): about 10 per 100 m
		// of every road's built stretch.
		for (const Road &s : roads) {
			for (int k = 0, n = Math::max(2, int((s.lots_s1 - s.lots_s0) / 10.0f + 0.5f)); k < n; ++k) {
				const float x = Math::lerp(s.lots_s0, s.lots_s1, rng_.range(0.06f, 0.94f));
				const float side = rng_.chance(0.5f) ? 1.0f : -1.0f;
				add_cand(spots, s.at(x) + s.left_at(x) * side * (s.edge() + rng_.range(2.0f, 4.5f)));
			}
		}
	}
	for (const Road &R : roads) {
		const auto b = built(R);
		for (int k = 0, n = Math::max(2, int((b.second - b.first) / 10.0f + 0.5f)); k < n; ++k) {
			add_cand(spots, R.point(Math::lerp(b.first, b.second, rng_.range(0.15f, 0.85f)), rng_.range(-2.5f, 2.5f)));
		}
	}
	if (town && !legacy) {
		// Stage 4c: a town holds 12-20 nests in a smaller area than a neighbourhood: every open spot on a 6 m grid
		// too (sidewalks, the square and the plazas, service yards, open ground between and behind the rows).
		for (float z = ctx.bound_z0 + 3.0f; z < ctx.bound_z1 - 3.0f; z += 6.0f) {
			for (float x = ctx.bound_x0 + 3.0f; x < ctx.bound_x1 - 3.0f; x += 6.0f) {
				const Vector3 p(x, 0.0f, z);
				const RoadZone zn = ctx.roads.zone(p);
				const RoadHit nh = ctx.roads.nearest(p);
				// (Out behind the rows only as a fallback, at a tiny weight, so the streets get the nests first.)
				const bool by_street = nh.road >= 0 && std::abs(nh.lateral) < roads[nh.road].edge() + 9.0f;
				if (zn == RoadZone::Walk || zn == RoadZone::Off) {
					add_cand(spots, p, zn == RoadZone::Walk ? WALK : OPEN);
					spots.back().back_row = !by_street;
				}
			}
		}
	}
	L.nest_candidates = int(spots.size());
	float nest_r = 0.0f; // clearance for the biggest nest in the mix
	for (int k : kinds) {
		nest_r = Math::max(nest_r, art[MonsterType(k)].nest_radius + 0.6f);
	}
	std::vector<float> spot_w;
	for (const Cand &c : spots) {
		bool ok = in_bounds(c.p, 1.0f) && clear_of_boxes(ctx, c.p, nest_r) && far_from_key_points(c.p, 3.0f) && walk_reachable(c.p);
		if (ok && c.road) {
			// Out on the asphalt: mid-road, well clear of junctions, the spawn, the gate and the cart.
			ok = c.along >= 0.15f && c.along <= 0.85f && !near_junction(c.p, 10.0f) && c.p.distance_to(spawn_local) >= 10.0f &&
					c.p.distance_to(gate_local) >= 7.0f && c.p.distance_to(shop_local) >= 7.0f;
		}
		spot_w.push_back(ok ? spot_weight(ctx, c.p) * (c.back_row ? 0.002f : 1.0f) : 0.0f); // (back row: in effect only once the street-side spots run dry)
		L.road_nest_candidates += ok && c.road ? 1 : 0;
	}
	// Hearts first (the widest gaps), then the nests. Random picks one after another can jam a small
	// area even at the relaxed gaps: then start over (the rng carries on, so it stays deterministic),
	// up to four tries, keeping the fullest.
	int best_n = -1;
	for (int attempt = 0; attempt < 4 && best_n < want_hearts + want_nests; ++attempt) {
		InfestationLayout T;
		placed.clear();
		scale = 1.0f;
		level = 0;
		std::vector<char> heart_used(heart_cands.size(), 0), spot_used(spots.size(), 0);
		int road_hearts = 0;
		while (int(T.hearts.size()) < want_hearts) {
			const int i = draw(heart_w, heart_used, [&](int j) {
				return !(heart_cands[j].road && road_hearts > 0) && spaced(heart_cands[j].p, true);
			});
			if (i < 0) {
				break;
			}
			T.hearts.push_back(heart_cands[i].p);
			T.heart_on_road.push_back(heart_cands[i].road ? 1 : 0);
			road_hearts += heart_cands[i].road ? 1 : 0;
			placed.push_back({ heart_cands[i].p, true });
		}
		T.relax_hearts = level;
		// Stage 4c: spread through the whole area (every street, cul-de-sac, yard and backyard), not clustered:
		// a spot's weight falls off with how close the nearest nest already placed is (under ~36 m).
		auto spread = [&](int j) {
			float d = 1e9f;
			for (const Placed &q : placed) {
				if (!q.heart) {
					d = Math::min(d, Vector2(spots[j].p.x - q.p.x, spots[j].p.z - q.p.z).length());
				}
			}
			const float f = Math::clamp(d / 36.0f, 0.08f, 1.0f);
			return f * f;
		};
		while (int(T.nests.size()) < want_nests) {
			const int i = draw(spot_w, spot_used, [&](int j) { return spaced(spots[j].p, false); }, spread);
			if (i < 0) {
				break;
			}
			T.nests.push_back(spots[i].p);
			T.nest_on_road.push_back(spots[i].road ? 1 : 0);
			placed.push_back({ spots[i].p, false });
		}
		T.relax = level;
		L.tries = attempt + 1;
		const int n = int(T.hearts.size() + T.nests.size());
		if (n > best_n) {
			best_n = n;
			L.hearts = T.hearts;
			L.heart_on_road = T.heart_on_road;
			L.nests = T.nests;
			L.nest_on_road = T.nest_on_road;
			L.relax_hearts = T.relax_hearts;
			L.relax = T.relax;
		}
	}
	for (size_t i = 0; i < L.nests.size(); ++i) {
		L.nest_seeds.push_back(rng_.next()); // each nest's own seed, drawn as the nests are made
	}
	for (size_t i = 0; i < L.hearts.size(); ++i) {
		L.heart_yaw.push_back(rng_.range(0.0f, Math_TAU)); // each root mass sprawls its own way
		L.heart_seeds.push_back(rng_.next());
	}

	// Thorn vines creep out over the ground from every thorn nest: a few tendrils each, one reaching
	// for the nearest free front door, where it knots into a big thorn cluster that blocks the doorbell.
	auto blocked = [&](const Vector3 &p) {
		if (!in_bounds(p, 1.0f)) {
			return true;
		}
		if (p.distance_to(spawn_local) < 5.5f || p.distance_to(gate_local) < 2.6f || p.distance_to(shop_local) < 3.0f) {
			return true;
		}
		return !clear_of_boxes(ctx, p, 0.3f, true);
	};
	auto grow =[&](Vector3 pos, Vector3 dir, float length, const Vector3 *target) {
		std::vector<Vector3> pts = { pos };
		const float step = 0.7f;
		for (float run = 0.0f; run < length; run += step) {
			if (target) {
				const Vector3 want = Vector3(target->x - pos.x, 0, target->z - pos.z).normalized();
				dir = (dir * 0.55f + want * 0.45f).normalized();
			}
			dir = dir.rotated(Vector3(0, 1, 0), rng_.range(-0.35f, 0.35f));
			bool moved = false;
			// Reaching for a door it may slide along a wall (nearly square turns) to get round a house.
			static const float free_turns[] = { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f };
			static const float door_turns[] = { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f };
			const float *turns = target ? door_turns : free_turns;
			const int n_turns = target ? 7 : 5;
			for (int t = 0; t < n_turns; ++t) {
				const Vector3 d = dir.rotated(Vector3(0, 1, 0), turns[t]);
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
		door_paths.push_back(town ? lot.to_world(h.door_x, 0, 1.5f) : lot.to_world(h.door_x, 0, lot.flight.empty() ? h.front_z + h.porch_d + 1.08f + 1.3f : Math::min(lot.flight_foot + 1.3f, Math::max(lot.flight_foot + 0.5f, lot.walk_end - 0.4f)))); // Stage 3c: past the flight as built, short of the sidewalk
	}
	std::vector<char> door_taken(door_paths.size(), 0);
	for (size_t ni = 0; ni < L.hearts.size(); ++ni) {
		const Vector3 c = L.hearts[ni];
		const float r = 2.0f;
		// Uneven spokes: a varied count, angles knocked off the even spread, lengths from stubby to long.
		const int n = rng_.irange(4, 8);
		const float a0 = rng_.range(0.0f, Math_TAU);
		for (int k = 0; k < n; ++k) {
			const float a = a0 + (k + rng_.range(-0.35f, 0.35f)) * Math_TAU / n;
			const Vector3 dir = Vector3(std::cos(a), 0, std::sin(a));
			const float len = rng_.chance(0.3f) ? rng_.range(4.0f, 8.0f) : rng_.range(8.0f, 18.0f);
			if (blocked(c + dir * r)) {
				continue; // the root mass's edge runs into a fence or a wall here
			}
			std::vector<Vector3> pts = grow(c + dir * r, dir, len, nullptr);
			if (pts.size() >= 4) {
				// A side branch partway along.
				if (pts.size() > 6 && rng_.chance(0.6f)) {
					const size_t at = Math::clamp(size_t(pts.size() * rng_.range(0.3f, 0.7f)), size_t(1), pts.size() - 2);
					const Vector3 bd = (pts[at] - pts[at - 1]).normalized().rotated(Vector3(0, 1, 0), rng_.chance(0.5f) ? 0.9f : -0.9f);
					std::vector<Vector3> branch = grow(pts[at], bd, rng_.range(4.0f, 7.0f), nullptr);
					if (branch.size() >= 4) {
						L.tendrils.push_back({ branch, int(ni) });
					}
				}
				L.tendrils.push_back({ pts, int(ni) });
			}
		}
		// Reach for the nearest free front door; if the way there is shut (a heart behind a house, a
		// fenced yard), the next nearest, up to three.
		std::vector<std::pair<float, int>> order;
		for (size_t d = 0; d < door_paths.size(); ++d) {
			const float dd = door_paths[d].distance_to(c);
			if (!door_taken[d] && dd < 26.0f && door_paths[d].distance_to(spawn_local) > 7.0f) {
				order.push_back({ dd, int(d) });
			}
		}
		std::sort(order.begin(), order.end());
		for (size_t k = 0; k < order.size() && k < 3; ++k) {
			const Vector3 target = door_paths[order[k].second];
			const Vector3 dir = Vector3(target.x - c.x, 0, target.z - c.z).normalized();
			if (blocked(c + dir * r)) {
				continue;
			}
			std::vector<Vector3> pts = grow(c + dir * r, dir, order[k].first * 1.8f, &target);
			if (pts.size() >= 3 && pts.back().distance_to(target) < 1.5f) {
				door_taken[order[k].second] = 1;
				L.tendrils.push_back({ pts, int(ni) });
				L.knots.push_back({ target, int(ni) });
				break;
			}
		}
	}
	if (ctx.terrain) {
		// Stage 3b: everything stands on the graded ground (picked in the plane above).
		auto settle = [&](Vector3 &p) { p.y = ctx.ground(p.x, p.z); };
		for (Vector3 &p : L.nests) {
			settle(p);
		}
		for (Vector3 &p : L.hearts) {
			settle(p);
		}
		for (auto &t : L.tendrils) {
			for (Vector3 &p : t.pts) {
				settle(p);
			}
		}
		for (auto &k : L.knots) {
			settle(k.first);
		}
		settle(L.gate);
		L.cart.origin.y = ctx.ground(L.cart.origin.x, L.cart.origin.z);
	}
	return L;
}

// The walk grid and the layout, in the order build() always made them (the same rng_ draws), with no
// nodes: WorldStreamer's area preloading runs this on a worker thread before the area is attached.
void Infestation::prepare(const GenContext &ctx, const RegionPlan &plan, const MonsterArtKit &art, bool peaceful) {
	rng_ = Rng(plan.seed ^ 0xBADC0DEull, 11);
	build_nav(ctx, plan);
	prepared_layout_ = plan_infestation(ctx, art, plan.origin, peaceful, rng_);
	prepared_ = true;
	prepared_peaceful_ = peaceful;
}

void Infestation::build(Area &area, MonsterWorld *world, Node3D *root, bool peaceful) {
	build_begin(area, world, root, peaceful);
	build_step(0);
}

// The gate and the cart now; the nests, thorn nests, tendrils and knots one by one in build_step(), in
// the order build() always made them (tendrils and knots draw from rng_).
void Infestation::build_begin(Area &area, MonsterWorld *world, Node3D *root, bool peaceful) {
	world_ = world;
	root_ = root;
	const GenContext &ctx = *area.gen->context();
	AreaState &st = *area.state;
	if (!prepared_ || prepared_peaceful_ != peaceful) {
		prepare(ctx, area.plan, *world->art, peaceful);
	}
	prepared_ = false;
	leash_ = Rect2(area.plan.x0 - 12.0f, area.plan.z0 - 12.0f, area.plan.x1 - area.plan.x0 + 24.0f, area.plan.z1 - area.plan.z0 + 24.0f);
	build_layout_ = std::move(prepared_layout_);
	const InfestationLayout &layout = build_layout_;
	if (!peaceful) {
		int nests_road = 0, hearts_road = 0;
		for (char c : layout.nest_on_road) {
			nests_road += c ? 1 : 0;
		}
		for (char c : layout.heart_on_road) {
			hearts_road += c ? 1 : 0;
		}
		String where;
		for (size_t i = 0; i < layout.nests.size(); ++i) {
			const Vector3 w = area.to_world(layout.nests[i]);
			where += String(" ") + (layout.nest_on_road[i] ? "road" : "off-road") + "(" + String::num(w.x, 1) + "," + String::num(w.z, 1) + ")";
		}
		UtilityFunctions::print("Infestation: ", int(layout.nests.size()), " nests (", nests_road, " on the road), ", int(layout.hearts.size()), " thorn nests (",
				hearts_road, " on the road), ", int(layout.knots.size()), " doors knotted, spacing relax ", layout.relax, ";", where);
	}

	// Travel gate: a pad beside the side street, just inside the lantern arch.
	{
		gate_pos_ = area.to_world(layout.gate);
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
		const Transform3D xf(layout.cart.basis, area.to_world(layout.cart.origin));
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
	if (st.spawner_down.size() != layout.nests.size()) {
		st.spawner_down.assign(layout.nests.size(), 0);
	}
	if (st.heart_down.size() != layout.hearts.size()) {
		st.heart_down.assign(layout.hearts.size(), 0);
	}
	if (st.vine_down.size() != layout.tendrils.size() + layout.knots.size()) {
		st.vine_down.assign(layout.tendrils.size() + layout.knots.size(), 0);
	}
	build_state_ = &st;
	build_origin_ = area.plan.origin;
	build_peaceful_ = peaceful;
	build_unit_ = 0;
	building_ = true;
}

bool Infestation::build_step(uint64_t deadline_us) {
	if (!building_) {
		return true;
	}
	MonsterWorld *world = world_;
	Node3D *root = root_;
	const InfestationLayout &layout = build_layout_;
	AreaState &st = *build_state_;
	const Vector3 origin = build_origin_;
	auto to_world = [&](const Vector3 &p) { return p + origin; }; // Area::to_world
	const std::vector<int> &kinds = layout.kinds;
	const std::vector<Vector3> &picked = layout.nests;
	const std::vector<Vector3> &heart_spots = layout.hearts;
	const std::vector<InfestationLayout::Tendril> &planned = layout.tendrils;
	const std::vector<std::pair<Vector3, int>> &knots = layout.knots;
	const size_t units = picked.size() + heart_spots.size() + planned.size() + knots.size();
	bool did = false;
	const float lift = 0.02f;
	for (; build_unit_ < units; ++build_unit_) {
		if (did && deadline_us != 0 && NeighborhoodGenerator::build_clock_us() >= deadline_us) {
			return false;
		}
		did = true;
		size_t i = build_unit_;
		if (i < picked.size()) {
			// Monster nests and thorn nests, from the layout (one monster type per nest).
			const MonsterType mt = MonsterType(kinds[i % kinds.size()]);
			MonsterNest *n = memnew(MonsterNest);
			root->add_child(n);
			n->set_global_position(to_world(picked[i]));
			n->init(mt, world, layout.nest_seeds[i]);
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
			continue;
		}
		i -= picked.size();
		if (i < heart_spots.size()) {
			ThornVine *h = memnew(ThornVine);
			root->add_child(h);
			h->set_global_position(to_world(heart_spots[i]));
			h->rotate_y(layout.heart_yaw[i]); // each root mass sprawls its own way
			h->init(world, int(i) + 1, 1.7f, layout.heart_seeds[i]);
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
			continue;
		}
		i -= heart_spots.size();
		if (i < planned.size()) {
			// Thorn vines creeping out of the thorn nests and the knots blocking front doors, from the layout.
			std::vector<Vector3> world_pts;
			for (const Vector3 &p : planned[i].pts) {
				world_pts.push_back(to_world(p + Vector3(0, lift, 0)));
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
			continue;
		}
		i -= planned.size();
		ThornVine *v = memnew(ThornVine);
		root->add_child(v);
		v->set_global_position(to_world(knots[i].first));
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
	building_ = false;
	freed_ = false;
	if (build_peaceful_) {
		st.freed = true;
	}
	if (st.freed) {
		set_freed(false);
	}
	return true;
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
	if (!freed_ && !building_ && nests_left() == 0 && hearts_left() == 0) { // (not while build_step() is still making them)
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
