#pragma once
// The game's display lettering (Fredoka, bold) for in-world text: shop signs, road signs and the
// floating pop-ups. The same face as the logo-style headers in the HUD (scripts/hud.gd).

#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace tot {

inline godot::Ref<godot::Font> display_font() {
	static godot::Ref<godot::Font> font;
	if (font.is_valid()) {
		return font;
	}
	const godot::String path = "res://assets/fonts/Fredoka-Variable.ttf";
	godot::Ref<godot::Font> base;
	if (godot::ResourceLoader::get_singleton()->exists(path)) {
		base = godot::ResourceLoader::get_singleton()->load(path);
	}
	if (base.is_null()) {
		// Not imported yet (fresh checkout in a running editor): straight from the file.
		godot::Ref<godot::FontFile> ff;
		ff.instantiate();
		if (ff->load_dynamic_font(godot::ProjectSettings::get_singleton()->globalize_path(path)) == godot::OK) {
			base = ff;
		}
	}
	if (base.is_null()) {
		return font;
	}
	godot::Ref<godot::FontVariation> fv;
	fv.instantiate();
	fv->set_base_font(base);
	godot::Dictionary axes;
	axes[godot::TextServerManager::get_singleton()->get_primary_interface()->name_to_tag("wght")] = 650;
	fv->set_variation_opentype(axes);
	font = fv;
	return font;
}

// A Label3D's text width in metres at its current font, size, outline and pixel size.
inline float label_width(godot::Label3D *l) {
	godot::Ref<godot::Font> f = l->get_font();
	if (f.is_null()) {
		return 0.0f;
	}
	const godot::Vector2 sz = f->get_string_size(l->get_text(), godot::HORIZONTAL_ALIGNMENT_LEFT, -1, l->get_font_size());
	return (sz.x + float(l->get_outline_size())) * l->get_pixel_size();
}

// Shrinks a Label3D's pixel size (never grows it) so its text fits inside max_w x max_h metres,
// measured with its own font, size and outline: lettering always sits inside its sign board.
inline void fit_label(godot::Label3D *l, float max_w, float max_h) {
	godot::Ref<godot::Font> f = l->get_font();
	if (f.is_null() || l->get_text().is_empty()) {
		return;
	}
	const godot::Vector2 sz = f->get_string_size(l->get_text(), godot::HORIZONTAL_ALIGNMENT_LEFT, -1, l->get_font_size());
	const float w = sz.x + float(l->get_outline_size()), h = f->get_height(l->get_font_size()) * 0.82f + float(l->get_outline_size());
	float px = l->get_pixel_size();
	if (w > 0.0f) {
		px = godot::Math::min(px, max_w / w);
	}
	if (h > 0.0f) {
		px = godot::Math::min(px, max_h / h);
	}
	l->set_pixel_size(px);
}

} // namespace tot
