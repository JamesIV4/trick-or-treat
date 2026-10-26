#include "play/combat.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "core/consts.h"
#include "play/vfx.h"

using namespace godot;

namespace tot {

namespace {
constexpr float SHOT_SPEED = 21.0f;
constexpr float SHOT_LIFE = 1.1f;
constexpr float TAIL_TIME = 0.3f; // the trail burning out after impact
constexpr int SHOT_POOL = 14;

Ref<StandardMaterial3D> glow_material(const Color &albedo, const Color &emission, float energy) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_albedo(albedo);
	m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	m->set_emission(emission);
	m->set_emission_energy_multiplier(energy);
	return m;
}

Ref<Gradient> fade_ramp(const Color &a, const Color &b) {
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(PackedFloat32Array({ 0.0f, 0.5f, 1.0f }));
	g->set_colors(PackedColorArray({ a, b, Color(b, 0.0f) }));
	return g;
}

CPUParticles3D *make_particles(const Ref<Mesh> &mesh) {
	CPUParticles3D *p = memnew(CPUParticles3D);
	p->set_mesh(mesh);
	p->set_use_local_coordinates(false);
	p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	return p;
}

Ref<QuadMesh> spark_quad(float size) {
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(size, size));
	q->set_material(mat);
	return q;
}
} // namespace

void Combat::_bind_methods() {
}

