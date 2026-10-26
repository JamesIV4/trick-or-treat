#include "play/festival_camera.h"

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

using namespace godot;

namespace tot {

namespace {
float wrap(float a) {
	return Math::fmod(a + Math_PI * 3.0f, Math_TAU) - Math_PI;
}
constexpr float DIAGONAL = 0.5f; // radians off the street axis: "looking diagonally down the street"
} // namespace

void FestivalCamera::_bind_methods() {
	ClassDB::bind_method(D_METHOD("snap"), &FestivalCamera::snap);
	ClassDB::bind_method(D_METHOD("shake", "amount"), &FestivalCamera::shake);
	ClassDB::bind_method(D_METHOD("set_overview", "on"), &FestivalCamera::set_overview);
	ClassDB::bind_method(D_METHOD("set_orbit_speed", "speed"), &FestivalCamera::set_orbit_speed);
	ClassDB::bind_method(D_METHOD("set_invert_orbit", "on"), &FestivalCamera::set_invert_orbit);
	ClassDB::bind_method(D_METHOD("set_auto_follow", "on"), &FestivalCamera::set_auto_follow);
	ClassDB::bind_method(D_METHOD("set_shake_scale", "scale"), &FestivalCamera::set_shake_scale);
	ClassDB::bind_method(D_METHOD("set_base_fov", "fov"), &FestivalCamera::set_base_fov);
}

void FestivalCamera::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	cam_ = memnew(Camera3D);
	cam_->set_fov(56.0f);
	cam_->set_near(0.1f);
	cam_->set_far(400.0f);
	add_child(cam_);
	cam_->make_current();
	set_process(true);
}

void FestivalCamera::face_direction(const Vector3 &dir) {
	guide_ = std::atan2(-dir.x, -dir.z) - DIAGONAL;
	heading_sign_ = 1.0f;
}

void FestivalCamera::snap() {
	if (!target_) {
		return;
	}
	focus_ = target_->get_global_position();
	user_ = 0.0f;
	yaw_ = (heading_sign_ > 0 ? guide_ : guide_ + Math_PI) + DIAGONAL;
	_process(0.0);
}

