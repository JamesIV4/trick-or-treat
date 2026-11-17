#include "gen/asset_library.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

#include "core/consts.h"
#include "mesh/mesh_builder.h"

using namespace godot;

namespace tot {

static std::string to_std(const String &s) {
	return std::string(s.utf8().get_data());
}

void apply_material_recursive(Node *node, const Ref<Material> &mat) {
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node)) {
		mi->set_material_override(mat);
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		apply_material_recursive(node->get_child(i), mat);
	}
}

// "alt_" nodes are alternates stacked on the same spot (alt_mouth_open, alt_lash_l ...): only an
// animator that knows them shows one at a time. Everything else sees the asset without them:
// instantiate() hides them, and the mesh bakes (merged_mesh, part_mesh) leave them out.
bool is_alternate(const Node *n) {
	return String(n->get_name()).begins_with("alt_");
}

void hide_alternates(Node *n) {
	if (is_alternate(n)) {
		if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
			n3->set_visible(false);
		}
		return; // hiding a node hides everything under it
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		hide_alternates(n->get_child(i));
	}
}

void AssetLibrary::load(const String &catalog_path, const Ref<Material> &material) {
	material_ = material;
	assets_.clear();
	published_ = 0;
	if (!FileAccess::file_exists(catalog_path)) {
		UtilityFunctions::push_warning("Trick or Treat: no published catalog at ", catalog_path, "; using placeholders.");
		return;
	}
	Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(catalog_path));
	if (parsed.get_type() != Variant::DICTIONARY) {
		UtilityFunctions::push_error("Trick or Treat: catalog is not a JSON object: ", catalog_path);
		return;
	}
	Array list = Dictionary(parsed).get("assets", Array());
	for (int i = 0; i < list.size(); ++i) {
		Dictionary d = list[i];
		AssetInfo a;
		a.id = to_std(d.get("id", ""));
		a.path = String("res://assets/models/") + String(d.get("path", "")) + ".gltf";
		a.collision = to_std(d.get("collision", "none"));
		a.walkable = bool(d.get("walkable", false));
		a.radius = float(d.get("radius", 0.2));
		a.height = float(d.get("height", 2.0));
		a.collision_top = float(d.get("collision_top", -1.0));
		a.bounce = float(d.get("bounce", 0.0));
		const Array hb = d.get("hood_aabb", Array());
		if (hb.size() == 6) {
			const Vector3 lo = Vector3(hb[0], hb[1], hb[2]), hi = Vector3(hb[3], hb[4], hb[5]);
			a.hood = AABB(lo, hi - lo);
			a.has_hood = true;
		}
		a.candy_tier = int(d.get("candy_tier", -1));
		if (d.has("glow")) {
			a.glow = Color::html(String(d["glow"]));
		}
		a.published = bool(d.get("published", true));
		assets_[a.id] = a;
	}
	for (auto &kv : assets_) {
		prepare(kv.second);
	}
}

static void walk_bounds(Node *n, const Transform3D &parent, AssetInfo &a, bool &first) {
	Transform3D xf = parent;
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		xf = parent * n3->get_transform();
		String name = n3->get_name();
		if (name.begins_with("socket_")) {
			a.sockets[to_std(name.substr(7))] = xf;
		}
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
		if (mi->get_mesh().is_valid()) {
			AABB box = xf.xform(mi->get_aabb());
			a.aabb = first ? box : a.aabb.merge(box);
			first = false;
			if (a.first_mesh.is_null()) {
				a.first_mesh = mi->get_mesh();
			}
		}
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		walk_bounds(n->get_child(i), xf, a, first);
	}
}

void AssetLibrary::prepare(AssetInfo &a) {
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (a.published && rl->exists(a.path)) {
		a.scene = rl->load(a.path);
	}
	if (a.scene.is_valid()) {
		Node *inst = a.scene->instantiate();
		bool first = true;
		walk_bounds(inst, Transform3D(), a, first);
		memdelete(inst);
		++published_;
		return;
	}
	a.scene.unref();
	a.placeholder = make_placeholder(a.id);
	a.first_mesh = a.placeholder;
	a.aabb = a.placeholder->get_aabb();
	if (a.id.find("lantern") != std::string::npos || a.id == "lamp_post") {
		a.sockets["light"] = Transform3D(Basis(), Vector3(0, a.aabb.position.y + a.aabb.size.y * 0.8f, 0));
	}
	if (a.id.rfind("tree_", 0) == 0) {
		a.sockets["branch"] = Transform3D(Basis(), Vector3(0.9f, 2.6f, 0));
	}
}

