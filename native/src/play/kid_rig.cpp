#include "play/kid_rig.h"

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector4.hpp>

#include <cmath>
#include <cstring>
#include <unordered_set>

using namespace godot;

namespace tot {

using namespace anim;

namespace {

constexpr float ROLL_CENTER = 0.5f; // the curled-up kid tumbles around this height (as DragonController's legacy path)
constexpr float AIR_PIVOT = 0.45f; // whole-model tilts turn about this height in the air, about the feet on the ground
constexpr float MUG_RANGE = 2.2f; // a camera this close to the kid's face gets played to

Quaternion E(float x, float y, float z) {
	return quat_from_euler(Vector3(x, y, z));
}

// The angle of a rotation that is purely about X (knee and elbow joints).
float hinge_angle(const Quaternion &q) {
	return wrap_angle(2.0f * std::atan2(q.x, q.w));
}

Quaternion basis_quat(const Basis &b) {
	return b.orthonormalized().get_rotation_quaternion();
}

// An elbow flex `e` with a resting bend: at least about `rest` (at e = 0), easing smoothly into the pose's
// own flex as that grows (e itself from 1.2 rad on), so a pumping run arm does not fold up past it.
float rest_elbow(float e, float rest) {
	const float t = Math::clamp(e / 1.2f, 0.0f, 1.0f);
	return e + rest * (1.0f - t * t * (3.0f - 2.0f * t));
}

// Spring presets for soft_* and bob_* parts, picked by the word after the prefix (soft_hair_back ->
// "hair"). hz / zeta are the design's stiffness k and damping d converted (hz = sqrt(k) / TAU,
// zeta = d / (2 sqrt(k))).
//   flatten: falling folds the part back by this many radians at full fall speed (ears).
//   lift:    falling lifts the part off its seat by this many metres (a bob part: the hat).
//   hem:     1 = the part is a hem: it ripples (KidTuning::hem_wave) and falling puffs it out. The
//            ripple and the puff push the cloth out from the mesh's Y axis (not along the normals), so a
//            sheet modelled with an inside wall moves as one layer.
//   down:    1 = the part hangs straight down from its pin (a sheet, a cowl): it bends by depth below
//            the pin rather than by distance from it, so the shoulders round the pin stay put.
//   rigid:   with down, the top fraction of the part's depth that does not bend at all.
struct Preset {
	const char *word;
	float hz, zeta, gravity, drag, limit, inertia, flatten, lift, hem, down, rigid, arms;
};
const Preset SOFT_PRESETS[] = {
	// The witch's (kid_witch, batch05 costume_witch.py). Hair locks hang from their roots under the hat brim:
	// they bend by depth below the pin with the roots held (rigid share), so they lag a turn and swing
	// without leaving the scalp. The cape (behind the body, see Soft::trail) and the skirt hang from the
	// body's axis like the ghost's sheet: rigid at the top (the collar, the waist under the belt), the hem sways.
	{ "hair", 1.3f, 0.4f, 0.0f, 0.25f, 0.42f, 0.85f, 0.0f, 0.0f, 0.0f, 1.0f, 0.22f, 0.0f },
	{ "cape", 1.05f, 0.36f, 0.0f, 0.35f, 0.55f, 0.95f, 0.0f, 0.0f, 1.0f, 1.0f, 0.22f, 0.0f },
	{ "skirt", 1.5f, 0.4f, 0.0f, 0.25f, 0.32f, 0.7f, 0.0f, 0.0f, 1.0f, 1.0f, 0.3f, 0.0f },
	// The sheet's rigid top reaches below the hood's join and below the arm holes (kid_ghost: the join is
	// 9 cm under the neck, the holes' lower rims about 25 cm), so the holes stay on the shoulders while the
	// skirt below them sways. No `arms`: the arms come out through holes and never push the cloth.
	// Final fixes (2026-11-16): 1.0 Hz / zeta 0.42 / drag 0.35 / inertia 0.85 / limit 0.42 stored a big trail at a run
	// (spring 0.58 rad) and threw it forward on a stop into the spring's hard limit (-0.84): the hem stood out as a stiff
	// cone at the soft limit (-0.40 shown) for about 15 frames, showing the leggings, then swung back +0.21 (trace:
	// wip_p\guard\g1). Now a calmer, better damped sheet: a modest forward swing that settles in a few frames.
	{ "sheet", 1.5f, 0.7f, 0.0f, 0.18f, 0.28f, 0.45f, 0.0f, 0.0f, 1.0f, 1.0f, 0.58f, 0.0f },
	// The cowl has no spring of its own: see COWL_COUNTER_* and Soft::counter.
	{ "cowl", 1.5f, 0.45f, 0.0f, 0.2f, 0.3f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ "ear", 2.2f, 0.32f, 0.0f, 0.1f, 0.6f, 0.7f, 0.75f, 0.0f, 0.0f }, // the cat's: see ear_fold_ for the fall / perk
	// The witch's hat tip (soft_tip, under bob_hat, origin where the cone starts to flop). `rigid` here (no
	// `down`) is a share of its length kept straight round the pin, so the ring it shares with the hat's
	// lower cone never opens.
	{ "tip", 1.25f, 0.3f, 0.0f, 0.2f, 0.85f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.42f, 0.0f },
	{ "stem", 1.75f, 0.27f, 0.0f, 0.05f, 0.5f, 0.7f, 0.0f, 0.0f, 0.0f },
	// The skeleton's hood drawstrings (soft_cord_l / _r, under body, pinned inside the chin, lying on the chest):
	// light and lively, they swing out in front and to the sides but never back into the chest (Soft::front).
	{ "cord", 1.6f, 0.22f, 2.0f, 0.2f, 0.9f, 1.0f, 0.0f, 0.0f, 0.0f },
	{ "vine", 1.6f, 0.3f, 0.0f, 0.15f, 0.7f, 0.9f, 0.0f, 0.0f, 0.0f },
};
const Preset SOFT_DEFAULT = { "", 1.5f, 0.38f, 0.0f, 0.15f, 0.7f, 0.8f, 0.0f, 0.0f, 0.0f };
// A hood that runs on into the body's sheet (soft_cowl, origin at the neck = the head's pivot): below the
// neck it is turned back against the head's own rotation, none of it above COWL_COUNTER_R0 and all of it
// from R0 + LEN down, so its lower edge stays on the body sheet whatever the head does and the two read
// as one cloth. The same ramp is baked against the head tilt in batch05/costume_ghost.py (COUNTER_R0 / _LEN).
constexpr float COWL_COUNTER_R0 = 0.012f;
constexpr float COWL_COUNTER_LEN = 0.048f;
const Preset BOB_PRESETS[] = {
	{ "bell", 1.4f, 0.17f, 0.0f, 0.1f, 0.9f, 1.0f, 0.0f, 0.0f, 0.0f },
	// limit 0.2: the wide brim must not reach the hair or shoulders. lift 0.015 (x float_up, and HAT_LIFT_MAX caps the
	// spring's overshoot): 0.045 floated the hat 6 cm off the head in a ground pound and showed the scalp.
	{ "hat", 2.25f, 0.45f, 0.0f, 0.1f, 0.2f, 0.55f, 0.0f, 0.015f, 0.0f },
	{ "bow", 2.6f, 0.3f, 0.0f, 0.1f, 0.5f, 0.6f, 0.0f, 0.0f, 0.0f },
	// The pumpkin cap's leaves (bob_leaf_*, Bob::surface keeps them off the cap): limit 0.42 (0.7 stood them up off
	// the cap like a pair of wings in the fidget's hop).
	{ "leaf", 2.4f, 0.25f, 0.0f, 0.15f, 0.42f, 0.8f, 0.0f, 0.0f, 0.0f },
};
const Preset BOB_DEFAULT = { "", 2.2f, 0.3f, 0.0f, 0.1f, 0.6f, 0.7f, 0.0f, 0.0f, 0.0f };

const Preset &find_preset(const String &name, int skip, const Preset *list, int count, const Preset &fallback) {
	const String word = name.substr(skip).get_slice("_", 0);
	for (int i = 0; i < count; ++i) {
		if (word == String(list[i].word)) {
			return list[i];
		}
	}
	return fallback;
}

// The spring's own (hard) limit is twice the preset's: the bend that is shown eases into the
// preset's limit instead of slamming into it (soft_limit below).
DangleParams params_of(const Preset &pr, float len) {
	DangleParams p;
	p.len = len;
	p.hz = pr.hz;
	p.zeta = pr.zeta;
	p.gravity = pr.gravity;
	p.drag = pr.drag;
	p.limit = pr.limit * 2.0f;
	p.inertia = pr.inertia;
	return p;
}

// A bend eased into its limit: unchanged while small, never past `limit`.
// A tail must not sink into the ground. The spring bends it wherever the kid's movement throws it,
// and the body leaning back in a stop (or a landing's tail slap) used to push its underside well
// below the floor. This follows the shader's bend for a few points along the underside, and while
// the lowest of them is under the floor it adds the least turn that lifts that point back onto it.
// The correction grows from zero as the tail reaches the ground, so it never pops.
//   pts      underside points, mesh space      to_world  the tail mesh's world transform
//   bend     rotation vector at the tip (soft_bend.xyz), mesh space
//   reach    soft_pin.w    rigid  soft_bend.w  floor_y   the ground's height, world
// Returns the bend to use; *clear_raw / *clear get the lowest point's height above the floor before
// and after (metres).
//   down     the part hangs from its pin (soft_pin axis (0, -1, 0)): the bend ramps in by depth below the pin
//            instead of by distance from it (a sheet's hem).
Vector3 keep_tail_above_ground(const std::vector<Vector3> &pts, const Transform3D &to_world, Vector3 bend, float reach, float rigid, float floor_y,
		float *clear_raw, float *clear, bool down = false) {
	constexpr float MARGIN = 0.01f;
	const Basis world_dirs = to_world.basis.orthonormalized();
	float k_low = 1.0f;
	Vector3 rel_low;
	auto lowest = [&](const Vector3 &b) {
		const float ang = b.length();
		const Vector3 ax = ang > 1e-4f ? b / ang : Vector3(1, 0, 0);
		float lo = 1.0e9f;
		for (const Vector3 &p : pts) {
			float k = saturate(((down ? -float(p.y) : float(p.length())) - rigid) / Math::max(reach, 0.01f));
			k = k * k * (3.0f - 2.0f * k); // as in toon.gdshader
			const Vector3 w = to_world.xform(ang > 1e-4f ? p.rotated(ax, ang * k) : p);
			if (w.y < lo) {
				lo = w.y;
				k_low = k;
				rel_low = w - to_world.origin;
			}
		}
		return lo;
	};
	float lo = lowest(bend);
	*clear_raw = lo - floor_y;
	// Final fixes 2: a hanging hem gets at most 0.15 rad of correction. Landing from a jump onto a run in real play
	// (autopilot, wip_p\real\ff2_runstop t=26.18) a point near the sheet's axis (small lever) 1.4 cm under the floor took
	// a 0.47 rad turn for one frame: the hem flicked up and back. Left a centimetre in for that frame instead.
	float budget = down ? 0.15f : 2.0f;
	for (int it = 0; it < 4 && lo < floor_y + MARGIN && budget > 1e-4f; ++it) {
		const Vector3 flat(rel_low.x, 0, rel_low.z);
		const float r = flat.length();
		if (r < 0.03f || k_low < 0.05f) {
			break; // right under the pin, or in the part that does not bend: nothing a bend can do
		}
		// Turning about this horizontal axis lifts the point by (angle * r), and it gets k_low of the turn.
		const Vector3 axis_world = Vector3(-flat.z, 0, flat.x) / r;
		const float turn = Math::min(Math::min((floor_y + MARGIN - lo) / (r * k_low), 0.5f), budget);
		budget -= turn;
		Quaternion q = quat_axis_angle(world_dirs.xform_inv(axis_world).normalized(), turn) * quat_from_rotvec(bend);
		if (q.w < 0.0f) {
			q = -q;
		}
		const Vector3 im(q.x, q.y, q.z);
		const float sn = im.length();
		bend = sn > 1e-6f ? im * (2.0f * std::atan2(sn, q.w) / sn) : Vector3();
		lo = lowest(bend);
	}
	*clear = lo - floor_y;
	return bend;
}

Vector3 soft_limit(const Vector3 &bend, float limit) {
	const float a = bend.length();
	if (a < 1e-5f || limit < 1e-4f) {
		return bend;
	}
	return bend * (limit * std::tanh(a / limit) / a);
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Per-costume tuning. Add a row (or change one) here; anything not set keeps the KidTuning default.
// The soft_* / bob_* parts a costume has are found by name and take their springs from the presets
// above, so a row only holds what is particular to the costume.
// ------------------------------------------------------------------------------------------------
const KidTuning &kid_tuning(const String &id) {
	static const std::vector<KidTuning> table = [] {
		std::vector<KidTuning> t;
		// Dragon: a heavy tail that drags behind and thumps down on a landing, wings whose tips lag.
		KidTuning dragon;
		dragon.id = "dragon_kid";
		dragon.costume = KID_DRAGON;
		dragon.tail_hz = 1.45f;
		dragon.tail_zeta = 0.36f;
		dragon.tail_limit = 0.8f;
		dragon.tail_inertia = 0.4f;
		dragon.tail_vertical = 0.4f;
		dragon.tail_thump = 1.0f;
		dragon.wing_hz = 2.6f;
		dragon.wing_zeta = 0.4f;
		dragon.wing_limit = 0.6f;
		t.push_back(dragon);
		// Ghost: the sheet (soft_sheet) ripples along its hem and balloons on a fall; calm arms under it.
		KidTuning ghost;
		ghost.id = "kid_ghost";
		ghost.costume = KID_GHOST;
		ghost.hem_wave = 0.026f; // radial, at the hem of the v2 sheet (12 and then 19 mm hardly showed at a walk)
		ghost.hem_lobes = 4.0f;
		ghost.float_up = 1.6f;
		ghost.bounce = 0.7f; // it glides more than it bounces
		ghost.bucket_fwd = 0.35f;
		// Arm holes (user, 2026-11-15): the arms' pivots sit in two holes in the sheet and the kid's own arms
		// come out through them; the sheet never follows the arms. This default pose keeps the upper arm
		// clear of the bell below the hole (batch05 costume_ghost.py ARM_OUT / ELBOW_REST: change both together).
		ghost.arm_out = 0.42f;
		ghost.elbow_rest = 0.3f;
		// Final fixes 2 (2026-11-16): one soft, heavy sheet (decision 15). On a stop from a run the hem swung forward to
		// 0.18 rad (frames 822-834: the knee push of the halting step, then the spring's swing), lifting the hem to the
		// knees; it now sways less. Walk, run, jump and glide are untouched (stop_calm_ is 0 there).
		ghost.stop_fwd = 0.07f;
		ghost.stop_knee = 0.4f;
		ghost.stop_lean = 1.0f; // the skid leans the body back 0.3 rad: rigid on the body, the sheet's front hem rode up to the knees
		t.push_back(ghost);
		// Cat: the S tail swishes (tail_wave) on top of a soft bend; ears (soft_ear_*) flatten in a
		// fall and spring back; a bell (bob_bell) and bow (bob_bow).
		KidTuning cat;
		cat.id = "kid_cat";
		cat.costume = KID_CAT;
		cat.tail_swish = true;
		cat.glide_spread = true;
		cat.tail_hz = 1.3f;
		cat.tail_zeta = 0.4f;
		cat.tail_limit = 0.7f;
		cat.tail_vertical = 0.6f;
		cat.bounce = 1.15f;
		// Final fixes 2 (2026-11-16): with the default highlight the black onesie read as glossy vinyl (hard streaks on the
		// back of the hood, the body and the tail). Plush: no highlight, a dimmer sky sheen keeps the form at night.
		cat.plush_gloss = -0.85f; // -1.2 flattened the tail and the fur roll; -0.7 left a brighter sheen on the hood's back
		t.push_back(cat);
		// Witch: the hat (bob_hat, soft_tip) lifts on a fall and whips; hair, cape and skirt trail.
		KidTuning witch;
		witch.id = "kid_witch";
		witch.costume = KID_WITCH;
		witch.hem_wave = 0.012f; // skirt and cape hems (6 mm hardly read)
		witch.hem_lobes = 6.0f;
		witch.float_up = 1.3f;
		witch.spin_flare = 1.0f; // the skirt and the cape fly out in the spin
		// The hands and their flared cuffs rest clear of the skirt (batch05 costume_witch.py: build_costumes ARM_OUT).
		witch.arm_out = 0.22f;
		witch.elbow_rest = 0.15f;
		// Polish (2026-11-15): the hat read as glossy plastic in the face beat; felt has no highlight and only a faint
		// sheen (-1 is the scarecrow's felt hat, -2 none at all).
		witch.hat_gloss = -2.0f; // -1.6 left a pale sky sheen on the brim's underside (seen at a grazing angle)
		// Polish round 3: with the default highlight and sky sheen the bangs read as glossy toffee sticks and the locks as
		// shiny chocolate in the face beat; hair is soft, not lacquered.
		witch.hair_gloss = -1.0f; // no highlight; the sky sheen stays (at -1.4 the hair warmed to ginger without it)
		// Final fixes 3 (2026-11-16): on the skid stop the short skirt flipped up (shown -0.41 rad at 822, its front edge
		// 26 -> 59 cm, the striped leggings and the orange underside on show for ~7 frames): the same three causes as the
		// ghost's sheet (knee push, the spring's forward swing into its limit, the body's lean back). The same stop hold.
		witch.stop_fwd = 0.07f;
		witch.stop_knee = 0.4f;
		witch.stop_lean = 1.0f;
		t.push_back(witch);
		// Skeleton: hood cords (soft_cord_*) swing. No limb rattle after landings, hits or the fidget (user, 2026-11-16:
		// remove the skeleton costume's bone rattle); the rattle code stays for tuning but is off (rattle = 0).
		KidTuning skeleton;
		skeleton.id = "kid_skeleton";
		skeleton.costume = KID_SKELETON;
		skeleton.rattle = 0.0f;
		skeleton.matte = -2.0f; // black fleece and printed bones: no vinyl highlights (batch05 costume_skeleton.py)
		t.push_back(skeleton);
		// Pumpkin: arms held clear of the round body; stem, leaves and vines wobble; a heavier bounce.
		KidTuning pumpkin;
		pumpkin.id = "kid_pumpkin";
		pumpkin.costume = KID_PUMPKIN;
		// v2 (batch05 costume_pumpkin.py ARM_OUT / ELBOW_REST: change both together): the bare arms in their puff
		// sleeves rest clear of the round ribbed body (the build prints the clearance: +0.2 .. +1.3 cm in the rest,
		// walk and run arm poses).
		pumpkin.arm_out = 0.47f;
		pumpkin.elbow_rest = 0.22f;
		pumpkin.bounce = 1.2f;
		pumpkin.bucket_fwd = 0.35f;
		// Polish (2026-11-15): 0.45 read as glazed ceramic (hard white streaks down every lobe); a soft-shell costume
		// with a gentle highlight: no catchlight, the soft highlight at 0.65 (0.06 still streaked the lobes from behind).
		pumpkin.shine = -0.35f;
		t.push_back(pumpkin);
		return t;
	}();
	static const KidTuning fallback;
	for (const KidTuning &t : table) {
		if (id == String(t.id)) {
			return t;
		}
	}
	return fallback;
}

// ------------------------------------------------------------------------------------------------
// Binding
// ------------------------------------------------------------------------------------------------

void KidRig::unbind() {
	root_ = visual_ = nullptr;
	for (int j = 0; j < J_COUNT; ++j) {
		n_[j] = nullptr;
		rest_[j] = Transform3D();
	}
	bucket_ = socket_wand_ = nullptr;
	face_.unbind();
	soft_.clear();
	bob_.clear();
	probes_.clear();
}

// Name-based lookup, at any depth. Every part is optional.
void KidRig::find(Node *n) {
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		const String name = n3->get_name();
		static const struct {
			const char *name;
			int joint;
		} JOINTS[] = {
			{ "body", J_BODY }, { "head", J_HEAD }, { "arm_l", J_ARM_L }, { "arm_r", J_ARM_R }, { "forearm_l", J_FOREARM_L }, { "forearm_r", J_FOREARM_R },
			{ "hand_l", J_HAND_L }, { "hand_r", J_HAND_R }, { "leg_l", J_LEG_L }, { "leg_r", J_LEG_R }, { "shin_l", J_SHIN_L }, { "shin_r", J_SHIN_R },
			{ "tail", J_TAIL }, { "wing_l", J_WING_L }, { "wing_r", J_WING_R },
		};
		for (const auto &j : JOINTS) {
			if (name == String(j.name) && !n_[j.joint]) {
				n_[j.joint] = n3;
			}
		}
		if (name == "bucket") {
			bucket_ = n3;
		} else if (name == "socket_wand") {
			socket_wand_ = n3;
		}
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		find(n->get_child(i));
	}
}

Transform3D KidRig::model_xf(Node *n) const {
	Transform3D xf;
	for (Node *q = n; q && q != visual_; q = q->get_parent()) {
		if (Node3D *q3 = Object::cast_to<Node3D>(q)) {
			xf = q3->get_transform() * xf;
		}
	}
	return xf;
}

// Moves a top-level part under the body, keeping where it is, so the body's lean and twist carry it.
void KidRig::adopt(Node3D *n) {
	Node3D *body = n_[J_BODY];
	if (!n || !body || n == body) {
		return;
	}
	for (Node *q = n->get_parent(); q; q = q->get_parent()) {
		if (q == body) {
			return; // already under the body
		}
		if (q == visual_) {
			break;
		}
	}
	const Transform3D local = model_xf(body).affine_inverse() * model_xf(n);
	if (Node *old = n->get_parent()) {
		old->remove_child(n);
	}
	body->add_child(n);
	n->set_transform(local);
}

void KidRig::add_soft(Node3D *n, int joint, int kind) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n);
	if (!mi || mi->get_mesh().is_null()) {
		return;
	}
	Soft s;
	s.g = mi;
	s.node = n;
	s.joint = joint;
	s.kind = kind;
	s.rest = n->get_transform();
	// The pin is the mesh origin. The part's length is how far the mesh reaches from it, and its
	// direction is toward the middle of its bounds (a tail's is back and a little up or down).
	const AABB box = mi->get_aabb();
	float far = 0.0f;
	for (int i = 0; i < 8; ++i) {
		far = Math::max(far, float(box.get_endpoint(i).length()));
	}
	const Vector3 mid = box.get_center();
	s.len = Math::max(0.05f, far * 0.9f);
	s.dir = mid.length() > 0.1f * far ? mid.normalized() : Vector3(0, -1, 0);
	const String name = n->get_name();
	s.side = name.ends_with("_l") ? 1 : (name.ends_with("_r") ? -1 : 0);
	s.on_head = n_[J_HEAD] && n_[J_HEAD]->is_ancestor_of(n);
	if (kind == 1) {
		const Preset pr = { "", tune_.tail_hz, tune_.tail_zeta, 0.0f, 0.2f, tune_.tail_limit, tune_.tail_inertia, 0.0f, 0.0f, 0.0f };
		s.p = params_of(pr, s.len);
		s.limit = pr.limit;
		s.vertical = tune_.tail_vertical;
		// The underside of the tail: the lowest vertex in each of a dozen slices along its length.
		constexpr int SLICES = 12;
		Vector3 low[SLICES];
		bool have[SLICES] = {};
		const Ref<Mesh> mesh = mi->get_mesh();
		for (int si = 0; si < mesh->get_surface_count(); ++si) {
			const Array arrays = mesh->surface_get_arrays(si);
			if (arrays.size() <= Mesh::ARRAY_VERTEX) {
				continue;
			}
			const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
			for (int vi = 0; vi < verts.size(); ++vi) {
				const Vector3 v = verts[vi];
				const int b = Math::clamp(int(v.length() / Math::max(far, 0.01f) * SLICES), 0, SLICES - 1);
				if (!have[b] || v.y < low[b].y) {
					low[b] = v;
					have[b] = true;
				}
			}
		}
		for (int b = 0; b < SLICES; ++b) {
			if (have[b]) {
				s.low_pts.push_back(low[b]);
			}
		}
	} else if (kind == 2 || kind == 3) {
		const Preset pr = { "", tune_.wing_hz, tune_.wing_zeta, 0.0f, 0.25f, tune_.wing_limit, 0.45f, 0.0f, 0.0f, 0.0f };
		s.p = params_of(pr, s.len);
		s.limit = pr.limit;
		s.vertical = 0.7f;
	} else {
		const Preset &pr = find_preset(name, 5, SOFT_PRESETS, int(sizeof(SOFT_PRESETS) / sizeof(Preset)), SOFT_DEFAULT);
		s.p = params_of(pr, s.len);
		s.limit = pr.limit;
		s.flatten = pr.flatten;
		s.arms = pr.arms;
		s.trail = String(pr.word) == "cape";
		s.front = String(pr.word) == "cord";
		if (s.front) {
			mi->set_instance_shader_parameter("gloss", -1.0f); // a fleece cord: no highlight
		}
		// (The ghost's sheet: 0.14 and a -0.7 roll bend were tried 2026-11-16 for the open hem in the roll; no visible change,
		// the tucked shoes stick out of the hem whatever the cloth does. Reverted.)
		s.wrap = String(pr.word) == "skirt" ? 0.03f : (s.trail ? 0.0f : 0.11f);
		if (String(pr.word) == "cowl") {
			s.counter = true;
			s.rigid = COWL_COUNTER_R0;
			s.pin_axis = Vector3(0, -1, 0);
			s.pin_w = COWL_COUNTER_LEN;
			mi->set_extra_cull_margin(0.5f);
			mi->set_instance_shader_parameter("soft_pin", Vector4(0.0f, -1.0f, 0.0f, COWL_COUNTER_LEN));
			mi->set_instance_shader_parameter("soft_bend", Vector4(0.0f, 0.0f, 0.0f, s.rigid));
			soft_.push_back(s);
			return;
		}
		if (pr.hem > 0.0f) {
			s.wave = tune_.hem_wave;
			s.lobes = tune_.hem_lobes;
			s.balloon = 0.035f * tune_.float_up;
		}
		if (pr.down > 0.0f) {
			// Hangs from its pin: bend by depth below it, the top `rigid` share of the depth held still,
			// and ripple only below that (a little further down still).
			const float depth = Math::max(0.05f, float(-box.position.y));
			s.rigid = depth * pr.rigid;
			s.reach = Math::max(0.05f, depth - s.rigid);
			s.wave_top = -(s.rigid + 0.2f * (depth - s.rigid));
			if (pr.hem > 0.0f) {
				// The hem: the lowest vertex in each of 24 sectors round the pin's Y axis, so the rig can keep
				// it off the ground (decision 18; the ripple and the puff push the cloth sideways only).
				constexpr int SECTORS = 24;
				Vector3 low[SECTORS];
				bool have[SECTORS] = {};
				const Ref<Mesh> mesh = mi->get_mesh();
				for (int si = 0; si < mesh->get_surface_count(); ++si) {
					const Array arrays = mesh->surface_get_arrays(si);
					if (arrays.size() <= Mesh::ARRAY_VERTEX) {
						continue;
					}
					const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
					for (int vi = 0; vi < verts.size(); ++vi) {
						const Vector3 v = verts[vi];
						const int b = Math::clamp(int((std::atan2(v.x, v.z) + PI) / TAU * SECTORS), 0, SECTORS - 1);
						if (!have[b] || v.y < low[b].y) {
							low[b] = v;
							have[b] = true;
						}
					}
				}
				for (int b = 0; b < SECTORS; ++b) {
					if (have[b]) {
						s.low_pts.push_back(low[b]);
					}
				}
			}
			mi->set_extra_cull_margin(0.5f);
			s.pin_axis = Vector3(0, -1, 0);
			s.pin_w = Math::max(0.05f, depth - s.rigid);
			mi->set_instance_shader_parameter("soft_pin", Vector4(0.0f, -1.0f, 0.0f, Math::max(0.05f, depth - s.rigid)));
			mi->set_instance_shader_parameter("soft_bend", Vector4(0.0f, 0.0f, 0.0f, s.rigid));
			soft_.push_back(s);
			return;
		}
	}
	mi->set_extra_cull_margin(0.5f); // the shader bend does not move the bounds
	if (kind == 0) {
		// A preset `rigid` without `down` (the witch's hat tip): this share of the length round the pin stays straight.
		const Preset &pr = find_preset(n->get_name(), 5, SOFT_PRESETS, int(sizeof(SOFT_PRESETS) / sizeof(Preset)), SOFT_DEFAULT);
		if (pr.rigid > 0.0f) {
			s.rigid = pr.rigid * s.len;
			s.pin_w = Math::max(0.05f, s.len - s.rigid);
			mi->set_instance_shader_parameter("soft_pin", Vector4(0.0f, 0.0f, 0.0f, Math::max(0.05f, s.len - s.rigid)));
			mi->set_instance_shader_parameter("soft_bend", Vector4(0.0f, 0.0f, 0.0f, s.rigid));
			soft_.push_back(s);
			return;
		}
	}
	s.pin_w = s.len;
	mi->set_instance_shader_parameter("soft_pin", Vector4(0.0f, 0.0f, 0.0f, s.len));
	mi->set_instance_shader_parameter("soft_bend", Vector4());
	soft_.push_back(s);
}

