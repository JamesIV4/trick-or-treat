#include "play/vfx.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

using namespace godot;

namespace tot {

Vfx *Vfx::singleton_ = nullptr;

namespace {

Ref<Curve> curve(std::initializer_list<std::pair<float, float>> pts) {
	Ref<Curve> c;
	c.instantiate();
	for (const auto &p : pts) {
		c->add_point(Vector2(p.first, p.second));
	}
	return c;
}

Ref<Gradient> fade(const Color &a, const Color &b, float hold = 0.6f) {
	Ref<Gradient> g;
	g.instantiate();
	g->set_offsets(PackedFloat32Array({ 0.0f, hold, 1.0f }));
	g->set_colors(PackedColorArray({ a, b, Color(b, 0.0f) }));
	return g;
}

} // namespace

void Vfx::_bind_methods() {
}

Ref<Texture2D> Vfx::load_texture(const char *name) {
	// The imported texture when there is one: an exported game only carries the imported copy, not
	// the PNG, so reading the file directly gave every particle a plain white square there.
	const String res_path = String("res://assets/vfx/") + name;
	if (ResourceLoader::get_singleton()->exists(res_path)) {
		Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(res_path);
		if (tex.is_valid()) {
			return tex;
		}
	}
	// Not imported yet (fresh art in a running editor session): straight from the PNG.
	const String path = ProjectSettings::get_singleton()->globalize_path(res_path);
	Ref<Image> img = Image::load_from_file(path);
	if (img.is_null() || img->is_empty()) {
		UtilityFunctions::push_warning("Vfx: missing ", path, " (run art/tools/make_vfx.py)");
		img = Image::create(4, 4, false, Image::FORMAT_RGBA8);
		img->fill(Color(1, 1, 1, 1));
	}
	img->generate_mipmaps();
	return ImageTexture::create_from_image(img);
}

Ref<StandardMaterial3D> Vfx::sprite_material(const Ref<Texture2D> &tex, bool additive, int frames) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_blend_mode(additive ? BaseMaterial3D::BLEND_MODE_ADD : BaseMaterial3D::BLEND_MODE_MIX);
	m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	m->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
	m->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, additive);
	if (frames > 1) {
		m->set_particles_anim_h_frames(frames);
		m->set_particles_anim_v_frames(frames);
		m->set_particles_anim_loop(false);
	}
	return m;
}

Ref<Mesh> Vfx::quad(const Ref<Material> &mat, float size) {
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(size, size));
	q->set_material(mat);
	return q;
}

Ref<Mesh> Vfx::flame_quad(float size) {
	return quad(flame_mat_, size);
}

void Vfx::make_pool(Pool &pool, int count, const Ref<Mesh> &mesh) {
	for (int i = 0; i < count; ++i) {
		CPUParticles3D *p = memnew(CPUParticles3D);
		p->set_mesh(mesh);
		p->set_emitting(false);
		p->set_one_shot(true);
		p->set_use_local_coordinates(false);
		p->set_explosiveness_ratio(0.92f);
		p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(p);
		pool.items.push_back(p);
	}
}

void Vfx::fire(CPUParticles3D *p, const Vector3 &pos) {
	p->set_global_position(pos);
	p->restart();
	p->set_emitting(true);
}

