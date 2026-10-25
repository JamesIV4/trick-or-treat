#pragma once
// Flat-shaded low-poly mesh builder with painted materials. Each face gets:
//   UV  = material tiling coordinates in metres (box projection, or explicit axes for roofs),
//   UV2 = (material layer + palette cell u, palette cell v), matching the Blender kit encoding,
//   COLOR = approximate ambient occlusion, plus tangents for the material normal maps.
// Pass `mat_cell(pal::X, mat::Y)` wherever a cell is expected; a bare palette cell means "flat".

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

#include "core/consts.h"
#include "mesh/materials.gen.h"
#include "mesh/palette_cells.gen.h"

namespace tot {

using godot::Transform3D;
using godot::Vector2;
using godot::Vector3;

inline Vector2 cell_uv(int cell) {
	const int col = cell % pal::GRID;
	const int row = cell / pal::GRID;
	return Vector2((col + 0.5f) / pal::GRID, (row + 0.5f) / pal::GRID);
}

// Packs a palette cell and a material layer into the int every builder call takes.
constexpr int mat_cell(int cell, int material) {
	return (cell & 0xFF) | (material << 8);
}

class MeshBuilder {
public:
	// Everything added is transformed by the current transform.
	void set_transform(const Transform3D &t) { xf_ = t; }
	const Transform3D &transform() const { return xf_; }
	// Explicit material UV axes (in the current transform's local space); clear to go back to box.
	void set_uv_axes(const Vector3 &u, const Vector3 &v) {
		uv_override_ = true;
		uv_u_ = u;
		uv_v_ = v;
	}
	void clear_uv_axes() { uv_override_ = false; }
	void set_ao(float ao) { ao_ = ao; }
	// Darken faces near the ground (contact shadow for walls, posts, fences).
	void set_ground_ao(bool on) { ground_ao_ = on; }

	void tri(Vector3 a, Vector3 b, Vector3 c, int cell);
	void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int cell);
	// Orients the face so its normal points away from `inside` (for convex pieces).
	void tri_out(Vector3 a, Vector3 b, Vector3 c, int cell, Vector3 inside);
	void quad_out(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int cell, Vector3 inside);

	// Axis-aligned box in local space. top_cell < 0 uses cell for the top as well.
	void box(Vector3 center, Vector3 half, int cell, int top_cell = -1);
	// Chamfered toy box: every edge bevelled by `bevel` metres.
	void rounded_box(Vector3 center, Vector3 half, float bevel, int cell, int top_cell = -1);
	// Cylinder along Y standing on `base`. radius_top < 0 means same as radius.
	void cylinder(Vector3 base, float radius, float height, int sides, int cell, float radius_top = -1.0f, int cap_cell = -1);
	// Extrudes a convex 2D outline in the XY plane along Z by depth (centred on z0).
	void prism_xy(const std::vector<Vector2> &outline, float z0, float depth, int cell, int side_cell = -1);
	// Low-poly blob (icosahedron, optionally subdivided) scaled per axis.
	void blob(Vector3 center, Vector3 radii, int cell, int subdiv = 1, float jitter = 0.0f, uint32_t seed = 0);
	// Rectangular beam from a to b (width across, height along `up` projected off the beam axis).
	void beam(Vector3 a, Vector3 b, float width, float height, int cell, Vector3 up = Vector3(0, 1, 0));
	// Flat horizontal quad strip along a polyline (roads, sidewalks): left/right offsets per point.
	void ribbon(const std::vector<Vector3> &centers, const std::vector<Vector3> &lefts, float off_a, float off_b, float y, int cell);

	bool empty() const { return verts_.empty(); }
	int vertex_count() const { return int(verts_.size()); }
	godot::Ref<godot::ArrayMesh> build() const;
	void clear();

private:
	Transform3D xf_;
	bool uv_override_ = false;
	Vector3 uv_u_, uv_v_;
	float ao_ = 1.0f;
	bool ground_ao_ = false;
	std::vector<Vector3> verts_;
	std::vector<Vector3> normals_;
	std::vector<Vector2> uvs_;
	std::vector<Vector2> uv2s_;
	std::vector<float> tangents_; // 4 per vertex
	std::vector<float> ao_values_;
};

} // namespace tot
