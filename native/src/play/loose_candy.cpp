#include "play/loose_candy.h"

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "gen/types.h"
#include "gen/candy_kinds.h"
#include "play/drop_shadow.h"
#include "play/vfx.h"

using namespace godot;

namespace tot {

namespace {
Color tint(int kind) {
	return candy_glow(kind);
}

// The soft column of light under a piece: from about 1.5 m below it up to just under it.
constexpr float BEAM_HEIGHT = 1.3f, BEAM_DROP = 0.76f;
} // namespace

void LooseCandy::_bind_methods() {
}

void LooseCandy::remove(size_t i) {
	Piece &p = pieces_[i];
	p.node->queue_free();
	if (p.glow) {
		p.glow->queue_free();
	}
	pieces_[i] = pieces_.back();
	pieces_.pop_back();
}

void LooseCandy::setup(const std::vector<Ref<Mesh>> &meshes, const Ref<Material> &mat) {
	meshes_ = meshes;
	mat_ = mat;
	beam_mesh_.instantiate();
	beam_mesh_->set_size(Vector2(1.0f, 1.0f));
	Ref<ShaderMaterial> beam;
	beam.instantiate();
	beam->set_shader(ResourceLoader::get_singleton()->load("res://shaders/candy_beam.gdshader"));
	beam->set_render_priority(1);
	beam_mat_ = beam;
	set_process(true);
}

void LooseCandy::clear() {
	while (!pieces_.empty()) {
		remove(pieces_.size() - 1);
	}
	fountains_.clear();
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
			kinds.push_back(candy_kind_of_tier(2, rng_.next()));
			left -= 25;
		} else if (left >= 5 && left > remaining_slots) {
			kinds.push_back(candy_kind_of_tier(1, rng_.next()));
			left -= 5;
		} else {
			kinds.push_back(candy_kind_of_tier(0, rng_.next()));
			left -= 1;
		}
	}
	const float a0 = rng_.range(0.0f, Math_TAU);
	for (size_t i = 0; i < kinds.size(); ++i) {
		const float a = a0 + float(i) * Math_TAU / float(kinds.size()) + rng_.range(-0.25f, 0.25f);
		const float sp = rng_.range(2.5f, 4.5f);
		launch(from + Vector3(0, 0.6f, 0), kinds[i], Vector3(std::cos(a) * sp, rng_.range(5.0f, 7.0f), std::sin(a) * sp), spilled ? 8.0f : 14.0f);
	}
}

void LooseCandy::fountain(const Vector3 &from, int count) {
	Fountain f;
	f.from = from;
	f.left = count;
	fountains_.push_back(f);
}

void LooseCandy::hover(const Vector3 &pos, int kind, float ttl) {
	if (meshes_.empty()) {
		return;
	}
	launch(pos, kind, Vector3(), ttl);
	pieces_.back().resting = true;
}