void KidRig::bind(Node3D *visual_root, Node3D *visual) {
	unbind();
	if (!visual_root || !visual) {
		return;
	}
	root_ = visual_root;
	visual_ = visual;
	id_ = visual->get_name();
	tune_ = kid_tuning(id_);
	// Review aid: TOT_RIG_HEM=<metres> gives any costume a hem ripple, to see a test model's soft_cape /
	// soft_sheet / soft_skirt move (art/team/characters_candy/batch05/p3_softtest.ps1).
	if (OS::get_singleton()->has_environment("TOT_RIG_HEM")) {
		tune_.hem_wave = float(OS::get_singleton()->get_environment("TOT_RIG_HEM").to_float());
	}
	// Review aid (final fixes 2): TOT_RIG_NOSTOP=1 turns the heavy sheet's stop hold off, for a before trace.
	if (OS::get_singleton()->has_environment("TOT_RIG_NOSTOP")) {
		tune_.stop_fwd = tune_.stop_lean = 0.0f;
		tune_.stop_knee = 1.0f;
	}
	find(visual_);
	n_[J_ROOT] = visual_;
	// The top-level parts are siblings in the asset. Hang the upper body from the torso; the legs stay
	// on the root and take only the pelvis offset.
	static const int UPPER[] = { J_HEAD, J_ARM_L, J_ARM_R, J_WING_L, J_WING_R, J_TAIL };
	for (int j : UPPER) {
		adopt(n_[j]);
	}
	for (int j = 1; j < J_COUNT; ++j) {
		if (n_[j]) {
			rest_[j] = n_[j]->get_transform();
		}
	}
	rest_[J_ROOT] = Transform3D();
	if (bucket_) {
		bucket_rest_ = bucket_->get_transform();
		const Transform3D m = model_xf(bucket_);
		bucket_rest_model_ = m.basis.orthonormalized();
		bucket_left_ = m.origin.x > 0.0f;
	}
	face_.bind(visual_);
	if (tune_.matte < 0.0f) {
		// Fleece, not vinyl: no highlight on the costume's cloth (the shoes keep theirs).
		static const int CLOTH[] = { J_BODY, J_ARM_L, J_ARM_R, J_FOREARM_L, J_FOREARM_R, J_HAND_L, J_HAND_R, J_LEG_L, J_LEG_R, J_HEAD };
		for (int j : CLOTH) {
			if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n_[j])) {
				mi->set_instance_shader_parameter("gloss", j == J_HEAD ? Math::max(tune_.matte, -1.0f) : tune_.matte);
			}
		}
	}
	if (tune_.plush_gloss != 0.0f) {
		// Plush, not vinyl (the cat): the onesie's cloth and the tail; the hood is a static node of its own under the head,
		// so the face (in the head mesh), the ears, the bow and the bell keep their sheen. The shoes keep theirs.
		static const int PLUSH[] = { J_BODY, J_ARM_L, J_ARM_R, J_FOREARM_L, J_FOREARM_R, J_HAND_L, J_HAND_R, J_LEG_L, J_LEG_R, J_TAIL };
		for (int j : PLUSH) {
			if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n_[j])) {
				mi->set_instance_shader_parameter("gloss", tune_.plush_gloss);
			}
		}
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(visual_->find_child("hood", true, false))) {
			mi->set_instance_shader_parameter("gloss", tune_.plush_gloss);
		}
	}
	if (tune_.shine != 0.0f) {
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n_[J_BODY])) {
			mi->set_instance_shader_parameter("gloss", tune_.shine);
		}
	}
	// Soft parts: the tail and wings (single meshes with their origin at the root), and any soft_*
	// mesh under any parent. bob_* parts swing rigidly about their origin.
	add_soft(n_[J_TAIL], J_TAIL, 1);
	add_soft(n_[J_WING_L], J_WING_L, 2);
	add_soft(n_[J_WING_R], J_WING_R, 3);
	std::vector<Node *> stack = { visual_ };
	while (!stack.empty()) {
		Node *n = stack.back();
		stack.pop_back();
		const String name = n->get_name();
		if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
			if (tune_.shine != 0.0f && (name.begins_with("soft_") || name.begins_with("bob_"))) {
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					mi->set_instance_shader_parameter("gloss", tune_.shine);
				}
			}
			if (tune_.hat_gloss != 0.0f && (name.begins_with("bob_hat") || name == "soft_tip")) {
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					mi->set_instance_shader_parameter("gloss", tune_.hat_gloss);
					// The controller's moonlight rim (0.22) lit the brim's underside, seen edge-on from the face camera,
					// a pale grey-lavender whatever its colour: felt keeps only a faint rim.
					mi->set_instance_shader_parameter("rim_strength", 0.07f);
				}
			}
			if (tune_.costume == KID_WITCH && (name.begins_with("soft_hair") || name.begins_with("hair_cap"))) {
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					mi->set_instance_shader_parameter("rim_strength", 0.12f); // the same rim paled the locks' edges toward ginger-grey
				}
			}
			if (tune_.hair_gloss != 0.0f && (name.begins_with("soft_hair") || name.begins_with("hair_cap"))) {
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					mi->set_instance_shader_parameter("gloss", tune_.hair_gloss);
				}
			}
			if (name.begins_with("matte_")) {
				// A part that must stay dark in any light (the pumpkin's carved face): no highlight and no sky sheen.
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					mi->set_instance_shader_parameter("gloss", -2.0f);
				}
			}
			if (name.begins_with("soft_")) {
				add_soft(n3, -1, 0);
			} else if (name.begins_with("bob_")) {
				Bob b;
				b.node = n3;
				b.rest = n3->get_transform();
				float len = 0.08f;
				b.dir = name.begins_with("bob_hat") || name.begins_with("bob_leaf") || name.begins_with("bob_bow") ? Vector3(0, 1, 0) : Vector3(0, -1, 0);
				if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n3)) {
					const AABB box = mi->get_aabb();
					if (box.get_center().length() > 0.01f) {
						b.dir = box.get_center().normalized();
						len = Math::max(0.03f, float(box.get_center().length()) * 1.6f);
					}
				}
				const Preset &pr = find_preset(name, 4, BOB_PRESETS, int(sizeof(BOB_PRESETS) / sizeof(Preset)), BOB_DEFAULT);
				b.p = params_of(pr, len);
				b.p.limit = pr.limit; // a rigid part stops at its limit
				b.lift = pr.lift;
				b.hat = name.begins_with("bob_hat");
				if (name.begins_with("bob_leaf")) {
					// A leaf lying on a round cap whose centre is about 0.23 m below its stalk (the pumpkin's): the
					// surface normal under the leaf's middle, and the turn that would press the tip into it.
					const Vector3 n = (b.dir * (len / 1.6f) + Vector3(0, 0.23f, 0)).normalized();
					const Vector3 a = b.dir.cross(n);
					if (a.length() > 1e-3f) {
						b.surface = true;
						b.away = a.normalized();
					}
				}
				bob_.push_back(b);
			}
		}
		for (int i = 0; i < n->get_child_count(); ++i) {
			stack.push_back(n->get_child(i));
		}
	}
	// Where the soles are, for keeping the feet on the ground: the heel and the toe of whichever node
	// carries the shoe (the shin on a v2 model, the whole leg on a v1 model).
	for (int side = 0; side < 2; ++side) {
		Node3D *foot = n_[J_SHIN_L + side] ? n_[J_SHIN_L + side] : n_[J_LEG_L + side];
		Vector3 heel(0, n_[J_SHIN_L + side] ? -0.165f : -0.3f, -0.07f), toe(0, heel.y, 0.15f);
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(foot)) {
			if (mi->get_mesh().is_valid()) {
				const AABB box = mi->get_aabb();
				const float x = box.get_center().x;
				heel = Vector3(x, box.position.y, box.position.z * 0.6f);
				toe = Vector3(x, box.position.y, (box.position.z + box.size.z) * 0.85f);
			}
		}
		sole_[side][0] = heel;
		sole_[side][1] = toe;
	}
	// The leg's measurements for the stride (the left leg's; the right mirrors it).
	leg_ = LegGeom();
	leg_.shin = n_[J_SHIN_L] != nullptr;
	leg_.hip_y = n_[J_LEG_L] ? float(rest_[J_LEG_L].origin.y) : 0.3f;
	leg_.thigh = leg_.shin ? Math::max(0.02f, float(-rest_[J_SHIN_L].origin.y)) : 0.0f;
	leg_.heel = Vector3(0, sole_[0][0].y, sole_[0][0].z);
	leg_.toe = Vector3(0, sole_[0][1].y, sole_[0][1].z);
	leg_.legacy_knee = tune_.legacy_knee;
	// The far end of each fist, for laying a hand on a ledge.
	for (int side = 0; side < 2; ++side) {
		hand_tip_[side] = Vector3(0, -0.09f, 0.02f);
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n_[J_HAND_L + side])) {
			if (mi->get_mesh().is_valid() && mi->get_aabb().get_center().length() > 0.01f) {
				hand_tip_[side] = mi->get_aabb().get_center() * 1.8f;
			}
		}
	}
	collect_probes();
	reset();
	bind_snap_ = true;
}

