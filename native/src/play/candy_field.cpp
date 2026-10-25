#include "play/candy_field.h"

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

#include "play/drop_shadow.h"
#include "play/vfx.h"

using namespace godot;

namespace tot {

namespace {
constexpr float CELL = 2.0f;
constexpr float SEQUENCE_GAP = 1.6f; // longest pause between pickups that still counts as one run
constexpr int SEQUENCE_MIN = 3;

Color halo_color(int kind) {
	switch (kind) {
		case CANDY_PINK:
			return Color(1.0f, 0.45f, 0.75f);
		case CANDY_MINT:
			return Color(0.4f, 1.0f, 0.8f);
		case CANDY_GRAPE:
			return Color(0.7f, 0.5f, 1.0f);
		case CANDY_LEMON:
			return Color(1.0f, 0.9f, 0.35f);
		case CANDY_CHOCOLATE:
			return Color(1.0f, 0.55f, 0.35f);
		case CANDY_LOLLIPOP:
			return Color(1.0f, 0.4f, 0.45f);
		default:
			return Color(1.0f, 0.82f, 0.3f);
	}
}

float candy_scale(int kind) {
	return kind == CANDY_GOLDEN ? 1.25f : (kind == CANDY_LOLLIPOP ? 1.1f : 1.0f);
}
} // namespace

void CandyField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("remaining_in_lot", "lot"), &CandyField::remaining_in_lot);
	ClassDB::bind_method(D_METHOD("total"), &CandyField::total);
	ClassDB::bind_method(D_METHOD("collected"), &CandyField::collected);
	ClassDB::bind_method(D_METHOD("total_value"), &CandyField::total_value);
	ClassDB::bind_method(D_METHOD("collected_value"), &CandyField::collected_value);
	ClassDB::bind_method(D_METHOD("nearest_remaining", "from"), &CandyField::nearest_remaining);
	ADD_SIGNAL(MethodInfo("candy_collected", PropertyInfo(Variant::INT, "kind"), PropertyInfo(Variant::INT, "value"),
			PropertyInfo(Variant::INT, "lot"), PropertyInfo(Variant::VECTOR3, "position"), PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("house_cleared", PropertyInfo(Variant::INT, "lot")));
	ADD_SIGNAL(MethodInfo("sequence_completed", PropertyInfo(Variant::INT, "size"), PropertyInfo(Variant::VECTOR3, "position")));
}

void CandyField::setup(const std::vector<Ref<Mesh>> &meshes, const Ref<Material> &candy_mat, const Ref<Material> &halo_mat, int lot_count) {
	meshes_ = meshes;
	candy_mat_ = candy_mat;
	halo_mat_ = halo_mat;
	per_lot_.assign(std::max(lot_count, 0), 0);
	slots_.assign(CANDY_KIND_COUNT, {});
	candy_.clear();
	grid_.clear();
	groups_.clear();
	shadow_tex_ = make_glow_texture();
}

void CandyField::add(const CandySpec &c) {
	Candy k;
	k.pos = c.pos;
	k.kind = c.kind;
	k.lot = c.lot;
	k.id = c.id;
	k.group = c.group;
	if (c.group >= 0) {
		if (c.group >= int(groups_.size())) {
			groups_.resize(c.group + 1);
		}
		groups_[c.group].size++;
	}
	k.slot = int(slots_[c.kind].size());
	slots_[c.kind].push_back(int(candy_.size()));
	candy_.push_back(k);
	if (c.lot >= 0 && c.lot < int(per_lot_.size())) {
		per_lot_[c.lot]++;
	}
	total_++;
	total_value_ += candy_value(c.kind);
}

