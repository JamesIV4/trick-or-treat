#include "play/dragon_controller.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "core/movement.h"
#include "play/drop_shadow.h"
#include "play/vfx.h"
#include "gen/neighborhood_generator.h"

using namespace godot;

namespace tot {

namespace {
using namespace move;
constexpr float ROLL_SPEED = 12.5f;
constexpr float GLIDE_FALL = 2.3f;

float angle_lerp(float a, float b, float t) {
	float d = Math::fmod(b - a + Math_PI * 3.0f, Math_TAU) - Math_PI;
	return a + d * t;
}
} // namespace

void DragonController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_visual", "visual"), &DragonController::set_visual);
	ClassDB::bind_method(D_METHOD("set_fill", "fill"), &DragonController::set_fill);
	ClassDB::bind_method(D_METHOD("teleport", "position", "yaw"), &DragonController::teleport);
	ClassDB::bind_method(D_METHOD("collect_radius"), &DragonController::collect_radius);
	ClassDB::bind_method(D_METHOD("is_spinning"), &DragonController::is_spinning);
	ClassDB::bind_method(D_METHOD("get_speed"), &DragonController::get_speed);
	ClassDB::bind_method(D_METHOD("set_input_enabled", "enabled"), &DragonController::set_input_enabled);
	ClassDB::bind_method(D_METHOD("get_input_enabled"), &DragonController::get_input_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "input_enabled"), "set_input_enabled", "get_input_enabled");
	ADD_SIGNAL(MethodInfo("jumped"));
	ADD_SIGNAL(MethodInfo("flapped"));
	ADD_SIGNAL(MethodInfo("landed", PropertyInfo(Variant::FLOAT, "impact")));
	ADD_SIGNAL(MethodInfo("pounded", PropertyInfo(Variant::VECTOR3, "position")));
	ADD_SIGNAL(MethodInfo("bounced"));
	ADD_SIGNAL(MethodInfo("spun"));
	ADD_SIGNAL(MethodInfo("fired"));
	ADD_SIGNAL(MethodInfo("stepped", PropertyInfo(Variant::INT, "parity")));
	ClassDB::bind_method(D_METHOD("celebrate", "kind"), &DragonController::celebrate);
}

// Small one-shot particle bursts (dust puffs, wing sparkles) from a shared pool.
void DragonController::puff(const Vector3 &pos, int amount, float spread, const Color &color, float speed) {
	if (Vfx *vfx = Vfx::get()) {
		// Purple puffs are wing magic (sparkles); everything else is a little cartoon dust poof.
		if (color.b > color.g && color.r > 0.6f) {
			vfx->sparkles(pos, color, amount, speed, 0.8f, 0.6f);
		} else {
			vfx->smoke(pos, color.lerp(Color(1, 1, 1), 0.3f), 0.35f + spread * 0.9f, Math::max(2, amount / 3));
		}
		return;
	}
	if (puffs_.empty()) {
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		Ref<SphereMesh> ball;
		ball.instantiate();
		ball->set_radius(0.09f);
		ball->set_height(0.18f);
		ball->set_radial_segments(8);
		ball->set_rings(4);
		ball->set_material(mat);
		Ref<Gradient> fade;
		fade.instantiate();
		fade->set_offsets(PackedFloat32Array({ 0.0f, 0.6f, 1.0f }));
		fade->set_colors(PackedColorArray({ Color(1, 1, 1, 0.85f), Color(1, 1, 1, 0.5f), Color(1, 1, 1, 0.0f) }));
		for (int i = 0; i < 8; ++i) {
			CPUParticles3D *p = memnew(CPUParticles3D);
			p->set_emitting(false);
			p->set_one_shot(true);
			p->set_explosiveness_ratio(1.0f);
			p->set_lifetime(0.55);
			p->set_mesh(ball);
			p->set_use_local_coordinates(false);
			p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
			p->set_direction(Vector3(0, 1, 0));
			p->set_gravity(Vector3(0, 0.6f, 0));
			p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.5f);
			p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
			p->set_param_min(CPUParticles3D::PARAM_DAMPING, 3.0f);
			p->set_param_max(CPUParticles3D::PARAM_DAMPING, 5.0f);
			p->set_color_ramp(fade);
			p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			add_child(p);
			puffs_.push_back(p);
		}
	}
	CPUParticles3D *p = puffs_[next_puff_];
	next_puff_ = (next_puff_ + 1) % int(puffs_.size());
	p->set_global_position(pos);
	p->set_amount(amount);
	p->set_emission_sphere_radius(spread);
	p->set_spread(spread > 0.3f ? 75.0f : 50.0f);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * 0.5f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed);
	p->set_color(color);
	p->restart();
	p->set_emitting(true);
}