void KidRig::reset() {
	for (Soft &s : soft_) {
		s.d.reset();
		s.frame_primed = false;
	}
	for (Bob &b : bob_) {
		b.d.reset();
		b.up.reset();
	}
	bucket_d_.reset();
	lean_pitch_.reset();
	lean_roll_.reset();
	head_pitch_.reset();
	head_roll_.reset();
	yaw_kick_.reset();
	pelvis_.reset();
	hang_sway_.reset();
	cele_look_.reset();
	cele_lift_.reset();
	head_lead_.reset();
	ear_fold_.reset();
	ride_primed_ = false;
	hop_t_ = -1.0f;
	hop_cool_ = 0.0f;
	look_.reset();
	face_.reset();
	w_ground_.snap(true);
	for (Weight *w : { &w_glide_, &w_roll_, &w_pound_, &w_stall_, &w_hang_, &w_climb_, &w_hurt_, &w_mount_, &w_spin_, &w_cast_, &w_flap_, &w_skid_, &w_fidget_, &w_glance_, &w_mug_ }) {
		w->snap(false);
	}
	bucket_force_.snap(false);
	accel_ = Vector3();
	yaw_rate_ = decel_ = stop_calm_ = 0.0f;
	primed_ = false;
	land_t_ = launch_t_ = cele_t_ = hurt_t_ = cast_t_ = rattle_t_ = -1.0f;
	bucket_punch_ = 0.0f;
	idle_t_ = run_t_ = air_t_ = 0.0f;
	mug_t_ = -1.0f;
	fidget_t_ = -1.0f;
	fidget_next_ = 3.0f;
	fidget_n_ = 0;
	rng_ = Rand(0x7e11u);
	was_spinning_ = was_rolling_ = false;
	ground_[0] = ground_[1] = 0.0f;
	gaze_ = Vector2();
	blink_req_ = false;
	blink_hold_req_ = 0.0f;
	hand_ik_ = 0.0f;
	roll_lift_ = 0.0f;
	want_ = KidFaceWant();
	last_ = Pose();
}

void KidRig::set_look_target(const Vector3 &pos, float weight) {
	look_pos_ = pos;
	look_w_ = saturate(weight);
}

// ------------------------------------------------------------------------------------------------
// The stride clock. The controller's gait phase times the footstep sounds (two per TAU); the legs
// are short, so below a run the kid takes two strides in that time (a trot: every sound still falls
// on a footfall) and at a run one. The clock follows the controller's phase at that rate and is
// pulled into step with it whenever the rate is steady.
// ------------------------------------------------------------------------------------------------
void KidRig::step_gait(const KidAnimInput &in, float sf, float speed, float dt) {
	(void)speed;
	const float dphi = Math::max(0.0f, angle_delta(phase_prev_, in.phase));
	phase_prev_ = in.phase;
	const float mult = lerp(2.0f, 1.0f, smoothstep01(remap01(sf, 0.5f, 0.8f)));
	gait_ += dphi * mult;
	if (mult > 1.97f || mult < 1.03f) {
		gait_ += angle_delta(gait_, in.phase * (mult > 1.5f ? 2.0f : 1.0f)) * damp_factor(0.12f, dt);
	}
	gait_ -= TAU * std::floor(gait_ / TAU);
	if (dt > 0.0f) {
		gait_rate_ = damp(gait_rate_, dphi * mult / dt, 0.05f, dt);
	}
}

// ------------------------------------------------------------------------------------------------
// The face for this frame, from what the kid is doing.
// ------------------------------------------------------------------------------------------------
KidFaceWant KidRig::face_want(const KidAnimInput &in, float sf, bool busy, bool mugging, float dt) {
	KidFaceWant f;
	const bool air = !in.on_floor && !in.mounted && !in.hanging && in.climb_k < 0.0f;
	const float vy = in.velocity.y;
	f.gaze = gaze_;
	f.lively = !busy;
	if (in.on_floor && sf > 0.3f) {
		f.brow_up = 0.2f;
		f.gaze.x += clampf(yaw_rate_ * 0.05f, -0.3f, 0.3f); // the eyes lead a turn
		if (run_t_ > 0.9f) {
			f.mouth = KidFaceWant::OPEN; // a long run: grinning into the wind
		}
	}
	if (air) {
		f.mouth = KidFaceWant::OPEN;
		if (in.gliding) {
			f.brow_up = 0.5f;
			f.wide = 0.25f;
			f.gaze.y += 0.12f;
		} else if (vy > 1.5f) {
			f.brow_up = 0.8f;
			f.wide = 0.35f;
			f.gaze.y -= 0.15f;
		} else if (vy < -7.0f) {
			f.mouth = KidFaceWant::O;
			f.brow_up = 0.5f;
			f.brow_tilt = -0.4f;
			f.wide = 0.8f;
			f.gaze.y += 0.22f;
		} else if (vy < -1.5f) {
			f.brow_up = 0.4f;
			f.wide = 0.3f;
			f.gaze.y += 0.2f;
		} else {
			f.brow_up = 1.0f; // the top of a jump: eyes wide
			f.wide = 1.0f;
		}
	}
	if (w_flap_.w > 0.3f) {
		f.mouth = KidFaceWant::O;
		f.wide = 0.6f;
		f.brow_up = 0.9f;
	}
	if (in.spin_k >= 0.0f) {
		f.mouth = KidFaceWant::O;
		f.brow_tilt = 0.4f;
		f.squint = 0.2f;
	}
	if (in.mounted) {
		f.mouth = KidFaceWant::OPEN;
		f.brow_up = 0.4f;
	}
	if (in.hanging) {
		f.mouth = KidFaceWant::O;
		f.brow_tilt = -0.8f;
		f.brow_up = 0.4f;
		f.wide = 0.4f;
		f.gaze.y = -0.15f;
	}
	if (in.climb_k >= 0.0f) {
		f.mouth = KidFaceWant::GRIT;
		f.brow_tilt = 0.6f;
		f.squint = 0.5f;
	}
	if (fidget_t_ >= 0.0f && w_fidget_.w > 0.3f) {
		switch (fidget_kind_) {
			case 0: // into the bucket
				f.gaze = Vector2(bucket_left_ ? 0.32f : -0.32f, 0.28f);
				f.brow_up = 0.5f;
				break;
			case 1: // watching the wand twirl
				f.gaze = Vector2(bucket_left_ ? -0.3f : 0.3f, 0.12f);
				f.mouth = KidFaceWant::OPEN;
				f.brow_up = 0.4f;
				break;
			case 2: { // the eyes go round ahead of the head
				const float len = kid_pose::fidget_length(2);
				f.gaze.x = 0.36f * std::sin(saturate((fidget_t_ - 0.05f) / (len - 0.3f)) * TAU);
				f.brow_up = 0.3f;
				break;
			}
			case 4: // a stretch: eyes squeezed, mouth open
				if (tune_.costume == KID_DRAGON || tune_.costume == KID_PLAIN) {
					f.squint = 0.9f;
					f.mouth = KidFaceWant::OPEN;
					f.brow_up = 0.4f;
				} else if (tune_.costume == KID_GHOST) {
					f.mouth = KidFaceWant::O;
					f.wide = 0.6f;
				} else if (tune_.costume == KID_SKELETON) {
					// "Brrr": an ooh with the hunch, teeth chattering through the shimmy (the grit and the open grin
					// swapped about eight times a second), then a pleased grin with the brows up.
					if (fidget_t_ < 0.32f) {
						f.mouth = KidFaceWant::O;
						f.wide = 0.5f;
						f.brow_up = 0.6f;
					} else if (fidget_t_ < 1.45f) {
						f.mouth = int(fidget_t_ * 16.0f) % 2 ? KidFaceWant::GRIT : KidFaceWant::OPEN;
						f.squint = 0.35f;
						f.brow_tilt = -0.5f;
						f.brow_up = 0.3f;
					} else {
						f.mouth = KidFaceWant::OPEN;
						f.brow_up = 0.8f;
						f.wide = 0.2f;
					}
				} else if (tune_.costume == KID_PUMPKIN) {
					// "Ooh" in the dip, a big open grin for the hop and the head wobble (a giggle: eyes half shut),
					// then a pleased smile with the brows up, looking out.
					if (fidget_t_ < 0.32f) {
						f.mouth = KidFaceWant::O;
						f.wide = 0.3f;
						f.brow_up = 0.5f;
					} else if (fidget_t_ < 1.9f) {
						f.mouth = KidFaceWant::OPEN;
						f.brow_up = 0.8f;
						f.squint = fidget_t_ > 0.75f ? 0.45f : 0.0f;
					} else {
						f.mouth = KidFaceWant::SMILE;
						f.brow_up = 0.6f;
						f.wide = 0.15f;
					}
				} else if (tune_.costume == KID_WITCH) {
					// "Oops" as the hat slips, eyes up at the brim while she fixes it, then a pleased grin.
					if (fidget_t_ < 0.9f) {
						f.mouth = KidFaceWant::O;
						f.wide = 0.35f;
						f.brow_up = 0.6f;
						f.gaze = Vector2(0.18f, 0.32f);
					} else if (fidget_t_ < 1.6f) {
						f.mouth = KidFaceWant::GRIT;
						f.gaze = Vector2(0.3f, 0.42f);
						f.brow_tilt = -0.3f;
					} else {
						f.mouth = KidFaceWant::OPEN;
						f.brow_up = 0.7f;
					}
				}
				break;
			default:
				break;
		}
		f.lively = false;
	}
	if (cele_t_ >= 0.0f) {
		f.mouth = KidFaceWant::OPEN;
		f.brow_up = 1.0f;
		f.wide = 0.3f;
		if (cele_t_ < 0.35f) {
			f.gaze = Vector2(bucket_left_ ? 0.3f : -0.3f, 0.26f); // a look at what landed in the bucket
		}
	}
	if (land_t_ >= 0.0f && land_t_ < 0.2f && land_s_ > 0.6f) {
		// A jump's landing: a quick squint with the knees. Only a long drop grits the teeth.
		f.squint = land_s_ > 0.9f ? 0.8f : 0.45f;
		f.brow_up = -0.4f;
		if (land_s_ > 0.9f) {
			f.mouth = KidFaceWant::GRIT;
		}
	}
	if (cast_t_ >= 0.0f && cast_t_ < 0.5f) {
		f.mouth = KidFaceWant::OPEN; // a shout with the shot, eyes on the target
		f.brow_tilt = 1.0f;
		f.brow_up = -0.7f;
		f.squint = 0.25f;
		f.gaze = Vector2();
		if (cast_t_ - dt < 0.42f && cast_t_ >= 0.42f) {
			blink_req_ = true; // ... and a blink as the wand comes down
		}
	}
	if (in.roll_k >= 0.0f) {
		f.mouth = KidFaceWant::GRIT;
		f.squint = 0.8f;
		f.brow_tilt = 0.6f;
		f.brow_up = -0.5f;
	}
	if (in.pounding) {
		f.brow_tilt = in.pound_stall ? 0.8f : 1.0f;
		f.brow_up = -0.7f;
		f.mouth = in.pound_stall ? KidFaceWant::GRIT : KidFaceWant::OPEN;
		f.squint = in.pound_stall ? 1.0f : 0.1f;
		f.wide = 0.0f;
		f.gaze = Vector2(0, in.pound_stall ? 0.0f : 0.3f);
	}
	if (hurt_t_ >= 0.0f && hurt_t_ < 1.0f) {
		f.mouth = hurt_t_ < 0.55f ? KidFaceWant::GRIT : KidFaceWant::O;
		f.brow_tilt = -1.0f;
		f.brow_up = 0.4f;
		f.squint = hurt_t_ < 0.45f ? 0.9f : 0.2f;
		f.wide = 0.0f;
		f.lively = false;
	}
	if (mugging && mug_t_ >= 0.0f) {
		// Playing to a close camera (kid_pose::mug moves the head on the same clock): a blink, a big
		// grin with the brows up, a shy look away and back, a double blink, then just looking.
		const float t = std::fmod(mug_t_, kid_pose::MUG_LENGTH), t0 = t - dt;
		const auto at = [&](float when) { return t0 < when && t >= when; };
		f.lively = t > 2.7f;
		if (t > 0.55f && t < 1.65f) {
			f.mouth = KidFaceWant::OPEN;
			f.brow_up = 0.9f;
			f.wide = 0.25f;
		}
		if (t > 1.72f && t < 2.08f) {
			f.gaze = Vector2(look_.ang.x > 0.0f ? -0.38f : 0.38f, 0.2f);
			f.brow_tilt = -0.5f;
			f.brow_up = 0.2f;
		}
		if (t > 3.4f && t < 4.5f) {
			f.brow_up = 0.5f;
		}
		if (at(0.28f) || at(2.2f) || at(2.5f) || at(4.7f)) {
			blink_req_ = true;
		}
	}
	// No blinks of the kid's own in the middle of a move (a long glide apart) or of a scripted face.
	f.hold_open = (air && !(in.gliding && air_t_ > 2.5f)) || in.spin_k >= 0.0f || in.roll_k >= 0.0f || in.climb_k >= 0.0f || cast_t_ >= 0.0f || cele_t_ >= 0.0f ||
			(land_t_ >= 0.0f && land_t_ < 0.3f) || (mugging && mug_t_ >= 0.0f) || (fidget_t_ >= 0.0f && w_fidget_.w > 0.3f);
	f.blink = blink_req_;
	f.blink_hold = blink_hold_req_;
	blink_req_ = false;
	blink_hold_req_ = 0.0f;
	return f;
}