void Combat::_ready() {
	Ref<SphereMesh> ball;
	ball.instantiate();
	ball->set_radius(0.22f);
	ball->set_height(0.44f);
	ball->set_radial_segments(10);
	ball->set_rings(6);
	ball->set_material(glow_material(Color(1.0f, 0.55f, 0.15f), Color(1.0f, 0.38f, 0.06f), 2.6f));
	ball_mesh_ = ball;
	Ref<Mesh> flame = spark_quad(0.28f);
	const bool flipbook = Vfx::get() != nullptr;
	if (flipbook) {
		flame = Vfx::get()->flame_quad(0.55f);
	}
	ribbon_mat_ = [] {
		Ref<ShaderMaterial> m;
		m.instantiate();
		m->set_shader(ResourceLoader::get_singleton()->load("res://shaders/fire_trail.gdshader"));
		return m;
	}();
	for (int i = 0; i < SHOT_POOL; ++i) {
		Shot s;
		s.node = memnew(MeshInstance3D);
		s.node->set_mesh(ball_mesh_);
		s.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		s.node->set_visible(false);
		add_child(s.node);
		s.light = memnew(OmniLight3D);
		s.light->set_color(Color(1.0f, 0.55f, 0.2f));
		s.light->set_param(Light3D::PARAM_RANGE, 4.5f);
		s.light->set_param(Light3D::PARAM_ENERGY, 2.2f);
		s.light->set_visible(false);
		add_child(s.light);
		s.ribbon_mesh.instantiate();
		s.ribbon = memnew(MeshInstance3D);
		s.ribbon->set_mesh(s.ribbon_mesh);
		s.ribbon->set_material_override(ribbon_mat_);
		s.ribbon->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		s.ribbon->set_visible(false);
		add_child(s.ribbon);
		// Licking flames wrapped around the hot core (the ribbon is the trail itself).
		s.trail = make_particles(flame);
		s.trail->set_amount(64);
		s.trail->set_lifetime(0.26);
		s.trail->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		s.trail->set_emission_sphere_radius(0.14f); // a full, round fire around the core
		s.trail->set_use_local_coordinates(true); // flames lick around the core and travel with it
		s.trail->set_emitting(false);
		s.trail->set_direction(Vector3(0, 1, 0));
		s.trail->set_spread(180.0f);
		s.trail->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.2f);
		s.trail->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.8f);
		s.trail->set_gravity(Vector3(0, 1.5f, 0));
		s.trail->set_param_min(CPUParticles3D::PARAM_SCALE, 1.2f);
		s.trail->set_param_max(CPUParticles3D::PARAM_SCALE, 2.2f);
		s.trail->set_color_ramp(flipbook ? fade_ramp(Color(1, 1, 1), Color(1, 1, 1)) : fade_ramp(Color(1.0f, 0.85f, 0.4f), Color(1.0f, 0.3f, 0.1f)));
		if (flipbook) {
			s.trail->set_param_min(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
			s.trail->set_param_max(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
			s.trail->set_param_min(CPUParticles3D::PARAM_ANGLE, -30.0f);
			s.trail->set_param_max(CPUParticles3D::PARAM_ANGLE, 30.0f);
		}
		add_child(s.trail);
		shots_.push_back(s);
	}
	// Fire ring: a flat annulus that races outward and fades.
	{
		PackedVector3Array v;
		PackedVector3Array n;
		const int seg = 48;
		for (int i = 0; i < seg; ++i) {
			const float a0 = i * Math_TAU / seg, a1 = (i + 1) * Math_TAU / seg;
			const Vector3 i0(std::cos(a0) * 0.72f, 0, std::sin(a0) * 0.72f), o0(std::cos(a0), 0, std::sin(a0));
			const Vector3 i1(std::cos(a1) * 0.72f, 0, std::sin(a1) * 0.72f), o1(std::cos(a1), 0, std::sin(a1));
			for (const Vector3 &p : { i0, o1, o0, i0, i1, o1 }) {
				v.push_back(p);
				n.push_back(Vector3(0, 1, 0));
			}
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = v;
		arrays[Mesh::ARRAY_NORMAL] = n;
		Ref<ArrayMesh> am;
		am.instantiate();
		am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		ring_mat_ = glow_material(Color(1.0f, 0.55f, 0.15f, 0.9f), Color(1.0f, 0.42f, 0.1f), 3.5f);
		ring_mat_->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		ring_mat_->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		am->surface_set_material(0, ring_mat_);
		ring_mesh_ = am;
	}
	for (int i = 0; i < 3; ++i) {
		Ring r;
		r.node = memnew(MeshInstance3D);
		r.node->set_mesh(ring_mesh_);
		r.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		r.node->set_visible(false);
		add_child(r.node);
		r.light = memnew(OmniLight3D);
		r.light->set_color(Color(1.0f, 0.5f, 0.15f));
		r.light->set_visible(false);
		add_child(r.light);
		rings_.push_back(r);
	}
	// Monster projectiles: a spinning bone and a glowing goo glob.
	{
		Ref<SphereMesh> goo;
		goo.instantiate();
		goo->set_radius(0.2f);
		goo->set_height(0.36f);
		goo->set_material(glow_material(Color(0.5f, 1.0f, 0.4f), Color(0.35f, 0.9f, 0.3f), 2.5f));
		goo_mesh_ = goo;
		Ref<SphereMesh> bone;
		bone.instantiate();
		bone->set_radius(0.13f);
		bone->set_height(0.5f);
		bone->set_material(glow_material(Color(0.95f, 0.92f, 0.82f), Color(0.6f, 0.58f, 0.5f), 0.6f));
		bone_mesh_ = bone;
	}
	for (int i = 0; i < 10; ++i) {
		Thrown t;
		t.node = memnew(MeshInstance3D);
		t.node->set_visible(false);
		t.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(t.node);
		thrown_.push_back(t);
	}
	Ref<QuadMesh> spark = spark_quad(0.2f);
	for (int i = 0; i < 10; ++i) {
		CPUParticles3D *p = make_particles(spark);
		p->set_one_shot(true);
		p->set_emitting(false);
		p->set_explosiveness_ratio(0.95f);
		p->set_lifetime(0.6);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(180.0f);
		p->set_gravity(Vector3(0, -2.0f, 0));
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, 2.0f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, 4.0f);
		p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
		p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
		add_child(p);
		puffs_.push_back(p);
	}
	set_process(true);
}

void Combat::clear() {
	for (Shot &s : shots_) {
		s.active = false;
		s.node->set_visible(false);
		s.light->set_visible(false);
		s.trail->set_emitting(false);
	}
	for (Thrown &t : thrown_) {
		t.active = false;
		t.node->set_visible(false);
	}
}