void FestivalCamera::_process(double delta) {
	if (!cam_ || !target_) {
		return;
	}
	const float dt = float(delta);
	time_ += dt;
	if (cinematic_) {
		cam_->set_global_transform(cine_xf_);
		cam_->set_fov(cine_fov_);
		RenderingServer *rs = RenderingServer::get_singleton();
		if (cine_cutout_) {
			rs->global_shader_parameter_set("cutout_from", cine_xf_.origin);
			rs->global_shader_parameter_set("cutout_to", target_->get_global_position() + Vector3(0, 0.7f, 0));
		}
		rs->global_shader_parameter_set("cutout_radius", cine_cutout_ ? 1.6f : 0.0f);
		return;
	}
	cam_->set_fov(base_fov_);
	Input *in = Input::get_singleton();
	const Vector3 tp = target_->get_global_position();

	// The player's orbit input turns the camera, and it stays where they leave it.
	float orbit = in->get_axis("cam_left", "cam_right");
	if (in->is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)) {
		orbit += Math::clamp(in->get_last_mouse_velocity().x * 0.0015f, -2.0f, 2.0f);
	}
	orbit *= orbit_speed_ * (invert_orbit_ ? -1.0f : 1.0f);
	if (std::abs(orbit) > 0.05f) {
		user_ = wrap(user_ + orbit * 2.0f * dt);
		orbit_quiet_ = 0.0f;
	}
	orbit_quiet_ += dt;
	if (attract_) {
		user_ = wrap(user_ + dt * 0.12f);
		orbit_quiet_ = 0.0f;
	}
	// While the kid keeps running one way, the camera swings round behind them (never while the
	// player is orbiting): barely at all for a small difference, faster the further the run points
	// away from the view, capped at a moderate turn rate so it never whips round.
	if (auto_follow_ && !overview_ && orbit_quiet_ > 1.2f && dt > 0.0f) {
		if (CharacterBody3D *body = Object::cast_to<CharacterBody3D>(follow_body_ ? follow_body_ : target_)) {
			const Vector3 v = body->get_velocity();
			const float speed = Vector2(v.x, v.z).length();
			if (speed > 2.0f) {
				const float behind = std::atan2(-v.x, -v.z);
				float off = wrap(behind - yaw_);
				// Running straight at the camera: keep turning the way it already was (no dithering).
				if (std::abs(off) > 2.9f && follow_sign_ != 0.0f) {
					off = follow_sign_ * std::abs(off);
				}
				follow_sign_ = off >= 0.0f ? 1.0f : -1.0f;
				const float away = std::abs(off) / Math_PI; // 0 lined up .. 1 running toward the camera
				const float gain = (0.3f + 1.1f * away) * Math::clamp((speed - 2.0f) / 5.0f, 0.0f, 1.0f);
				const float turn = Math::clamp(off * gain, -1.4f, 1.4f); // rad/s
				user_ = wrap(user_ + turn * dt);
			}
		}
	}
	const float base = heading_sign_ > 0 ? guide_ : guide_ + Math_PI;
	const float desired = base + DIAGONAL + user_;
	yaw_ += wrap(desired - yaw_) * (1.0f - std::exp(-dt * 10.0f));

	// Pull back when the player is high up (roofs, tree house).
	const float want_dist = overview_ ? overview_dist_ : 9.5f + Math::clamp((tp.y - 2.0f) * 0.45f, 0.0f, 3.5f);
	const float want_pitch = overview_ ? 0.95f : 0.52f;
	dist_ = Math::lerp(dist_, want_dist, 1.0f - std::exp(-dt * 2.0f));
	pitch_ = Math::lerp(pitch_, want_pitch, 1.0f - std::exp(-dt * 2.0f));
	// Lazy follow: horizontal quick, vertical slower so jumps don't bounce the frame.
	focus_.x = Math::lerp(focus_.x, tp.x, 1.0f - std::exp(-dt * 8.0f));
	focus_.z = Math::lerp(focus_.z, tp.z, 1.0f - std::exp(-dt * 8.0f));
	focus_.y = Math::lerp(focus_.y, tp.y, 1.0f - std::exp(-dt * 3.0f));
	if (dt == 0.0f) {
		focus_ = tp;
		dist_ = want_dist;
		pitch_ = want_pitch;
	}
	const Vector3 offset = Vector3(std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_)) * dist_;
	// Aim a little ahead of the player so they sit in the lower third with the street opening up.
	const Vector3 ahead = Vector3(-std::sin(yaw_), 0, -std::cos(yaw_)) * (overview_ ? 0.0f : 3.0f);
	const Vector3 pivot = focus_ + Vector3(0, 0.9f, 0);
	Vector3 pos = pivot + offset;
	Vector3 look_at = focus_ + Vector3(0, 1.4f, 0) + ahead;
	// Spring arm against big structures only (layer value 4): pull in fast, ease back out.
	float allowed = dist_;
	if (!overview_ && is_inside_tree()) {
		PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
		if (space) {
			Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(pivot, pivot + offset * 1.08f, 4);
			Dictionary hit = space->intersect_ray(q);
			if (!hit.is_empty()) {
				allowed = Math::max(3.0f, pivot.distance_to(Vector3(hit["position"])) - 0.6f);
			}
		}
	}
	arm_ = allowed < arm_ ? Math::lerp(arm_, allowed, 1.0f - std::exp(-dt * 18.0f)) : Math::lerp(arm_, allowed, 1.0f - std::exp(-dt * 2.0f));
	if (dt == 0.0f) {
		arm_ = allowed;
	}
	pos = pivot + offset * (arm_ / Math::max(dist_, 0.01f));
	if (shake_ > 0.0f) {
		pos += Vector3(std::sin(time_ * 53.0f), std::sin(time_ * 71.0f), std::sin(time_ * 37.0f)) * shake_ * shake_scale_ * 0.12f;
		shake_ = Math::max(0.0f, shake_ - dt * 3.0f);
	}
	cam_->set_global_position(pos);
	cam_->look_at(look_at, Vector3(0, 1, 0));

	RenderingServer *rs = RenderingServer::get_singleton();
	rs->global_shader_parameter_set("cutout_from", pos);
	rs->global_shader_parameter_set("cutout_to", tp + Vector3(0, 0.7f, 0));
	rs->global_shader_parameter_set("cutout_radius", overview_ ? 0.0f : 1.6f);
}

} // namespace tot