// ------------------------------------------------------------------------------------------------
// The frame
// ------------------------------------------------------------------------------------------------

void KidRig::update(const KidAnimInput &in) {
	if (!visual_ || !root_) {
		return;
	}
	const float dt = safe_dt(in.dt);
	const uint32_t ev = in.events;
	if (ev & KID_EV_TELEPORT) {
		reset();
		bind_snap_ = false;
	}

	// --- How the kid is moving.
	const Vector3 hv(in.velocity.x, 0, in.velocity.z);
	const float speed = hv.length();
	const float sf_raw = clampf(speed / Math::max(in.run_speed, 0.1f), 0.0f, 1.3f);
	if (!primed_) {
		vel_prev_ = in.velocity;
		yaw_prev_ = in.yaw;
		speed_prev_ = speed;
		sf_.reset(sf_raw);
		phase_prev_ = in.phase;
		gait_ = in.phase * 2.0f;
		primed_ = true;
	}
	const float sf = sf_.follow(sf_raw, 0.05f, dt);
	if (dt > 0.0f) {
		Vector3 a = (in.velocity - vel_prev_) / dt;
		if (a.length() > 120.0f) {
			a = a.normalized() * 120.0f;
		}
		accel_ = damp(accel_, a, 0.05f, dt);
		yaw_rate_ = damp(yaw_rate_, clampf(angle_delta(yaw_prev_, in.yaw) / dt, -14.0f, 14.0f), 0.05f, dt);
		decel_ = damp(decel_, (speed_prev_ - speed) / dt, 0.04f, dt);
	}
	vel_prev_ = in.velocity;
	yaw_prev_ = in.yaw;
	speed_prev_ = speed;
	step_gait(in, sf, speed, dt);
	const Vector3 fwd(std::sin(in.yaw), 0, std::cos(in.yaw)), left(std::cos(in.yaw), 0, -std::sin(in.yaw));
	const float a_fwd = accel_.dot(fwd), a_left = accel_.dot(left);
	const bool rolling = in.roll_k >= 0.0f, spinning = in.spin_k >= 0.0f, climbing = in.climb_k >= 0.0f;
	const bool grounded_now = in.on_floor || in.mounted;
	run_t_ = in.on_floor && sf_raw > 0.75f ? run_t_ + dt : 0.0f;
	air_t_ = grounded_now ? 0.0f : air_t_ + dt;
	for (int i = 0; i < 2; ++i) {
		ground_[i] = damp(ground_[i], in.ground_ok ? clampf(in.ground[i], -0.3f, 0.12f) : 0.0f, 0.06f, dt);
	}

	// --- One-shot events.
	if (ev & KID_EV_JUMP) {
		launch_t_ = 0.0f;
		++jump_parity_;
		head_pitch_.kick(-1.4f);
	}
	if (ev & KID_EV_BOUNCE) {
		launch_t_ = 0.0f;
		head_pitch_.kick(-2.0f);
	}
	if (ev & KID_EV_FLAP) {
		head_pitch_.kick(-1.2f);
	}
	if (ev & KID_EV_LAND) {
		// 0 a step down .. about 0.55 an ordinary jump (it lands at 10 m/s) .. 1 a drop off a roof.
		const float s = clampf((in.land_impact - 1.5f) / 16.0f, 0.0f, 1.0f);
		if (s > 0.03f) {
			land_t_ = 0.0f;
			land_s_ = 0.3f + 0.8f * s;
			head_pitch_.kick(0.4f + 1.0f * s);
			pelvis_.kick(-0.35f * s);
			if (s > 0.75f) {
				blink_req_ = true; // only a real drop shuts the eyes
			}
			if (tune_.rattle > 0.0f && s > 0.2f) {
				rattle_t_ = 0.0f;
			}
			// Cords on the chest flick forward off it with the landing and swing back.
			for (Soft &sp : soft_) {
				if (sp.front) {
					sp.d.kick(fwd * (0.9f + 1.8f * s) + left * (0.5f * float(sp.side)));
				}
			}
			// The tail slaps down after the body has stopped, and bounces.
			for (Soft &sp : soft_) {
				if (sp.kind == 1 && tune_.tail_thump > 0.0f) {
					sp.d.kick(Vector3(0, -(1.2f + 3.0f * s) * tune_.tail_thump, 0));
				}
			}
			// Ears that were folded back by the fall pop up and forward past upright (the perk).
			ear_fold_.kick(-(2.0f + 6.0f * s));
			// A hat that floated up drops back onto the head and nods forward with the landing.
			for (Bob &b : bob_) {
				if (b.hat) {
					b.d.kick(fwd * (0.6f + 1.4f * s));
				}
			}
			// The pumpkin: the stem nods forward and the tendrils spring, the cap's leaves flap up off it.
			if (tune_.costume == KID_PUMPKIN) {
				for (Soft &sp : soft_) {
					if (sp.kind == 0) {
						sp.d.kick(fwd * (0.7f + 1.6f * s) + left * (0.25f * float(jump_parity_ % 2 ? 1 : -1)));
					}
				}
				for (Bob &b : bob_) {
					if (b.surface) {
						b.d.kick(Vector3(0, 1.0f + 2.2f * s, 0));
					}
				}
			}
		}
	}
	if (ev & KID_EV_POUND_LAND) {
		land_t_ = 0.0f;
		land_s_ = 1.35f;
		head_pitch_.kick(2.2f);
		pelvis_.kick(-0.6f);
		blink_req_ = true;
		blink_hold_req_ = 0.12f;
		if (tune_.rattle > 0.0f) {
			rattle_t_ = 0.0f;
		}
	}
	if (ev & KID_EV_CLIMB_DONE) {
		land_t_ = 0.0f;
		land_s_ = 0.45f;
	}
	if (ev & KID_EV_CELEBRATE) {
		// Candy lands several times a second along a trail. Each pickup only renews the celebration:
		// nothing here may restart a pose from zero, or the head snaps on every one of them.
		const float since = cele_t_; // seconds since the pickup before this one; negative: none lately
		const bool fresh = since < 0.0f || since > 0.35f;
		cele_t_ = 0.0f;
		cele_tier_ = since < 0.0f ? in.celebrate_tier : Math::max(cele_tier_, in.celebrate_tier);
		if (bucket_punch_ <= 0.05f) {
			bucket_punch_ = in.celebrate_tier >= 1 ? 1.4f : 1.0f; // a pulse already under way finishes; it is not restarted
		}
		if (fresh) {
			bucket_d_.kick(fwd * 0.35f + Vector3(0, 0.3f, 0));
		}
		// The happy hop (a chocolate or better): only from a standstill, never cut short, never stacked.
		if (in.celebrate_tier >= 1 && hop_t_ < 0.0f && hop_cool_ <= 0.0f && in.on_floor && sf_raw < 0.25f) {
			hop_t_ = 0.0f;
			hop_cool_ = 1.4f;
			blink_req_ = true;
			blink_hold_req_ = 0.1f;
		}
	}
	if (ev & KID_EV_HURT) {
		hurt_t_ = 0.0f;
		Vector3 away(in.hurt_dir.dot(left), 0, in.hurt_dir.dot(fwd));
		hurt_local_ = away.length() > 0.01f ? away.normalized() : Vector3(0, 0, -1);
		head_pitch_.kick(-3.0f * hurt_local_.z);
		head_roll_.kick(2.0f * hurt_local_.x);
		for (Soft &s : soft_) {
			s.d.kick(in.hurt_dir * -2.0f + Vector3(0, 1.5f, 0));
		}
		for (Bob &b : bob_) {
			b.d.kick(in.hurt_dir * -1.5f + Vector3(0, 1.0f, 0));
		}
		blink_req_ = true;
		blink_hold_req_ = 0.3f;
		if (tune_.rattle > 0.0f) {
			rattle_t_ = 0.0f;
		}
	}
	if (ev & KID_EV_CAST) {
		cast_t_ = 0.0f;
		w_cast_.snap(true);
	}
	if (ev & KID_EV_GRAB) {
		hang_sway_.kick(2.4f);
	}
	if (was_spinning_ && !spinning) {
		yaw_kick_.x = 0.0f;
		yaw_kick_.kick(5.0f); // carries on a little past the full turn, then settles back
	}
	if (was_rolling_ && !rolling && in.on_floor) {
		land_t_ = 0.0f;
		land_s_ = 0.5f; // comes out of the tuck into a crouch
	}
	was_spinning_ = spinning;
	was_rolling_ = rolling;
	for (float *t : { &land_t_, &launch_t_, &cele_t_, &hurt_t_, &cast_t_, &rattle_t_ }) {
		if (*t >= 0.0f) {
			*t += dt;
		}
	}
	if (land_t_ > 0.6f) {
		land_t_ = -1.0f;
	}
	if (launch_t_ > 0.2f) {
		launch_t_ = -1.0f;
	}
	if (cele_t_ > 0.9f) {
		cele_t_ = -1.0f;
	}
	if (hop_t_ >= 0.0f) {
		hop_t_ += dt;
		if (hop_t_ > kid_pose::CELEBRATE_HOP_LENGTH) {
			hop_t_ = -1.0f;
		}
	}
	hop_cool_ = Math::max(0.0f, hop_cool_ - dt);
	if (hurt_t_ > 1.2f) {
		hurt_t_ = -1.0f;
	}
	if (cast_t_ > kid_pose::CAST_LENGTH) {
		cast_t_ = -1.0f;
	}
	if (rattle_t_ > kid_pose::RATTLE_LENGTH) {
		rattle_t_ = -1.0f;
	}

	// --- Blend weights.
	if (bind_snap_) {
		// A costume change in play (bind() reset everything to standing): start the new costume in the state the
		// kid is in, or a change in the air or mid-spin shows the standing pose for a few frames.
		bind_snap_ = false;
		w_ground_.snap(grounded_now);
		w_glide_.snap(in.gliding && !grounded_now);
		w_roll_.snap(rolling);
		w_pound_.snap(in.pounding);
		w_stall_.snap(in.pounding && in.pound_stall);
		w_hang_.snap(in.hanging);
		w_climb_.snap(climbing);
		w_hurt_.snap(in.knocked);
		w_mount_.snap(in.mounted);
		w_spin_.snap(spinning);
		w_flap_.snap(in.flap_k >= 0.0f);
		hand_ik_ = in.hanging ? 1.0f : 0.0f;
	}
	w_ground_.step(grounded_now, 0.06f, 0.09f, dt);
	w_glide_.step(in.gliding && !grounded_now, 0.16f, 0.12f, dt);
	w_roll_.step(rolling, 0.05f, 0.1f, dt);
	w_pound_.step(in.pounding, 0.05f, 0.08f, dt);
	w_stall_.step(in.pounding && in.pound_stall, 0.03f, 0.07f, dt);
	w_hang_.step(in.hanging, 0.12f, 0.1f, dt);
	w_climb_.step(climbing, 0.06f, 0.16f, dt);
	w_hurt_.step(in.knocked, 0.04f, 0.3f, dt);
	w_mount_.step(in.mounted, 0.15f, 0.15f, dt);
	w_spin_.step(spinning, 0.04f, 0.13f, dt);
	w_flap_.step(in.flap_k >= 0.0f, 0.04f, 0.18f, dt);
	w_cast_.step(cast_t_ >= 0.0f && cast_t_ < kid_pose::CAST_LENGTH - 0.2f, 0.0f, 0.2f, dt);
	w_skid_.step(in.on_floor && decel_ > 12.0f && !rolling && !in.mounted, 0.05f, 0.24f, dt);
	// A stop's hold on a heavy sheet (KidTuning::stop_fwd): renewed by the skid, fading over about half a second, so
	// the sheet's forward swing after the halt is held too. A knockback is not a stop.
	stop_calm_ = (in.knocked || w_hurt_.w > 0.01f) ? 0.0f : Math::max(w_skid_.w, stop_calm_ - dt / 0.5f);
	const float g = w_ground_.w;

	// Standing about: fidgets, glances at the camera, and an act for a camera that comes close.
	// (Candy landing in the bucket does not end the standing about: the kid goes on looking where it
	// was looking, and only the fidgets wait.)
	const bool moving = !in.on_floor || in.mounted || in.hanging || climbing || rolling || spinning || in.knocked || in.pounding || sf_raw > 0.05f ||
			cast_t_ >= 0.0f || (land_t_ >= 0.0f && land_t_ < 0.4f);
	const bool busy = moving || cele_t_ >= 0.0f;
	idle_t_ = moving ? 0.0f : idle_t_ + dt;
	bool mugging = false;
	if (!busy && in.has_camera && n_[J_HEAD] && n_[J_HEAD]->is_inside_tree() && look_w_ <= 0.0f) {
		const Vector3 to_cam = in.camera_pos - (n_[J_HEAD]->get_global_position() + Vector3(0, 0.2f, 0));
		mugging = to_cam.length() < MUG_RANGE && Vector3(to_cam.x, 0, to_cam.z).normalized().dot(fwd) > 0.35f;
	}
	w_mug_.step(mugging, 0.15f, 0.3f, dt);
	// The act's clock runs on while it fades out, so the head eases back instead of snapping to
	// neutral on the frame the kid starts to move.
	mug_t_ = mugging ? (mug_t_ < 0.0f ? 0.0f : mug_t_ + dt) : (mug_t_ >= 0.0f && w_mug_.w > 0.0f ? mug_t_ + dt : -1.0f);
	// Review aid: with the environment variable TOT_RIG_FIDGET=<kind> that fidget starts half a second
	// into standing still and repeats (to see one in a showcase recording, whatever the camera is doing).
	static const int fidget_review = OS::get_singleton()->has_environment("TOT_RIG_FIDGET") ? int(OS::get_singleton()->get_environment("TOT_RIG_FIDGET").to_int()) : -1;
	if (busy || mugging) {
		fidget_next_ = busy ? idle_t_ + 3.0f : Math::max(3.0f, idle_t_ + 2.5f);
	}
	if (fidget_review >= 0 && fidget_n_ == 0) {
		fidget_next_ = Math::min(fidget_next_, 0.5f);
	}
	if (!busy && !mugging && fidget_t_ < 0.0f && idle_t_ >= fidget_next_) {
		static const int ORDER[kid_pose::FIDGET_KINDS] = { 4, 0, 1, 2, 3 }; // the costume's own first
		fidget_kind_ = fidget_review >= 0 ? fidget_review % kid_pose::FIDGET_KINDS : ORDER[fidget_n_ % kid_pose::FIDGET_KINDS];
		++fidget_n_;
		fidget_t_ = 0.0f;
		if (fidget_kind_ == 4) {
			// The costume's own fidget sets its loose parts going.
			for (Soft &s : soft_) {
				if (s.kind == 0 && s.flatten <= 0.0f) {
					s.d.kick(left * 1.2f + Vector3(0, 0.8f, 0));
				} else if (s.kind == 1 && tune_.costume == KID_CAT) {
					s.d.kick(left * 1.6f + Vector3(0, 0.6f, 0)); // the cat's tail flick (ears flick in step_secondary)
				}
			}
			for (Bob &b : bob_) {
				b.d.kick(left * 1.0f + Vector3(0, 0.6f, 0));
			}
			if (tune_.rattle > 0.0f) {
				rattle_t_ = 0.0f;
			}
		}
	}
	if (fidget_t_ >= 0.0f) {
		fidget_t_ += dt;
		if (fidget_t_ > kid_pose::fidget_length(fidget_kind_, tune_.costume) || (busy && w_fidget_.w <= 0.0f)) {
			fidget_t_ = -1.0f;
			fidget_next_ = idle_t_ + (fidget_review >= 0 ? 0.4f : rng_.range(7.0f, 10.0f));
		}
	}
	w_fidget_.step(fidget_t_ >= 0.0f && !busy, 0.01f, 0.14f, dt);
	const bool glance = !moving && idle_t_ > 0.3f && std::fmod(idle_t_ - 0.3f, 6.5f) < 4.2f;
	w_glance_.step((glance || mugging) && in.has_camera, 0.3f, 0.4f, dt);

	// --- Pose layers.
	KidPoseCtx c;
	c.time = in.time;
	c.gait = gait_;
	c.gait_rate = gait_rate_;
	c.speed = sf * in.run_speed;
	c.sf = sf;
	c.bounce = tune_.bounce;
	c.wings = has_wings();
	c.tail = has_tail();
	c.swish = c.tail && (tune_.tail_swish || tail_style_ == 1);
	c.spread = tune_.glide_spread;
	c.costume = tune_.costume;
	c.parity = jump_parity_;
	c.leg = &leg_;

	PoseBlender<J_COUNT> bl;
	bl.begin();
	Pose p;
	const float wi = 1.0f - remap01(sf, 0.03f, 0.2f), wr = (1.0f - wi) * remap01(sf, 0.5f, 0.8f), ww = (1.0f - wi) * (1.0f - remap01(sf, 0.5f, 0.8f));
	if (g * wi > 0.0f) {
		p = Pose();
		kid_pose::idle(p, c);
		bl.blend(p, g * wi, "idle");
	}
	if (g * ww > 0.0f) {
		p = Pose();
		kid_pose::walk(p, c);
		bl.blend(p, g * ww, "walk");
	}
	if (g * wr > 0.0f) {
		p = Pose();
		kid_pose::run(p, c);
		bl.blend(p, g * wr, "run");
	}
	const float air = 1.0f - g, air_free = air * (1.0f - w_glide_.w);
	if (air_free > 0.0f) {
		const float vy = in.velocity.y;
		const float up = remap01(vy, 0.8f, 4.5f), down = remap01(vy, -0.8f, -5.0f), top = Math::max(0.0f, 1.0f - up - down);
		if (up > 0.0f) {
			p = Pose();
			kid_pose::rise(p, c);
			bl.blend(p, air_free * up, "rise");
		}
		if (top > 0.0f) {
			p = Pose();
			kid_pose::apex(p, c);
			bl.blend(p, air_free * top, "apex");
		}
		if (down > 0.0f) {
			p = Pose();
			kid_pose::fall(p, c);
			bl.blend(p, air_free * down, "fall");
		}
	}
	if (air * w_glide_.w > 0.0f) {
		p = Pose();
		kid_pose::glide(p, c);
		bl.blend(p, air * w_glide_.w, "glide");
	}

	// Full-body overrides, lowest priority first.
	constexpr uint64_t ALL = PoseBlender<J_COUNT>::ALL;
	if (w_mount_.w > 0.0f) {
		p = Pose();
		kid_pose::mounted(p, c);
		bl.override_with(p, w_mount_.w, ALL, "mounted");
	}
	if (w_flap_.w > 0.0f) {
		p = Pose();
		kid_pose::flap(p, c, in.flap_k >= 0.0f ? in.flap_k : 1.0f);
		bl.override_with(p, w_flap_.w, ALL, "flap");
	}
	if (w_hang_.w > 0.0f) {
		hang_sway_.step(0.0f, 1.3f, 0.25f, dt);
		p = Pose();
		kid_pose::hang(p, c, clampf(hang_sway_.x, -0.5f, 0.5f));
		bl.override_with(p, w_hang_.w, ALL, "hang");
	}
	if (w_climb_.w > 0.0f) {
		p = Pose();
		kid_pose::climb(p, c, climbing ? in.climb_k : 1.0f, in.vault);
		bl.override_with(p, w_climb_.w, ALL, "climb");
	}
	if (w_spin_.w > 0.0f) {
		p = Pose();
		kid_pose::spin(p, c, spinning ? in.spin_k : 1.0f);
		bl.override_with(p, w_spin_.w, ALL, "spin");
	}
	if (w_roll_.w > 0.0f) {
		p = Pose();
		kid_pose::roll(p, c, rolling ? in.roll_k : 1.0f);
		bl.override_with(p, w_roll_.w, ALL, "roll");
	}
	if (w_pound_.w > 0.0f) {
		p = Pose();
		kid_pose::pound(p, c, false);
		bl.override_with(p, w_pound_.w, ALL, "pound");
		if (w_stall_.w > 0.0f) {
			p = Pose();
			kid_pose::pound(p, c, true);
			bl.override_with(p, w_pound_.w * w_stall_.w, ALL, "pound_stall");
		}
	}
	if (w_hurt_.w > 0.0f) {
		p = Pose();
		kid_pose::hurt(p, c, Math::max(hurt_t_, 0.0f), hurt_local_);
		bl.override_with(p, w_hurt_.w, ALL, "hurt");
	}

	// Additive layers. One that bends the legs also adds to J_AUX so the rig stands the kid on the result.
	const float calm = 1.0f - Math::max(Math::max(w_roll_.w, w_hurt_.w), Math::max(w_pound_.w, w_spin_.w));
	p = Pose();
	kid_pose::breath(p, c, tune_.sway * (1.0f - 0.6f * saturate(sf * 2.0f)));
	bl.additive(p, calm, ALL, "breath");
	if (launch_t_ >= 0.0f && !grounded_now) {
		p = Pose();
		kid_pose::launch(p, c, launch_t_);
		bl.additive(p, calm, ALL, "launch");
	}
	float land_w = 0.0f;
	if (land_t_ >= 0.0f) {
		p = Pose();
		kid_pose::land(p, c, land_t_, land_s_);
		land_w = g * (1.0f - w_roll_.w);
		bl.additive(p, land_w, ALL, "land");
		land_w *= smooth_pulse(land_t_, 0.05f, 0.03f, 0.32f);
	}
	{
		// After candy lands the bucket arm lifts and, standing still, the head tips toward the bucket.
		// Both are eased levels, held while candy keeps landing. On the move the head stays where it is
		// (the eyes glance down instead) and the arm lifts less.
		const bool on = cele_t_ >= 0.0f && cele_t_ < 0.3f;
		const float still = 1.0f - smoothstep01(remap01(sf, 0.12f, 0.45f));
		const float look = cele_look_.follow(on ? still : 0.0f, on ? 0.09f : 0.16f, dt);
		const float lift = cele_lift_.follow(on ? lerp(0.45f, 1.0f, still) : 0.0f, on ? 0.08f : 0.16f, dt);
		if (look > 0.002f || lift > 0.002f || hop_t_ >= 0.0f) {
			p = Pose();
			kid_pose::celebrate(p, c, look, lift, hop_t_, cele_tier_);
			p[J_AUX].pos.x = hop_t_ >= 0.0f ? smooth_pulse(hop_t_, 0.05f, 0.3f, 0.2f) : 0.0f;
			bl.additive(p, calm, ALL, "celebrate");
		}
	}
	if (w_skid_.w > 0.0f) {
		p = Pose();
		kid_pose::skid(p, c);
		p[J_AUX].pos.x = 1.0f;
		bl.additive(p, w_skid_.w * calm * saturate(decel_ / 30.0f + 0.4f), ALL, "skid");
	}
	if (w_fidget_.w > 0.0f && fidget_t_ >= 0.0f) {
		p = Pose();
		kid_pose::fidget(p, c, fidget_kind_, fidget_t_);
		bl.additive(p, w_fidget_.w, ALL, "fidget");
	}
	if (w_mug_.w > 0.0f && mug_t_ >= 0.0f) {
		p = Pose();
		kid_pose::mug(p, c, std::fmod(mug_t_, kid_pose::MUG_LENGTH));
		bl.additive(p, w_mug_.w, ALL, "mug");
	}
	if (rattle_t_ >= 0.0f && tune_.rattle > 0.0f) {
		p = Pose();
		kid_pose::rattle(p, c, rattle_t_, tune_.rattle);
		bl.additive(p, 1.0f, ALL, "rattle");
	}

	// Secondary motion on the pose: the torso leans into acceleration and overshoots when it stops,
	// the whole kid banks into turns, the head lags the torso and leads the turn.
	{
		const float lp = lean_pitch_.step(clampf(a_fwd * 0.0075f, -0.3f, 0.32f) * g, 3.0f, 0.42f, dt);
		const float bank = -clampf(yaw_rate_ * 0.05f, -0.3f, 0.3f) * saturate(sf) - clampf(a_left * 0.004f, -0.12f, 0.12f) * g;
		const float lr = lean_roll_.step(clampf(bank, -0.35f, 0.35f), 3.0f, 0.5f, dt);
		// The head lags the torso as before (a little better damped, so it settles without a wobble).
		// Its lead into a turn is eased, not taken straight from the turn rate, which jumps whenever
		// the stick changes direction and used to flick the head.
		const float hp = head_pitch_.step(-0.55f * lp, 2.6f, 0.6f, dt);
		const float hr = head_roll_.step(-0.5f * lr, 2.6f, 0.6f, dt);
		const float hy = head_lead_.follow(clampf(yaw_rate_ * 0.03f, -0.2f, 0.2f) * saturate(sf * 2.0f), 0.14f, dt);
		pelvis_.step(0.0f, 3.6f, 0.38f, dt);
		yaw_kick_.step(0.0f, 3.0f, 0.42f, dt);
		p = Pose();
		p[J_ROOT].rot = E(0, 0, lr);
		p[J_BODY].rot = E(lp, 0, 0);
		p[J_HEAD].rot = E(clampf(hp, -0.5f, 0.5f), hy, clampf(hr, -0.35f, 0.35f));
		p[J_ARM_L].rot = E(clampf(lp * 1.3f, -0.5f, 0.5f), 0, 0); // the wand arm drags behind a start and swings through a stop
		p[J_FOREARM_L].rot = E(clampf(-lp * 1.2f, -0.4f, 0.4f), 0, 0); // ... and its forearm a little later still
		p[J_ARM_R].rot = E(clampf(lp * 0.3f, -0.15f, 0.15f), 0, 0);
		bl.additive(p, calm, ALL, "lean");
	}

	// Looking: an explicit target, or the camera now and then while standing about. The eyes get there
	// first; the head follows and takes most of the turn, and the eyes keep what is left.
	{
		Vector3 target;
		float w = 0.0f;
		if (look_w_ > 0.0f) {
			target = look_pos_;
			w = look_w_;
		} else if (w_glance_.w > 0.0f && in.has_camera) {
			target = in.camera_pos;
			w = Math::max(0.8f * w_glance_.w * (1.0f - 0.7f * w_fidget_.w), w_mug_.w);
		}
		w *= calm;
		Vector3 dir(0, 0, 1);
		Vector2 total;
		if (w > 0.0f && n_[J_BODY] && n_[J_HEAD] && n_[J_BODY]->is_inside_tree()) {
			dir = look_dir_local(n_[J_BODY]->get_global_transform(), rest_[J_HEAD].origin + Vector3(0, 0.2f, 0), target);
			const float front = 1.0f - smoothstep01(remap01(std::fabs(std::atan2(dir.x, dir.z)), 2.2f, 2.9f));
			total = look_angles(dir, 1.3f, 0.6f) * (w * front);
		}
		const Vector2 a = look_.step(dir, 0.95f * tune_.head_look, 0.4f * tune_.head_look, w, 0.2f, dt);
		const Vector2 head(a.x * 0.7f, a.y * 0.8f);
		gaze_ = Vector2(clampf(total.x - head.x, -0.4f, 0.4f), clampf(total.y - head.y, -0.32f, 0.32f));
		if (std::fabs(a.x) + std::fabs(a.y) > 1e-4f) {
			p = Pose();
			p[J_HEAD].rot = E(head.y, head.x, 0);
			p[J_BODY].rot = E(0, a.x * 0.12f, 0);
			bl.additive(p, 1.0f, ALL, "look");
		}
	}

	// The wand flick goes on last so nothing moves the arm off the pose the shot left from.
	if (w_cast_.w > 0.0f) {
		p = Pose();
		kid_pose::cast(p, c, Math::max(cast_t_, 0.0f));
		p[J_AUX].pos.x = 1.0f;
		constexpr uint64_t ARM = (uint64_t(1) << J_ARM_L) | (uint64_t(1) << J_FOREARM_L) | (uint64_t(1) << J_HAND_L);
		constexpr uint64_t LEGS = (uint64_t(1) << J_LEG_L) | (uint64_t(1) << J_LEG_R) | (uint64_t(1) << J_SHIN_L) | (uint64_t(1) << J_SHIN_R) | (uint64_t(1) << J_AUX);
		const float in_k = smoothstep01(Math::max(cast_t_, 0.0f) / 0.07f);
		bl.additive(p, w_cast_.w * in_k, ALL & ~ARM & ~LEGS, "cast_body");
		bl.additive(p, w_cast_.w * in_k * g * (1.0f - saturate(sf * 3.0f)), LEGS, "cast_legs"); // the half step only when standing
		bl.override_with(p, w_cast_.w, ARM, "cast");
	}

	Pose out = bl.result();
	debug_ = bl.dominant();

	// How much the rig stands the kid on its lowest sole: as much as the poses ask for (J_AUX), not
	// at all off the ground or in a tumble, and fully through a landing.
	const float off = Math::max(Math::max(w_roll_.w, w_hurt_.w), Math::max(Math::max(w_mount_.w, w_hang_.w), w_climb_.w));
	out[J_AUX].pos.x = Math::max(saturate(out[J_AUX].pos.x) * g * (1.0f - off), land_w);
	// The wand hand on the ledge lip: while hanging, and for the first part of the climb.
	const float hik = in.hanging ? w_hang_.w : (climbing && !in.vault ? 1.0f - smoothstep01((in.climb_k - 0.2f) / 0.25f) : 0.0f);
	hand_ik_ = damp(hand_ik_, hik, 0.025f, dt);
	const float foot_ik = g * (1.0f - off) * (1.0f - smoothstep01((speed - 0.8f) / 1.0f));
	apply(in, out, g * (1.0f - Math::max(w_mount_.w, w_roll_.w)), foot_ik, hand_ik_ > 0.003f ? hand_ik_ : 0.0f);
	last_ = out;
	want_ = face_want(in, sf, busy, mugging, dt);
	step_secondary(in, out);

	// Debug trace (set the environment variable TOT_RIG_DEBUG): where the soles are in the world each
	// frame, to measure foot slide. art/team/characters_candy/batch05/p3_slide.py reads it.
	static const bool trace = OS::get_singleton()->has_environment("TOT_RIG_DEBUG");
	if (trace && root_->is_inside_tree()) {
		String line = vformat("RIGDBG t=%.4f state=%s sf=%.3f gait=%.3f floor=%.4f", in.time, debug_, sf, gait_, float(root_->get_global_position().y - in.step_offset));
		for (int side = 0; side < 2; ++side) {
			Node3D *foot = n_[J_SHIN_L + side] ? n_[J_SHIN_L + side] : n_[J_LEG_L + side];
			for (int k = 0; k < 2 && foot; ++k) {
				const Vector3 w = foot->get_global_transform().xform(sole_[side][k]);
				line += vformat(" %.4f %.4f %.4f", w.x, w.y, w.z);
			}
		}
		// The head relative to the controller's node (pitch, yaw, roll in radians; height in metres),
		// to measure how smoothly it moves. art/team/characters_candy/batch05/head_smooth.py reads it.
		if (n_[J_HEAD] && n_[J_HEAD]->is_inside_tree()) {
			const Transform3D h = root_->get_global_transform().affine_inverse() * n_[J_HEAD]->get_global_transform();
			const Vector3 e = h.basis.orthonormalized().get_euler();
			line += vformat(" HEAD dt=%.5f ev=%d %.5f %.5f %.5f %.5f %.5f", dt, int(ev), e.x, e.y, e.z, float(h.origin.y), float(h.origin.z));
			line += vformat(" W %.5f g=%.2f calm=%.2f", float(n_[J_HEAD]->get_global_position().y), g, calm);
			line += vformat(" TAIL raw=%.4f now=%.4f", tail_clear_raw_, tail_clear_); // lowest point above the floor, last frame
			line += vformat(" HEM raw=%.4f now=%.4f", hem_clear_raw_, hem_clear_); // a hanging sheet's hem, the same
			// The ground guard (roll): the head and what is on it, and the whole kid, above the floor before and after
			// the raise; then the raise itself.
			line += vformat(" TOP raw=%.4f now=%.4f BODY raw=%.4f now=%.4f LIFT=%.4f", top_clear_raw_, top_clear_, body_clear_raw_, body_clear_, roll_lift_);
			line += vformat(" WAND=%.4f TRUE=%.4f", wand_clear_, true_clear_);
			line += vformat(" OF=%d LOWPART=%s", int(in.on_floor),body_low_part_ >= 0 && body_low_part_ < int(probes_.size()) ? String(probes_[body_low_part_].node->get_name()) : String("-"));
			// The first hanging hem's bend as shown (x forward / back, z sideways) and the spring's own state.
			for (const Soft &s : soft_) {
				if (s.reach > 0.0f && s.wave > 0.0f && !s.trail) {
					line += vformat(" SHEET %.4f %.4f %.4f d %.4f %.4f", s.shown.x, s.shown.y, s.shown.z, s.d.bend.x, s.d.bend.z);
					// final fixes 2: how high the hem's front edge (its foremost low point, bend applied) is above the floor
					if (!s.low_pts.empty()) {
						// (and BACK: its rearmost low point, the trailing side)
						Vector3 fp = s.low_pts[0], bp = s.low_pts[0];
						for (const Vector3 &p : s.low_pts) {
							if (p.z > fp.z) {
								fp = p;
							}
							if (p.z < bp.z) {
								bp = p;
							}
						}
						const float ang = s.shown.length();
						auto bent = [&](Vector3 p) {
							if (ang > 1e-4f) {
								float k = saturate((-float(p.y) - s.rigid) / Math::max(s.reach, 0.01f));
								k = k * k * (3.0f - 2.0f * k);
								p = p.rotated(s.shown / ang, ang * k);
							}
							return p;
						};
						const float fl = float(root_->get_global_position().y - root_->get_position().y) + float(in.smooth_offset.y);
						const Transform3D sx = s.node->get_global_transform();
						line += vformat(" FRONT=%.4f BACK=%.4f", float(sx.xform(bent(fp)).y) - fl, float(sx.xform(bent(bp)).y) - fl);
					}
					break;
				}
			}
		}
		UtilityFunctions::print(line);
	}
}