void DragonController::celebrate(int kind) {
	bucket_punch_ = kind >= 4 ? 1.4f : 1.0f;
	squash_v_ -= kind >= 4 ? 1.6f : 0.7f;
}

void DragonController::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	add_to_group("player");
	set_collision_mask(1 | 2);
	set_floor_max_angle(Math::deg_to_rad(46.0f));
	set_floor_snap_length(0.3f);
	set_up_direction(Vector3(0, 1, 0));
	bool has_shape = false;
	for (int i = 0; i < get_child_count(); ++i) {
		if (Object::cast_to<CollisionShape3D>(get_child(i))) {
			has_shape = true;
		}
	}
	if (!has_shape) {
		CollisionShape3D *cs = memnew(CollisionShape3D);
		Ref<CapsuleShape3D> cap;
		cap.instantiate();
		cap->set_radius(0.3f);
		cap->set_height(1.05f);
		cs->set_shape(cap);
		cs->set_position(Vector3(0, 0.525f, 0));
		add_child(cs);
	}
	visual_root_ = memnew(Node3D);
	visual_root_->set_name("VisualRoot");
	add_child(visual_root_);
	Decal *shadow = make_drop_shadow(make_shadow_texture(), 1.05f, 12.0f, Color(1, 1, 1, 0.9f));
	shadow->set_name("DropShadow");
	shadow->set_position(Vector3(0, 0.25f - 6.0f, 0));
	add_child(shadow);
	set_physics_process(true);
	set_process(true);
}

void DragonController::find_parts(Node *n) {
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		const String name = n3->get_name();
		Part *p = nullptr;
		if (name == "body") p = &body_;
		else if (name == "head") p = &head_;
		else if (name == "wing_l") p = &wing_l_;
		else if (name == "wing_r") p = &wing_r_;
		else if (name == "tail") p = &tail_;
		else if (name == "leg_l") p = &leg_l_;
		else if (name == "leg_r") p = &leg_r_;
		else if (name == "arm_l") p = &arm_l_;
		else if (name == "arm_r") p = &arm_r_;
		else if (name == "bucket_candy") p = &bucket_candy_;
		if (p) {
			p->node = n3;
			p->rest = n3->get_transform();
		}
		if (name == "bucket") {
			bucket_ = n3;
			bucket_rest_ = n3->get_transform();
		}
		if (name == "socket_bucket_light" && !bucket_light_) {
			bucket_light_ = memnew(OmniLight3D);
			bucket_light_->set_color(Color(1.0f, 0.55f, 0.2f));
			bucket_light_->set_param(Light3D::PARAM_RANGE, 2.6f);
			bucket_light_->set_param(Light3D::PARAM_ENERGY, 0.7f);
			bucket_light_->set_shadow(false);
			n3->add_child(bucket_light_);
		}
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		find_parts(n->get_child(i));
	}
}

void DragonController::set_visual(Node3D *visual) {
	if (!visual_root_) {
		return;
	}
	if (visual_) {
		visual_->queue_free();
	}
	visual_ = visual;
	visual_root_->add_child(visual_);
	body_ = head_ = wing_l_ = wing_r_ = tail_ = leg_l_ = leg_r_ = arm_l_ = arm_r_ = bucket_candy_ = Part();
	bucket_light_ = nullptr;
	find_parts(visual_);
	if (!bucket_light_) {
		bucket_light_ = memnew(OmniLight3D);
		bucket_light_->set_color(Color(1.0f, 0.55f, 0.2f));
		bucket_light_->set_param(Light3D::PARAM_RANGE, 2.6f);
		bucket_light_->set_shadow(false);
		bucket_light_->set_position(Vector3(-0.35f, 0.45f, 0.1f));
		visual_root_->add_child(bucket_light_);
	}
	// Moonlight rim keeps the teal silhouette readable against the blue night.
	set_instance_param_recursive(visual_, "rim_strength", 0.22f);
	set_layers_recursive(visual_, LAYER_NO_DECALS);
	set_fill(fill_);
}

