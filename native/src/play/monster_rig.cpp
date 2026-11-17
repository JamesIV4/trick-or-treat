#include "play/monster_rig.h"

#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/vector4.hpp>

using namespace godot;

namespace tot {

namespace {
// Shader instance uniform names, built once (a StringName from a literal hashes the string every time).
struct Names {
	StringName rim_strength = "rim_strength", rim_color = "rim_color", keep_visible = "keep_visible", body_flash = "body_flash", glow_boost = "glow_boost",
			   gloss = "gloss", soft_pin = "soft_pin", soft_bend = "soft_bend", cloth_wave = "cloth_wave";
};
const Names &names() {
	static const Names n;
	return n;
}
constexpr float SOFT_CULL_MARGIN = 0.35f; // metres a shader-bent part may leave its rest bounds
} // namespace

void MonsterRig::build(Node3D *visual, const std::vector<RigPart> &parts, const std::vector<RigSocket> *sockets, const Ref<Material> &toon, const Color &rim) {
	visual_ = visual;
	slots_.clear();
	slots_.reserve(parts.size());
	sockets_.clear();
	if (sockets) {
		sockets_ = *sockets;
	}
	const Names &n = names();
	for (const RigPart &p : parts) {
		Slot s;
		s.name = p.name;
		s.parent = p.parent >= 0 && p.parent < int(slots_.size()) ? p.parent : -1;
		s.rest = p.rest;
		s.reach = p.reach;
		s.tip = p.tip;
		if (p.mesh.is_valid()) {
			s.mesh = memnew(MeshInstance3D);
			s.mesh->set_mesh(p.mesh);
			s.mesh->set_material_override(toon);
			s.mesh->set_instance_shader_parameter(n.rim_strength, 0.35f);
			s.mesh->set_instance_shader_parameter(n.rim_color, rim);
			s.mesh->set_instance_shader_parameter(n.keep_visible, 1.0f); // never dithered away by the player cutout
			if (p.flags & RigPart::TINY) {
				s.mesh->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			}
			s.node = s.mesh;
		} else {
			s.node = memnew(Node3D);
		}
		s.node->set_name(String(p.name));
		s.node->set_transform(p.rest);
		if (p.flags & RigPart::HIDDEN) {
			s.node->set_visible(false);
		}
		(s.parent >= 0 ? slots_[s.parent].node : visual_)->add_child(s.node);
		slots_.push_back(s);
	}
}

int MonsterRig::slot(const StringName &name) const {
	for (int i = 0; i < int(slots_.size()); ++i) {
		if (slots_[i].name == name) {
			return i;
		}
	}
	return -1;
}

void MonsterRig::pose(int slot, const Basis &rot, const Vector3 &offset, const Vector3 &scale) {
	if (!has(slot)) {
		return;
	}
	const Slot &s = slots_[slot];
	Basis b = s.rest.basis * rot;
	if (scale != Vector3(1, 1, 1)) {
		b = b.scaled_local(scale);
	}
	s.node->set_transform(Transform3D(b, s.rest.origin + offset));
}

void MonsterRig::pose(int slot, const anim::JointPose &p) {
	if (has(slot)) {
		slots_[slot].node->set_transform(anim::apply_pose(slots_[slot].rest, p));
	}
}

void MonsterRig::reset(int slot) {
	if (has(slot)) {
		slots_[slot].node->set_transform(slots_[slot].rest);
	}
}

void MonsterRig::reset_all() {
	for (const Slot &s : slots_) {
		s.node->set_transform(s.rest);
	}
}

void MonsterRig::show(int slot, bool visible) {
	if (has(slot) && slots_[slot].node->is_visible() != visible) {
		slots_[slot].node->set_visible(visible);
	}
}

bool MonsterRig::shown(int slot) const {
	return has(slot) && slots_[slot].node->is_visible();
}

void MonsterRig::param(int slot, const StringName &uniform, const Variant &v) {
	if (has(slot) && slots_[slot].mesh) {
		slots_[slot].mesh->set_instance_shader_parameter(uniform, v);
	}
}

void MonsterRig::param_all(const StringName &uniform, const Variant &v) {
	for (const Slot &s : slots_) {
		if (s.mesh) {
			s.mesh->set_instance_shader_parameter(uniform, v);
		}
	}
}

void MonsterRig::hit_flash(const Color &c, float peak) {
	// toon.gdshader's body_flash: the surface's own colour brightened (a floor shaded by AO, a strong rim), never a
	// flat added glow, which is a veil at partial strength (parts in front read as seen through glass). alpha = peak.
	param_all(names().body_flash, Color(c.r, c.g, c.b, peak));
}

void MonsterRig::glow(int slot, float boost) {
	param(slot, names().glow_boost, boost);
}

void MonsterRig::gloss(int slot, float amount) {
	param(slot, names().gloss, amount);
}

void MonsterRig::widen_cull(Slot &s) {
	if (!s.deforms && s.mesh) {
		s.mesh->set_extra_cull_margin(SOFT_CULL_MARGIN);
		s.deforms = true;
	}
}

void MonsterRig::soft(int slot, const Vector3 &dir, float length, float rigid) {
	if (!has(slot) || !slots_[slot].mesh) {
		return;
	}
	Slot &s = slots_[slot];
	s.soft_rigid = rigid;
	const float len = length >= 0.0f ? length : Math::max(s.reach - rigid, 0.01f);
	s.mesh->set_instance_shader_parameter(names().soft_pin, Vector4(dir.x, dir.y, dir.z, len));
	widen_cull(s);
}

void MonsterRig::bend(int slot, const Vector3 &rotvec) {
	if (has(slot) && slots_[slot].mesh) {
		slots_[slot].mesh->set_instance_shader_parameter(names().soft_bend, Vector4(rotvec.x, rotvec.y, rotvec.z, slots_[slot].soft_rigid));
	}
}

void MonsterRig::ripple(int slot, float amplitude, float phase, float lobes, float below_height) {
	if (!has(slot) || !slots_[slot].mesh) {
		return;
	}
	Slot &s = slots_[slot];
	s.mesh->set_instance_shader_parameter(names().cloth_wave, Vector4(amplitude, phase, lobes, below_height));
	if (amplitude != 0.0f) {
		widen_cull(s);
	}
}

void MonsterRig::cast_shadow(int slot, bool on) {
	if (has(slot) && slots_[slot].mesh) {
		slots_[slot].mesh->set_cast_shadows_setting(on ? GeometryInstance3D::SHADOW_CASTING_SETTING_ON : GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	}
}

Transform3D MonsterRig::world_xf(int slot) const {
	if (has(slot)) {
		return slots_[slot].node->get_global_transform();
	}
	return visual_ ? visual_->get_global_transform() : Transform3D();
}

Vector3 MonsterRig::world_pos(int slot) const {
	return world_xf(slot).origin;
}

Transform3D MonsterRig::world_rest(int slot) const {
	if (!has(slot)) {
		return world_xf(-1);
	}
	return world_xf(slots_[slot].parent) * slots_[slot].rest;
}

Transform3D MonsterRig::model_xf(int slot) const {
	Transform3D xf;
	for (int i = slot; has(i); i = slots_[i].parent) {
		xf = slots_[i].node->get_transform() * xf;
	}
	return xf;
}

Transform3D MonsterRig::model_rest(int slot) const {
	if (!has(slot)) {
		return Transform3D();
	}
	return model_xf(slots_[slot].parent) * slots_[slot].rest;
}

bool MonsterRig::socket_world(const StringName &name, Transform3D &out) const {
	for (const RigSocket &s : sockets_) {
		if (s.name == name) {
			out = world_xf(s.parent >= 0 && s.parent < int(slots_.size()) ? s.parent : -1) * s.local;
			return true;
		}
	}
	return false;
}

} // namespace tot