// Writes the pose to the nodes: folds knee and elbow bends into limbs that have no such joint, puts
// the feet on the ground, lays the wand hand on a ledge, and builds the root transform (yaw, spin,
// tumble, squash).
void KidRig::apply(const KidAnimInput &in, Pose &out, float grounded, float foot_ik, float hand_ik) {
	const Quaternion arm_out[2] = { E(0, 0, tune_.arm_out), E(0, 0, -tune_.arm_out) };
	for (int side = 0; side < 2; ++side) {
		const int leg = J_LEG_L + side, shin = J_SHIN_L + side, arm = J_ARM_L + side, fore = J_FOREARM_L + side;
		if (!n_[shin]) {
			const float knee = clampf(hinge_angle(out[shin].rot), 0.0f, 2.4f);
			out[leg].rot = out[leg].rot * quat_axis_angle(Vector3(1, 0, 0), knee * tune_.legacy_knee);
			out[leg].scale.y *= clampf(std::cos(knee * 0.5f), 0.6f, 1.0f);
		}
		if (!n_[fore]) {
			const float elbow = clampf(-hinge_angle(out[fore].rot), 0.0f, 2.4f);
			out[arm].rot = out[arm].rot * quat_axis_angle(Vector3(1, 0, 0), -elbow * tune_.legacy_elbow);
		} else if (tune_.elbow_rest > 0.0f) {
			// The costume's resting elbow bend (the ghost's arms out of their holes). Off for the wand
			// arm while it casts, so the shot leaves from exactly where cast_now() put the wand.
			const float rest = tune_.elbow_rest * (side == 0 ? 1.0f - w_cast_.w : 1.0f);
			const float e = -hinge_angle(out[fore].rot);
			out[fore].rot = out[fore].rot * quat_axis_angle(Vector3(1, 0, 0), -(rest_elbow(e, rest) - e));
		}
		out[arm].rot = arm_out[side] * out[arm].rot;
	}

	// Feet: the pelvis height that puts the lowest sole on the ground.
	float lowest = 1.0e9f;
	for (int side = 0; side < 2; ++side) {
		const int leg = J_LEG_L + side, shin = J_SHIN_L + side;
		if (!n_[leg]) {
			continue;
		}
		Transform3D t = apply_pose(rest_[leg], out[leg]);
		if (n_[shin]) {
			t = t * apply_pose(rest_[shin], out[shin]);
		}
		for (int k = 0; k < 2; ++k) {
			lowest = Math::min(lowest, float(t.xform(sole_[side][k]).y));
		}
	}
	Vector3 pelvis = out[J_BODY].pos;
	const float plant = saturate(out[J_AUX].pos.x);
	if (lowest < 1.0e8f) {
		const float on_ground = clampf(-lowest, -0.16f, 0.07f);
		float y = pelvis.y * (1.0f - plant) + on_ground * plant;
		y = lerp(y, Math::max(y, on_ground), grounded); // never through the floor

		// Standing the kid on its lowest sole frame by frame puts a sharp kink in the body's height at
		// every footfall (it stops falling and starts rising within one frame), which reads as a jolt
		// in the head on top of it. Only the kink is rounded off here: the bounce itself, and the head
		// moving with the body, are left as they are. (Do not steady the head against the body: tried
		// 2026-11-15, the user found it looked as if the body hung from the head.)
		const float dt = safe_dt(in.dt);
		if (!ride_primed_) {
			ride_primed_ = true;
			ride_s_.reset(y);
		}
		y = lerp(y, ride_s_.follow(y, 0.024f, dt), grounded);
		pelvis.y = y + clampf(pelvis_.x, -0.05f, 0.015f) * grounded;
	}
	// Uneven ground (standing or creeping): the pelvis sinks toward the lower foot's floor and the
	// other leg folds, its shoe kept level, to stand on the higher one.
	if (foot_ik > 0.0f) {
		const float low = Math::min(ground_[0], ground_[1]);
		const float drop = clampf(low, -0.08f, 0.0f);
		for (int side = 0; side < 2; ++side) {
			const float lift = clampf(ground_[side] - drop, 0.0f, 0.1f) * foot_ik;
			if (lift < 1e-4f || !n_[J_LEG_L + side]) {
				continue;
			}
			if (n_[J_SHIN_L + side] && leg_.thigh > 0.02f) {
				const float th = std::acos(clampf(1.0f - lift / leg_.thigh, -0.2f, 1.0f));
				out[J_LEG_L + side].rot = out[J_LEG_L + side].rot * quat_axis_angle(Vector3(1, 0, 0), -th);
				out[J_SHIN_L + side].rot = out[J_SHIN_L + side].rot * quat_axis_angle(Vector3(1, 0, 0), th);
			} else {
				out[J_LEG_L + side].pos.y += lift;
			}
		}
		pelvis.y += drop * foot_ik;
	}
	out[J_BODY].pos = pelvis;
	out[J_LEG_L].pos += pelvis;
	out[J_LEG_R].pos += pelvis;

	// The controller's node: yaw (with the spin and its overshoot), the roll's tumble, squash and stretch.
	const float spin_yaw = in.spin_k >= 0.0f ? in.spin_k * TAU : 0.0f;
	const float tumble = in.roll_k >= 0.0f ? in.roll_k * TAU : 0.0f;
	Basis b = Basis(Vector3(0, 1, 0), in.yaw + spin_yaw + yaw_kick_.x) * Basis(Vector3(1, 0, 0), tumble);
	const float s = in.squash;
	b = b.scaled(Vector3(1.0f + s * 0.6f + in.roar, 1.0f - s + in.roar, 1.0f + s * 0.6f + in.roar));
	Vector3 origin = Vector3(0, in.step_offset, 0) + in.smooth_offset;
	if (in.roll_k >= 0.0f) {
		const Vector3 c(0, ROLL_CENTER, 0);
		origin += c - b.xform(c);
	}
	const Transform3D root_xf(b, origin);
	root_->set_transform(root_xf);

	// The model: whole-body tilt about the feet on the ground and about the middle in the air.
	const JointPose &r = out[J_ROOT];
	const Vector3 pivot(0, lerp(AIR_PIVOT, 0.0f, grounded), 0);
	const Basis rb(r.rot, r.scale);
	Transform3D vis_xf(rb, pivot - rb.xform(pivot) + r.pos);
	if (in.on_floor && !in.mounted && !in.hanging && in.roll_k < 0.0f && in.climb_k < 0.0f) {
		// Final fixes (2026-11-16, decision 18): on the floor the soles never go more than 2.5 cm into it. Touching down
		// from a ground pound, the dive's stretch (scale 1.14) still turned about the air pivot (0.45 m) while the grounded
		// weight eased in, and with the landing's pelvis dip it put the shoes 6-8 cm into the road for about five frames
		// (4 cm after a roll; trace: wip_p\guard). Walking stays within the tolerance, so nothing changes there.
		float low = 1.0e9f;
		for (int side = 0; side < 2; ++side) {
			const int leg = J_LEG_L + side, shin = J_SHIN_L + side;
			if (!n_[leg]) {
				continue;
			}
			Transform3D t = vis_xf * apply_pose(rest_[leg], out[leg]);
			if (n_[shin]) {
				t = t * apply_pose(rest_[shin], out[shin]);
			}
			for (int k = 0; k < 2; ++k) {
				low = Math::min(low, float(t.xform(sole_[side][k]).y));
			}
		}
		// (A foot may stand lower than the controller on a kerb: the lower of the two floors under the feet is the floor.)
		const float floor_ref = Math::min(0.0f, Math::min(ground_[0], ground_[1])) - 0.025f;
		if (low < floor_ref && low > floor_ref - 0.5f) {
			vis_xf.origin.y += floor_ref - low;
		}
	}
	visual_->set_transform(vis_xf);

	// The wand hand on the lip of the ledge: a two-bone reach from the shoulder, the fist laid on top.
	Node3D *root_parent = root_->get_parent_node_3d();
	if (hand_ik > 0.0f && n_[J_BODY] && n_[J_ARM_L] && n_[J_FOREARM_L] && n_[J_HAND_L] && n_[J_ARM_L]->get_parent() == n_[J_BODY] && root_parent && root_parent->is_inside_tree()) {
		const Transform3D body_w = root_parent->get_global_transform() * root_xf * vis_xf * apply_pose(rest_[J_BODY], out[J_BODY]);
		const Vector3 n = in.ledge_n, to_wall = -n, side(to_wall.z, 0, -to_wall.x); // side: the kid's left along the wall
		const Vector3 grip = in.ledge + side * 0.17f + Vector3(0, 0.025f, 0) + n * 0.015f;
		const Vector3 target = body_w.affine_inverse().xform(grip);
		const Vector3 shoulder = rest_[J_ARM_L].origin + out[J_ARM_L].pos;
		const Ik2 ik = two_bone_ik(shoulder, rest_[J_FOREARM_L].origin, rest_[J_HAND_L].origin, Vector3(-1, 0, 0), target, shoulder + Vector3(0.25f, -0.4f, -0.15f), 2.4f);
		out[J_ARM_L].rot = slerp_safe(out[J_ARM_L].rot, ik.upper, hand_ik);
		out[J_FOREARM_L].rot = slerp_safe(out[J_FOREARM_L].rot, ik.lower, hand_ik);
		const Quaternion fore_w = basis_quat(body_w.basis) * out[J_ARM_L].rot * out[J_FOREARM_L].rot;
		// The fist lies on the lip pointing along it, away from the face, and a little in over it and down onto it. (It
		// used to point straight in over the lip: the wand, which runs out of the fist almost along it, then went down
		// into the roof and its tip glow, drawn on top, sat on the kid's chin.)
		const Vector3 lay = (side * 0.75f + to_wall * 0.45f - Vector3(0, 0.35f, 0)).normalized();
		const Vector3 lay_f = rotate(fore_w.inverse(), lay);
		Quaternion hand = quat_between(hand_tip_[0].normalized(), lay_f);
		// Turn the fist about that line so the rest of the wand's direction points up, off the roof.
		if (socket_wand_ && socket_wand_->get_parent() == n_[J_HAND_L]) {
			const Vector3 wl = socket_wand_->get_transform().basis.get_column(2).normalized();
			Vector3 cur = rotate(fore_w * hand, wl), want = (Vector3(0, 1, 0) + to_wall * 0.3f).normalized();
			cur -= lay * cur.dot(lay);
			want -= lay * want.dot(lay);
			if (cur.length() > 1e-3f && want.length() > 1e-3f) {
				cur.normalize();
				want.normalize();
				const float a = std::atan2(lay.dot(cur.cross(want)), cur.dot(want));
				hand = quat_axis_angle(lay_f.normalized(), a) * hand;
			}
		}
		out[J_HAND_L].rot = slerp_safe(out[J_HAND_L].rot, hand, hand_ik);
	}

	for (int j = 1; j < J_COUNT; ++j) {
		if (n_[j]) {
			n_[j]->set_transform(apply_pose(rest_[j], out[j]));
		}
	}
}