void Vfx::load_meshes() {
	// Through res:// (not an OS path) so it also reads from an exported game's pack.
	const String text = FileAccess::get_file_as_string("res://assets/vfx/particle_meshes.json");
	Variant parsed = JSON::parse_string(text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary d = parsed;
	for (const char *name : { "star", "heart", "confetti", "corn" }) {
		if (!d.has(name)) {
			continue;
		}
		Dictionary m = d[name];
		Array v = m["v"], n = m["n"], c = m["c"];
		PackedVector3Array verts, norms;
		PackedColorArray cols;
		for (int i = 0; i + 2 < v.size(); i += 3) {
			verts.push_back(Vector3(float(v[i]), float(v[i + 1]), float(v[i + 2])));
			norms.push_back(Vector3(float(n[i]), float(n[i + 1]), float(n[i + 2])));
			cols.push_back(Color(float(c[i]), float(c[i + 1]), float(c[i + 2])));
		}
		// Blender winds counter-clockwise; Godot's front faces are clockwise.
		for (int i = 0; i + 2 < verts.size(); i += 3) {
			const Vector3 t = verts[i + 1];
			verts.set(i + 1, verts[i + 2]);
			verts.set(i + 2, t);
			const Vector3 tn = norms[i + 1];
			norms.set(i + 1, norms[i + 2]);
			norms.set(i + 2, tn);
			const Color tc = cols[i + 1];
			cols.set(i + 1, cols[i + 2]);
			cols.set(i + 2, tc);
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = verts;
		arrays[Mesh::ARRAY_NORMAL] = norms;
		arrays[Mesh::ARRAY_COLOR] = cols;
		Ref<ArrayMesh> am;
		am.instantiate();
		am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		am->surface_set_material(0, confetti_mat_);
		confetti_meshes_.push_back(am);
	}
}

void Vfx::_ready() {
	singleton_ = this;
	sparkle_tex_ = load_texture("sparkle.png");
	glow_tex_ = load_texture("glow.png");
	ring_tex_ = load_texture("ring.png");
	smoke_tex_ = load_texture("smoke_4x4.png");
	flame_tex_ = load_texture("flame_4x4.png");
	swirl_tex_ = load_texture("swirl_4x4.png");
	sparkle_mat_ = sprite_material(sparkle_tex_, true, 1);
	glow_mat_ = sprite_material(glow_tex_, true, 1);
	ring_mat_ = sprite_material(ring_tex_, true, 1);
	smoke_mat_ = sprite_material(smoke_tex_, false, 4);
	flame_mat_ = sprite_material(flame_tex_, true, 4);
	swirl_mat_ = sprite_material(swirl_tex_, true, 4);
	confetti_mat_.instantiate();
	confetti_mat_->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	confetti_mat_->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	confetti_mat_->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	load_meshes();

	// Sparkles: twinkle (grow, flash, shrink) as they fly out and float.
	make_pool(sparkle_pool_, 14, quad(sparkle_mat_, 0.32f));
	for (CPUParticles3D *p : sparkle_pool_.items) {
		p->set_lifetime(0.7);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(180.0f);
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, 3.0f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, 5.0f);
		p->set_param_min(CPUParticles3D::PARAM_ANGLE, 0.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANGLE, 45.0f);
		p->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -90.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 90.0f);
		p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 0.2f }, { 0.15f, 1.0f }, { 0.4f, 0.6f }, { 0.6f, 0.9f }, { 1.0f, 0.0f } }));
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		p->set_emission_sphere_radius(0.12f);
	}
	// Smoke poofs: the flipbook plays once over each puff's life.
	make_pool(smoke_pool_, 10, quad(smoke_mat_, 1.0f));
	for (CPUParticles3D *p : smoke_pool_.items) {
		p->set_lifetime(0.75);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(70.0f);
		p->set_gravity(Vector3(0, 0.8f, 0));
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, 2.0f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, 3.0f);
		p->set_param_min(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
		p->set_param_min(CPUParticles3D::PARAM_ANGLE, -180.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANGLE, 180.0f);
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	}
	// One-particle effects: glow pop, star ring, magic swirl.
	make_pool(flash_pool_, 8, quad(glow_mat_, 1.0f));
	for (CPUParticles3D *p : flash_pool_.items) {
		p->set_amount(1);
		p->set_lifetime(0.28);
		p->set_gravity(Vector3());
		p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 0.4f }, { 0.25f, 1.0f }, { 1.0f, 0.0f } }));
	}
	make_pool(ring_pool_, 6, quad(ring_mat_, 2.0f));
	for (CPUParticles3D *p : ring_pool_.items) {
		p->set_amount(1);
		p->set_gravity(Vector3());
		p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 0.15f }, { 0.6f, 0.9f }, { 1.0f, 1.0f } }));
	}
	make_pool(swirl_pool_, 6, quad(swirl_mat_, 1.4f));
	for (CPUParticles3D *p : swirl_pool_.items) {
		p->set_amount(1);
		p->set_lifetime(0.6);
		p->set_gravity(Vector3(0, 0.6f, 0));
		p->set_param_min(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
	}
	make_pool(flame_pool_, 24, quad(flame_mat_, 0.55f)); // wand puffs and fireball bursts use plenty
	for (CPUParticles3D *p : flame_pool_.items) {
		p->set_lifetime(0.5);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(25.0f);
		p->set_gravity(Vector3(0, 3.0f, 0));
		p->set_param_min(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
		p->set_param_max(CPUParticles3D::PARAM_ANIM_SPEED, 1.0f);
		p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 0.5f }, { 0.2f, 1.0f }, { 1.0f, 0.3f } }));
		p->set_color_ramp(fade(Color(1, 1, 1, 1), Color(1, 1, 1, 1), 0.7f));
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	}
	// Confetti: stars, hearts, curls and candy corn tumbling down.
	for (const Ref<Mesh> &m : confetti_meshes_) {
		Pool pool;
		make_pool(pool, 4, m);
		for (CPUParticles3D *p : pool.items) {
			p->set_direction(Vector3(0, 1, 0));
			p->set_particle_flag(CPUParticles3D::PARTICLE_FLAG_ROTATE_Y, true);
			p->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -400.0f);
			p->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 400.0f);
			p->set_param_min(CPUParticles3D::PARAM_ANGLE, 0.0f);
			p->set_param_max(CPUParticles3D::PARAM_ANGLE, 360.0f);
			p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
			p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
			Ref<Gradient> rainbow;
			rainbow.instantiate();
			rainbow->set_offsets(PackedFloat32Array({ 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f }));
			rainbow->set_colors(PackedColorArray({ Color(1, 0.45f, 0.75f), Color(1, 0.6f, 0.2f), Color(1, 0.92f, 0.35f), Color(0.45f, 1, 0.75f),
					Color(0.55f, 0.75f, 1), Color(0.8f, 0.5f, 1) }));
			p->set_color_initial_ramp(rainbow);
			p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 1.0f }, { 0.8f, 1.0f }, { 1.0f, 0.0f } }));
		}
		confetti_pools_.push_back(pool);
	}
	// Trails: continuous sparkle emitters that follow candy on its way to the bucket.
	Ref<Mesh> trail_quad = quad(sparkle_mat_, 0.2f);
	for (int i = 0; i < 12; ++i) {
		Trail t;
		t.p = memnew(CPUParticles3D);
		t.p->set_mesh(trail_quad);
		t.p->set_amount(24);
		t.p->set_lifetime(0.45);
		t.p->set_emitting(false);
		t.p->set_use_local_coordinates(false);
		t.p->set_direction(Vector3(0, 1, 0));
		t.p->set_spread(180.0f);
		t.p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.1f);
		t.p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.5f);
		t.p->set_gravity(Vector3(0, 0.4f, 0));
		t.p->set_param_min(CPUParticles3D::PARAM_ANGLE, 0.0f);
		t.p->set_param_max(CPUParticles3D::PARAM_ANGLE, 45.0f);
		t.p->set_param_curve(CPUParticles3D::PARAM_SCALE, curve({ { 0.0f, 1.0f }, { 1.0f, 0.0f } }));
		t.p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(t.p);
		trails_.push_back(t);
	}
}