std::vector<std::string> AssetLibrary::ids() const {
	std::vector<std::string> out;
	for (const auto &kv : assets_) {
		out.push_back(kv.first);
	}
	std::sort(out.begin(), out.end());
	return out;
}

const AssetInfo *AssetLibrary::get(const std::string &id) {
	auto it = assets_.find(id);
	if (it == assets_.end()) {
		AssetInfo a;
		a.id = id;
		a.published = false;
		prepare(a);
		it = assets_.emplace(id, a).first;
	}
	return &it->second;
}

Node3D *AssetLibrary::instantiate(const std::string &id) {
	const AssetInfo *a = get(id);
	Node3D *root = nullptr;
	if (a->scene.is_valid()) {
		root = Object::cast_to<Node3D>(a->scene->instantiate());
	}
	if (root == nullptr) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(a->placeholder);
		root = mi;
	}
	root->set_name(String(id.c_str()));
	apply_material_recursive(root, material_);
	hide_alternates(root);
	return root;
}

Ref<Mesh> AssetLibrary::mesh(const std::string &id) {
	return get(id)->first_mesh;
}

static void merge_into(Node *n, const Transform3D &parent, Ref<SurfaceTool> &st, bool &any) {
	if (is_alternate(n)) {
		return; // stacked alternates (mouth shapes, closed-eye lashes) never go into a baked mesh
	}
	Transform3D xf = parent;
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		xf = parent * n3->get_transform();
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
		Ref<Mesh> m = mi->get_mesh();
		if (m.is_valid()) {
			for (int s = 0; s < m->get_surface_count(); ++s) {
				st->append_from(m, s, xf);
				any = true;
			}
		}
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		merge_into(n->get_child(i), xf, st, any);
	}
}

Ref<Mesh> AssetLibrary::merged_mesh(const std::string &id) {
	const AssetInfo *a = get(id);
	if (a->scene.is_null()) {
		return a->placeholder;
	}
	Node *inst = a->scene->instantiate();
	Ref<SurfaceTool> st;
	st.instantiate();
	bool any = false;
	merge_into(inst, Transform3D(), st, any);
	memdelete(inst);
	if (!any) {
		return a->first_mesh;
	}
	return st->commit();
}

static Node3D *find_part(Node *n, const String &name) {
	if (n->get_name() == StringName(name)) {
		return Object::cast_to<Node3D>(n);
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		if (Node3D *r = find_part(n->get_child(i), name)) {
			return r;
		}
	}
	return nullptr;
}

Ref<Mesh> AssetLibrary::part_mesh(const std::string &id, const String &part, Vector3 *origin) {
	const AssetInfo *a = get(id);
	if (a->scene.is_null()) {
		return Ref<Mesh>();
	}
	Node *inst = a->scene->instantiate();
	Ref<Mesh> out;
	if (Node3D *p = find_part(inst, part)) {
		// The parent's transform in the asset's space; the part's own origin is its pivot.
		Transform3D parent;
		for (Node *q = p->get_parent(); q && q != inst; q = q->get_parent()) {
			if (Node3D *q3 = Object::cast_to<Node3D>(q)) {
				parent = q3->get_transform() * parent;
			}
		}
		const Vector3 pivot = (parent * p->get_transform()).origin;
		Ref<SurfaceTool> st;
		st.instantiate();
		bool any = false;
		merge_into(p, Transform3D(parent.basis, parent.origin - pivot), st, any);
		if (any) {
			out = st->commit();
			if (origin) {
				*origin = pivot;
			}
		}
	}
	memdelete(inst);
	return out;
}

