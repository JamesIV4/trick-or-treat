#include "mesh/mesh_builder.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <cmath>

#include "core/rng.h"

using namespace godot;

namespace tot {

// tri(a, b, c): counter-clockwise seen from the front (math convention). Godot treats clockwise as
// front-facing, so vertices are emitted as a, c, b.
void MeshBuilder::tri(Vector3 a, Vector3 b, Vector3 c, int cell) {
	a = xf_.xform(a);
	b = xf_.xform(b);
	c = xf_.xform(c);
	Vector3 n = (b - a).cross(c - a);
	if (n.length_squared() < 1e-12f) {
		return;
	}
	n.normalize();
	const int layer = cell >> 8;
	const Vector2 tint = cell_uv(cell & 0xFF) + Vector2(float(layer), 0.0f);
	// Material projection axes.
	Vector3 U, V;
	if (uv_override_) {
		U = xf_.basis.xform(uv_u_).normalized();
		V = xf_.basis.xform(uv_v_).normalized();
	} else {
		const float ax = std::abs(n.x), ay = std::abs(n.y), az = std::abs(n.z);
		if (ay >= ax && ay >= az) {
			U = Vector3(1, 0, 0);
			V = Vector3(0, 0, 1);
		} else if (ax >= az) {
			U = Vector3(0, 0, n.x > 0 ? -1.0f : 1.0f);
			V = Vector3(0, -1, 0);
		} else {
			U = Vector3(n.z > 0 ? 1.0f : -1.0f, 0, 0);
			V = Vector3(0, -1, 0);
		}
	}
	Vector3 T = (U - n * n.dot(U));
	T = T.length_squared() > 1e-8f ? T.normalized() : U;
	const float w = n.cross(T).dot(V) >= 0.0f ? 1.0f : -1.0f;
	const Vector3 ps[3] = { a, c, b };
	for (const Vector3 &p : ps) {
		verts_.push_back(p);
		normals_.push_back(n);
		uvs_.push_back(Vector2(p.dot(U), p.dot(V)));
		uv2s_.push_back(tint);
		tangents_.insert(tangents_.end(), { T.x, T.y, T.z, w });
		float ao = ao_;
		if (ground_ao_) {
			ao *= Math::clamp(0.6f + p.y * 0.55f, 0.6f, 1.0f);
		}
		ao_values_.push_back(ao);
	}
}

void MeshBuilder::quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int cell) {
	tri(a, b, c, cell);
	tri(a, c, d, cell);
}

void MeshBuilder::tri_out(Vector3 a, Vector3 b, Vector3 c, int cell, Vector3 inside) {
	Vector3 n = (b - a).cross(c - a);
	if (n.dot((a + b + c) / 3.0f - inside) < 0.0f) {
		tri(a, c, b, cell);
	} else {
		tri(a, b, c, cell);
	}
}

void MeshBuilder::quad_out(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int cell, Vector3 inside) {
	Vector3 n = (b - a).cross(c - a);
	if (n.dot((a + b + c + d) / 4.0f - inside) < 0.0f) {
		quad(a, d, c, b, cell);
	} else {
		quad(a, b, c, d, cell);
	}
}

void MeshBuilder::box(Vector3 c, Vector3 h, int cell, int top_cell) {
	const int top = top_cell < 0 ? cell : top_cell;
	Vector3 p[8];
	for (int i = 0; i < 8; ++i) {
		p[i] = c + Vector3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
	}
	quad_out(p[0], p[1], p[3], p[2], cell, c); // -z
	quad_out(p[4], p[5], p[7], p[6], cell, c); // +z
	quad_out(p[0], p[2], p[6], p[4], cell, c); // -x
	quad_out(p[1], p[3], p[7], p[5], cell, c); // +x
	quad_out(p[0], p[1], p[5], p[4], cell, c); // -y
	quad_out(p[2], p[3], p[7], p[6], top, c); // +y
}