void CandyField::finalize() {
	for (int kind = 0; kind < CANDY_KIND_COUNT; ++kind) {
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_custom_data(true);
		mm->set_mesh(meshes_[kind]);
		const int extra = kind <= CANDY_LEMON ? capacity_extra_ / 4 : 0;
		mm->set_instance_count(int(slots_[kind].size()) + extra);
		for (int i = 0; i < mm->get_instance_count(); ++i) {
			mm->set_instance_transform(i, Transform3D(Basis().scaled(Vector3(0, 0, 0)), Vector3()));
		}
		mmi->set_multimesh(mm);
		mmi->set_material_override(candy_mat_);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		mmi->set_layer_mask(LAYER_NO_DECALS);
		mmi->set_name(String("Candy") + String::num_int64(kind));
		add_child(mmi);
		mm_.push_back(mmi);
	}
	if (halo_mat_.is_valid()) {
	halo_ = memnew(MultiMeshInstance3D);
	Ref<MultiMesh> hm;
	hm.instantiate();
	hm->set_transform_format(MultiMesh::TRANSFORM_3D);
	hm->set_use_colors(true);
	hm->set_use_custom_data(true);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(1.0f, 1.0f));
	hm->set_mesh(quad);
	hm->set_instance_count(int(candy_.size()) + capacity_extra_);
	for (int i = 0; i < hm->get_instance_count(); ++i) {
		hm->set_instance_transform(i, Transform3D(Basis().scaled(Vector3(0, 0, 0)), Vector3()));
	}
	halo_->set_multimesh(hm);
	halo_->set_material_override(halo_mat_);
	halo_->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	halo_->set_name("Halos");
	add_child(halo_);
	}
	for (int i = 0; i < int(candy_.size()); ++i) {
		write_instance(candy_[i], 1.0f);
		index_candy(i);
		add_shadow(candy_[i]);
	}
	// Pickup sparkles.
	Ref<StandardMaterial3D> spark_mat;
	spark_mat.instantiate();
	spark_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	spark_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	spark_mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	spark_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	spark_mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	Ref<QuadMesh> sq;
	sq.instantiate();
	sq->set_size(Vector2(0.14f, 0.14f));
	sq->set_material(spark_mat);
	for (int i = 0; i < 10; ++i) {
		CPUParticles3D *p = memnew(CPUParticles3D);
		p->set_emitting(false);
		p->set_one_shot(true);
		p->set_amount(14);
		p->set_lifetime(0.55);
		p->set_explosiveness_ratio(0.95f);
		p->set_mesh(sq);
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		p->set_emission_sphere_radius(0.15f);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(180.0f);
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.6f);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 3.2f);
		p->set_gravity(Vector3(0, -3.0f, 0));
		p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
		p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.2f);
		add_child(p);
		sparks_.push_back(p);
	}
	finalized_ = true;
	set_process(true);
}

void CandyField::write_instance(const Candy &c, float scale) {
	const float s = scale * candy_scale(c.kind);
	const float phase = float(hash64(uint64_t(c.slot) * 977 + uint64_t(c.kind)) % 1000) / 1000.0f * Math_TAU;
	Ref<MultiMesh> mm = mm_[c.kind]->get_multimesh();
	if (c.slot < mm->get_instance_count()) {
		mm->set_instance_transform(c.slot, Transform3D(Basis().scaled(Vector3(s, s, s)), c.pos));
		mm->set_instance_custom_data(c.slot, Color(phase, float(c.kind), 0, 0));
	}
	if (!halo_) {
		return;
	}
	Ref<MultiMesh> hm = halo_->get_multimesh();
	const int id = int(&c - candy_.data());
	if (id >= 0 && id < hm->get_instance_count()) {
		const float hs = s * (c.kind >= CANDY_CHOCOLATE ? 1.5f : 1.05f);
		hm->set_instance_transform(id, Transform3D(Basis().scaled(Vector3(hs, hs, hs)), c.pos));
		hm->set_instance_color(id, halo_color(c.kind));
		hm->set_instance_custom_data(id, Color(phase, 0, 0, 0));
	}
}

void CandyField::add_shadow(Candy &c) {
	c.shadow = make_drop_shadow(shadow_tex_, c.kind >= CANDY_LOLLIPOP ? 0.8f : 0.6f, 8.0f, Color(halo_color(c.kind), 0.35f), 0.9f);
	add_child(c.shadow);
	c.shadow->set_position(c.pos - Vector3(0, 4.0f - 0.2f, 0));
}

// Pickup bookkeeping: the shadow goes, and the rhythm group checks whether this piece completes
// a clean run (every piece, no long pause in between).
void CandyField::on_picked(Candy &c) {
	if (c.shadow) {
		c.shadow->queue_free();
		c.shadow = nullptr;
	}
	if (c.group < 0 || c.group >= int(groups_.size())) {
		return;
	}
	Group &g = groups_[c.group];
	if (g.got > 0 && time_ - g.last > SEQUENCE_GAP) {
		g.broken = true;
	}
	g.got++;
	g.last = time_;
	if (g.got == g.size && !g.broken && g.size >= SEQUENCE_MIN) {
		emit_signal("sequence_completed", g.size, c.pos);
	}
}

void CandyField::index_candy(int id) {
	const Candy &c = candy_[id];
	grid_[cell_key(int(std::floor(c.pos.x / CELL)), int(std::floor(c.pos.z / CELL)))].push_back(id);
}

