#pragma once
// Particle effects built from the art designed and rendered in Blender (art/tools/make_vfx.py):
// twinkle sparkles, soft glows, star rings, cartoon smoke poofs, flickering flames, a magic swirl,
// and little star / heart / candy-corn / confetti meshes. Pooled one-shot emitters behind a small
// API; one instance lives under the game and is reachable through Vfx::get().

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include <vector>

namespace tot {

class Vfx : public godot::Node3D {
	GDCLASS(Vfx, godot::Node3D)

public:
	static Vfx *get() { return singleton_; }
	void _ready() override;
	void _exit_tree() override;

	// Candy: the pickup moment, scaled by how special the candy is (0 plain .. 3 golden).
	void candy_pickup(const godot::Vector3 &pos, const godot::Color &color, int tier);
	// A sparkle trail following candy as it flies to the bucket.
	int trail_begin(const godot::Color &color);
	void trail_move(int handle, const godot::Vector3 &pos);
	void trail_end(int handle);
	// General effects.
	void sparkles(const godot::Vector3 &pos, const godot::Color &color, int count, float speed, float size = 1.0f, float rise = 1.0f);
	void smoke(const godot::Vector3 &pos, const godot::Color &tint, float size, int count = 5);
	void flash(const godot::Vector3 &pos, const godot::Color &color, float size);
	void ring(const godot::Vector3 &pos, const godot::Color &color, float radius, float life = 0.45f);
	void swirl(const godot::Vector3 &pos, const godot::Color &color, float size);
	void flames(const godot::Vector3 &pos, int count, float spread, float size);
	void confetti(const godot::Vector3 &pos, bool firework);
	// For emitters owned elsewhere (the fireball trail).
	godot::Ref<godot::Mesh> flame_quad(float size);

protected:
	static void _bind_methods();

private:
	struct Pool {
		std::vector<godot::CPUParticles3D *> items;
		int next = 0;
		godot::CPUParticles3D *take() {
			godot::CPUParticles3D *p = items[next];
			next = (next + 1) % int(items.size());
			return p;
		}
	};
	godot::Ref<godot::Texture2D> load_texture(const char *name);
	godot::Ref<godot::StandardMaterial3D> sprite_material(const godot::Ref<godot::Texture2D> &tex, bool additive, int frames);
	godot::Ref<godot::Mesh> quad(const godot::Ref<godot::Material> &mat, float size);
	void make_pool(Pool &pool, int count, const godot::Ref<godot::Mesh> &mesh);
	void fire(godot::CPUParticles3D *p, const godot::Vector3 &pos);
	void load_meshes();

	static Vfx *singleton_;
	godot::Ref<godot::Texture2D> sparkle_tex_, glow_tex_, ring_tex_, smoke_tex_, flame_tex_, swirl_tex_;
	godot::Ref<godot::StandardMaterial3D> sparkle_mat_, glow_mat_, ring_mat_, smoke_mat_, flame_mat_, swirl_mat_, confetti_mat_;
	std::vector<godot::Ref<godot::Mesh>> confetti_meshes_;
	Pool sparkle_pool_, smoke_pool_, flash_pool_, ring_pool_, swirl_pool_, flame_pool_;
	std::vector<Pool> confetti_pools_;
	struct Trail {
		godot::CPUParticles3D *p = nullptr;
		bool used = false;
	};
	std::vector<Trail> trails_;
};

} // namespace tot