// The bucket hangs from its grip and swings as a pendulum in world space, held clear of the body.
void KidRig::step_bucket(const KidAnimInput &in, float dt) {
	if (!bucket_ || !bucket_->is_inside_tree()) {
		return;
	}
	Node3D *par = bucket_->get_parent_node_3d();
	if (!par) {
		return;
	}
	const Transform3D pg = par->get_global_transform();
	const Transform3D vg = visual_->get_global_transform();
	const Basis M = vg.basis.orthonormalized(); // model axes in the world
	const Quaternion parent_model = basis_quat(M.inverse() * pg.basis);
	const Quaternion rest_model = basis_quat(bucket_rest_model_);
	const float inward = bucket_left_ ? -1.0f : 1.0f; // a turn about +Z swings the bottom toward +X: inward for a bucket on the right

	const int forced = in.roll_k >= 0.0f || in.mounted ? 2 : (in.spin_k >= 0.0f ? 1 : 0);
	if (forced) {
		bucket_forced_ = forced;
	}
	bucket_force_.step(forced != 0, 0.05f, 0.16f, dt);
	Vector3 bend_model;
	if (forced) {
		bucket_d_.reset();
	} else {
		DangleParams p;
		p.len = 0.2f;
		p.hz = tune_.bucket_hz;
		p.zeta = tune_.bucket_zeta;
		p.drag = 0.12f;
		p.inertia = tune_.bucket_inertia;
		p.limit = 0.95f;
		bucket_d_.step(Transform3D(Basis(), pg.xform(bucket_rest_.origin)), Vector3(0, -1, 0), p, dt);
		const Vector3 m = M.xform_inv(bucket_d_.bend);
		// Forward as far as the costume lets it (below the waist; hardly at all against a wall), back
		// only a little (the leg is there), outward freely, inward hardly at all.
		const float fwd = in.hanging ? 0.06f : tune_.bucket_fwd;
		const Vector3 cl(clampf(m.x, -fwd * 1.6f, 0.4f), 0.0f, clampf(m.z * inward, -0.9f, 0.16f) * inward);
		if ((cl - m).length_squared() > 1e-8f) {
			bucket_d_.bend = M.xform(cl);
			bucket_d_.rate *= 0.6f;
		}
		// It eases into the forward limit rather than hitting it.
		bend_model = Vector3(cl.x < 0.0f ? -fwd * std::tanh(-cl.x / fwd) : cl.x, 0.0f, cl.z);
	}
	Quaternion q = parent_model.inverse() * quat_from_rotvec(bend_model) * rest_model;
	if (bucket_force_.w > 0.0f) {
		const Quaternion flung = parent_model.inverse() * quat_from_rotvec(Vector3(0, 0, -0.85f * inward)) * rest_model;
		const Quaternion held = bucket_forced_ == 1 ? flung : basis_quat(bucket_rest_.basis);
		q = slerp_safe(q, held, bucket_force_.w);
	}
	Vector3 scale(1, 1, 1);
	if (bucket_punch_ > 0.0f) {
		bucket_punch_ = Math::max(0.0f, bucket_punch_ - dt * 4.0f);
		const float k = std::sin(bucket_punch_ * PI * 2.0f) * 0.25f * bucket_punch_;
		scale = Vector3(1.0f + k, 1.0f - k * 0.8f, 1.0f + k);
	}
	bucket_->set_transform(Transform3D(Basis(q, scale), bucket_rest_.origin));
}