// part_tree: one RigPart per named node. `carry` is the transform of the containers (the scene root,
// the asset's root empty) between the parent part and this node.
static void rig_walk(Node *n, int parent, const Transform3D &carry, std::vector<RigPart> &out, std::vector<RigSocket> *sockets) {
	int self = parent;
	Transform3D next = carry;
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		const String name = n3->get_name();
		const Transform3D local = carry * n3->get_transform();
		if (name.begins_with("socket_")) {
			if (sockets) {
				RigSocket s;
				s.name = StringName(name.substr(7));
				s.parent = parent;
				s.local = local;
				sockets->push_back(s);
			}
			return; // a socket is a point, not a joint: nothing under it is rigged
		}
		const MeshInstance3D *as_mesh = Object::cast_to<MeshInstance3D>(n);
		if (parent < 0 && name != "body" && !(as_mesh && as_mesh->get_mesh().is_valid())) {
			// A meshless node above every part is a container (the importer's scene root, the asset's
			// own root empty), not a joint: only its transform is carried down.
			for (int i = 0; i < n->get_child_count(); ++i) {
				rig_walk(n->get_child(i), -1, local, out, sockets);
			}
			return;
		}
		RigPart p;
		p.name = n3->get_name();
		p.parent = parent;
		p.rest = local;
		const Vector3 sc = local.basis.get_scale();
		if (std::abs(sc.x - 1.0f) > 0.01f || std::abs(sc.y - 1.0f) > 0.01f || std::abs(sc.z - 1.0f) > 0.01f) {
			UtilityFunctions::push_warning("Trick or Treat: rig part '", name, "' has a rest scale (", sc, "); rigs allow rest rotation only.");
		}
		p.rest.basis = local.basis.orthonormalized();
		if (is_alternate(n)) {
			p.flags |= RigPart::HIDDEN;
		}
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
			const Ref<Mesh> m = mi->get_mesh();
			if (m.is_valid() && m->get_surface_count() > 0) {
				// Baked through a SurfaceTool like part_mesh, so parts render exactly as the baked
				// monsters always have (no importer LODs or shadow meshes).
				Ref<SurfaceTool> st;
				st.instantiate();
				float far2 = 0.0f;
				for (int s = 0; s < m->get_surface_count(); ++s) {
					st->append_from(m, s, Transform3D());
					const Array arrays = m->surface_get_arrays(s);
					const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
					for (int v = 0; v < verts.size(); ++v) {
						const float d2 = verts[v].length_squared();
						if (d2 > far2) {
							far2 = d2;
							p.tip = verts[v];
						}
					}
				}
				p.mesh = st->commit();
				p.reach = std::sqrt(far2);
				if (p.reach < 0.1f) {
					p.flags |= RigPart::TINY;
				}
			}
		}
		for (const RigPart &q : out) {
			if (q.name == p.name) {
				UtilityFunctions::push_warning("Trick or Treat: rig part name '", name, "' is used twice; slot lookups find the first.");
			}
		}
		out.push_back(p);
		self = int(out.size()) - 1;
		next = Transform3D();
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		rig_walk(n->get_child(i), self, next, out, sockets);
	}
}

int AssetLibrary::part_tree(const std::string &id, std::vector<RigPart> &out, std::vector<RigSocket> *sockets) {
	out.clear();
	if (sockets) {
		sockets->clear();
	}
	const AssetInfo *a = get(id);
	if (a->scene.is_null()) {
		return 0;
	}
	Node *inst = a->scene->instantiate();
	// Godot wraps the glTF in a scene root named after the file, and the glTF's own root empty
	// (monster_<key>) sits under it: both are containers, see rig_walk.
	rig_walk(inst, -1, Transform3D(), out, sockets);
	memdelete(inst);
	static const StringName body("body"), part_a("part_a"), part_b("part_b");
	bool has_body = false, extra = false;
	for (const RigPart &p : out) {
		has_body = has_body || p.name == body;
		extra = extra || (p.name != body && p.name != part_a && p.name != part_b);
	}
	if (!has_body) {
		out.clear();
		if (sockets) {
			sockets->clear();
		}
		return 0;
	}
	if (!extra) {
		// A v1 model (body / part_a / part_b): its big limbs have always cast shadows.
		for (RigPart &p : out) {
			p.flags &= uint8_t(~RigPart::TINY);
		}
	}
	return extra ? 2 : 1;
}

