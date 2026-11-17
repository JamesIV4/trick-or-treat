#pragma once
// A monster's body as posed parts: one Node3D (a MeshInstance3D when the part has a mesh) per RigPart,
// nested as the part list says, under the monster's visual root. Animators look parts up by name
// once (slot(), -1 when the model has no such part), then pose them every frame relative to their
// rest transform. Every call takes a slot and quietly does nothing for -1, so an animator written
// for the full rig still runs on a model that lacks some parts.
//
// Conventions: metres, radians, Y up, +Z the monster's front, +X its left (see anim_kit.h).

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <vector>

#include "gen/rig_part.h"
#include "play/anim_kit.h"

namespace tot {

class MonsterRig {
public:
	// Creates the nodes under `visual`. Every mesh gets the toon material and the monster look
	// (rim light in `rim`, never dithered away by the player cutout); TINY parts cast no shadow;
	// HIDDEN (alt_*) parts start hidden. `sockets` may be null.
	void build(godot::Node3D *visual, const std::vector<RigPart> &parts, const std::vector<RigSocket> *sockets, const godot::Ref<godot::Material> &toon,
			const godot::Color &rim);

	// ---- lookup
	int slot(const godot::StringName &name) const; // -1 if the model has no such part
	int count() const { return int(slots_.size()); }
	bool has(int slot) const { return slot >= 0 && slot < int(slots_.size()); }
	godot::StringName name(int slot) const { return has(slot) ? slots_[slot].name : godot::StringName(); }
	int parent(int slot) const { return has(slot) ? slots_[slot].parent : -1; }
	godot::Node3D *node(int slot) const { return has(slot) ? slots_[slot].node : nullptr; }
	godot::MeshInstance3D *mesh(int slot) const { return has(slot) ? slots_[slot].mesh : nullptr; }
	godot::Node3D *visual() const { return visual_; }
	// Rest transform in the parent part's space, the reach (pivot to farthest vertex) and that vertex.
	godot::Transform3D rest(int slot) const { return has(slot) ? slots_[slot].rest : godot::Transform3D(); }
	float reach(int slot) const { return has(slot) ? slots_[slot].reach : 0.0f; }
	godot::Vector3 tip(int slot) const { return has(slot) ? slots_[slot].tip : godot::Vector3(); }

	// ---- posing (relative to rest): node transform = (rest.basis * rot * scale, rest.origin + offset)
	void pose(int slot, const godot::Basis &rot, const godot::Vector3 &offset = godot::Vector3(), const godot::Vector3 &scale = godot::Vector3(1, 1, 1));
	void pose(int slot, const anim::JointPose &p);
	void reset(int slot); // back to rest
	void reset_all();
	void show(int slot, bool visible); // alt_* parts, a thrown bone, a snuffed face
	bool shown(int slot) const;

	// ---- per-part shader parameters (toon.gdshader instance uniforms)
	void param(int slot, const godot::StringName &uniform, const godot::Variant &v);
	void param_all(const godot::StringName &uniform, const godot::Variant &v);
	// The hit / telegraph glow, on every mesh. Monster calls this only while a flash is fading.
	// c = flash colour * strength (linear); peak 0..1 = the whole body near white (the first frames of a hit).
	void hit_flash(const godot::Color &c, float peak = 0.0f);
	void glow(int slot, float boost); // glow_boost: 1 = as authored, 0.35 = dim eyes, 1.6 = flare
	void gloss(int slot, float amount); // 0 off, 1 = tight wet highlight (eyes, slime)
	// Marks a part as one continuous soft mesh bent about its pivot: sets soft_pin and widens the
	// cull margin. dir = the part's run from the pivot in its own space (zero = measure by distance
	// from the pivot); length < 0 uses the part's reach; rigid = distance from the pin that stays stiff.
	void soft(int slot, const godot::Vector3 &dir = godot::Vector3(), float length = -1.0f, float rigid = 0.0f);
	// Bends a soft part: the rotation vector (axis * radians, part space) reached at its tip. This is
	// what anim::Dangle::step returns.
	void bend(int slot, const godot::Vector3 &rotvec);
	// Cloth ripple round the part's local Y axis: amplitude (m), phase (radians), lobes, and the local
	// height below which it applies (0 = whole mesh, growing with distance from the pivot).
	void ripple(int slot, float amplitude, float phase, float lobes, float below_height = 0.0f);
	void cast_shadow(int slot, bool on);

	// ---- world space (valid once the monster is in the scene tree)
	godot::Vector3 world_pos(int slot) const; // the part's pivot; the visual root's origin for -1
	godot::Transform3D world_xf(int slot) const;
	// World transform of the part's REST frame (parent's current global transform * rest): the frame
	// anim::Dangle::step and look_dir_local want.
	godot::Transform3D world_rest(int slot) const;
	// A socket_* point riding on its part. False (out untouched) when the model has no such socket.
	bool socket_world(const godot::StringName &name, godot::Transform3D &out) const;

	// ---- model space: the visual root's own space (the monster's facing frame before the root's
	// squash, lean and spin). Cheap (local transforms only), usable before the monster is in the
	// tree and never singular when the root is scaled to nothing, so look-at maths belongs here.
	godot::Transform3D model_xf(int slot) const; // the part as posed now; identity for -1
	godot::Transform3D model_rest(int slot) const; // parent as posed now * this part's rest

private:
	struct Slot {
		godot::StringName name;
		int parent = -1;
		godot::Node3D *node = nullptr;
		godot::MeshInstance3D *mesh = nullptr;
		godot::Transform3D rest;
		godot::Vector3 tip;
		float reach = 0.0f;
		float soft_rigid = 0.0f;
		bool deforms = false;
	};
	void widen_cull(Slot &s);
	std::vector<Slot> slots_;
	std::vector<RigSocket> sockets_;
	godot::Node3D *visual_ = nullptr;
};

} // namespace tot
