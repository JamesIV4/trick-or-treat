#pragma once
// Drop shadows straight below airborne things: the 3D Mario rule that anything in the air has a
// shadow directly beneath it, so players can read where they will land and where to jump for a
// floating pickup (Hayashida and Motokura, Iwata Asks: Super Mario 3D Land). A projected decal
// box that reaches several metres down, so the blob lands on whatever surface is below.

#include <godot_cpp/classes/decal.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/visual_instance3d.hpp>

namespace tot {

// Visual layer for things that must not receive drop shadows themselves (the kid, candy).
constexpr uint32_t LAYER_NO_DECALS = 1u << 10;

// Soft round blob in `ink` (optionally premultiplied), built synchronously (the decal atlas does not pick up lazily generated
// textures).
inline godot::Ref<godot::Texture2D> make_blob_texture(const godot::Color &ink, bool premultiply = false) {
	using namespace godot;
	constexpr int S = 64;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < S; ++y) {
		for (int x = 0; x < S; ++x) {
			const float r = Vector2(x + 0.5f - S * 0.5f, y + 0.5f - S * 0.5f).length() / (S * 0.5f);
			const float a = r >= 1.0f ? 0.0f : (r < 0.55f ? 1.0f - r * 0.5f : 0.72f * (1.0f - (r - 0.55f) / 0.45f));
			img->set_pixel(x, y, Color(premultiply ? ink * a : ink, a));
		}
	}
	img->generate_mipmaps();
	return ImageTexture::create_from_image(img);
}

inline godot::Ref<godot::Texture2D> make_shadow_texture() {
	return make_blob_texture(godot::Color(0.02f, 0.03f, 0.1f));
}

// Night version of the rule for floating candy: a dark blob would vanish on the dark street, so
// candy marks the spot below with a small pool of its own glow instead.
inline godot::Ref<godot::Texture2D> make_glow_texture() {
	return make_blob_texture(godot::Color(1, 1, 1), true); // emission ignores alpha
}

// The decal's box hangs below its origin: top `lift` above the node, `depth` deep.
inline godot::Decal *make_drop_shadow(const godot::Ref<godot::Texture2D> &tex, float diameter, float depth, const godot::Color &modulate,
		float emission = 0.0f) {
	using namespace godot;
	Decal *d = memnew(Decal);
	d->set_size(Vector3(diameter, depth, diameter));
	d->set_texture(Decal::TEXTURE_ALBEDO, tex);
	if (emission > 0.0f) {
		d->set_texture(Decal::TEXTURE_EMISSION, tex);
		d->set_emission_energy(emission);
	}
	d->set_modulate(modulate);
	d->set_upper_fade(0.02f);
	d->set_lower_fade(0.9f);
	d->set_cull_mask(0xFFFFFu & ~LAYER_NO_DECALS);
	d->set_enable_distance_fade(true);
	d->set_distance_fade_begin(32.0f);
	d->set_distance_fade_length(10.0f);
	return d;
}

inline void set_layers_recursive(godot::Node *n, uint32_t layers) {
	if (godot::VisualInstance3D *vi = godot::Object::cast_to<godot::VisualInstance3D>(n)) {
		vi->set_layer_mask(layers);
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		set_layers_recursive(n->get_child(i), layers);
	}
}

} // namespace tot