void Vfx::_exit_tree() {
	if (singleton_ == this) {
		singleton_ = nullptr;
	}
}

// ---------------------------------------------------------------- effects
// Pushes a colour toward full saturation (keeps its hue and brightness).
Color Vfx::saturated(const Color &c) {
	return Color::from_hsv(c.get_h(), Math::min(1.0f, c.get_s() * 1.4f + 0.2f), Math::max(c.get_v(), 0.85f), c.a);
}

// Candy pickups: sparkles in the candy's own colour mixed with the other candy colours, all
// saturated (no near-white ones).
void Vfx::candy_sparkles(const Vector3 &pos, const Color &color, int count, float speed, float size, float rise) {
	CPUParticles3D *p = sparkle_pool_.take();
	p->set_amount(Math::max(1, count));
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * 0.4f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f * size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f * size);
	p->set_gravity(Vector3(0, rise, 0));
	const Color c = saturated(color);
	Ref<Gradient> mix;
	mix.instantiate();
	mix->set_offsets(PackedFloat32Array({ 0.0f, 0.4f, 0.55f, 0.7f, 0.85f, 1.0f }));
	mix->set_colors(PackedColorArray({ c, c, Color(1.0f, 0.3f, 0.7f), Color(1.0f, 0.85f, 0.15f), Color(0.25f, 1.0f, 0.7f), Color(0.65f, 0.35f, 1.0f) }));
	mix->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CONSTANT);
	p->set_color_initial_ramp(mix);
	p->set_color_ramp(fade(Color(1, 1, 1, 1), Color(1, 1, 1, 1), 0.7f));
	fire(p, pos);
}

void Vfx::sparkles(const Vector3 &pos, const Color &color, int count, float speed, float size, float rise) {
	CPUParticles3D *p = sparkle_pool_.take();
	p->set_amount(Math::max(1, count));
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * 0.4f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f * size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f * size);
	p->set_gravity(Vector3(0, rise, 0));
	// Half the sparkles in the candy's colour, half near-white, all fading out.
	Ref<Gradient> mix;
	mix.instantiate();
	mix->set_offsets(PackedFloat32Array({ 0.0f, 1.0f }));
	mix->set_colors(PackedColorArray({ color.lightened(0.15f), Color(1, 1, 1).lerp(color, 0.25f) }));
	p->set_color_initial_ramp(mix);
	p->set_color_ramp(fade(Color(1, 1, 1, 1), Color(1, 1, 1, 1), 0.7f));
	fire(p, pos);
}

void Vfx::smoke(const Vector3 &pos, const Color &tint, float size, int count) {
	CPUParticles3D *p = smoke_pool_.take();
	p->set_amount(Math::max(1, count));
	p->set_emission_sphere_radius(0.15f * size);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.6f * size);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.6f * size);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f * size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.1f * size);
	p->set_color(tint);
	fire(p, pos);
}

