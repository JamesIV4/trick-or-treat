#pragma once
// Every candy in the neighborhood: one MultiMesh per candy kind plus one for the glow halos, and a
// drop shadow below each piece. Pickup is a spatial-hash query against the player each frame (no
// Area3D per candy). Candy laid along one jump forms a rhythm group; sweeping up a whole group in
// one flowing run is reported as a completed sequence.

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/decal.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include <unordered_map>
#include <vector>

#include "core/rng.h"
#include "gen/types.h"

namespace tot {

class CandyField : public godot::Node3D {
	GDCLASS(CandyField, godot::Node3D)

public:
	void setup(const std::vector<godot::Ref<godot::Mesh>> &meshes, const godot::Ref<godot::Material> &candy_mat,
			const godot::Ref<godot::Material> &halo_mat, int lot_count);
	void add(const CandySpec &c);
	void finalize();
	// Quietly removes the pieces within r of a point (not collected, no effects): trailer shots clear
	// candy that would hang right in front of the lens.
	void hide_near(const godot::Vector3 &local, float r);

	// Collects candy within radius of the player. Returns the total value collected this call.
	int collect(const godot::Vector3 &player, float radius, float height, float dt);
	// Doorbell treat burst: candies arc out from the door into the yard.
	void burst(const godot::Vector3 &from, const godot::Vector3 &toward, int lot, int count, uint64_t seed);
	int remaining_in_lot(int lot) const;
	int total() const { return total_; }
	int collected() const { return collected_; }
	int total_value() const { return total_value_; }
	int collected_value() const { return collected_value_; }
	godot::Vector3 nearest_remaining(const godot::Vector3 &from) const;
	// Nearest candy no more than max_rise above `from` (for the review bot). Returns false if none.
	bool nearest_within(const godot::Vector3 &from, float max_rise, godot::Vector3 &out,
			const std::vector<std::pair<godot::Vector3, float>> *avoid = nullptr) const;
	godot::PackedInt32Array take_cleared_lots();

	void _process(double delta) override;
	void _exit_tree() override;

protected:
	static void _bind_methods();

private:
	struct Candy {
		godot::Vector3 pos;
		int kind;
		int lot;
		int slot; // instance index in its kind's multimesh
		bool alive = true;
		float fly_t = 1.0f; // < 1 while arcing out of a doorbell burst
		godot::Vector3 fly_from;
		float pop = 0.0f; // > 0 while playing the pickup pop
		bool homing = false; // pulled toward the player by the candy magnet
		float home_t = 0.0f;
		int group = -1;
		int id = -1; // index in the settlement's candy list; -1 for doorbell treats
		int trail = -1; // sparkle trail while flying to the bucket
	};
	struct Group {
		int size = 0, got = 0;
		float last = 0.0f;
		bool broken = false;
	};
	void on_picked(Candy &c);
	int64_t cell_key(int x, int z) const { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); }
	void index_candy(int id);
	void write_instance(const Candy &c, float scale);
	void spark(const godot::Vector3 &p, int kind);

	std::vector<Candy> candy_;
	std::vector<godot::MultiMeshInstance3D *> mm_;
	godot::MultiMeshInstance3D *halo_ = nullptr;
	std::vector<std::vector<int>> slots_; // kind -> candy ids
	std::unordered_map<int64_t, std::vector<int>> grid_;
	std::vector<int> per_lot_;
	std::vector<int> flying_;
	std::vector<int> popping_;
	std::vector<int> homing_;
	std::vector<int> cleared_;
	std::vector<godot::CPUParticles3D *> sparks_;
	std::vector<Group> groups_;
	float time_ = 0.0f;
	int next_spark_ = 0;
	std::vector<godot::Ref<godot::Mesh>> meshes_;
	godot::Ref<godot::Material> candy_mat_, halo_mat_;
	int total_ = 0, collected_ = 0, total_value_ = 0, collected_value_ = 0;
	int capacity_extra_ = 160; // room for doorbell bursts
	bool finalized_ = false;
};

} // namespace tot