Combat::Shot &Combat::free_shot() {
	Shot *oldest = &shots_[0];
	for (Shot &s : shots_) {
		if (!s.active) {
			return s;
		}
		if (s.life > oldest->life) {
			oldest = &s;
		}
	}
	return *oldest;
}

void Combat::puff(const Vector3 &pos, const Color &color, int amount, float speed) {
	if (Vfx *vfx = Vfx::get()) {
		// Sparkles for every puff; big ones (defeats, broken nests) get a cartoon smoke poof too.
		vfx->sparkles(pos, color, Math::max(4, amount / 2), speed, 0.9f, 0.5f);
		if (amount >= 20) {
			vfx->smoke(pos, color.lerp(Color(1, 1, 1), 0.55f), 0.6f + amount * 0.02f, 4 + amount / 8);
			vfx->flash(pos, color, 1.0f + amount * 0.03f);
		}
		return;
	}
	CPUParticles3D *p = puffs_[next_puff_];
	next_puff_ = (next_puff_ + 1) % int(puffs_.size());
	p->set_global_position(pos);
	p->set_amount(amount);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * 0.4f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed);
	p->set_color(color);
	p->restart();
	p->set_emitting(true);
}

void Combat::fireball(const Vector3 &origin, const Vector3 &dir, float damage, int count) {
	const Vector3 d = dir.normalized();
	for (int k = 0; k < count; ++k) {
		const float spread = count == 1 ? 0.0f : (float(k) - (count - 1) * 0.5f) * 0.28f;
		Shot &s = free_shot();
		s.active = true;
		s.pos = origin;
		s.vel = d.rotated(Vector3(0, 1, 0), spread) * SHOT_SPEED;
		s.life = 0.0f;
		s.damage = damage;
		s.origin = origin;
		s.hist.assign(1, origin);
		s.tail_t = 0.0f;
		s.node->set_global_position(origin);
		s.node->set_visible(true);
		s.light->set_visible(true);
		s.trail->set_global_position(origin);
		s.trail->restart(); // a fresh burst every shot, even when the pool reuses a burning one
		s.trail->set_emitting(true);
	}
}

void Combat::fire_ring(const Vector3 &center, float radius, float damage) {
	for (Ring &r : rings_) {
		if (r.t >= 1.0f) {
			r.t = 0.0f;
			r.center = center;
			r.radius = radius;
			r.node->set_visible(true);
			r.light->set_visible(true);
			r.light->set_global_position(center + Vector3(0, 0.8f, 0));
			break;
		}
	}
	if (Vfx *vfx = Vfx::get()) {
		for (int k = 0; k < 10; ++k) {
			const float a = k * Math_TAU / 10.0f;
			vfx->flames(center + Vector3(std::cos(a) * radius * 0.75f, 0.2f, std::sin(a) * radius * 0.75f), 5, 0.4f, 1.3f);
		}
		vfx->flash(center + Vector3(0, 0.4f, 0), Color(1.0f, 0.6f, 0.2f), radius * 1.2f);
		vfx->smoke(center, Color(1.0f, 0.8f, 0.6f), 1.4f, 8);
	} else {
		for (int k = 0; k < 8; ++k) {
			const float a = k * Math_TAU / 8.0f;
			puff(center + Vector3(std::cos(a) * radius * 0.6f, 0.3f, std::sin(a) * radius * 0.6f), Color(1.0f, 0.55f, 0.2f), 14, 2.5f);
		}
	}
	if (!gather_targets) {
		return;
	}
	std::vector<Damageable *> targets;
	gather_targets(targets);
	for (Damageable *t : targets) {
		if (!t->target_alive()) {
			continue;
		}
		const Vector3 p = t->target_point();
		const float dy = p.y - center.y;
		if (t->surface_distance(Vector3(center.x, p.y, center.z)) <= radius && dy > -1.5f && dy < 3.0f) {
			const bool destroyed = t->take_hit(damage, center);
			if (on_target_hit) {
				on_target_hit(p, destroyed);
			}
		}
	}
}