void MeshBuilder::rounded_box(Vector3 c, Vector3 h, float b, int cell, int top_cell) {
	b = Math::min(b, Math::min(h.x, Math::min(h.y, h.z)) * 0.45f);
	if (b <= 0.001f) {
		box(c, h, cell, top_cell);
		return;
	}
	const int top = top_cell < 0 ? cell : top_cell;
	const Vector3 in = h - Vector3(b, b, b);
	// Corner point on face `axis` (0,1,2) of corner signs s.
	auto fp = [&](int axis, Vector3 s) {
		Vector3 p = c + Vector3(in.x * s.x, in.y * s.y, in.z * s.z);
		p[axis] = c[axis] + h[axis] * s[axis];
		return p;
	};
	// Six faces.
	for (int axis = 0; axis < 3; ++axis) {
		for (float sa : { -1.0f, 1.0f }) {
			const int u = (axis + 1) % 3, v = (axis + 2) % 3;
			Vector3 s[4];
			const float su[4] = { -1, 1, 1, -1 }, sv[4] = { -1, -1, 1, 1 };
			Vector3 q[4];
			for (int k = 0; k < 4; ++k) {
				s[k] = Vector3();
				s[k][axis] = sa;
				s[k][u] = su[k];
				s[k][v] = sv[k];
				q[k] = fp(axis, s[k]);
			}
			quad_out(q[0], q[1], q[2], q[3], (axis == 1 && sa > 0) ? top : cell, c);
		}
	}
	// Twelve edge chamfers: between faces a and b, running along the third axis.
	for (int a = 0; a < 3; ++a) {
		for (int bb = a + 1; bb < 3; ++bb) {
			const int run = 3 - a - bb;
			for (float sa : { -1.0f, 1.0f }) {
				for (float sb : { -1.0f, 1.0f }) {
					Vector3 s0, s1;
					s0[a] = sa;
					s0[bb] = sb;
					s0[run] = -1;
					s1 = s0;
					s1[run] = 1;
					const int edge_cell = (a == 1 || bb == 1) && ((a == 1 ? sa : sb) > 0) ? top : cell;
					quad_out(fp(a, s0), fp(a, s1), fp(bb, s1), fp(bb, s0), edge_cell, c);
				}
			}
		}
	}
	// Eight corner triangles.
	for (int i = 0; i < 8; ++i) {
		Vector3 s((i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1);
		tri_out(fp(0, s), fp(1, s), fp(2, s), s.y > 0 ? top : cell, c);
	}
}

void MeshBuilder::cylinder(Vector3 base, float r, float hgt, int sides, int cell, float rt, int cap_cell) {
	if (rt < 0.0f) {
		rt = r;
	}
	const int cap = cap_cell < 0 ? cell : cap_cell;
	const Vector3 mid = base + Vector3(0, hgt * 0.5f, 0);
	const Vector3 top = base + Vector3(0, hgt, 0);
	for (int i = 0; i < sides; ++i) {
		const float a0 = Math_TAU * i / sides, a1 = Math_TAU * (i + 1) / sides;
		const Vector3 d0(std::cos(a0), 0, std::sin(a0)), d1(std::cos(a1), 0, std::sin(a1));
		quad_out(base + d0 * r, base + d1 * r, top + d1 * rt, top + d0 * rt, cell, mid);
		tri_out(base, base + d0 * r, base + d1 * r, cell, mid);
		if (rt > 0.001f) {
			tri_out(top, top + d0 * rt, top + d1 * rt, cap, mid);
		}
	}
}

void MeshBuilder::prism_xy(const std::vector<Vector2> &o, float z0, float depth, int cell, int side_cell) {
	const int side = side_cell < 0 ? cell : side_cell;
	Vector3 center;
	for (const Vector2 &p : o) {
		center += Vector3(p.x, p.y, z0);
	}
	center /= float(o.size());
	const float za = z0 - depth * 0.5f, zb = z0 + depth * 0.5f;
	const size_t n = o.size();
	for (size_t i = 1; i + 1 < n; ++i) {
		tri_out(Vector3(o[0].x, o[0].y, za), Vector3(o[i].x, o[i].y, za), Vector3(o[i + 1].x, o[i + 1].y, za), cell, center);
		tri_out(Vector3(o[0].x, o[0].y, zb), Vector3(o[i].x, o[i].y, zb), Vector3(o[i + 1].x, o[i + 1].y, zb), cell, center);
	}
	for (size_t i = 0; i < n; ++i) {
		const Vector2 &p = o[i], &q = o[(i + 1) % n];
		quad_out(Vector3(p.x, p.y, za), Vector3(q.x, q.y, za), Vector3(q.x, q.y, zb), Vector3(p.x, p.y, zb), side, center);
	}
}

void MeshBuilder::blob(Vector3 c, Vector3 radii, int cell, int subdiv, float jitter, uint32_t seed) {
	const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
	std::vector<Vector3> v = {
		{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 }, { 0, -1, t }, { 0, 1, t },
		{ 0, -1, -t }, { 0, 1, -t }, { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 }
	};
	std::vector<int> f = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
		3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };
	for (Vector3 &p : v) {
		p.normalize();
	}
	for (int s = 0; s < subdiv; ++s) {
		std::vector<int> nf;
		for (size_t i = 0; i < f.size(); i += 3) {
			int a = f[i], b = f[i + 1], cc = f[i + 2];
			int ab = int(v.size());
			v.push_back(((v[a] + v[b]) * 0.5f).normalized());
			int bc = int(v.size());
			v.push_back(((v[b] + v[cc]) * 0.5f).normalized());
			int ca = int(v.size());
			v.push_back(((v[cc] + v[a]) * 0.5f).normalized());
			nf.insert(nf.end(), { a, ab, ca, b, bc, ab, cc, ca, bc, ab, bc, ca });
		}
		f.swap(nf);
	}
	// Jitter must be identical for shared positions, so hash by position.
	Rng rng(seed);
	std::vector<float> scale(v.size(), 1.0f);
	for (size_t i = 0; i < v.size(); ++i) {
		uint64_t key = uint64_t(int64_t(std::lround(v[i].x * 1000)) * 73856093) ^ uint64_t(int64_t(std::lround(v[i].y * 1000)) * 19349663) ^
				uint64_t(int64_t(std::lround(v[i].z * 1000)) * 83492791) ^ seed;
		scale[i] = 1.0f + jitter * (float(hash64(key) % 1000) / 1000.0f - 0.5f) * 2.0f;
	}
	for (size_t i = 0; i < f.size(); i += 3) {
		Vector3 p[3];
		for (int k = 0; k < 3; ++k) {
			const Vector3 &u = v[f[i + k]];
			p[k] = c + Vector3(u.x * radii.x, u.y * radii.y, u.z * radii.z) * scale[f[i + k]];
		}
		tri_out(p[0], p[1], p[2], cell, c);
	}
}

