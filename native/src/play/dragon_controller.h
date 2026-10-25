#pragma once
// The dragon kid: a forgiving full-3D platformer moveset (coyote time, jump buffering, variable
// jump with apex hang, ceiling-edge and ledge-lip assists, wing-flap double jump with glide, tail
// spin, roll, ground pound, bounce pads), fire breath, knockback when hurt, riding the car, a drop
// shadow straight below, plus
// procedural squash-and-stretch animation of the authored character's parts.

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>

#include <vector>

namespace tot {

class DragonController : public godot::CharacterBody3D {
	GDCLASS(DragonController, godot::CharacterBody3D)

public:
	void _ready() override;
	void _physics_process(double delta) override;
	void _process(double delta) override;

	void set_visual(godot::Node3D *visual);
	void set_fill(float fill);
	// Pickup feedback: the bucket bounces and the kid does a happy little squash.
	void celebrate(int kind);
	void teleport(const godot::Vector3 &pos, float yaw);
	float collect_radius() const;
	bool is_spinning() const { return spin_t_ > 0.0f; }
	float get_speed() const;
	void set_input_enabled(bool e) { input_enabled_ = e; }
	bool get_input_enabled() const { return input_enabled_; }
	// Autopilot (review bot): world-space wish direction and buttons replace device input.
	void set_bot(bool active) { bot_ = active; }
	void bot_input(const godot::Vector3 &wish, bool jump_pressed, bool jump_held, bool spin_pressed, bool fire = false) {
		bot_wish_ = wish;
		bot_jump_ = bot_jump_ || jump_pressed;
		bot_hold_ = jump_held;
		bot_spin_ = bot_spin_ || spin_pressed;
		bot_fire_ = bot_fire_ || fire;
	}
	// Combat hooks.
	void knockback(const godot::Vector3 &from, float force);
	void set_fire_cooldown(float s) { fire_cooldown_ = s; }
	void set_mounted(bool m);
	bool is_mounted() const { return mounted_; }
	void set_blink(bool hidden);
	godot::Vector3 facing_dir() const;
	void set_facing(float yaw) { facing_ = visual_yaw_ = yaw; }
	bool is_pounding() const { return pounding_; }

protected:
	static void _bind_methods();

private:
	struct Part {
		godot::Node3D *node = nullptr;
		godot::Transform3D rest;
	};
	void find_parts(godot::Node *n);
	void assist(const godot::Vector3 &wish);
	void step_up(const godot::Vector3 &wish);
	void puff(const godot::Vector3 &pos, int amount, float spread, const godot::Color &color, float speed);
	std::vector<godot::CPUParticles3D *> puffs_;
	int next_puff_ = 0;
	float bucket_punch_ = 0.0f;
	godot::Transform3D bucket_rest_;
	godot::Node3D *bucket_ = nullptr;
	float step_phase_prev_ = 0.0f;
	int step_parity_ = 0;
	void pose(Part &p, const godot::Vector3 &euler, const godot::Vector3 &offset = godot::Vector3());

	godot::Node3D *visual_root_ = nullptr; // yaw + squash
	godot::Node3D *visual_ = nullptr; // authored model
	godot::OmniLight3D *bucket_light_ = nullptr;
	Part body_, head_, wing_l_, wing_r_, tail_, leg_l_, leg_r_, arm_l_, arm_r_, bucket_candy_;

	bool input_enabled_ = true;
	bool bot_ = false, bot_jump_ = false, bot_hold_ = false, bot_spin_ = false, bot_fire_ = false;
	float fire_cd_ = 0.0f, fire_cooldown_ = 0.45f, knock_t_ = 0.0f;
	bool mounted_ = false;
	float step_smooth_ = 0.0f;
	godot::Vector3 bot_wish_;
	float coyote_ = 0.0f, buffer_ = 0.0f;
	bool flap_used_ = false, jump_cut_ = false, spin_air_used_ = false;
	float glide_t_ = 0.0f, flap_anim_ = 0.0f;
	float spin_t_ = 0.0f, roll_t_ = 0.0f, roll_cd_ = 0.0f;
	float pound_stall_ = 0.0f;
	bool pounding_ = false;
	float shockwave_t_ = 0.0f;
	float facing_ = 0.0f, visual_yaw_ = 0.0f;
	float phase_ = 0.0f, squash_ = 0.0f, squash_v_ = 0.0f;
	float roar_t_ = 0.0f, time_ = 0.0f;
	bool was_on_floor_ = true;
	float last_vy_ = 0.0f;
	float fill_ = 0.0f;
};

} // namespace tot