void DragonController::set_fill(float fill) {
	fill_ = Math::clamp(fill, 0.0f, 1.0f);
	if (bucket_candy_.node) {
		const float s = Math::max(0.04f, fill_);
		bucket_candy_.node->set_transform(Transform3D(bucket_candy_.rest.basis.scaled(Vector3(1.0f, s, 1.0f)), bucket_candy_.rest.origin));
	}
	if (bucket_light_) {
		bucket_light_->set_param(Light3D::PARAM_ENERGY, 0.6f + fill_ * 1.8f);
		bucket_light_->set_param(Light3D::PARAM_RANGE, 2.2f + fill_ * 2.0f);
	}
}

void DragonController::teleport(const Vector3 &pos, float yaw) {
	set_global_position(pos);
	set_velocity(Vector3());
	facing_ = visual_yaw_ = yaw;
	if (visual_root_) {
		visual_root_->set_rotation(Vector3(0, yaw, 0));
	}
}

// Forgiveness in the player's favour (Thorson, "Celeste & Forgiveness"): bonking the edge of a
// ceiling slides the kid around it, and clipping a ledge lip in the air pops them up onto it.
void DragonController::assist(const Vector3 &wish) {
	const Transform3D gt = get_global_transform();
	Vector3 v = get_velocity();
	if (is_on_ceiling() && last_vy_ > 2.0f) {
		const Vector3 up(0, 0.35f, 0);
		for (int k = 0; k < 8; ++k) {
			const float a = k * Math_TAU / 8.0f;
			const Vector3 off(std::cos(a) * 0.32f, 0, std::sin(a) * 0.32f);
			if (!test_move(gt, off) && !test_move(gt.translated(off), up)) {
				set_global_position(gt.origin + off);
				v.y = last_vy_;
				set_velocity(v);
				return;
			}
		}
	}
	if (!is_on_wall() || is_on_floor() || v.y > 3.0f || wish.length() < 0.3f) {
		return;
	}
	Vector3 n = get_wall_normal();
	n.y = 0.0f;
	if (n.length() < 0.5f || wish.normalized().dot(-n.normalized()) < 0.5f) {
		return;
	}
	const Vector3 up(0, 0.6f, 0);
	const Vector3 in = -n.normalized() * 0.35f;
	Ref<KinematicCollision3D> hit;
	hit.instantiate();
	if (!test_move(gt, up) && !test_move(gt.translated(up), in) && test_move(gt.translated(up + in), Vector3(0, -0.75f, 0), hit) &&
			hit->get_normal().y > 0.7f) {
		set_global_position(gt.origin + up + in);
		v.y = 0.0f;
		set_velocity(v);
		squash_v_ += 1.2f;
	}
}

void DragonController::knockback(const Vector3 &from, float force) {
	Vector3 away = get_global_position() - from;
	away.y = 0.0f;
	away = away.length() > 0.01f ? away.normalized() : Vector3(std::sin(facing_), 0, std::cos(facing_)) * -1.0f;
	pounding_ = false;
	roll_t_ = 0.0f;
	knock_t_ = 0.35f;
	set_velocity(away * force + Vector3(0, force * 0.75f, 0));
	squash_v_ += 2.0f;
}

void DragonController::set_mounted(bool m) {
	mounted_ = m;
	set_collision_layer(m ? 0 : 1);
	set_collision_mask(m ? 0 : (1 | 2));
	set_velocity(Vector3());
	pounding_ = false;
	roll_t_ = 0.0f;
}