void MeshBuilder::beam(Vector3 a, Vector3 b, float width, float height, int cell, Vector3 up) {
	const Vector3 d = b - a;
	const float len = d.length();
	if (len < 1e-4f) {
		return;
	}
	const Vector3 f = d / len;
	Vector3 u = up - f * f.dot(up);
	if (u.length_squared() < 1e-6f) {
		u = std::abs(f.y) < 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
		u = u - f * f.dot(u);
	}
	u.normalize();
	const Vector3 s = f.cross(u).normalized();
	const Vector3 hs = s * (width * 0.5f), hu = u * (height * 0.5f);
	const Vector3 c = (a + b) * 0.5f;
	const Vector3 p[8] = { a - hs - hu, a + hs - hu, a + hs + hu, a - hs + hu, b - hs - hu, b + hs - hu, b + hs + hu, b - hs + hu };
	quad_out(p[0], p[1], p[2], p[3], cell, c);
	quad_out(p[4], p[5], p[6], p[7], cell, c);
	quad_out(p[0], p[1], p[5], p[4], cell, c);
	quad_out(p[1], p[2], p[6], p[5], cell, c);
	quad_out(p[2], p[3], p[7], p[6], cell, c);
	quad_out(p[3], p[0], p[4], p[7], cell, c);
}

void MeshBuilder::ribbon(const std::vector<Vector3> &centers, const std::vector<Vector3> &lefts, float off_a, float off_b, float y, int cell) {
	for (size_t i = 0; i + 1 < centers.size(); ++i) {
		Vector3 a0 = centers[i] + lefts[i] * off_a, b0 = centers[i] + lefts[i] * off_b;
		Vector3 a1 = centers[i + 1] + lefts[i + 1] * off_a, b1 = centers[i + 1] + lefts[i + 1] * off_b;
		a0.y = b0.y = a1.y = b1.y = y;
		const Vector3 below = (a0 + b1) * 0.5f - Vector3(0, 1, 0);
		quad_out(a0, a1, b1, b0, cell, below);
	}
}

Ref<ArrayMesh> MeshBuilder::build() const {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (verts_.empty()) {
		return mesh;
	}
	const int64_t count = int64_t(verts_.size());
	PackedVector3Array v, n;
	PackedVector2Array uv, uv2;
	PackedFloat32Array tan;
	PackedColorArray col;
	v.resize(count);
	n.resize(count);
	uv.resize(count);
	uv2.resize(count);
	tan.resize(count * 4);
	col.resize(count);
	for (int64_t i = 0; i < count; ++i) {
		v.set(i, verts_[i]);
		n.set(i, normals_[i]);
		uv.set(i, uvs_[i]);
		uv2.set(i, uv2s_[i]);
		for (int k = 0; k < 4; ++k) {
			tan.set(i * 4 + k, tangents_[i * 4 + k]);
		}
		const float ao = ao_values_[i];
		col.set(i, Color(ao, ao, ao, 1.0f));
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = v;
	arrays[Mesh::ARRAY_NORMAL] = n;
	arrays[Mesh::ARRAY_TANGENT] = tan;
	arrays[Mesh::ARRAY_COLOR] = col;
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	arrays[Mesh::ARRAY_TEX_UV2] = uv2;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void MeshBuilder::clear() {
	verts_.clear();
	normals_.clear();
	uvs_.clear();
	uv2s_.clear();
	tangents_.clear();
	ao_values_.clear();
}

} // namespace tot
