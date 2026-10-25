#pragma once
// Fire and damage. The dragon kid's fireballs (with gentle auto-aim) and the ground-pound fire
// ring; monsters' thrown bones and goo; small explosion puffs. Monsters, nests and thorn vines
// implement Damageable so fire can find and hurt them.

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include <functional>
#include <vector>
#include <vector>

namespace tot {

class Damageable {
public:
	virtual ~Damageable() = default;
	virtual godot::Vector3 target_point() const = 0;
	virtual float target_radius() const = 0;
	virtual bool target_alive() const = 0;
	// Returns true when this hit destroyed it.
	virtual bool take_hit(float damage, const godot::Vector3 &from) = 0;
	virtual bool is_monster() const { return false; }
	// Monster spawners and thorn nests: worth auto-targeting from close by, like monsters.
	virtual bool is_nest() const { return false; }
	// Distance from p to the target's surface (long targets such as vines override this).
	virtual float surface_distance(const godot::Vector3 &p) const { return p.distance_to(target_point()) - target_radius(); }
	// Auto-aim prefers targets with a lower weight.
	virtual float aim_weight() const { return 1.0f; }
};

class Combat : public godot::Node3D {
	GDCLASS(Combat, godot::Node3D)

public:
	enum ProjectileKind { BONE, GOO };

	void _ready() override;
	void _process(double delta) override;

	// Player fire.
	void fireball(const godot::Vector3 &origin, const godot::Vector3 &dir, float damage, int count);
	void fire_ring(const godot::Vector3 &center, float radius, float damage);
	// Monster attacks.
	void throw_projectile(const godot::Vector3 &origin, const godot::Vector3 &velocity, ProjectileKind kind);
	void puff(const godot::Vector3 &pos, const godot::Color &color, int amount = 18, float speed = 3.0f);
	// Best auto-aim target within a cone in front of the player.
	Damageable *aim_assist(const godot::Vector3 &origin, const godot::Vector3 &facing, float range, float cone_cos);
	// What a fireball should home in on: a close monster first, then one ahead, then anything else.
	Damageable *pick_target(const godot::Vector3 &origin, const godot::Vector3 &facing);
	void clear();

	std::function<void(std::vector<Damageable *> &)> gather_targets;
	std::function<godot::Vector3()> player_position;
	std::function<void(const godot::Vector3 &from)> on_player_hit;
	std::function<void(const godot::Vector3 &pos, bool destroyed)> on_target_hit;
	std::function<void(const godot::Vector3 &pos)> on_fizzle;

protected:
	static void _bind_methods();

private:
	struct Shot {
		godot::MeshInstance3D *node = nullptr;
		godot::OmniLight3D *light = nullptr;
		godot::CPUParticles3D *trail = nullptr;
		godot::Vector3 pos, vel;
		float life = 0.0f, damage = 1.0f;
		bool active = false;
	};
	struct Ring {
		godot::MeshInstance3D *node = nullptr;
		godot::OmniLight3D *light = nullptr;
		godot::Vector3 center;
		float t = 1.0f, radius = 3.0f;
	};
	struct Thrown {
		godot::MeshInstance3D *node = nullptr;
		godot::Vector3 pos, vel;
		float life = 0.0f;
		ProjectileKind kind = BONE;
		bool active = false;
	};
	Shot &free_shot();
	void explode(Shot &s, bool hit);
	bool ray_blocked(const godot::Vector3 &a, const godot::Vector3 &b, godot::Vector3 &hit);

	std::vector<Shot> shots_;
	std::vector<Ring> rings_;
	std::vector<Thrown> thrown_;
	std::vector<godot::CPUParticles3D *> puffs_;
	int next_puff_ = 0;
	godot::Ref<godot::Mesh> ball_mesh_, ring_mesh_, bone_mesh_, goo_mesh_;
	godot::Ref<godot::StandardMaterial3D> ring_mat_;
};

} // namespace tot
