#include "play/loose_candy.h"

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "gen/types.h"
#include "play/vfx.h"

using namespace godot;

namespace tot {

void LooseCandy::_bind_methods() {
}

void LooseCandy::setup(const std::vector<Ref<Mesh>> &meshes, const Ref<Material> &mat) {
	meshes_ = meshes;
	mat_ = mat;
	set_process(true);
}

void LooseCandy::clear() {
	for (Piece &p : pieces_) {
		p.node->queue_free();
	}
	pieces_.clear();
}

void LooseCandy::spill(const Vector3 &from, int value, bool spilled) {
	if (value <= 0 || meshes_.empty()) {
		return;
	}
	// Break the value into at most a dozen pieces.
	std::vector<int> kinds;
	int left = value;
	while (left > 0 && kinds.size() < 12) {
		const int remaining_slots = 12 - int(kinds.size());
		if (left >= 25 && left > remaining_slots * 5) {
			kinds.push_back(CANDY_LOLLIPOP);
			left -= 25;
		} else if (left >= 5 && left > remaining_slots) {
			kinds.push_back(CANDY_CHOCOLATE);
			left -= 5;
		} else {
			kinds.push_back(rng_.irange(CANDY_PINK, CANDY_LEMON));
			left -= 1;
		}
	}
	const float a0 = rng_.range(0.0f, Math_TAU);
	for (size_t i = 0; i < kinds.size(); ++i) {
		Piece p;
		p.kind = kinds[i];
		p.value = candy_value(p.kind);
		p.ttl = spilled ? 8.0f : 14.0f;
		const float a = a0 + float(i) * Math_TAU / float(kinds.size()) + rng_.range(-0.25f, 0.25f);
		const float sp = rng_.range(2.5f, 4.5f);
		p.pos = from + Vector3(0, 0.6f, 0);
		p.vel = Vector3(std::cos(a) * sp, rng_.range(5.0f, 7.0f), std::sin(a) * sp);
		p.node = memnew(MeshInstance3D);
		p.node->set_mesh(meshes_[Math::clamp(p.kind, 0, int(meshes_.size()) - 1)]);
		p.node->set_material_override(mat_);
		p.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(p.node);
		p.node->set_global_position(p.pos);
		pieces_.push_back(p);
	}
}

int LooseCandy::collect(const Vector3 &player, float radius, float dt) {
	int value = 0;
	for (size_t i = 0; i < pieces_.size();) {
		Piece &p = pieces_[i];
		const Vector3 to = player - p.pos;
		const float dist = to.length();
		if (!p.homing && p.life > 0.35f && Vector2(to.x, to.z).length() < radius + 1.6f && to.y > -1.2f && to.y < 1.6f) {
			p.homing = true;
			p.node->set_visible(true);
			if (Vfx *vfx = Vfx::get()) {
				static const Color tint[] = { Color(1.0f, 0.45f, 0.75f), Color(0.4f, 1.0f, 0.8f), Color(0.7f, 0.5f, 1.0f), Color(1.0f, 0.9f, 0.35f),
					Color(1.0f, 0.55f, 0.35f), Color(1.0f, 0.4f, 0.45f), Color(1.0f, 0.82f, 0.3f) };
				p.trail = vfx->trail_begin(tint[Math::clamp(p.kind, 0, 6)]);
			}
		}
		if (p.homing) {
			const float sp = Math::min(6.0f + p.life * 2.0f, 24.0f);
			p.pos += to / Math::max(dist, 1e-4f) * Math::min(sp * dt, dist);
			p.node->set_global_position(p.pos);
			if (Vfx *vfx = Vfx::get()) {
				vfx->trail_move(p.trail, p.pos);
			}
			if (dist < 0.45f) {
				if (Vfx *vfx = Vfx::get()) {
					vfx->trail_end(p.trail);
				}
				value += p.value;
				if (on_pickup) {
					on_pickup(p.kind, p.pos);
				}
				p.node->queue_free();
				pieces_[i] = pieces_.back();
				pieces_.pop_back();
				continue;
			}
		}
		++i;
	}
	return value;
}

void LooseCandy::_process(double delta) {
	const float dt = float(delta);
	PhysicsDirectSpaceState3D *space = is_inside_tree() ? get_world_3d()->get_direct_space_state() : nullptr;
	for (size_t i = 0; i < pieces_.size();) {
		Piece &p = pieces_[i];
		p.life += dt;
		if (p.life > p.ttl && !p.homing) {
			p.node->queue_free();
			pieces_[i] = pieces_.back();
			pieces_.pop_back();
			continue;
		}
		if (!p.resting && !p.homing) {
			p.vel.y -= 18.0f * dt;
			const Vector3 next = p.pos + p.vel * dt;
			Dictionary hit;
			if (space) {
				Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(p.pos, next - Vector3(0, 0.3f, 0), 1);
				hit = space->intersect_ray(q);
			}
			if (!hit.is_empty() && p.vel.y < 0.0f) {
				const Vector3 n = hit["normal"];
				if (n.y > 0.6f) {
					p.pos = Vector3(hit["position"]) + Vector3(0, 0.3f, 0);
					// One little bounce, then settle.
					if (std::abs(p.vel.y) > 3.0f) {
						p.vel = Vector3(p.vel.x * 0.4f, -p.vel.y * 0.35f, p.vel.z * 0.4f);
					} else {
						p.resting = true;
					}
				} else {
					p.vel = p.vel.bounce(n) * 0.4f;
				}
			} else {
				p.pos = next;
			}
			if (p.life > 3.0f) {
				p.resting = true; // never fall forever
			}
			p.node->set_global_position(p.pos);
		}
		// Blink faster and faster through the last three seconds.
		const float left = p.ttl - p.life;
		if (left < 3.0f) {
			const float rate = Math::lerp(14.0f, 5.0f, left / 3.0f);
			p.node->set_visible(std::fmod(p.life * rate, 1.0f) < 0.6f);
		}
		++i;
	}
}

} // namespace tot
