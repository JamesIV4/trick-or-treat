#include "play/ride_on_car.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "mesh/mesh_builder.h"

using namespace godot;

namespace tot {

namespace {
constexpr float MAX_SPEED = 15.0f;
constexpr float GRAVITY = 26.0f;

float wrap_angle(float a) {
	return Math::fmod(a + Math_PI * 3.0f, Math_TAU) - Math_PI;
}
} // namespace

void RideOnCar::_bind_methods() {
}

void RideOnCar::build(const Ref<Material> &toon) {
	set_collision_layer(0);
	set_collision_mask(1 | 2);
	set_floor_max_angle(Math::deg_to_rad(50.0f));
	set_floor_snap_length(0.6f);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<BoxShape3D> box;
	box.instantiate();
	box->set_size(Vector3(1.4f, 0.8f, 2.3f));
	cs->set_shape(box);
	cs->set_position(Vector3(0, 0.4f, 0)); // bottom at y 0, where the wheels touch (0.6 sank the tyres 0.2 m)
	add_child(cs);
	visual_ = memnew(Node3D);
	add_child(visual_);

	body_ = memnew(MeshInstance3D);
	body_->set_material_override(toon);
	body_->set_instance_shader_parameter("rim_strength", 0.25f);
	visual_->add_child(body_);
	const Vector3 at[4] = { Vector3(-0.72f, 0.33f, 0.72f), Vector3(0.72f, 0.33f, 0.72f), Vector3(-0.72f, 0.33f, -0.72f), Vector3(0.72f, 0.33f, -0.72f) };
	for (int i = 0; i < 4; ++i) {
		wheels_[i] = memnew(MeshInstance3D);
		wheels_[i]->set_material_override(toon);
		wheels_[i]->set_position(at[i]);
		visual_->add_child(wheels_[i]);
	}
	set_scheme(0);
	lamp_ = memnew(SpotLight3D);
	lamp_->set_color(Color(1.0f, 0.9f, 0.7f));
	lamp_->set_param(Light3D::PARAM_RANGE, 16.0f);
	lamp_->set_param(Light3D::PARAM_ENERGY, 2.5f);
	lamp_->set_param(Light3D::PARAM_SPOT_ANGLE, 32.0f);
	lamp_->set_position(Vector3(0, 0.7f, 1.2f));
	lamp_->set_rotation(Vector3(-0.12f, Math_PI, 0));
	visual_->add_child(lamp_);
	set_physics_process(true);
	set_process(true);
}

namespace {

struct CarScheme {
	const char *name;
	int paint, trim, seat, flag, hub;
	int paint_mat;
	int cost;
};
const CarScheme SCHEMES[] = {
	{ "Pumpkin Cruiser", pal::PUMPKIN, pal::WITCH_PURPLE, pal::WITCH_PURPLE, pal::BANNER_PURPLE, pal::PUMPKIN, mat::VINYL, 0 },
	{ "Ghostly Glow", pal::GHOST_WHITE, pal::CANDY_SKY_LIGHT, pal::SHADOW_NAVY, pal::CANDY_SKY_LIGHT, pal::SILVER, mat::VINYL, 40 },
	{ "Candy Corn", pal::CANDY_CORN_YELLOW, pal::PUMPKIN, pal::CANDY_CORN_WHITE, pal::PUMPKIN, pal::CANDY_CORN_WHITE, mat::VINYL, 60 },
	{ "Witch's Brew", pal::WITCH_PURPLE, pal::WITCH_GREEN, pal::CAT_BLACK, pal::WITCH_GREEN, pal::WITCH_GREEN, mat::VINYL, 60 },
	{ "Bubblegum", pal::CANDY_PINK, pal::CANDY_MINT, pal::LOLLY_WHITE, pal::CANDY_MINT, pal::CANDY_MINT, mat::VINYL, 80 },
	{ "Midnight Bat", pal::CAT_BLACK, pal::CANDY_VIOLET, pal::BAT_WING, pal::CANDY_VIOLET, pal::CANDY_VIOLET, mat::VINYL, 80 },
	{ "Mint Chip", pal::CANDY_MINT, pal::CHOCO, pal::CHOCO_DARK, pal::CHOCO, pal::CANDY_MINT_LIGHT, mat::VINYL, 100 },
	{ "Slime Time", pal::SLIME_GREEN, pal::SLIME_DARK, pal::CAT_BLACK, pal::CANDY_LIME, pal::SLIME_LIGHT, mat::VINYL, 100 },
	{ "Golden Treat", pal::FOIL_GOLD, pal::CHOCO_DARK, pal::WRAPPER_RED, pal::WRAPPER_RED, pal::FOIL_GOLD, mat::METAL, 200 },
};
constexpr int SCHEME_COUNT = int(sizeof(SCHEMES) / sizeof(SCHEMES[0]));

const CarScheme &scheme_at(int s) {
	return SCHEMES[Math::clamp(s, 0, SCHEME_COUNT - 1)];
}

Ref<ArrayMesh> body_mesh(const CarScheme &cs) {
	MeshBuilder m;
	const int paint = mat_cell(cs.paint, cs.paint_mat), trim = mat_cell(cs.trim, mat::VINYL), chrome = mat_cell(pal::SILVER, mat::METAL);
	m.rounded_box(Vector3(0, 0.55f, -0.05f), Vector3(0.72f, 0.26f, 1.12f), 0.18f, paint);
	m.rounded_box(Vector3(0, 0.62f, 0.78f), Vector3(0.66f, 0.2f, 0.38f), 0.16f, paint);
	m.rounded_box(Vector3(0, 0.36f, 0), Vector3(0.75f, 0.06f, 1.16f), 0.04f, trim);
	m.rounded_box(Vector3(0, 0.78f, -0.3f), Vector3(0.46f, 0.1f, 0.36f), 0.06f, mat_cell(cs.seat, mat::FABRIC));
	m.rounded_box(Vector3(0, 1.05f, -0.62f), Vector3(0.46f, 0.3f, 0.08f), 0.06f, mat_cell(cs.seat, mat::FABRIC));
	// Windshield frame and glass.
	m.beam(Vector3(-0.55f, 0.8f, 0.42f), Vector3(-0.5f, 1.25f, 0.3f), 0.05f, 0.05f, mat_cell(pal::LAMP_BLACK, mat::METAL));
	m.beam(Vector3(0.55f, 0.8f, 0.42f), Vector3(0.5f, 1.25f, 0.3f), 0.05f, 0.05f, mat_cell(pal::LAMP_BLACK, mat::METAL));
	m.beam(Vector3(-0.5f, 1.25f, 0.3f), Vector3(0.5f, 1.25f, 0.3f), 0.05f, 0.05f, mat_cell(pal::LAMP_BLACK, mat::METAL));
	m.set_transform(Transform3D(Basis(Vector3(1, 0, 0), -0.25f), Vector3(0, 1.02f, 0.37f)));
	m.box(Vector3(), Vector3(0.5f, 0.22f, 0.012f), pal::WATER_BLUE);
	m.set_transform(Transform3D(Basis(Vector3(1, 0, 0), -1.0f), Vector3(0, 0.98f, 0.12f)));
	m.cylinder(Vector3(0, -0.02f, 0), 0.17f, 0.04f, 10, mat_cell(pal::LAMP_BLACK, mat::VINYL));
	m.set_transform(Transform3D());
	// Jack-o'-lantern grille and headlights.
	for (float sx : { -1.0f, 1.0f }) {
		m.blob(Vector3(sx * 0.42f, 0.66f, 1.15f), Vector3(0.13f, 0.11f, 0.06f), pal::GLOW_LAMP, 1);
		m.tri_out(Vector3(sx * 0.2f, 0.62f, 1.16f), Vector3(sx * 0.06f, 0.62f, 1.165f), Vector3(sx * 0.13f, 0.72f, 1.16f), pal::GLOW_JACK, Vector3(sx * 0.13f, 0.66f, 0.0f));
	}
	m.box(Vector3(0, 0.5f, 1.165f), Vector3(0.18f, 0.035f, 0.01f), pal::GLOW_JACK);
	m.rounded_box(Vector3(0, 0.38f, 1.2f), Vector3(0.7f, 0.07f, 0.06f), 0.03f, chrome);
	m.rounded_box(Vector3(0, 0.38f, -1.2f), Vector3(0.7f, 0.07f, 0.06f), 0.03f, chrome);
	for (float sx : { -1.0f, 1.0f }) {
		m.blob(Vector3(sx * 0.5f, 0.6f, -1.17f), Vector3(0.08f, 0.06f, 0.03f), pal::GLOW_LANTERN_RED, 0);
	}
	// Antenna flag with a little pumpkin on top.
	m.beam(Vector3(0.55f, 0.8f, -0.9f), Vector3(0.55f, 1.9f, -0.95f), 0.025f, 0.025f, chrome);
	m.tri_out(Vector3(0.56f, 1.88f, -0.95f), Vector3(0.56f, 1.62f, -0.95f), Vector3(0.56f, 1.75f, -1.35f), mat_cell(cs.flag, mat::FABRIC), Vector3(0, 1.75f, -1.1f));
	m.tri_out(Vector3(0.56f, 1.88f, -0.95f), Vector3(0.56f, 1.62f, -0.95f), Vector3(0.56f, 1.75f, -1.35f), mat_cell(cs.flag, mat::FABRIC), Vector3(1, 1.75f, -1.1f));
	m.blob(Vector3(0.55f, 1.98f, -0.95f), Vector3(0.09f, 0.08f, 0.09f), mat_cell(pal::PUMPKIN, mat::PUMPKIN), 0);
	return m.build();
}

Ref<ArrayMesh> wheel_mesh(const CarScheme &cs) {
	MeshBuilder w;
	w.set_transform(Transform3D(Basis(Vector3(0, 0, 1), Math_PI * 0.5f), Vector3(0.13f, 0, 0)));
	w.cylinder(Vector3(), 0.33f, 0.26f, 12, mat_cell(pal::CHARCOAL, mat::VINYL));
	w.cylinder(Vector3(0, -0.01f, 0), 0.17f, 0.28f, 8, mat_cell(pal::SILVER, mat::METAL));
	w.cylinder(Vector3(0, 0.12f, 0), 0.08f, 0.18f, 6, mat_cell(cs.hub, cs.hub == pal::FOIL_GOLD ? mat::METAL : mat::VINYL));
	return w.build();
}

const Vector3 WHEEL_AT[4] = { Vector3(-0.72f, 0.33f, 0.72f), Vector3(0.72f, 0.33f, 0.72f), Vector3(-0.72f, 0.33f, -0.72f), Vector3(0.72f, 0.33f, -0.72f) };

} // namespace

int RideOnCar::scheme_count() {
	return SCHEME_COUNT;
}

const char *RideOnCar::scheme_name(int scheme) {
	return scheme_at(scheme).name;
}

int RideOnCar::scheme_cost(int scheme) {
	return scheme_at(scheme).cost;
}

void RideOnCar::set_scheme(int scheme) {
	const CarScheme &cs = scheme_at(scheme);
	if (body_) {
		body_->set_mesh(body_mesh(cs));
	}
	const Ref<ArrayMesh> wheel = wheel_mesh(cs);
	for (MeshInstance3D *w : wheels_) {
		if (w) {
			w->set_mesh(wheel);
		}
	}
}

Node3D *RideOnCar::make_preview(int scheme, const Ref<Material> &toon) {
	const CarScheme &cs = scheme_at(scheme);
	Node3D *root = memnew(Node3D);
	MeshInstance3D *body = memnew(MeshInstance3D);
	body->set_mesh(body_mesh(cs));
	body->set_material_override(toon);
	body->set_instance_shader_parameter("rim_strength", 0.25f);
	root->add_child(body);
	const Ref<ArrayMesh> wheel = wheel_mesh(cs);
	for (const Vector3 &p : WHEEL_AT) {
		MeshInstance3D *w = memnew(MeshInstance3D);
		w->set_mesh(wheel);
		w->set_material_override(toon);
		w->set_position(p);
		root->add_child(w);
	}
	return root;
}

void RideOnCar::place(const Vector3 &pos, float yaw) {
	set_global_position(pos);
	set_velocity(Vector3());
	yaw_ = yaw;
	speed_ = 0.0f;
}

void RideOnCar::set_driving(bool on) {
	driving_ = on;
	lamp_->set_visible(on);
}

Vector3 RideOnCar::forward() const {
	return Vector3(std::sin(yaw_), 0, std::cos(yaw_));
}

Transform3D RideOnCar::seat() const {
	return get_global_transform() * Transform3D(Basis(Vector3(0, 1, 0), yaw_) * tilt_, Vector3()) * Transform3D(Basis(), Vector3(0, 0.62f, -0.3f));
}

void RideOnCar::_physics_process(double delta) {
	const float dt = float(delta);
	Input *in = Input::get_singleton();
	Vector3 v = get_velocity();
	Vector3 wish;
	if (driving_) {
		const Vector2 stick = in->get_vector("move_left", "move_right", "move_forward", "move_back");
		if (Camera3D *cam = get_viewport()->get_camera_3d()) {
			const Basis cb = cam->get_global_transform().basis;
			Vector3 fwd = -cb.get_column(2);
			fwd.y = 0;
			Vector3 right = cb.get_column(0);
			right.y = 0;
			wish = right.normalized() * stick.x - fwd.normalized() * stick.y;
		}
		if (auto_) {
			wish = auto_wish_;
		}
		if (wish.length() > 1.0f) {
			wish = wish.normalized();
		}
	}
	const bool floor = is_on_floor();
	// Point and go: steer toward the stick direction, faster turns at low speed.
	if (wish.length() > 0.2f) {
		const float want = std::atan2(wish.x, wish.z);
		const float diff = wrap_angle(want - yaw_);
		const float turn = Math::lerp(4.2f, 2.4f, Math::clamp(speed_ / MAX_SPEED, 0.0f, 1.0f));
		yaw_ += Math::clamp(diff, -turn * dt, turn * dt);
		const float align = Math::max(0.0f, std::cos(diff));
		speed_ = Math::move_toward(speed_, MAX_SPEED * wish.length() * (0.35f + 0.65f * align), (floor ? 14.0f : 4.0f) * dt);
	} else {
		speed_ = Math::move_toward(speed_, 0.0f, (floor ? 12.0f : 2.0f) * dt);
	}
	const Vector3 f = forward() * speed_;
	v.x = f.x;
	v.z = f.z;
	v.y -= GRAVITY * dt;
	if (driving_ && floor && in->is_action_just_pressed("jump")) {
		v.y = 8.0f;
		bounce_v_ -= 2.5f;
	}
	set_velocity(v);
	move_and_slide();
	// Bumping into something big scrubs speed.
	if (get_slide_collision_count() > 0 && get_real_velocity().length() < speed_ * 0.5f && speed_ > 3.0f) {
		speed_ *= 0.5f;
		bounce_v_ -= 2.0f;
	}
	const bool now = is_on_floor();
	if (now && !was_floor_) {
		bounce_v_ += 3.0f;
	}
	was_floor_ = now;
	// Lean with the ground.
	const Vector3 up = now ? get_floor_normal() : Vector3(0, 1, 0);
	const Basis yawb(Vector3(0, 1, 0), yaw_);
	const Vector3 local_up = yawb.inverse().xform(up);
	const Basis target = Basis(Vector3(1, 0, 0), std::atan2(local_up.z, local_up.y)) * Basis(Vector3(0, 0, 1), -std::atan2(local_up.x, local_up.y));
	tilt_ = tilt_.slerp(target.orthonormalized(), 1.0f - std::exp(-dt * 8.0f));
	if (get_global_position().y < -40.0f) {
		set_global_position(get_global_position() + Vector3(0, 60.0f, 0));
	}
}

void RideOnCar::_process(double delta) {
	const float dt = float(delta);
	bounce_v_ += (-bounce_ * 140.0f - bounce_v_ * 10.0f) * dt;
	bounce_ += bounce_v_ * dt;
	wheel_spin_ += speed_ * dt / 0.33f;
	const float b = Math::clamp(bounce_, -0.2f, 0.2f);
	visual_->set_transform(Transform3D(Basis(Vector3(0, 1, 0), yaw_) * tilt_ * Basis().scaled(Vector3(1.0f - b * 0.3f, 1.0f + b, 1.0f - b * 0.3f)), Vector3()));
	for (int i = 0; i < 4; ++i) {
		// Left wheels are the right-hand wheel turned around, hub outward.
		const bool left = i % 2 == 0;
		wheels_[i]->set_basis(Basis(Vector3(0, 1, 0), left ? Math_PI : 0.0f) * Basis(Vector3(1, 0, 0), left ? -wheel_spin_ : wheel_spin_));
	}
}

} // namespace tot