void DragonController::set_blink(bool hidden) {
	if (visual_root_) {
		visual_root_->set_visible(!hidden);
	}
}

Vector3 DragonController::facing_dir() const {
	return Vector3(std::sin(facing_), 0, std::cos(facing_));
}

// Stairs and kerbs: walking into a step up to 0.45 m high lifts the kid onto it instead of
// stopping them (a capsule alone would need a jump for every porch step). The visual body eases
// up afterwards so the lift doesn't pop.
void DragonController::step_up(const Vector3 &wish) {
	constexpr float STEP = 0.45f;
	if (!is_on_wall() || wish.length() < 0.2f) {
		return;
	}
	Vector3 n = get_wall_normal();
	n.y = 0.0f;
	const Vector3 dir = Vector3(wish.x, 0, wish.z).normalized();
	if (n.length() < 0.5f || dir.dot(-n.normalized()) < 0.3f) {
		return;
	}
	const Transform3D gt = get_global_transform();
	const Vector3 up(0, STEP, 0);
	const Vector3 fwd = dir * 0.32f;
	if (test_move(gt, up) || test_move(gt.translated(up), fwd)) {
		return; // a ceiling above, or the "step" is a wall
	}
	Ref<KinematicCollision3D> hit;
	hit.instantiate();
	const Transform3D ahead = gt.translated(up + fwd);
	if (!test_move(ahead, Vector3(0, -STEP - 0.05f, 0), hit) || hit->get_normal().y < 0.7f) {
		return;
	}
	const float rise = STEP - hit->get_travel().length();
	if (rise < 0.03f || rise > STEP) {
		return;
	}
	set_global_position(gt.origin + Vector3(0, rise, 0) + fwd * 0.5f);
	step_smooth_ -= rise;
}

float DragonController::collect_radius() const {
	if (shockwave_t_ > 0.0f) {
		return 2.6f;
	}
	return spin_t_ > 0.0f ? 1.9f : 0.95f;
}

float DragonController::get_speed() const {
	const Vector3 v = get_velocity();
	return Vector2(v.x, v.z).length();
}