void LooseCandy::launch(const Vector3 &from, int kind, const Vector3 &vel, float ttl) {
	Piece p;
	p.kind = kind;
	p.value = candy_value(p.kind);
	p.ttl = ttl;
	p.pos = from;
	p.vel = vel;
	p.node = memnew(MeshInstance3D);
	p.node->set_mesh(meshes_[Math::clamp(p.kind, 0, int(meshes_.size()) - 1)]);
	p.node->set_material_override(mat_);
	p.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	p.node->set_layer_mask(LAYER_NO_DECALS);
	p.node->set_instance_shader_parameter("outline_tint", tint(p.kind)); // never a black outline
	add_child(p.node);
	p.node->set_global_position(p.pos);
	// The same soft column of light as placed candy, standing under the piece.
	p.glow = memnew(MeshInstance3D);
	p.glow->set_mesh(beam_mesh_);
	p.glow->set_material_override(beam_mat_);
	p.glow->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	p.glow->set_instance_shader_parameter("beam_tint", tint(p.kind));
	const float bw = candy_tier(p.kind) >= 2 ? 1.0f : 0.75f;
	p.glow->set_scale(Vector3(bw, BEAM_HEIGHT, bw));
	add_child(p.glow);
	p.glow->set_global_position(p.pos - Vector3(0, BEAM_DROP, 0));
	pieces_.push_back(p);
}
int LooseCandy::collect(const Vector3 &player, float radius, float dt) {
	int value = 0;
	for (size_t i = 0; i < pieces_.size();) {
		Piece &p = pieces_[i];
		const Vector3 to = player - p.pos;
		const float dist = to.length();
		if (!p.homing && p.life > 0.35f && Vector2(to.x, to.z).length() < radius + reach_extra_ && to.y > -1.2f && to.y < 1.6f) {
			p.homing = true;
			p.node->set_visible(true);
			if (Vfx *vfx = Vfx::get()) {
				p.trail = vfx->trail_begin(tint(p.kind));
			}
		}
		if (p.homing) {
			const float sp = Math::min(6.0f + p.life * 2.0f, 24.0f);
			p.pos += to / Math::max(dist, 1e-4f) * Math::min(sp * dt, dist);
			p.node->set_global_position(p.pos);
			p.home_t += dt;
			if (p.glow) {
				// Rides along to the bucket, fading out over 150 ms.
				p.glow->set_global_position(p.pos - Vector3(0, BEAM_DROP, 0));
				const float fade = Math::clamp(1.0f - p.home_t / 0.15f, 0.0f, 1.0f);
				p.glow->set_instance_shader_parameter("beam_tint", tint(p.kind) * fade);
				p.glow->set_visible(fade > 0.0f);
			}
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
				remove(i);
				continue;
			}
		}
		++i;
	}
	return value;
}

void LooseCandy::_process(double delta) {
	const float dt = float(delta);
	// Fountains: a piece every few hundredths of a second, shot high with a little spread, so they
	// arc up and rain down around the spot (mostly everyday candy, now and then a chocolate).
	for (size_t i = 0; i < fountains_.size();) {
		Fountain &f = fountains_[i];
		f.next -= dt;
		while (f.next <= 0.0f && f.left > 0) {
			f.next += 0.055f;
			--f.left;
			const float a = rng_.range(0.0f, Math_TAU), sp = rng_.range(1.2f, 3.4f);
			const int kind = candy_kind_of_tier(rng_.chance(0.12f) ? 1 : 0, rng_.next());
			launch(f.from, kind, Vector3(std::cos(a) * sp, rng_.range(8.5f, 11.0f), std::sin(a) * sp), 14.0f);
			if (Vfx *vfx = Vfx::get()) {
				vfx->candy_sparkles(f.from, tint(kind), 3, 3.0f, 0.8f, 1.5f);
			}
		}
		if (f.left <= 0) {
			fountains_[i] = fountains_.back();
			fountains_.pop_back();
		} else {
			++i;
		}
	}
	PhysicsDirectSpaceState3D *space = is_inside_tree() ? get_world_3d()->get_direct_space_state() : nullptr;
	for (size_t i = 0; i < pieces_.size();) {
		Piece &p = pieces_[i];
		p.life += dt;
		if (p.life > p.ttl && !p.homing) {
			remove(i);
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
			if (p.glow) {
				p.glow->set_global_position(p.pos - Vector3(0, BEAM_DROP, 0));
			}
		}
		// Blink a little faster through the last three seconds (2 to 5 blinks a second). The phase is
		// advanced by dt at the current rate, so speeding up never makes it stutter.
		const float left = p.ttl - p.life;
		if (left < 3.0f && !p.homing) {
			const float rate = Math::lerp(5.0f, 2.0f, Math::clamp(left / 3.0f, 0.0f, 1.0f));
			p.blink = std::fmod(p.blink + dt * rate, 1.0f);
			const bool on = p.blink < 0.65f;
			p.node->set_visible(on);
			if (p.glow) {
				p.glow->set_visible(on);
			}
		}
		++i;
	}
}

} // namespace tot
