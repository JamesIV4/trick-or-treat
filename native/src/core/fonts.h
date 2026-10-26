#pragma once
// The game's display lettering (Fredoka, bold) for in-world text: shop signs, road signs and the
// floating pop-ups. The same face as the logo-style headers in the HUD (scripts/hud.gd).

#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/font_variation.hpp>
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

} // namespace tot