void Combat::throw_projectile(const Vector3 &origin, const Vector3 &velocity, ProjectileKind kind) {
	for (Thrown &t : thrown_) {
		if (!t.active) {
			t.active = true;
			t.pos = origin;
			t.vel = velocity;
			t.life = 0.0f;
			t.kind = kind;
			t.node->set_mesh(kind == BONE ? bone_mesh_ : goo_mesh_);
			t.node->set_global_position(origin);
			t.node->set_visible(true);
			return;
		}
	}
}

Damageable *Combat::aim_assist(const Vector3 &origin, const Vector3 &facing, float range, float cone_cos) {
	if (!gather_targets) {
		return nullptr;
	}
	std::vector<Damageable *> targets;
	gather_targets(targets);
	Damageable *best = nullptr;
	float best_score = 1e9f;
	const Vector3 f = Vector3(facing.x, 0, facing.z).normalized();
	for (Damageable *t : targets) {
		if (!t->target_alive()) {
			continue;
		}
		const Vector3 d = t->target_point() - origin;
		const float dist = d.length();
		if (dist > range || dist < 0.01f) {
			continue;
		}
		const float c = Vector3(d.x, 0, d.z).normalized().dot(f);
		if (c < cone_cos || std::abs(d.y) > 5.0f) {
			continue;
		}
		const float score = dist * (1.6f - c) * t->aim_weight();
		if (score < best_score) {
			best_score = score;
			best = t;
		}
	}
	return best;
}

// Fireball targeting. Heavy lock-on only within about four body lengths of the kid (any direction);
// out to about ten, only things already roughly ahead get a light nudge. Monsters first, then monster
// spawners and thorn nests, vines last; within a tier the nearest wins, things ahead counting closer.
// Nothing qualifies: the fireball flies straight.
Damageable *Combat::pick_target(const Vector3 &origin, const Vector3 &facing) {
	constexpr float HEAVY = 4.5f, LIGHT = 11.0f, LIGHT_CONE = 0.87f; // ~30 degrees
	if (!gather_targets) {
		return nullptr;
	}
	std::vector<Damageable *> targets;
	gather_targets(targets);
	const Vector3 f = Vector3(facing.x, 0, facing.z).normalized();
	Damageable *best[3] = { nullptr, nullptr, nullptr };
	float best_score[3] = { 1e9f, 1e9f, 1e9f };
	for (Damageable *t : targets) {
		if (!t->target_alive()) {
			continue;
		}
		const Vector3 d = t->target_point() - origin;
		if (std::abs(d.y) > 4.0f) {
			continue;
		}
		const float dist = Math::max(0.0f, t->surface_distance(origin));
		const float c = Vector3(d.x, 0, d.z).normalized().dot(f);
		if (dist > LIGHT || (dist > HEAVY && c < LIGHT_CONE)) {
			continue;
		}
		const int tier = t->is_monster() ? 0 : (t->is_nest() ? 1 : 2);
		const float score = dist * (1.25f - 0.25f * c);
		if (score < best_score[tier]) {
			best_score[tier] = score;
			best[tier] = t;
		}
	}
	for (Damageable *t : best) {
		if (t) {
			return t;
		}
	}
	return nullptr;
}
bool Combat::ray_blocked(const Vector3 &a, const Vector3 &b, Vector3 &hit) {
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (!space) {
		return false;
	}
	Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(a, b, 1);
	// Shots fly through yard fences (bodies tagged "shot_through"): skip them and look further.
	TypedArray<RID> skip;
	for (int tries = 0; tries < 4; ++tries) {
		Dictionary r = space->intersect_ray(q);
		if (r.is_empty()) {
			return false;
		}
		Object *col = r["collider"];
		if (col && col->has_meta("shot_through")) {
			skip.push_back(RID(r["rid"]));
			q->set_exclude(skip);
			continue;
		}
		hit = r["position"];
		return true;
	}
	return false;
}