void CandyField::spark(const Vector3 &p, int kind) {
	if (Vfx *vfx = Vfx::get()) {
		const int tier = kind == CANDY_GOLDEN ? 3 : (kind == CANDY_LOLLIPOP ? 2 : (kind == CANDY_CHOCOLATE ? 1 : 0));
		vfx->candy_pickup(to_global(p), halo_color(kind), tier);
		return;
	}
	if (sparks_.empty()) {
		return;
	}
	CPUParticles3D *s = sparks_[next_spark_];
	next_spark_ = (next_spark_ + 1) % int(sparks_.size());
	s->set_global_position(p);
	s->set_color(halo_color(kind) * 1.6f);
	s->restart();
	s->set_emitting(true);
}

int CandyField::collect(const Vector3 &player, float radius, float height, float dt) {
	const float magnet = radius + 1.7f;
	if (!finalized_) {
		return 0;
	}
	int value = 0;
	const int cx0 = int(std::floor((player.x - magnet) / CELL)), cx1 = int(std::floor((player.x + magnet) / CELL));
	const int cz0 = int(std::floor((player.z - magnet) / CELL)), cz1 = int(std::floor((player.z + magnet) / CELL));
	for (int cx = cx0; cx <= cx1; ++cx) {
		for (int cz = cz0; cz <= cz1; ++cz) {
			auto it = grid_.find(cell_key(cx, cz));
			if (it == grid_.end()) {
				continue;
			}
			for (int id : it->second) {
				Candy &c = candy_[id];
				if (!c.alive || c.homing || c.fly_t < 1.0f) {
					continue;
				}
				const float dx = c.pos.x - player.x, dz = c.pos.z - player.z;
				const float dy = c.pos.y - player.y;
				// The magnet widens horizontal reach; height still has to be earned with a jump.
				if (dx * dx + dz * dz > magnet * magnet || dy < -0.9f || dy > height) {
					continue;
				}
				c.homing = true;
				c.home_t = 0.0f;
				homing_.push_back(id);
				if (Vfx *vfx = Vfx::get()) {
					c.trail = vfx->trail_begin(halo_color(c.kind));
				}
			}
		}
	}
	// Homing candy: a little hop of anticipation, then an accelerating swoop into the bucket.
	for (size_t i = 0; i < homing_.size();) {
		Candy &c = candy_[homing_[i]];
		c.home_t += dt;
		const Vector3 to = player - c.pos;
		const float dist = to.length();
		if (c.home_t < 0.07f) {
			c.pos.y += dt * 3.0f;
		} else {
			const float t = c.home_t - 0.07f;
			const float speed = Math::min(3.0f + 30.0f * std::pow(t, 1.3f), 26.0f);
			c.pos += to / Math::max(dist, 1e-4f) * Math::min(speed * dt, dist);
		}
		if (Vfx *vfx = Vfx::get()) {
			vfx->trail_move(c.trail, to_global(c.pos));
		}
		if (dist < 0.42f && c.home_t >= 0.07f) {
			const int id = homing_[i];
			if (Vfx *vfx = Vfx::get()) {
				vfx->trail_end(c.trail);
			}
			c.trail = -1;
			c.alive = false;
			c.homing = false;
			c.pop = 0.001f;
			popping_.push_back(id);
			const int v = candy_value(c.kind);
			value += v;
			collected_++;
			collected_value_ += v;
			spark(c.pos, c.kind);
			emit_signal("candy_collected", c.kind, v, c.lot, c.pos, c.id);
			on_picked(c);
			if (c.lot >= 0 && c.lot < int(per_lot_.size())) {
				if (--per_lot_[c.lot] == 0) {
					cleared_.push_back(c.lot);
					emit_signal("house_cleared", c.lot);
				}
			}
			homing_[i] = homing_.back();
			homing_.pop_back();
			continue;
		}
		write_instance(c, Math::lerp(1.0f, 0.75f, Math::clamp(c.home_t * 2.0f, 0.0f, 1.0f)));
		++i;
	}
	return value;
}

