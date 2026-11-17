#pragma once
// One joint of a rigid-part character rig, as read from a published glTF by AssetLibrary::part_tree
// (or filled in by hand for a procedural model). A rig is a flat list of these in parent-first order,
// so parts[i].parent < i always holds.

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>

namespace tot {

struct RigPart {
	enum Flags : uint8_t {
		HIDDEN = 1, // named alt_*: built hidden (with everything under it) until an animator shows it
		TINY = 2, // small enough that its shadow is not worth a draw (eyes, pupils, teeth, fingers)
	};
	godot::StringName name; // the glTF node name; unique within the rig
	int parent = -1; // index into the same list; -1 hangs from the character's visual root
	godot::Transform3D rest; // in the parent's space: origin = the pivot, basis = the rest rotation
	godot::Ref<godot::Mesh> mesh; // this node's own mesh only (children are their own parts); may be null
	float reach = 0.0f; // distance from the pivot to the farthest vertex of the own mesh (the bend length)
	godot::Vector3 tip; // that farthest vertex, in the part's own space (its direction is the part's run)
	uint8_t flags = 0;
};

// A socket_* node: a named point riding on a part (a muzzle, a grip). Not a joint, never posed.
struct RigSocket {
	godot::StringName name; // without the socket_ prefix
	int parent = -1; // part it rides on; -1 = the visual root
	godot::Transform3D local; // in that part's space
};

} // namespace tot