void DragonController::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const float dt = float(delta);
	Input *in = Input::get_singleton();
	Vector3 v = get_velocity();
	const bool on_floor = is_on_floor();

	Vector2 stick = input_enabled_ ? in->get_vector("move_left", "move_right", "move_forward", "move_back") : Vector2();
	Vector3 wish;
	if (Camera3D *cam = get_viewport()->get_camera_3d()) {
		const Basis cb = cam->get_global_transform().basis;
		Vector3 fwd = -cb.get_column(2);
		fwd.y = 0;
		fwd = fwd.normalized();
		Vector3 right = cb.get_column(0);
		right.y = 0;
		right = right.normalized();
		wish = right * stick.x - fwd * stick.y;
	} else {
		wish = Vector3(stick.x, 0, stick.y);
	}
	if (bot_) {
		wish = Vector3(bot_wish_.x, 0, bot_wish_.z);
	}
	if (wish.length() > 1.0f) {
		wish = wish.normalized();
	}
	const bool jump_pressed = (input_enabled_ && in->is_action_just_pressed("jump")) || (bot_ && bot_jump_);
	const bool jump_held = (input_enabled_ && in->is_action_pressed("jump")) || (bot_ && bot_hold_);
	const bool spin_pressed = (input_enabled_ && in->is_action_just_pressed("spin")) || (bot_ && bot_spin_);
	bot_jump_ = false;
	bot_spin_ = false;
	const bool roll_pressed = input_enabled_ && in->is_action_just_pressed("roll");
	fire_cd_ = Math::max(0.0f, fire_cd_ - dt);
	if (((input_enabled_ && in->is_action_just_pressed("attack")) || (bot_ && bot_fire_)) && fire_cd_ <= 0.0f) {
		fire_cd_ = fire_cooldown_;
		roar_t_ = 0.3f;
		squash_v_ -= 1.2f;
		emit_signal("fired");
	}
	bot_fire_ = false;
	if (mounted_) {
		set_velocity(Vector3());
		return;
	}

	coyote_ = on_floor ? COYOTE : Math::max(0.0f, coyote_ - dt);
	buffer_ = jump_pressed ? BUFFER : Math::max(0.0f, buffer_ - dt);
	roll_cd_ = Math::max(0.0f, roll_cd_ - dt);
	spin_t_ = Math::max(0.0f, spin_t_ - dt);
	shockwave_t_ = Math::max(0.0f, shockwave_t_ - dt);
	flap_anim_ = Math::max(0.0f, flap_anim_ - dt);

	// Horizontal movement (knockback briefly takes the wheel).
	Vector3 hv(v.x, 0, v.z);
	knock_t_ = Math::max(0.0f, knock_t_ - dt);
	if (knock_t_ > 0.0f) {
		hv = hv.move_toward(Vector3(), 6.0f * dt);
	} else if (roll_t_ > 0.0f) {
		roll_t_ -= dt;
		const Vector3 dir(std::sin(facing_), 0, std::cos(facing_));
		hv = dir * ROLL_SPEED;
	} else if (!pounding_) {
		const Vector3 target = wish * RUN_SPEED;
		const float accel = on_floor ? (wish.length() > 0.05f ? 56.0f : 42.0f) : 30.0f;
		hv = hv.move_toward(target, accel * dt);
	} else {
		hv = Vector3();
	}
	if (wish.length() > 0.1f && roll_t_ <= 0.0f) {
		facing_ = std::atan2(wish.x, wish.z);
	}

	// Vertical.
	if (on_floor) {
		flap_used_ = false;
		spin_air_used_ = false;
		glide_t_ = 0.0f;
		jump_cut_ = false;
	}
	if (buffer_ > 0.0f && coyote_ > 0.0f && !pounding_) {
		v.y = JUMP_V;
		buffer_ = 0.0f;
		coyote_ = 0.0f;
		jump_cut_ = false;
		squash_v_ -= 2.6f;
		puff(get_global_position() + Vector3(0, 0.05f, 0), 7, 0.2f, Color(0.78f, 0.8f, 0.95f), 1.6f);
		emit_signal("jumped");
	} else if (jump_pressed && !on_floor && coyote_ <= 0.0f && !flap_used_ && !pounding_) {
		v.y = FLAP_V;
		flap_used_ = true;
		flap_anim_ = 0.4f;
		squash_v_ -= 1.8f;
		puff(get_global_position() + Vector3(0, 0.7f, 0), 12, 0.35f, Color(0.75f, 0.5f, 1.0f), 2.4f);
		emit_signal("flapped");
	}
	if (!jump_held && v.y > 0.0f && !jump_cut_ && !flap_used_) {
		v.y *= JUMP_CUT;
		jump_cut_ = true;
	}
	if (pounding_) {
		if (pound_stall_ > 0.0f) {
			pound_stall_ -= dt;
			v.y = 0.0f;
		} else {
			v.y = -24.0f;
		}
	} else {
		// Floaty rise, a hang at the apex while jump is held, a heavier fall.
		v.y = Math::max(v.y - gravity_at(v.y, jump_held) * dt, -MAX_FALL);
		// Glide: after the wing flap, holding jump while falling.
		if (flap_used_ && jump_held && v.y < -GLIDE_FALL && glide_t_ < 1.4f) {
			v.y = -GLIDE_FALL;
			glide_t_ += dt;
		}
	}
	// Tail spin: on the ground a sweep, in the air a little hover once per jump.
	if (spin_pressed && spin_t_ <= 0.0f) {
		spin_t_ = 0.42f;
		if (!on_floor && !spin_air_used_) {
			v.y = Math::max(v.y, 2.2f);
			spin_air_used_ = true;
		}
		emit_signal("spun");
	}
	// Roll on the ground; ground pound in the air.
	if (roll_pressed) {
		if (on_floor && roll_cd_ <= 0.0f) {
			roll_t_ = 0.38f;
			roll_cd_ = 0.55f;
			if (wish.length() > 0.1f) {
				facing_ = std::atan2(wish.x, wish.z);
			}
		} else if (!on_floor && !pounding_) {
			pounding_ = true;
			pound_stall_ = 0.14f;
		}
	}
	v.x = hv.x;
	v.z = hv.z;
	last_vy_ = v.y;
	set_velocity(v);
	move_and_slide();
	if (!pounding_) {
		assist(wish);
		if (on_floor && v.y <= 0.5f) {
			step_up(wish);
		}
	}

	// Landing, ground pound impact, bounce pads.
	const bool now_floor = is_on_floor();
	if (now_floor && !was_on_floor_) {
		const float impact = -last_vy_;
		squash_v_ += Math::min(impact * 0.22f, 4.0f);
		if (impact > 4.0f) {
			puff(get_global_position() + Vector3(0, 0.05f, 0), int(Math::clamp(impact * 1.2f, 6.0f, 18.0f)), 0.3f, Color(0.78f, 0.8f, 0.95f), 1.8f);
		}
		emit_signal("landed", impact);
		if (pounding_) {
			pounding_ = false;
			shockwave_t_ = 0.12f;
			squash_v_ += 3.0f;
			puff(get_global_position() + Vector3(0, 0.1f, 0), 24, 0.6f, Color(1.0f, 0.75f, 0.45f), 4.5f);
			emit_signal("pounded", get_global_position());
		}
		for (int i = 0; i < get_slide_collision_count(); ++i) {
			Ref<KinematicCollision3D> c = get_slide_collision(i);
			if (c.is_null() || c->get_normal().y < 0.7f) {
				continue;
			}
			Object *col = c->get_collider();
			if (col && col->has_meta("bounce")) {
				Vector3 nv = get_velocity();
				// Holding jump on contact springs higher: a timing skill the pad rewards.
				nv.y = float(col->get_meta("bounce")) * (jump_held ? 1.2f : 1.0f);
				set_velocity(nv);
				flap_used_ = false;
				squash_v_ -= 3.0f;
				puff(get_global_position() + Vector3(0, 0.1f, 0), 14, 0.4f, Color(1.0f, 0.65f, 0.3f), 3.0f);
				emit_signal("bounced");
				break;
			}
		}
	}
	was_on_floor_ = now_floor;
	// Safety: never fall out of the world.
	if (get_global_position().y < -10.0f) {
		set_global_position(Vector3(get_global_position().x, 2.0f, get_global_position().z));
		set_velocity(Vector3());
	}
}