void CandyField::burst(const Vector3 &from, const Vector3 &toward, int lot, int count, uint64_t seed) {
	Rng rng(seed);
	const Vector3 fwd = Vector3(toward.x, 0, toward.z).normalized();
	for (int i = 0; i < count; ++i) {
		const int kind = rng.irange(CANDY_PINK, CANDY_LEMON);
		Ref<MultiMesh> mm = mm_[kind]->get_multimesh();
		if (int(slots_[kind].size()) >= mm->get_instance_count() || (halo_ && int(candy_.size()) >= halo_->get_multimesh()->get_instance_count())) {
			continue;
		}
		Candy c;
		const float a = rng.range(-1.1f, 1.1f);
		const Vector3 dir = fwd.rotated(Vector3(0, 1, 0), a);
		c.pos = from + dir * rng.range(2.2f, 5.0f);
		c.pos.y = 0.8f;
		c.fly_from = from + Vector3(0, 1.1f, 0);
		c.fly_t = 0.0f - float(i) * 0.04f;
		c.kind = kind;
		c.lot = lot;
		c.slot = int(slots_[kind].size());
		slots_[kind].push_back(int(candy_.size()));
		candy_.push_back(c);
		flying_.push_back(int(candy_.size()) - 1);
		if (lot >= 0 && lot < int(per_lot_.size())) {
			per_lot_[lot]++;
		}
		total_++;
		total_value_ += 1;
	}
}

int CandyField::remaining_in_lot(int lot) const {
	return lot >= 0 && lot < int(per_lot_.size()) ? per_lot_[lot] : 0;
}

Vector3 CandyField::nearest_remaining(const Vector3 &from) const {
	float best = 1e18f;
	Vector3 out = from;
	for (const Candy &c : candy_) {
		if (!c.alive) {
			continue;
		}
		const float d = c.pos.distance_squared_to(from);
		if (d < best) {
			best = d;
			out = c.pos;
		}
	}
	return out;
}

bool CandyField::nearest_within(const Vector3 &from, float max_rise, Vector3 &out, const std::vector<std::pair<Vector3, float>> *avoid) const {
	float best = 1e18f;
	bool found = false;
	for (const Candy &c : candy_) {
		if (!c.alive || c.fly_t < 1.0f || c.pos.y - from.y > max_rise) {
			continue;
		}
		bool skip = false;
		if (avoid) {
			for (const auto &a : *avoid) {
				if (a.first.distance_squared_to(c.pos) < 4.0f) {
					skip = true;
					break;
				}
			}
		}
		if (skip) {
			continue;
		}
		const float dx = c.pos.x - from.x, dz = c.pos.z - from.z;
		const float d = dx * dx + dz * dz + Math::max(0.0f, c.pos.y - from.y - 1.0f) * 6.0f;
		if (d < best) {
			best = d;
			out = c.pos;
			found = true;
		}
	}
	return found;
}

PackedInt32Array CandyField::take_cleared_lots() {
	PackedInt32Array out;
	for (int l : cleared_) {
		out.push_back(l);
	}
	cleared_.clear();
	return out;
}

void CandyField::_exit_tree() {
	// Hand back the sparkle trails of candy still in flight.
	if (Vfx *vfx = Vfx::get()) {
		for (int id : homing_) {
			vfx->trail_end(candy_[id].trail);
		}
	}
}

void CandyField::_process(double delta) {
	const float dt = float(delta);
	time_ += dt;
	for (size_t i = 0; i < flying_.size();) {
		Candy &c = candy_[flying_[i]];
		c.fly_t += dt / 0.75f;
		const float t = Math::clamp(c.fly_t, 0.0f, 1.0f);
		Candy shown = c;
		shown.pos = c.fly_from.lerp(c.pos, t) + Vector3(0, std::sin(t * Math_PI) * 2.2f, 0);
		const int id = flying_[i];
		// write_instance derives the halo slot from the element address, so write through the vector.
		std::swap(candy_[id].pos, shown.pos);
		write_instance(candy_[id], c.fly_t <= 0.0f ? 0.0f : 1.0f);
		std::swap(candy_[id].pos, shown.pos);
		if (c.fly_t >= 1.0f) {
			c.fly_t = 1.0f;
			write_instance(c, 1.0f);
			index_candy(id);
			add_shadow(c);
			flying_[i] = flying_.back();
			flying_.pop_back();
		} else {
			++i;
		}
	}
	for (size_t i = 0; i < popping_.size();) {
		Candy &c = candy_[popping_[i]];
		c.pop += dt / 0.22f;
		const float t = Math::min(c.pop, 1.0f);
		const float s = t < 0.35f ? 1.0f + t * 1.6f : (1.0f - t) / 0.65f * 1.56f;
		write_instance(c, Math::max(s, 0.0f));
		if (c.pop >= 1.0f) {
			write_instance(c, 0.0f);
			popping_[i] = popping_.back();
			popping_.pop_back();
		} else {
			++i;
		}
	}
}

} // namespace tot