void KidRig::step_secondary(const KidAnimInput &in, const Pose &out) {
	(void)out;
	const float dt = safe_dt(in.dt);
	if (!visual_->is_inside_tree()) {
		return;
	}
	step_bucket(in, dt);

	const bool rolling = in.roll_k >= 0.0f, spinning = in.spin_k >= 0.0f;
	const float head_down = rolling ? kid_pose::roll_head_down(in.roll_k) : 0.0f; // rolling with the head pointing down
	const float sf = saturate(sf_.x);
	// Falling lifts everything floppy; rising presses it down.
	const Vector3 updraft(0, clampf(-in.velocity.y * 1.6f, -14.0f, 24.0f) * tune_.float_up, 0);
	const float falling = in.on_floor ? 0.0f : saturate(-in.velocity.y / 9.0f);
	const bool swish = n_[J_TAIL] && (tune_.tail_swish || tail_style_ == 1);
	// For cloth over the body (a ghost's cowl and sheet): how far each arm is raised (0 hanging .. 1
	// overhead) and how far forward the knees reach, from the pose just written.
	float arm_up[2] = { 0.0f, 0.0f }, knee_fwd = 0.0f;
	// How far the body leans back in the pose just written (the skid's brace): a sheet hanging from it tilts with it and
	// its front hem rides up (KidTuning::stop_lean).
	const float body_back = Math::max(0.0f, -float(rotate(out[J_BODY].rot, Vector3(0, 1, 0)).z));
	for (int side = 0; side < 2; ++side) {
		arm_up[side] = saturate((rotate(out[J_ARM_L + side].rot, Vector3(0, -1, 0)).y + 0.6f) / 1.2f);
		knee_fwd = Math::max(knee_fwd, float(rotate(out[J_LEG_L + side].rot, Vector3(0, -1, 0)).z));
	}
	// Ears (soft parts with `flatten`): folded back by a fall, a glide or a roll. The fold is an
	// underdamped level, so when the fall ends they spring forward past upright (the perk; a landing
	// kicks it further, see KID_EV_LAND). The cat's own fidget flicks one ear twice, then the other.
	const float glide_air = !in.on_floor ? w_glide_.w : 0.0f;
	const float fold = ear_fold_.step(Math::max(falling, 0.6f * glide_air) + (rolling ? 0.9f : 0.0f), 2.4f, 0.3f, dt);
	float flick[2] = { 0.0f, 0.0f };
	if (fidget_t_ >= 0.0f && fidget_kind_ == 4 && tune_.costume == KID_CAT) {
		auto twitch = [&](float t0) {
			const float u = (fidget_t_ - t0) / 0.045f; // a snap back, a slightly slower return (about 0.15 s in all)
			return u < -2.5f || u > 5.0f ? 0.0f : (u < 0.0f ? std::exp(-u * u) : std::exp(-u * u * 0.45f));
		};
		flick[0] = Math::min(1.0f, twitch(0.3f) + 0.8f * twitch(0.52f)) * w_fidget_.w;
		flick[1] = twitch(1.0f) * w_fidget_.w;
	}
	hem_clear_raw_ = hem_clear_ = 9.99f; // (trace: not measured this frame)
	for (Soft &s : soft_) {
		if (s.counter) {
			// The hood's part below the neck turns back against the head's pose (rest -> now), so its edge
			// stays where the body sheet is: the crown and the face opening go with the head, the drape
			// over the shoulders stays with the body. Its mesh space is the head's (translation-only child).
			Vector3 rv;
			if (Node3D *h = n_[J_HEAD]) {
				const Basis back = h->get_transform().basis.orthonormalized().inverse() * rest_[J_HEAD].basis.orthonormalized();
				Quaternion q = back.get_rotation_quaternion();
				if (q.w < 0.0f) {
					q = -q;
				}
				const Vector3 im(q.x, q.y, q.z);
				const float sn = im.length();
				if (sn > 1e-6f) {
					rv = im * (2.0f * std::atan2(sn, q.w) / sn);
				}
			}
			s.shown = rv;
			s.g->set_instance_shader_parameter("soft_bend", Vector4(rv.x, rv.y, rv.z, s.rigid));
			continue;
		}
		Vector3 bias;
		if (rolling) {
			// The tail curls up over the back of the ball; a hanging sheet wraps forward round the tucked knees.
			// A cape behind the body streams back over the ball instead (it must not wrap into the body); a short
			// skirt only tucks a little.
			const bool hem = s.wave > 0.0f && s.rigid > 0.0f;
			s.d.freeze(s.kind == 1 ? Vector3(0.75f, 0, 0)
								   : (s.trail ? Vector3(0.45f, 0, 0) : (hem ? Vector3(s.wrap < 0.1f ? -0.3f : -0.5f, 0, 0) : Vector3())));
			s.frame_primed = false;
		} else if (spinning) {
			// Flung out and trailing the turn (a cape streams out behind rather than swinging round into the arms).
			// Cords on the chest fly out sideways and forward.
			s.d.freeze(s.kind == 1 ? Vector3(0.1f, -0.75f, 0)
								   : (s.kind == 2 || s.kind == 3 ? Vector3(0, -0.35f, 0)
																 : (s.trail ? Vector3(0.45f, -0.22f, 0) : (s.front ? Vector3(-0.35f, 0, 0.75f * float(s.side)) : Vector3(0, -0.4f, 0)))));
			s.frame_primed = false;
		} else {
			// The spring feels the pin's movement, with its up-and-down part scaled (a takeoff or a
			// landing should set a heavy tail swinging, not slam it to its limit).
			Transform3D frame = s.node->get_global_transform();
			const Vector3 pin = frame.origin;
			if (!s.frame_primed || (pin - s.pin_prev).length_squared() > 16.0f) {
				s.frame_origin = pin;
				s.frame_primed = true;
			} else {
				const Vector3 d = pin - s.pin_prev;
				s.frame_origin += Vector3(d.x, d.y * s.vertical, d.z);
			}
			s.pin_prev = pin;
			frame.origin = s.frame_origin;
			s.d.step(frame, s.dir, s.p, dt, updraft);
		}
		// A heavy sheet in a stop (the ghost): the halt throws the hem forward, but the shins are in the way. Its spring
		// state may not swing past the stop limit (cloth meeting the legs: the forward speed is lost, it falls back from
		// there), so the hem sways a little instead of lifting to the knees (final fixes 2; peak 0.18 -> about 0.08 rad).
		const bool hanging_hem = s.kind == 0 && s.wave > 0.0f && s.rigid > 0.0f && !s.trail;
		const float stop_hold = hanging_hem && tune_.stop_fwd > 0.0f && !rolling && !spinning ? stop_calm_ : 0.0f;
		if (stop_hold > 0.0f) {
			const float lim = tune_.stop_fwd + (1.0f - stop_hold) * 0.5f;
			if (s.d.bend.x < -lim) {
				s.d.bend.x = -lim;
				s.d.rate.x = Math::max(float(s.d.rate.x), 0.0f);
			}
		}
		if (s.kind == 1 && !rolling && !spinning) {
			// The tip trails the root's wag (the root's yaw is in the pose): one smooth curve, not a stick.
			if (swish) {
				bias = Vector3(0.05f * std::sin(in.time * 0.9f), 0, 0);
			} else {
				const float idle = 0.28f * std::sin(in.time * 1.7f - 1.1f) * (1.0f - sf);
				const float gait = 0.3f * std::cos(gait_ - 2.0f) * sf;
				bias = Vector3(0.06f * std::sin(in.time * 1.1f - 0.8f), -(idle + gait) * tune_.tail_wag, 0);
			}
		} else if ((s.kind == 2 || s.kind == 3) && in.gliding && !in.on_floor) {
			const float f = 0.1f * std::sin(in.time * 21.0f + (s.kind == 3 ? 0.9f : 0.0f));
			bias = Vector3(0, 0, s.kind == 2 ? f : -f); // the membrane's tip flutters
		} else if (s.kind == 0 && tune_.costume == KID_PUMPKIN && fidget_t_ >= 0.0f && fidget_kind_ == 4 && !rolling && !spinning) {
			// The pumpkin's fidget: the stem and the tendrils whip from side to side a beat behind the head's wobble.
			bias = Vector3(0, 0, -0.5f * kid_pose::pumpkin_wobble(fidget_t_ - 0.09f) * w_fidget_.w);
		} else if (s.kind == 0 && s.flatten > 0.0f) {
			// Ears: the fold (back in a fall, forward in the perk) and the flick (back and out).
			const float f = s.side > 0 ? flick[0] : (s.side < 0 ? flick[1] : 0.0f);
			bias = Vector3(-s.flatten * fold - 0.42f * f, 0, -float(s.side) * 0.22f * f);
		} else if (s.kind == 0 && s.arms > 0.0f && s.wave <= 0.0f && !rolling) {
			bias = Vector3(0, 0, 0.32f * s.arms * (arm_up[0] - arm_up[1])); // a raised arm lifts its side of a capelet
		} else if (s.kind == 0 && s.wave > 0.0f && s.rigid > 0.0f && !s.trail && !rolling && !spinning) {
			// a hanging sheet is pushed forward a little by raised knees (in a stop only stop_knee of it: the halt's big step)
			// (while the hem still trails, the push may go on taking that trail back, so the hem does not flick back first)
			const float push = 0.16f * saturate((knee_fwd - 0.5f) / 0.4f);
			const float trail = Math::max(0.0f, float(soft_limit(s.d.bend, s.limit).x));
			const float held = Math::max(push * tune_.stop_knee, Math::min(push, trail));
			// and the body's lean back is taken out of the sheet, so it goes on hanging down rather than tilting with the brace
			bias = Vector3(-Math::lerp(push, held, stop_hold) + stop_hold * body_back * tune_.stop_lean, 0, 0);
		} else if (s.kind == 0 && s.front && !rolling && !spinning) {
			// The skeleton's cords: a rattle shakes them (a quick jiggle with the limbs), and its shimmy fidget
			// swings them from side to side together (kid_pose::skeleton_shimmy).
			const float rat = rattle_t_ >= 0.0f ? (1.0f - smoothstep01(rattle_t_ / kid_pose::RATTLE_LENGTH)) * tune_.rattle : 0.0f;
			const float shim = fidget_t_ >= 0.0f && fidget_kind_ == 4 && tune_.costume == KID_SKELETON ? kid_pose::skeleton_shimmy(fidget_t_) * w_fidget_.w : 0.0f;
			const float jig = std::sin(in.time * 41.0f + (s.side > 0 ? 0.0f : 1.9f));
			bias = Vector3(-0.1f * rat * std::fabs(jig) - 0.12f * shim, 0, 0.16f * rat * jig + 0.38f * shim * std::sin(fidget_t_ * TAU * 2.25f - 0.6f));
		}
		if (head_down > 0.0f && s.on_head && s.reach <= 0.0f) {
			// Rolling head-down: whatever stands up off the head (ears, the hat's tip, the stem, the tendrils) folds back
			// flat against it, so it does not stick out of the ball into the road (decision 18).
			bias.x -= 0.6f * head_down;
		}
		Vector3 bend = soft_limit(s.d.bend, s.limit) + bias;
		if (stop_hold > 0.0f && bend.x < 0.0f) {
			// ... and what is shown (swing + knee push) eases into the same limit while the stop holds.
			const float lim = tune_.stop_fwd * 1.25f;
			const float x = -float(bend.x);
			bend.x = -Math::lerp(x, lim * std::tanh(x / lim), stop_hold);
		}
		if (s.trail) {
			// A cape behind the back: it may stream back and sway, never swing forward into the body or round into the arms.
			// Polish (2026-11-15): in the run its sideways swing (up to 0.3) read as a flap from the three-quarter front
			// camera; it now streams back with speed and sways sideways only a little.
			if (!rolling && !spinning) {
				bend.x += 0.22f * sf;
				bend.z = clampf(bend.z * (1.0f - 0.6f * sf), -0.3f + 0.2f * sf, 0.3f - 0.2f * sf);
			}
			bend.x = Math::max(float(bend.x), -0.05f);
			bend.z = clampf(bend.z, -0.3f, 0.3f);
		}
		if (s.front) {
			// A cord lying on the chest: out in front and to the sides, never back into the body.
			bend.x = Math::min(float(bend.x), 0.03f);
		}
		if (s.kind == 1 && !s.low_pts.empty() && in.on_floor && !rolling && !spinning && !in.mounted) {
			// On the ground a tail rests on it; it never goes through (stopping from a run used to push it under).
			const float floor_y = float(root_->get_global_position().y) - in.step_offset;
			bend = keep_tail_above_ground(s.low_pts, s.node->get_global_transform(), bend, s.len, s.rigid, floor_y, &tail_clear_raw_, &tail_clear_);
		} else if (s.kind == 0 && s.reach > 0.0f && !s.low_pts.empty() && !in.mounted && (in.on_floor || rolling)) {
			// A hanging sheet's hem rests on the ground too (crouches, landings, the roll): it never goes under it.
			// The floor is the controller's own height (the roll moves this node about its middle).
			const float floor_y = float(root_->get_global_position().y - root_->get_position().y) + float(in.smooth_offset.y);
			bend = keep_tail_above_ground(s.low_pts, s.node->get_global_transform(), bend, s.reach, s.rigid, floor_y, &hem_clear_raw_, &hem_clear_, true);
		}
		s.shown = bend;
		s.g->set_instance_shader_parameter("soft_bend", Vector4(bend.x, bend.y, bend.z, s.rigid));
		if (s.kind == 1) {
			// The cat's swish: a sideways wave travelling from the root to the tip (as before), on top of the bend.
			Vector4 wave;
			if (swish) {
				const float len = s.len / 0.9f;
				const float rate = 2.6f + 1.2f * sf;
				const float flick = std::pow(Math::max(0.0f, std::sin(in.time * 0.7f)), 12.0f);
				// About one full S along the tail, so the undulation reads as a wave running to the tip.
				const float amp = (0.05f + 0.022f * sf) * (1.0f + 0.8f * flick) * (rolling ? 0.0f : 1.0f);
				wave = Vector4(amp, in.time * rate, 0.95f * TAU / len, len);
			}
			s.g->set_instance_shader_parameter("tail_wave", wave);
		} else if (s.kind == 0 && (s.wave > 0.0f || s.balloon > 0.0f)) {
			// A hem: it ripples as the kid moves; a fall puffs the whole thing out instead, and a glide
			// fills it like a parachute that breathes (no lobes at a quarter turn of phase is an even push).
			// Negative lobes: pushed out from the mesh's Y axis, so the inside wall moves with the outside.
			const float glide = !in.on_floor ? w_glide_.w : 0.0f;
			// Arms raised under a sheet push it out too (presets with `arms`; the ghost's sheet has arm holes,
			// so its arms do not touch the cloth: its preset has arms 0).
			const float arms_up = s.arms * 0.55f * smoothstep01(saturate((Math::max(arm_up[0], arm_up[1]) - 0.3f) / 0.5f));
			// A spin flies the hem out (tune_.spin_flare: the witch's skirt and cape), breathing a little.
			const float flare = tune_.spin_flare * w_spin_.w * (0.9f + 0.1f * std::sin(in.time * 9.0f));
			const float puff = Math::max(Math::max(Math::max(saturate(falling * 2.0f - 1.0f), glide * (0.82f + 0.18f * std::sin(in.time * 3.3f))), arms_up), flare);
			const float ripple = rolling ? 0.0f : s.wave * (0.45f + 0.55f * sf) * saturate(1.0f - falling * 2.0f) * (1.0f - glide) * (1.0f - w_spin_.w);
			Vector4 wave;
			if (w_roll_.w > 0.01f && s.rigid > 0.0f && s.wrap > 0.0f) {
				// Curled into a ball: the hem is drawn in round the tucked knees instead of standing open.
				wave = Vector4(-s.wrap * w_roll_.w, PI * 0.5f, -1.0e-4f, s.wave_top);
			} else if (s.balloon * puff > ripple) {
				wave = Vector4(s.balloon * puff, PI * 0.5f, -1.0e-4f, s.wave_top);
			} else if (ripple > 0.0f) {
				wave = Vector4(ripple, in.time * (2.4f + 3.0f * sf), -s.lobes, s.wave_top);
			}
			s.g->set_instance_shader_parameter("cloth_wave", wave);
		} else if (s.kind == 0 && s.arms > 0.0f) {
			// A cowl puffs open when the arms go up under it (and a little in a fall or a glide).
			const float open = rolling ? 0.0f : Math::max(Math::max(arm_up[0], arm_up[1]) * 0.045f, saturate(falling * 2.0f - 1.0f) * 0.03f + (in.on_floor ? 0.0f : w_glide_.w * 0.02f));
			s.g->set_instance_shader_parameter("cloth_wave", open > 1e-4f ? Vector4(open * s.arms, PI * 0.5f, -1.0e-4f, s.wave_top) : Vector4());
		}
	}
	for (Bob &b : bob_) {
		Node3D *par = b.node->get_parent_node_3d();
		if (!par) {
			continue;
		}
		if (rolling || spinning) {
			b.d.freeze(Vector3());
		} else {
			b.d.step(par->get_global_transform() * b.rest, b.dir, b.p, dt, updraft * 0.5f);
		}
		if (b.surface) {
			// A leaf on the cap lifts off it but never sinks into it (a little give for the overshoot).
			const float k = b.d.bend.dot(b.away);
			if (k < -0.04f) {
				b.d.bend -= b.away * (k + 0.04f);
				const float r = b.d.rate.dot(b.away);
				if (r < 0.0f) {
					b.d.rate -= b.away * (1.4f * r); // a soft bounce off the surface
				}
			}
		}
		// A hat lifts off its seat in a fall and drops back with a bounce (pressing on for a moment as it lands).
		constexpr float HAT_LIFT_MAX = 0.02f; // never more than 2 cm: the hair under the brim must not show a gap
		const float up = b.lift > 0.0f ? clampf(b.up.step(b.lift * falling * tune_.float_up, 3.0f, 0.4f, dt), -0.006f, HAT_LIFT_MAX) : 0.0f;
		Quaternion q = quat_from_rotvec(b.d.bend);
		if (b.hat && fidget_t_ >= 0.0f && fidget_kind_ == 4 && tune_.costume == KID_WITCH) {
			// The witch's fidget: the hat slips down over her left eye and forward, and is pushed back straight.
			q = quat_from_rotvec(Vector3(0.16f, 0.06f, -0.24f) * (kid_pose::witch_hat_askew(fidget_t_) * w_fidget_.w)) * q;
		}
		if (b.hat && land_t_ >= 0.0f && land_s_ > 1.2f) {
			// Final fixes 3: the ground pound's slam crouch bows the head and the wide brim hid the whole face from the high
			// camera for ~12 frames; the hat tips back with the crouch so the eyes stay in view (only the pound lands > 1.1).
			// (A chin lift against the crouch, -0.12 x the land pose's late pulse, was tried as well: no more face shown
			// than the hat alone, so the head was left as it was.)
			q = quat_from_rotvec(Vector3(-0.3f * smooth_pulse(land_t_, 0.05f, 0.1f, 0.3f), 0, 0)) * q;
		}
		Basis sq;
		if (b.hat && head_down > 0.0f) {
			// Rolling head-down, the tall felt hat squashes down onto its seat (and spreads a little), as a soft hat
			// rolled over would; it springs back up as the head comes round (decision 18: never into the road).
			sq = Basis().scaled(Vector3(1.0f + 0.12f * head_down, 1.0f - 0.5f * head_down, 1.0f + 0.12f * head_down));
		}
		b.node->set_transform(Transform3D(b.rest.basis * Basis(q) * sq, b.rest.origin + b.rest.basis.xform(b.dir * up)));
	}
	ground_guard(in, dt);
	face_.step(want_, dt);
}