void DragonController::pose(Part &p, const Vector3 &euler, const Vector3 &offset) {
	if (!p.node) {
		return;
	}
	p.node->set_transform(Transform3D(p.rest.basis * Basis::from_euler(euler), p.rest.origin + offset));
}

void DragonController::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint() || !visual_root_) {
		return;
	}
	const float dt = float(delta);
	time_ += dt;
	const float speed = get_speed();
	const float sf = Math::clamp(speed / RUN_SPEED, 0.0f, 1.3f);
	const bool air = !is_on_floor() && !mounted_;
	phase_ += dt * (4.0f + speed * 1.7f);

	// Spring-damped squash/stretch.
	squash_v_ += (-squash_ * 120.0f - squash_v_ * 11.0f) * dt;
	squash_ += squash_v_ * dt;
	const float s = Math::clamp(squash_, -0.25f, 0.3f);
	visual_yaw_ = angle_lerp(visual_yaw_, facing_, 1.0f - std::exp(-dt * 14.0f));
	float spin_yaw = 0.0f;
	if (spin_t_ > 0.0f) {
		spin_yaw = (1.0f - spin_t_ / 0.42f) * Math_TAU;
	}
	float tumble = 0.0f;
	if (roll_t_ > 0.0f) {
		tumble = (1.0f - roll_t_ / 0.38f) * Math_TAU;
	}
	const float bob = air ? 0.0f : std::abs(std::sin(phase_)) * 0.07f * sf;
	Basis b = Basis(Vector3(0, 1, 0), visual_yaw_ + spin_yaw) * Basis(Vector3(1, 0, 0), tumble);
	const float roar = roar_t_ > 0.0f ? std::sin(roar_t_ * 30.0f) * 0.06f * roar_t_ : 0.0f;
	roar_t_ = Math::max(0.0f, roar_t_ - dt);
	b = b.scaled(Vector3(1.0f + s * 0.6f + roar, 1.0f - s + roar, 1.0f + s * 0.6f + roar));
	step_smooth_ = Math::lerp(step_smooth_, 0.0f, 1.0f - std::exp(-dt * 18.0f));
	visual_root_->set_transform(Transform3D(b, Vector3(0, bob + step_smooth_ + (roll_t_ > 0.0f ? 0.35f : 0.0f), 0)));

	// Limbs.
	const float swing = air ? 0.0f : std::sin(phase_) * 0.75f * Math::min(sf, 1.0f);
	const float air_tuck = mounted_ ? -1.3f : (air ? (pounding_ ? -0.9f : -0.35f) : 0.0f);
	pose(leg_l_, Vector3(swing + air_tuck, 0, 0));
	pose(leg_r_, Vector3(-swing + air_tuck, 0, 0));
	pose(arm_l_, Vector3(-swing * 0.7f + (air ? -0.6f : 0.0f), 0, air ? 0.5f : 0.0f));
	pose(arm_r_, Vector3(swing * 0.25f, 0, air ? -0.2f : 0.0f));
	pose(head_, Vector3(std::sin(phase_ * 2.0f) * 0.04f * sf + (roar_t_ > 0.0f ? -0.25f : 0.0f), std::sin(time_ * 0.7f) * 0.08f * (1.0f - sf), 0));
	pose(body_, Vector3(sf * 0.12f, 0, std::sin(phase_) * 0.04f * sf));
	pose(tail_, Vector3(std::sin(time_ * 2.0f) * 0.05f + (air ? 0.25f : 0.0f), std::sin(time_ * 3.1f + phase_ * 0.5f) * (0.25f + 0.2f * sf), 0));
	// Wings: idle flutter, big flaps on the double jump, spread while gliding.
	float wing = std::sin(time_ * 3.0f) * 0.12f;
	if (flap_anim_ > 0.0f) {
		wing = std::sin(flap_anim_ * 40.0f) * 0.9f;
	} else if (air && flap_used_ && glide_t_ > 0.0f) {
		wing = 0.75f + std::sin(time_ * 22.0f) * 0.12f;
	} else if (air) {
		wing = 0.3f + std::sin(time_ * 9.0f) * 0.2f;
	}
	pose(wing_l_, Vector3(0, -wing * 0.5f, wing));
	pose(wing_r_, Vector3(0, wing * 0.5f, -wing));

	// Footsteps: a blip and a little dust puff every half stride.
	const float step_phase = std::sin(phase_);
	if (!air && speed > 1.5f && roll_t_ <= 0.0f && ((step_phase_prev_ < 0.0f) != (step_phase < 0.0f))) {
		step_parity_ ^= 1;
		emit_signal("stepped", step_parity_);
		if (speed > 4.0f) {
			puff(get_global_position() + Vector3(0, 0.04f, 0), 3, 0.08f, Color(0.78f, 0.8f, 0.95f), 0.8f);
		}
	}
	step_phase_prev_ = step_phase;
	// Gliding leaves a faint purple sparkle trail.
	if (air && flap_used_ && glide_t_ > 0.0f && int(time_ * 12.0f) != int((time_ - dt) * 12.0f)) {
		puff(get_global_position() + Vector3(0, 0.6f, 0), 3, 0.2f, Color(0.78f, 0.55f, 1.0f), 0.6f);
	}
	// Bucket bounce when candy lands in it.
	if (bucket_ && bucket_punch_ > 0.0f) {
		bucket_punch_ = Math::max(0.0f, bucket_punch_ - dt * 4.0f);
		const float k = std::sin(bucket_punch_ * Math_PI * 2.0f) * 0.25f * bucket_punch_;
		bucket_->set_transform(Transform3D(bucket_rest_.basis.scaled(Vector3(1.0f + k, 1.0f - k * 0.8f, 1.0f + k)), bucket_rest_.origin));
	}
}

} // namespace tot
