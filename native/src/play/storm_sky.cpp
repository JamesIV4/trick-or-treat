#include "play/storm_sky.h"

#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "gen/neighborhood_generator.h"

using namespace godot;

namespace tot {

void StormSky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_lightning_enabled", "enabled"), &StormSky::set_lightning_enabled);
	ClassDB::bind_method(D_METHOD("get_lightning_enabled"), &StormSky::get_lightning_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lightning_enabled"), "set_lightning_enabled", "get_lightning_enabled");
	ADD_SIGNAL(MethodInfo("lightning", PropertyInfo(Variant::FLOAT, "strength")));
}

static Node3D *find_named(Node *n, const String &name) {
	if (n->get_name() == StringName(name)) {
		return Object::cast_to<Node3D>(n);
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		if (Node3D *r = find_named(n->get_child(i), name)) {
			return r;
		}
	}
	return nullptr;
}

void StormSky::setup(NeighborhoodGenerator *gen, const Vector3 &center, const Vector3 &moon_dir, uint64_t seed) {
	for (Cloud &c : clouds_) {
		c.node->queue_free();
	}
	for (Bat &b : bats_) {
		b.node->queue_free();
	}
	clouds_.clear();
	bats_.clear();
	gen_ = gen;
	center_ = center;
	target_ = godot::Vector3(center.x, 0.0f, center.z);
	base_y_ = center.y;
	moon_dir_ = moon_dir.normalized();
	rng_ = Rng(seed ^ 0x5eedc10dULL);
	const char *ids[] = { "cloud_a", "cloud_b", "cloud_c" };
	// A big friendly flock: spread evenly round the sky (jittered), closer and lower than distant
	// weather, so they fill the skyline in every direction.
	constexpr int COUNT = 16;
	for (int i = 0; i < COUNT; ++i) {
		Cloud c;
		c.node = gen->instantiate_asset(ids[rng_.irange(0, 2)]);
		c.angle = (float(i) + rng_.range(-0.35f, 0.35f)) * float(Math_TAU) / float(COUNT);
		c.radius = rng_.range(85.0f, 165.0f);
		c.height = rng_.range(24.0f, 40.0f) + (c.radius - 85.0f) * 0.08f;
		c.speed = rng_.range(0.004f, 0.008f);
		c.scale = rng_.range(3.4f, 5.4f);
		c.node->set_scale(Vector3(c.scale, c.scale * rng_.range(0.85f, 1.0f), c.scale));
		// No shadows from the clouds: far beyond the moon's shadow range they only threw blocky
		// shadow-map steps onto each other.
		{
			std::vector<Node *> stack = { c.node };
			while (!stack.empty()) {
				Node *n = stack.back();
				stack.pop_back();
				if (GeometryInstance3D *gi = Object::cast_to<GeometryInstance3D>(n)) {
					gi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				}
				for (int k = 0; k < n->get_child_count(); ++k) {
					stack.push_back(n->get_child(k));
				}
			}
		}
		set_instance_param_recursive(c.node, "rim_strength", 0.8f);
		set_instance_param_recursive(c.node, "rim_color", Color(0.86f, 0.8f, 1.0f));
		add_child(c.node);
		clouds_.push_back(c);
	}
	set_process(true);
}

void StormSky::spawn_bats() {
	if (!gen_) {
		return;
	}
	// A little V crossing in front of the moon.
	const Vector3 m = Vector3(moon_dir_.x, 0, moon_dir_.z).normalized();
	const Vector3 side = Vector3(0, 1, 0).cross(m);
	const Vector3 mid = center_ + moon_dir_ * 60.0f;
	const float dir = rng_.sign();
	const int n = rng_.irange(3, 5);
	for (int i = 0; i < n; ++i) {
		Bat b;
		b.node = gen_->instantiate_asset("bat");
		const float row = float((i + 1) / 2) * (i % 2 ? 1.0f : -1.0f);
		const Vector3 off = m * std::abs(row) * 1.6f + Vector3(0, row * 0.9f, 0);
		b.from = mid - side * dir * 45.0f + off;
		b.to = mid + side * dir * 45.0f + off + Vector3(0, rng_.range(-4.0f, 6.0f), 0);
		b.t = -std::abs(row) * 0.03f;
		b.dur = rng_.range(9.0f, 12.0f);
		b.phase = rng_.range(0.0f, Math_TAU);
		b.wing_l = find_named(b.node, "wing_l");
		b.wing_r = find_named(b.node, "wing_r");
		b.node->set_scale(Vector3(3.0f, 3.0f, 3.0f));
		add_child(b.node);
		bats_.push_back(b);
	}
}

void StormSky::_process(double delta) {
	const float dt = float(delta);
	// Follow the player across the ground only, lazily (snapping after a teleport).
	const Vector3 flat_c(center_.x, 0.0f, center_.z);
	if (flat_c.distance_to(target_) > 150.0f) {
		center_ = Vector3(target_.x, base_y_, target_.z);
	} else {
		const Vector3 f = flat_c.lerp(target_, 1.0f - std::exp(-dt * 0.08f));
		center_ = Vector3(f.x, base_y_, f.z);
	}
	for (Cloud &c : clouds_) {
		c.angle += c.speed * dt;
		c.node->set_position(center_ + Vector3(std::cos(c.angle) * c.radius, c.height, std::sin(c.angle) * c.radius));
		c.node->set_rotation(Vector3(0, -c.angle + float(Math_PI) * 0.5f, 0)); // broadside to the middle, not end-on
	}
	// Heat lightning: dim and slow, never more than one burst every 20 s.
	next_flash_ -= dt;
	if (lightning_ && next_flash_ <= 0.0f && flash_t_ < 0.0f) {
		flash_t_ = 0.0f;
		next_flash_ = rng_.range(20.0f, 38.0f);
		emit_signal("lightning", 1.0);
	}
	if (flash_t_ >= 0.0f) {
		flash_t_ += dt;
		const float t = flash_t_;
		float f = 0.0f;
		if (t < 0.12f) f = t / 0.12f;
		else if (t < 0.3f) f = 1.0f - (t - 0.12f) / 0.18f * 0.8f;
		else if (t < 0.45f) f = 0.2f + (t - 0.3f) / 0.15f * 0.5f;
		else if (t < 1.1f) f = 0.7f * (1.0f - (t - 0.45f) / 0.65f);
		else {
			f = 0.0f;
			flash_t_ = -1.0f;
		}
		for (Cloud &c : clouds_) {
			set_instance_param_recursive(c.node, "flash", f * 0.6f);
		}
		if (flash_light_) {
			flash_light_->set_param(Light3D::PARAM_ENERGY, f * 0.35f);
		}
		if (sky_mat_.is_valid()) {
			sky_mat_->set_shader_parameter("lightning", f * 0.7f);
		}
	}
	next_bats_ -= dt;
	if (next_bats_ <= 0.0f) {
		spawn_bats();
		next_bats_ = rng_.range(18.0f, 30.0f);
	}
	for (size_t i = 0; i < bats_.size();) {
		Bat &b = bats_[i];
		b.t += dt / b.dur;
		if (b.t >= 1.0f) {
			b.node->queue_free();
			bats_[i] = bats_.back();
			bats_.pop_back();
			continue;
		}
		const float t = Math::max(b.t, 0.0f);
		const Vector3 p = b.from.lerp(b.to, t) + Vector3(0, std::sin(t * 12.0f + b.phase) * 0.6f, 0);
		b.node->set_position(p);
		const Vector3 d = (b.to - b.from).normalized();
		b.node->set_rotation(Vector3(0, std::atan2(d.x, d.z), 0));
		const float flap = std::sin((t * b.dur) * 16.0f + b.phase) * 0.9f;
		if (b.wing_l) b.wing_l->set_rotation(Vector3(0, 0, flap));
		if (b.wing_r) b.wing_r->set_rotation(Vector3(0, 0, -flap));
		++i;
	}
}

} // namespace tot