// Points on every mesh under the visual, one per 1.5 cm cell of its mesh space (the wand, attached later by the
// controller, is added when ground_guard() sees socket_wand's children change: probes are collected again).
void KidRig::collect_probes() {
	probes_.clear();
	constexpr float CELL = 0.015f;
	std::vector<Node *> stack = { visual_ };
	while (!stack.empty()) {
		Node *n = stack.back();
		stack.pop_back();
		for (int i = 0; i < n->get_child_count(); ++i) {
			stack.push_back(n->get_child(i));
		}
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n);
		if (!mi || mi->get_mesh().is_null()) {
			continue;
		}
		Probe pr;
		pr.node = mi;
		pr.top = n_[J_HEAD] && (mi == n_[J_HEAD] || n_[J_HEAD]->is_ancestor_of(mi));
		pr.wand = socket_wand_ && socket_wand_->is_ancestor_of(mi);
		for (int i = 0; i < int(soft_.size()); ++i) {
			if (soft_[i].node == mi) {
				pr.soft = i;
			}
		}
		std::unordered_set<int64_t> seen;
		const Ref<Mesh> mesh = mi->get_mesh();
		for (int si = 0; si < mesh->get_surface_count(); ++si) {
			const Array arrays = mesh->surface_get_arrays(si);
			if (arrays.size() <= Mesh::ARRAY_VERTEX) {
				continue;
			}
			const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
			for (int vi = 0; vi < verts.size(); ++vi) {
				const Vector3 v = verts[vi];
				const int64_t ix = int64_t(std::floor(v.x / CELL)) & 0xfffff, iy = int64_t(std::floor(v.y / CELL)) & 0xfffff,
							  iz = int64_t(std::floor(v.z / CELL)) & 0xfffff;
				if (seen.insert((ix << 40) | (iy << 20) | iz).second) {
					pr.pts.push_back(v);
				}
			}
		}
		if (!pr.pts.empty()) {
			probes_.push_back(std::move(pr));
		}
	}
	probe_wand_n_ = socket_wand_ ? socket_wand_->get_child_count() : -1;
}

// The lowest world height of the kid's visible meshes (or of the head and what is on it), following the shader's
// soft bend of each soft part as last set (as in toon.gdshader; the cloth ripple is left out).
float KidRig::lowest_point(bool top_only, int *which, bool wand_only, Vector3 *at) const {
	float lo = 1.0e9f;
	if (which) {
		*which = -1;
	}
	for (int pi = 0; pi < int(probes_.size()); ++pi) {
		const Probe &pr = probes_[pi];
		if ((top_only && !pr.top) || (wand_only && !pr.wand) || !pr.node->is_visible_in_tree()) {
			continue;
		}
		const Transform3D xf = pr.node->get_global_transform();
		const Vector3 row(xf.basis.rows[1]);
		const float oy = float(xf.origin.y);
		float ang = 0.0f, rigid = 0.0f, reach = 1.0f;
		Vector3 ax, along_dir;
		if (pr.soft >= 0) {
			const Soft &s = soft_[pr.soft];
			ang = s.shown.length();
			if (ang > 1e-4f) {
				ax = s.shown / ang;
				along_dir = s.pin_axis.length_squared() > 1e-6f ? s.pin_axis.normalized() : Vector3();
				rigid = s.rigid;
				reach = Math::max(s.pin_w, 1e-3f);
			} else {
				ang = 0.0f;
			}
		}
		const bool by_dir = along_dir.length_squared() > 0.5f;
		for (const Vector3 &p : pr.pts) {
			Vector3 q = p;
			if (ang > 0.0f) {
				float k = saturate(((by_dir ? float(p.dot(along_dir)) : float(p.length())) - rigid) / reach);
				k = k * k * (3.0f - 2.0f * k);
				if (k > 0.0f) {
					q = p.rotated(ax, ang * k);
				}
			}
			const float y = float(row.dot(q)) + oy;
			if (y < lo) {
				lo = y;
				if (which) {
					*which = pi;
				}
				if (at) {
					*at = xf.xform(q);
				}
			}
		}
	}
	return lo;
}

// Decision 18 for the roll: the kid tumbles round a fixed centre, and whatever sticks out of the ball (the witch's
// hat brim, the cat's ear tips, the dragon's horns, a hood, the bucket) used to go through the road. While rolling,
// the whole visual is raised by just as much as its lowest point (bends included) is under the floor, like a ball
// rolling over its own hat brim; the raise eases back out after the roll. Visual only: the body does not move.
void KidRig::ground_guard(const KidAnimInput &in, float dt) {
	static const bool trace = OS::get_singleton()->has_environment("TOT_RIG_DEBUG");
	top_clear_raw_ = top_clear_ = body_clear_raw_ = body_clear_ = wand_clear_ = 9.99f;
	if (socket_wand_ && socket_wand_->get_child_count() != probe_wand_n_) {
		collect_probes(); // the controller has attached (or swapped) the wand since the last collection: the ball rolls over it too
	}
	const bool rolling = in.roll_k >= 0.0f && !in.mounted && !in.hanging;
	if (probes_.empty() || !root_->is_inside_tree() || (!rolling && roll_lift_ <= 0.0f && !trace)) {
		roll_lift_ = 0.0f;
		return;
	}
	constexpr float MARGIN = 0.005f;
	// The floor is the controller's own height, as for the hem (the roll moves this node about its middle).
	const float floor_y = float(root_->get_global_position().y - root_->get_position().y) + float(in.smooth_offset.y);
	Vector3 low_at;
	const float low = lowest_point(false, &body_low_part_, false, &low_at);
	const float need = rolling ? Math::max(0.0f, floor_y + MARGIN - low) : 0.0f;
	// While rolling it follows the geometry both ways (continuous through the tumble): the lowest point stays on the floor.
	// Final fixes 2: it used to ease down (0.05 s) as soon as less was needed, so as the curled-up ball came up out of the
	// head-down part of the tumble the kid floated up to 10 cm over the road for about five frames (real play, a roll
	// off a kerb: wip_p\real\rolls_ff2_dragon_kid.log, t 1.60-1.70). Eased out only after the roll.
	roll_lift_ = (rolling || need >= roll_lift_) ? need : damp(roll_lift_, need, 0.05f, dt);
	if (roll_lift_ < 1e-4f) {
		roll_lift_ = 0.0f;
	}
	if (trace) {
		top_clear_raw_ = lowest_point(true) - floor_y;
		top_clear_ = top_clear_raw_ + roll_lift_;
		body_clear_raw_ = low - floor_y;
		body_clear_ = body_clear_raw_ + roll_lift_;
		const float w = lowest_point(false, nullptr, true);
		wand_clear_ = w < 1.0e8f ? w - floor_y + roll_lift_ : 9.99f;
		// The real ground under the lowest point (a kerb edge, a slope), not the controller's own height: does the raise
		// lift the ball off the ground there, or leave it in the ground? (rolling only)
		true_clear_ = 9.99f;
		CollisionObject3D *body = Object::cast_to<CollisionObject3D>(root_->get_parent());
		if (rolling && body && root_->get_world_3d().is_valid()) {
			if (PhysicsDirectSpaceState3D *space = root_->get_world_3d()->get_direct_space_state()) {
				TypedArray<RID> self;
				self.push_back(body->get_rid());
				Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(low_at + Vector3(0, 1.0f, 0), low_at - Vector3(0, 1.5f, 0), 1);
				q->set_exclude(self);
				const Dictionary hit = space->intersect_ray(q);
				if (!hit.is_empty()) {
					true_clear_ = low + roll_lift_ - float(Vector3(hit["position"]).y);
				}
			}
		}
	}
	if (roll_lift_ > 0.0f) {
		Transform3D xf = root_->get_transform();
		xf.origin.y += roll_lift_;
		root_->set_transform(xf);
	}
}

// The shot leaves the wand's tip on the frame it is cast, before the next update(): put the whole
// wand arm (shoulder, elbow, wrist) on the cast pose now. update() then starts the flick from
// exactly this pose.
void KidRig::cast_now() {
	if (!visual_) {
		return;
	}
	cast_t_ = 0.0f;
	w_cast_.snap(true);
	JointPose arm;
	arm.rot = E(0, 0, tune_.arm_out) * quat_from_euler(kid_pose::CAST_ARM);
	last_[J_ARM_L] = arm;
	last_[J_FOREARM_L] = JointPose();
	last_[J_HAND_L] = JointPose();
	for (int j : { int(J_ARM_L), int(J_FOREARM_L), int(J_HAND_L) }) {
		if (n_[j]) {
			n_[j]->set_transform(apply_pose(rest_[j], last_[j]));
		}
	}
}

} // namespace tot