// The flame trail: a camera-facing ribbon through the ball's recent positions, widest at the ball.
void Combat::update_ribbon(Shot &s) {
	s.ribbon_mesh->clear_surfaces();
	Camera3D *cam = get_viewport() ? get_viewport()->get_camera_3d() : nullptr;
	if (s.hist.size() < 2 || !cam) {
		s.ribbon->set_visible(false);
		return;
	}
	const Vector3 eye = cam->get_global_position();
	PackedVector3Array v;
	PackedVector2Array uv;
	const int n = int(s.hist.size());
	for (int i = 0; i < n; ++i) {
		const Vector3 p = s.hist[i];
		const Vector3 along = (s.hist[std::max(i - 1, 0)] - s.hist[std::min(i + 1, n - 1)]).normalized();
		Vector3 side = along.cross(eye - p).normalized();
		if (side.length() < 0.5f) {
			side = Vector3(0, 1, 0);
		}
		const float u = float(i) / float(n - 1);
		const float grow = 1.0f + 0.22f * Math::max(0.0f, s.damage - 1.0f); // and a fatter flame
		const float w = 0.55f * grow * (1.0f - 0.4f * u) * (0.75f + 0.25f * std::sin(u * 9.0f + s.life * 30.0f));
		v.push_back(p + side * w);
		v.push_back(p - side * w);
		uv.push_back(Vector2(u, 0.0f));
		uv.push_back(Vector2(u, 1.0f));
	}
	PackedInt32Array idx;
	for (int i = 0; i + 1 < n; ++i) {
		const int a = i * 2;
		for (int k : { a, a + 1, a + 2, a + 1, a + 3, a + 2 }) {
			idx.push_back(k);
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = v;
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	arrays[Mesh::ARRAY_INDEX] = idx;
	s.ribbon_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	s.ribbon->set_visible(true);
	if (false) {
		UtilityFunctions::print("RIBBON n=", n, " mat=", ribbon_mat_.is_valid(), " shader=", ribbon_mat_.is_valid() ? Ref<ShaderMaterial>(ribbon_mat_)->get_shader().is_valid() : false, " aabb=", s.ribbon_mesh->get_aabb(), " head=", s.hist[0]);
	}
}

void Combat::explode(Shot &s, bool hit) {
	s.active = false;
	s.node->set_visible(false);
	s.light->set_visible(false);
	s.trail->set_emitting(false);
	s.tail_t = TAIL_TIME;
	s.hist.insert(s.hist.begin(), s.pos); // the head stops at the impact point
	puff(s.pos, hit ? Color(1.0f, 0.7f, 0.25f) : Color(1.0f, 0.5f, 0.2f), hit ? 22 : 12, hit ? 4.0f : 2.2f);
	if (Vfx *vfx = Vfx::get()) {
		// A puff of fire where it lands, bigger on a hit.
		const float big = hit ? 1.0f : 0.7f;
		vfx->flames(s.pos, hit ? 14 : 9, 0.3f * big, 1.6f * big);
		vfx->flash(s.pos, Color(1.0f, 0.55f, 0.18f), 1.8f * big);
		vfx->smoke(s.pos + Vector3(0, 0.2f, 0), Color(1.0f, 0.78f, 0.6f), 0.6f * big, 3);
		// A point-blank shot never really flew: lay fire along the short path so it still reads.
		const float path = s.origin.distance_to(s.pos);
		if (s.life < 0.12f && path > 0.4f) {
			const int n = Math::min(3, int(path / 0.6f) + 1);
			for (int i = 1; i <= n; ++i) {
				vfx->flames(s.origin.lerp(s.pos, float(i) / float(n + 1)), 6, 0.12f, 1.2f);
			}
		}
	}
	if (!hit && on_fizzle) {
		on_fizzle(s.pos);
	}
}

void Combat::_process(double delta) {
	const float dt = float(delta);
	std::vector<Damageable *> targets;
	if (gather_targets) {
		gather_targets(targets);
	}
	for (Shot &s : shots_) {
		if (!s.active) {
			if (s.tail_t > 0.0f) {
				// After impact the tail catches up with the impact point while it fades.
				s.tail_t -= dt;
				for (int k = 0; k < 2 && s.hist.size() > 2; ++k) {
					s.hist.pop_back();
				}
				s.ribbon->set_instance_shader_parameter("fade", Math::max(0.0f, s.tail_t / TAIL_TIME));
				update_ribbon(s);
				if (s.tail_t <= 0.0f) {
					s.ribbon->set_visible(false);
				}
			}
			continue;
		}
		s.life += dt;
		const Vector3 next = s.pos + s.vel * dt;
		// Targets first (nests and vines also have world collision that would stop the ray).
		bool done = false;
		for (Damageable *t : targets) {
			if (!t->target_alive()) {
				continue;
			}
			// Sample the step (a shot moves ~0.35 m a frame) against the target's surface.
			bool touched = false;
			for (float u : { 0.5f, 1.0f }) {
				const Vector3 q = s.pos.lerp(next, u);
				if (t->surface_distance(q) < 0.3f) {
					s.pos = q;
					touched = true;
					break;
				}
			}
			if (touched) {
				const bool destroyed = t->take_hit(s.damage, s.pos - s.vel.normalized());
				if (on_target_hit) {
					on_target_hit(s.pos, destroyed);
				}
				explode(s, true);
				done = true;
				break;
			}
		}
		if (done) {
			continue;
		}
		Vector3 hit;
		if (ray_blocked(s.pos, next, hit)) {
			s.pos = hit;
			explode(s, false);
			continue;
		}
		s.pos = next;
		s.node->set_global_position(s.pos);
		s.light->set_global_position(s.pos);
		s.trail->set_global_position(s.pos);
		s.hist.insert(s.hist.begin(), s.pos);
		// Hotter (upgraded) fire leaves a longer trail.
		const size_t len = size_t(13 + 5 * std::max(0.0f, (s.damage - 1.0f) / 0.65f));
		while (s.hist.size() > len) {
			s.hist.pop_back();
		}
		s.ribbon->set_instance_shader_parameter("fade", 1.0f);
		update_ribbon(s);
		const float pulse = 1.0f + 0.15f * std::sin(s.life * 40.0f);
		s.node->set_scale(Vector3(pulse, pulse, pulse));
		if (s.life > SHOT_LIFE) {
			explode(s, false);
		}
	}
	for (Ring &r : rings_) {
		if (r.t >= 1.0f) {
			continue;
		}
		r.t = Math::min(1.0f, r.t + dt / 0.45f);
		const float e = 1.0f - (1.0f - r.t) * (1.0f - r.t);
		const float s = 0.4f + r.radius * e;
		r.node->set_global_transform(Transform3D(Basis().scaled(Vector3(s, 1.0f, s)), r.center + Vector3(0, 0.15f, 0)));
		r.node->set_transparency(r.t * r.t);
		r.light->set_param(Light3D::PARAM_ENERGY, 3.0f * (1.0f - r.t));
		r.light->set_param(Light3D::PARAM_RANGE, 2.0f + r.radius * 1.5f);
		if (r.t >= 1.0f) {
			r.node->set_visible(false);
			r.light->set_visible(false);
		}
	}
	const Vector3 pp = player_position ? player_position() + Vector3(0, 0.6f, 0) : Vector3(1e9f, 0, 0);
	for (Thrown &t : thrown_) {
		if (!t.active) {
			continue;
		}
		t.life += dt;
		t.vel.y -= (t.kind == BONE ? 14.0f : 10.0f) * dt;
		const Vector3 next = t.pos + t.vel * dt;
		Vector3 hit;
		bool end = false;
		if (next.distance_to(pp) < 0.75f) {
			if (on_player_hit) {
				on_player_hit(t.pos);
			}
			end = true;
		} else if (ray_blocked(t.pos, next, hit) || t.life > 3.0f) {
			end = true;
		}
		t.pos = next;
		if (end) {
			t.active = false;
			t.node->set_visible(false);
			puff(t.pos, t.kind == BONE ? Color(0.9f, 0.88f, 0.8f) : Color(0.5f, 1.0f, 0.4f), 10, 2.0f);
			continue;
		}
		t.node->set_global_transform(Transform3D(Basis(Vector3(1, 0, 0), t.life * 14.0f) * Basis(Vector3(0, 0, 1), t.life * 9.0f), t.pos));
	}
}

} // namespace tot