void Vfx::flash(const Vector3 &pos, const Color &color, float size) {
	CPUParticles3D *p = flash_pool_.take();
	p->set_param_min(CPUParticles3D::PARAM_SCALE, size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, size);
	p->set_color(color);
	fire(p, pos);
}

void Vfx::ring(const Vector3 &pos, const Color &color, float radius, float life) {
	CPUParticles3D *p = ring_pool_.take();
	p->set_lifetime(life);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, radius);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, radius);
	p->set_color(color);
	p->set_color_ramp(fade(Color(1, 1, 1, 1), Color(1, 1, 1, 1), 0.5f));
	fire(p, pos);
}

void Vfx::swirl(const Vector3 &pos, const Color &color, float size) {
	CPUParticles3D *p = swirl_pool_.take();
	p->set_param_min(CPUParticles3D::PARAM_SCALE, size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, size);
	p->set_param_min(CPUParticles3D::PARAM_ANGLE, -180.0f);
	p->set_param_max(CPUParticles3D::PARAM_ANGLE, 180.0f);
	p->set_color(color.lightened(0.25f));
	fire(p, pos);
}

void Vfx::flames(const Vector3 &pos, int count, float spread, float size) {
	CPUParticles3D *p = flame_pool_.take();
	p->set_amount(Math::max(1, count));
	p->set_emission_sphere_radius(spread);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.5f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.5f);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f * size);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.2f * size);
	fire(p, pos);
}

void Vfx::confetti(const Vector3 &pos, bool firework) {
	for (size_t k = 0; k < confetti_pools_.size(); ++k) {
		CPUParticles3D *p = confetti_pools_[k].take();
		p->set_amount(firework ? 26 : 12);
		p->set_lifetime(firework ? 2.0 : 1.4);
		p->set_spread(firework ? 180.0f : 50.0f);
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, firework ? 5.0f : 3.0f);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, firework ? 8.0f : 5.5f);
		p->set_param_min(CPUParticles3D::PARAM_DAMPING, firework ? 2.5f : 1.2f);
		p->set_param_max(CPUParticles3D::PARAM_DAMPING, firework ? 3.5f : 2.0f);
		p->set_gravity(Vector3(0, firework ? -2.5f : -6.0f, 0));
		fire(p, pos);
	}
	sparkles(pos, Color(1.0f, 0.9f, 0.6f), firework ? 30 : 14, firework ? 7.0f : 4.0f, firework ? 1.6f : 1.1f, firework ? -1.0f : 0.5f);
	if (firework) {
		flash(pos, Color(1.0f, 0.8f, 0.5f), 4.0f);
		ring(pos, Color(1.0f, 0.75f, 0.95f), 3.0f, 0.7f);
	}
}

// The pickup: a soft glow pops at the bucket, sparkles twinkle out in the candy's colour and
// float up; special candy adds a magic swirl, a star ring and, for the best, a confetti fountain.
void Vfx::candy_pickup(const Vector3 &pos, const Color &color, int tier) {
	// A small coloured pop and a shower of candy-coloured sparkles (no white rings or swirls).
	flash(pos, saturated(color), 0.4f + 0.25f * float(tier));
	candy_sparkles(pos, color, 8 + tier * 6, 2.2f + tier * 0.8f, 0.85f + tier * 0.15f, 1.2f);
	if (tier >= 2) {
		for (size_t k = 0; k < confetti_pools_.size(); k += 2) {
			CPUParticles3D *p = confetti_pools_[k].take();
			p->set_amount(tier >= 3 ? 16 : 8);
			p->set_lifetime(1.2);
			p->set_spread(35.0f);
			p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 3.5f);
			p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 5.5f);
			p->set_param_min(CPUParticles3D::PARAM_DAMPING, 1.0f);
			p->set_param_max(CPUParticles3D::PARAM_DAMPING, 1.5f);
			p->set_gravity(Vector3(0, -7.0f, 0));
			fire(p, pos);
		}
	}
	if (tier >= 3) {
		candy_sparkles(pos + Vector3(0, 0.6f, 0), Color(1.0f, 0.8f, 0.2f), 24, 4.0f, 1.4f, 2.5f);
	}
}

int Vfx::trail_begin(const Color &color) {
	for (int i = 0; i < int(trails_.size()); ++i) {
		if (!trails_[i].used) {
			trails_[i].used = true;
			trails_[i].p->set_color(saturated(color));
			trails_[i].p->set_emitting(true);
			return i;
		}
	}
	return -1;
}

void Vfx::trail_move(int handle, const Vector3 &pos) {
	if (handle >= 0 && handle < int(trails_.size())) {
		trails_[handle].p->set_global_position(pos);
	}
}

void Vfx::trail_end(int handle) {
	if (handle >= 0 && handle < int(trails_.size())) {
		trails_[handle].used = false;
		trails_[handle].p->set_emitting(false);
	}
}

} // namespace tot