// Rough stand-ins so the generator runs before the art batches are published.
Ref<Mesh> AssetLibrary::make_placeholder(const std::string &id) const {
	MeshBuilder b;
	auto has = [&](const char *s) { return id.find(s) != std::string::npos; };
	if (has("string_jack")) {
		b.blob(Vector3(0, -0.08f, 0), Vector3(0.07f, 0.06f, 0.07f), pal::GLOW_JACK, 0);
	} else if (has("bunting")) {
		b.box(Vector3(0, -0.15f, 0), Vector3(0.12f, 0.15f, 0.01f), has("purple") ? pal::BANNER_PURPLE : has("cream") ? pal::BUNTING_CREAM : pal::BANNER_ORANGE);
	} else if (has("wreath")) {
		for (int i = 0; i < 8; ++i) {
			const float a = i * Math_TAU / 8.0f;
			b.blob(Vector3(std::cos(a) * 0.2f, std::sin(a) * 0.2f, 0.05f), Vector3(0.08f, 0.08f, 0.05f), i % 3 ? pal::HEDGE : pal::MAPLE_ORANGE, 0);
		}
	} else if (has("jackolantern") || has("pumpkin")) {
		float r = has("giant") ? 0.9f : has("small") ? 0.15f : 0.3f;
		b.blob(Vector3(0, r * 0.8f, 0), Vector3(r, r * 0.8f, r), pal::PUMPKIN, 1);
		b.cylinder(Vector3(0, r * 1.5f, 0), r * 0.12f, r * 0.4f, 5, pal::STEM_BROWN);
		if (has("jackolantern")) {
			b.box(Vector3(0, r * 0.9f, r * 0.95f), Vector3(r * 0.5f, r * 0.12f, 0.03f), pal::GLOW_JACK);
		}
	} else if (has("hay_bale_round")) {
		b.cylinder(Vector3(), 0.6f, 1.0f, 10, pal::HAY);
	} else if (has("hay_bale")) {
		b.rounded_box(Vector3(0, 0.275f, 0), Vector3(0.55f, 0.275f, 0.3f), 0.06f, pal::HAY, pal::STRAW);
	} else if (has("tree")) {
		b.cylinder(Vector3(), 0.25f, 2.8f, 7, pal::BARK, 0.18f);
		int c = has("red") ? pal::MAPLE_RED : has("orange") ? pal::MAPLE_ORANGE : pal::GOLDEN_LEAF;
		b.blob(Vector3(0, 3.6f, 0), Vector3(1.8f, 1.5f, 1.8f), c, 1, 0.15f, 7);
	} else if (has("bush")) {
		b.blob(Vector3(0, 0.45f, 0), Vector3(0.5f, 0.45f, 0.5f), pal::HEDGE, 1, 0.1f, 3);
	} else if (has("lamp_post")) {
		b.cylinder(Vector3(), 0.1f, 3.0f, 6, pal::LAMP_BLACK);
		b.box(Vector3(0, 3.2f, 0), Vector3(0.22f, 0.25f, 0.22f), pal::GLOW_LAMP);
	} else if (has("lantern") || has("bulb")) {
		float r = has("bulb") ? 0.05f : 0.17f;
		b.blob(Vector3(0, -r, 0), Vector3(r, r, r), pal::GLOW_LANTERN, 0);
	} else if (has("candy_wrapped")) {
		int c = has("pink") ? pal::CANDY_PINK : has("mint") ? pal::CANDY_MINT : has("grape") ? pal::CANDY_GRAPE : pal::CANDY_LEMON;
		b.blob(Vector3(), Vector3(0.12f, 0.09f, 0.09f), c, 0);
		b.cylinder(Vector3(0.1f, -0.06f, 0), 0.0f, 0.12f, 4, c, 0.07f);
	} else if (has("chocolate")) {
		b.rounded_box(Vector3(), Vector3(0.2f, 0.04f, 0.1f), 0.02f, pal::CHOCO, pal::WRAPPER_RED);
	} else if (has("lollipop")) {
		b.cylinder(Vector3(0, -0.1f, -0.03f), 0.24f, 0.06f, 12, pal::CANDY_CHERRY);
		b.box(Vector3(0, -0.4f, 0), Vector3(0.025f, 0.25f, 0.025f), pal::LOLLY_WHITE);
	} else if (has("golden")) {
		b.rounded_box(Vector3(), Vector3(0.28f, 0.06f, 0.13f), 0.03f, pal::FOIL_GOLD, pal::GLOW_CANDY_GOLD);
	} else if (has("cloud")) {
		for (int i = 0; i < 5; ++i) {
			b.blob(Vector3(-5.0f + i * 2.5f, (i % 2) * 0.8f, 0), Vector3(2.6f, 1.8f, 2.4f), pal::ROOF_SLATE, 1, 0.12f, 11 + i);
		}
	} else if (has("car")) {
		b.rounded_box(Vector3(0, 0.6f, 0), Vector3(0.85f, 0.45f, 1.8f), 0.2f, pal::HOUSE_SKY);
		b.rounded_box(Vector3(0, 1.25f, -0.2f), Vector3(0.75f, 0.3f, 1.0f), 0.18f, pal::HOUSE_SKY, pal::HOUSE_SKY);
	} else if (has("scarecrow") || has("inflatable") || has("corn") || has("sunflower")) {
		int c = has("ghost") ? pal::GHOST_WHITE : has("cat") ? pal::CAT_BLACK : has("sunflower") ? pal::SUNFLOWER_YELLOW : pal::STRAW;
		b.cylinder(Vector3(), 0.3f, 1.8f, 8, c, 0.2f);
	} else if (has("dragon")) {
		b.blob(Vector3(0, 0.45f, 0), Vector3(0.3f, 0.4f, 0.28f), pal::DRAGON_JADE, 1);
		b.blob(Vector3(0, 0.95f, 0), Vector3(0.24f, 0.22f, 0.24f), pal::DRAGON_JADE, 1);
	} else {
		b.rounded_box(Vector3(0, 0.35f, 0), Vector3(0.35f, 0.35f, 0.35f), 0.05f, pal::WOOD);
	}
	return b.build();
}

} // namespace tot
