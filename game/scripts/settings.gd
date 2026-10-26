extends Node
## Autoload: the player's options (the Options menu in scripts/hud.gd), saved to user://settings.cfg
## and applied to the window, the renderer, the main scene's environment and post effects, the
## camera rig, the audio buses and the input map. It also tracks which kind of controller was used
## last, so on-screen prompts show the keys or buttons the player is actually holding.
##
## Scripted runs (anything with user arguments: trailer, screenshots, reviews) skip all of this and
## render with the game as designed (the Highest preset).

signal input_method_changed
signal changed(section: String, key: String)

const PATH := "user://settings.cfg"
const BootScript := preload("res://scripts/boot.gd")

const DEFAULTS := {
	"display": {"window_mode": 0, "resolution": "native", "vsync": 0, "max_fps": 0,
		"brightness": 1.0, "show_fps": false},
	"graphics": {"preset": 3, "shadows": 4, "ssao": 4, "ssil": 2, "ssr": 2, "volumetric_fog": 2,
		"glow": true, "dof": true, "contact_ao": true, "sharpen": true, "vignette": true, "aa": 2, "aniso": 2},
	"gameplay": {"auto_follow": true, "shake": 1.0, "fov": 56, "hint": true},
	"controls": {"camera_speed": 1.0, "invert_camera": false, "deadzone": 0.2, "prompts": 0},
	"audio": {"master": 100, "music": 100, "sfx": 100},
}

## Low, Medium, High, Highest. Highest is the game as designed (the old fixed configuration).
const GFX_PRESETS := [
	{"shadows": 1, "ssao": 0, "ssil": 0, "ssr": 0, "volumetric_fog": 0, "glow": true, "dof": false,
		"contact_ao": false, "sharpen": true, "vignette": true, "aa": 1, "aniso": 1},
	{"shadows": 2, "ssao": 1, "ssil": 0, "ssr": 0, "volumetric_fog": 1, "glow": true, "dof": true,
		"contact_ao": false, "sharpen": true, "vignette": true, "aa": 2, "aniso": 2},
	{"shadows": 3, "ssao": 3, "ssil": 1, "ssr": 1, "volumetric_fog": 2, "glow": true, "dof": true,
		"contact_ao": true, "sharpen": true, "vignette": true, "aa": 2, "aniso": 2},
	{"shadows": 4, "ssao": 4, "ssil": 2, "ssr": 2, "volumetric_fog": 2, "glow": true, "dof": true,
		"contact_ao": true, "sharpen": true, "vignette": true, "aa": 2, "aniso": 2},
]
# Moon shadow per level: atlas size, soft filter quality, max distance (m). Ultra is the original.
const SHADOWS := [[0, 0, 0.0], [2048, 1, 40.0], [4096, 2, 55.0], [8192, 3, 70.0], [8192, 5, 70.0]]
const SSAO_Q := [0, RenderingServer.ENV_SSAO_QUALITY_LOW, RenderingServer.ENV_SSAO_QUALITY_MEDIUM,
	RenderingServer.ENV_SSAO_QUALITY_HIGH, RenderingServer.ENV_SSAO_QUALITY_ULTRA]

const RESOLUTIONS := [Vector2i(1280, 720), Vector2i(1280, 800), Vector2i(1366, 768), Vector2i(1440, 900),
	Vector2i(1600, 900), Vector2i(1680, 1050), Vector2i(1920, 1080), Vector2i(1920, 1200), Vector2i(2560, 1080),
	Vector2i(2560, 1440), Vector2i(2560, 1600), Vector2i(3440, 1440), Vector2i(3840, 1600), Vector2i(3840, 2160)]
const FPS_LIMITS := [0, 30, 60, 75, 90, 120, 144, 165, 240, -1] # 0 unlimited, -1 the monitor's refresh rate

## Rebindable actions: id, name, keyboard column, gamepad column (false: on a stick, not rebindable).
const REBIND := [
	["move_forward", "Move forward", true, false],
	["move_back", "Move back", true, false],
	["move_left", "Move left", true, false],
	["move_right", "Move right", true, false],
	["jump", "Jump, flap and glide", true, true],
	["attack", "Cast fire", true, true],
	["spin", "Magic spin", true, true],
	["roll", "Roll / ground pound", true, true],
	["drop", "Drop from a ledge", true, true],
	["summon_car", "Car: summon, get in or out", true, true],
	["next_costume", "Switch costume", true, true],
	["shop", "Shop", true, true],
	["pause", "Pause", true, true],
	["cam_left", "Turn the camera left", true, false],
	["cam_right", "Turn the camera right", true, false],
	["shop_tab_prev", "Previous shop tab", true, true],
	["shop_tab_next", "Next shop tab", true, true],
	["new_night", "New night", true, true],
]

const XBOX := {JOY_BUTTON_A: "A", JOY_BUTTON_B: "B", JOY_BUTTON_X: "X", JOY_BUTTON_Y: "Y",
	JOY_BUTTON_BACK: "Back", JOY_BUTTON_GUIDE: "Guide", JOY_BUTTON_START: "Start",
	JOY_BUTTON_LEFT_STICK: "LS", JOY_BUTTON_RIGHT_STICK: "RS", JOY_BUTTON_LEFT_SHOULDER: "LB",
	JOY_BUTTON_RIGHT_SHOULDER: "RB", JOY_BUTTON_DPAD_UP: "D-pad up", JOY_BUTTON_DPAD_DOWN: "D-pad down",
	JOY_BUTTON_DPAD_LEFT: "D-pad left", JOY_BUTTON_DPAD_RIGHT: "D-pad right", JOY_BUTTON_MISC1: "Share"}
const PLAYSTATION := {JOY_BUTTON_A: "Cross", JOY_BUTTON_B: "Circle", JOY_BUTTON_X: "Square",
	JOY_BUTTON_Y: "Triangle", JOY_BUTTON_BACK: "Share", JOY_BUTTON_GUIDE: "PS", JOY_BUTTON_START: "Options",
	JOY_BUTTON_LEFT_STICK: "L3", JOY_BUTTON_RIGHT_STICK: "R3", JOY_BUTTON_LEFT_SHOULDER: "L1",
	JOY_BUTTON_RIGHT_SHOULDER: "R1", JOY_BUTTON_DPAD_UP: "D-pad up", JOY_BUTTON_DPAD_DOWN: "D-pad down",
	JOY_BUTTON_DPAD_LEFT: "D-pad left", JOY_BUTTON_DPAD_RIGHT: "D-pad right", JOY_BUTTON_MISC1: "Mute",
	JOY_BUTTON_TOUCHPAD: "Touchpad"}

var method := "kb" # the controller used last: "kb" (keyboard and mouse) or "pad"
var scripted := false
var _pad_name := ""
var _cfg := ConfigFile.new()
var _fps: Label
var _fps_t := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	scripted = not OS.get_cmdline_user_args().is_empty()
	var pads := Input.get_connected_joypads()
	if not pads.is_empty():
		method = "pad"
		_pad_name = Input.get_joy_name(pads[0])
	for bus in ["Music", "SFX"]:
		if AudioServer.get_bus_index(bus) < 0:
			AudioServer.add_bus()
			AudioServer.set_bus_name(AudioServer.bus_count - 1, bus)
			AudioServer.set_bus_send(AudioServer.bus_count - 1, "Master")
	if scripted:
		# Reviews and captures keep their plain window unless they asked for fullscreen.
		var args := OS.get_cmdline_args()
		if not (args.has("-f") or args.has("--fullscreen")):
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			var size := Vector2i(int(ProjectSettings.get_setting("display/window/size/window_width_override", 1600)),
				int(ProjectSettings.get_setting("display/window/size/window_height_override", 900)))
			_place_window(size)
		return
	_cfg.load(PATH) # missing on a first run: everything stays at its default
	_apply_bindings()
	_apply_display()
	_apply_audio()
	_apply_controls()
	# The main scene's nodes (environment, post effects, camera rig) are ready after this frame.
	await get_tree().process_frame
	_apply_scene()


func _input(event: InputEvent) -> void:
	var m := ""
	if event is InputEventKey or event is InputEventMouseButton:
		if event.is_pressed():
			m = "kb"
	elif event is InputEventMouseMotion:
		if (event as InputEventMouseMotion).relative.length() > 6.0:
			m = "kb"
	elif event is InputEventJoypadButton:
		if event.is_pressed():
			m = "pad"
			_pad_name = Input.get_joy_name(event.device)
	elif event is InputEventJoypadMotion:
		if absf((event as InputEventJoypadMotion).axis_value) > 0.5:
			m = "pad"
			_pad_name = Input.get_joy_name(event.device)
	if m != "" and m != method:
		method = m
		input_method_changed.emit()


func _process(delta: float) -> void:
	if _fps and _fps.visible:
		_fps_t -= delta
		if _fps_t <= 0.0:
			_fps_t = 0.25
			_fps.text = "%d fps" % Engine.get_frames_per_second()


# ---------------------------------------------------------------- values
func get_value(section: String, key: String) -> Variant:
	return _cfg.get_value(section, key, DEFAULTS[section][key])


func set_value(section: String, key: String, value: Variant) -> void:
	_cfg.set_value(section, key, value)
	if section == "graphics":
		if key == "preset":
			if int(value) < GFX_PRESETS.size():
				var p: Dictionary = GFX_PRESETS[int(value)]
				for k: String in p:
					_cfg.set_value("graphics", k, p[k])
		else:
			_cfg.set_value("graphics", "preset", _matching_preset())
	_apply(section)
	_cfg.save(PATH)
	changed.emit(section, key)
	if section == "controls" and key == "prompts":
		input_method_changed.emit()


## Puts a whole section back to its defaults (Controls also resets the key and button bindings).
func reset_section(section: String) -> void:
	if _cfg.has_section(section):
		_cfg.erase_section(section)
	if section == "controls":
		if _cfg.has_section("bindings"):
			_cfg.erase_section("bindings")
		for row in REBIND:
			_restore_default(row[0], "kb")
			_restore_default(row[0], "pad")
	_apply(section)
	_cfg.save(PATH)
	changed.emit(section, "")
	input_method_changed.emit()


func _matching_preset() -> int:
	for i in GFX_PRESETS.size():
		var p: Dictionary = GFX_PRESETS[i]
		var same := true
		for k: String in p:
			if get_value("graphics", k) != p[k]:
				same = false
				break
		if same:
			return i
	return GFX_PRESETS.size() # Custom


func _apply(section: String) -> void:
	if scripted:
		return
	match section:
		"display":
			_apply_display()
			_apply_scene()
		"graphics", "gameplay":
			_apply_scene()
		"controls":
			_apply_controls()
		"audio":
			_apply_audio()


# ---------------------------------------------------------------- the Options menu's rows
## The rows of an Options tab. A choice: {section, key, label, desc, values, labels}; a slider:
## {section, key, label, desc, min, max, step, fmt, scale}; the Controls tab's bindings follow its rows.
func rows(tab: String) -> Array:
	var on_off := ["On", "Off"]
	match tab:
		"Gameplay":
			return [
				_choice("gameplay", "auto_follow", "Camera auto-follow", "While you run or drive one way, the camera swings round behind you.", [true, false], on_off),
				_choice("gameplay", "shake", "Camera shake", "The jolt from ground pounds, explosions and bumps.", [0.0, 0.5, 1.0], ["Off", "Low", "Full"]),
				_range("gameplay", "fov", "Field of view", "How wide the camera sees.", 45, 75, 1, "%d°"),
				_choice("gameplay", "hint", "Controls hint", "The line of controls at the bottom of the screen when a night starts.", [true, false], ["Show", "Hide"]),
			]
		"Controls":
			return [
				_range("controls", "camera_speed", "Camera turn speed", "How fast the camera turns with {camera}{kb: (or by dragging with the right mouse button)|pad:}.", 0.25, 2.5, 0.25, "%.2fx"),
				_choice("controls", "invert_camera", "Invert camera turning", "Swap which way the camera turns.", [false, true], ["Off", "On"]),
				_range("controls", "deadzone", "Stick dead zone", "{kb:Gamepad only: how far a stick moves before it counts.|pad:How far a stick moves before it counts. Raise it if the kid or the camera drifts.}", 0.05, 0.5, 0.05, "%.2f"),
				_choice("controls", "prompts", "Button prompts", "Which keys or buttons the game shows. Auto follows whatever you used last.", [0, 1, 2, 3], ["Auto (last used)", "Keyboard", "Xbox", "PlayStation"]),
			]
		"Display":
			var res_values := ["native"]
			var res_labels := ["Desktop (%s)" % _res_text(_screen_size())]
			for r: Vector2i in resolutions():
				res_values.append("%dx%d" % [r.x, r.y])
				res_labels.append(_res_text(r))
			var fps_labels := []
			for f: int in FPS_LIMITS:
				fps_labels.append("Unlimited" if f == 0 else ("Monitor (%d Hz)" % _refresh_rate() if f < 0 else "%d" % f))
			return [
				_choice("display", "window_mode", "Window mode", "Borderless fullscreen switches apps instantly. Exclusive fullscreen can have a little less latency.", [0, 1, 2], ["Borderless fullscreen", "Exclusive fullscreen", "Windowed"]),
				_choice("display", "resolution", "Resolution", "The window's size in windowed mode. Fullscreen always uses the screen's own resolution.", res_values, res_labels),
				_choice("display", "vsync", "VSync", "Syncs frames to the monitor: no tearing, a little more input latency.", [0, 1, 2], ["Off", "On", "Adaptive"]),
				_choice("display", "max_fps", "Frame rate limit", "Caps the frame rate (saves power and heat).", FPS_LIMITS, fps_labels),
				_range("display", "brightness", "Brightness", "Makes the night lighter or darker.", 0.6, 1.4, 0.05, "%d%%", 100.0),
				_choice("display", "show_fps", "Show frame rate", "A small frame-rate counter in the corner.", [false, true], ["Off", "On"]),
			]
		"Graphics":
			var levels := ["Off", "Low", "Medium", "High", "Ultra"]
			var three := ["Off", "Low", "High"]
			return [
				_choice("graphics", "preset", "Preset", "Sets everything below at once. Highest is the game as it was made to look. Change anything below for a custom mix.", [0, 1, 2, 3, 4], ["Low", "Medium", "High", "Highest", "Custom"]),
				_choice("graphics", "shadows", "Shadows", "Moonlight shadows: their sharpness and how far they reach.", [0, 1, 2, 3, 4], levels),
				_choice("graphics", "ssao", "Ambient occlusion", "Soft shading where things meet (SSAO).", [0, 1, 2, 3, 4], levels),
				_choice("graphics", "contact_ao", "Contact shading", "Extra-fine darkening right where objects touch the ground.", [true, false], on_off),
				_choice("graphics", "ssil", "Indirect light", "Glows from lanterns and windows bouncing onto nearby surfaces (SSIL).", [0, 1, 2], three),
				_choice("graphics", "ssr", "Reflections", "Screen-space reflections on shiny surfaces.", [0, 1, 2], three),
				_choice("graphics", "volumetric_fog", "Volumetric fog", "The misty light shafts around lamps and the moon.", [0, 1, 2], three),
				_choice("graphics", "glow", "Glow", "Bloom around candy, magic and lights.", [true, false], on_off),
				_choice("graphics", "dof", "Depth of field", "The soft toy-lens blur far away and very close up.", [true, false], on_off),
				_choice("graphics", "aa", "Anti-aliasing", "Smooths jagged edges. MSAA is sharpest; FXAA is cheapest; TAA is smoothest.", [0, 1, 2, 3, 4, 5], ["Off", "FXAA", "MSAA 2x", "MSAA 4x", "MSAA 8x", "TAA"]),
				_choice("graphics", "aniso", "Texture filtering", "Keeps textures crisp at shallow angles (anisotropic filtering).", [0, 1, 2, 3, 4], ["Off", "2x", "4x", "8x", "16x"]),
				_choice("graphics", "sharpen", "Sharpen", "A light sharpening pass.", [true, false], on_off),
				_choice("graphics", "vignette", "Vignette", "Darkened screen corners.", [true, false], on_off),
			]
		"Audio":
			return [
				_range("audio", "master", "Master volume", "Everything.", 0, 100, 5, "%d%%"),
				_range("audio", "music", "Music", "The night's music.", 0, 100, 5, "%d%%"),
				_range("audio", "sfx", "Sound effects", "Candy, doorbells, fire, monsters and everything else.", 0, 100, 5, "%d%%"),
			]
	return []


func _choice(section: String, key: String, label: String, desc: String, values: Array, labels: Array) -> Dictionary:
	return {"section": section, "key": key, "label": label, "desc": desc, "values": values, "labels": labels}


func _range(section: String, key: String, label: String, desc: String, lo: float, hi: float, step: float, fmt: String, scale := 1.0) -> Dictionary:
	return {"section": section, "key": key, "label": label, "desc": desc, "min": lo, "max": hi, "step": step, "fmt": fmt, "scale": scale}


## Common sizes that fit the screen (the desktop size itself is the "native" entry).
func resolutions() -> Array:
	var screen := _screen_size()
	var out := []
	for r: Vector2i in RESOLUTIONS:
		if r.x <= screen.x and r.y <= screen.y and r != screen:
			out.append(r)
	return out


func _screen_size() -> Vector2i:
	return DisplayServer.screen_get_size(DisplayServer.window_get_current_screen())


func _refresh_rate() -> int:
	var hz := DisplayServer.screen_get_refresh_rate(DisplayServer.window_get_current_screen())
	return roundi(hz) if hz > 0.0 else 60


func _res_text(r: Vector2i) -> String:
	return "%d × %d" % [r.x, r.y]


func _resolution() -> Vector2i:
	var s: String = str(get_value("display", "resolution"))
	var parts := s.split("x")
	if parts.size() == 2 and parts[0].is_valid_int() and parts[1].is_valid_int():
		return Vector2i(parts[0].to_int(), parts[1].to_int())
	return _screen_size()


# ---------------------------------------------------------------- applying
func _place_window(size: Vector2i) -> void:
	var screen := DisplayServer.window_get_current_screen()
	var ss := DisplayServer.screen_get_size(screen)
	DisplayServer.window_set_size(size)
	DisplayServer.window_set_position(DisplayServer.screen_get_position(screen) + (ss - size) / 2)


func _apply_display() -> void:
	var modes := [DisplayServer.WINDOW_MODE_FULLSCREEN, DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN, DisplayServer.WINDOW_MODE_WINDOWED]
	var mode: int = modes[clampi(int(get_value("display", "window_mode")), 0, 2)]
	var screen := _screen_size()
	var res := _resolution()
	var root := get_tree().root
	if mode == DisplayServer.WINDOW_MODE_WINDOWED:
		if res.x >= screen.x or res.y >= screen.y:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MAXIMIZED)
		else:
			if DisplayServer.window_get_mode() != mode:
				DisplayServer.window_set_mode(mode)
			_place_window(res)
	elif DisplayServer.window_get_mode() != mode:
		DisplayServer.window_set_mode(mode)
	# No upscaling: the 3D view always renders at the window's full size.
	root.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
	root.scaling_3d_scale = 1.0
	var vsyncs := [DisplayServer.VSYNC_DISABLED, DisplayServer.VSYNC_ENABLED, DisplayServer.VSYNC_ADAPTIVE]
	DisplayServer.window_set_vsync_mode(vsyncs[clampi(int(get_value("display", "vsync")), 0, 2)])
	var fps := int(get_value("display", "max_fps"))
	Engine.max_fps = _refresh_rate() if fps < 0 else fps
	var show_fps := bool(get_value("display", "show_fps"))
	if show_fps and _fps == null:
		var layer := CanvasLayer.new()
		layer.layer = 100
		add_child(layer)
		_fps = Label.new()
		_fps.add_theme_font_size_override("font_size", 18)
		_fps.add_theme_color_override("font_color", Color(0.8, 1.0, 0.8))
		_fps.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.8))
		_fps.add_theme_constant_override("outline_size", 5)
		_fps.anchor_left = 1.0
		_fps.anchor_right = 1.0
		_fps.anchor_top = 1.0
		_fps.anchor_bottom = 1.0
		_fps.offset_left = -130
		_fps.offset_top = -40
		_fps.offset_right = -16
		_fps.offset_bottom = -12
		_fps.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		layer.add_child(_fps)
	if _fps:
		_fps.visible = show_fps


## The renderer and the main scene: environment effects, the moon's shadow, the lens, post effects
## and the camera rig.
func _apply_scene() -> void:
	var g := {}
	for k: String in DEFAULTS["graphics"]:
		g[k] = get_value("graphics", k)
	var root := get_tree().root
	var aa := int(g["aa"])
	var msaa := [Viewport.MSAA_DISABLED, Viewport.MSAA_DISABLED, Viewport.MSAA_2X, Viewport.MSAA_4X, Viewport.MSAA_8X, Viewport.MSAA_DISABLED]
	root.msaa_3d = msaa[clampi(aa, 0, 5)]
	root.screen_space_aa = Viewport.SCREEN_SPACE_AA_FXAA if aa == 1 else Viewport.SCREEN_SPACE_AA_DISABLED
	root.use_taa = aa == 5
	if "anisotropic_filtering_level" in root:
		root.set("anisotropic_filtering_level", clampi(int(g["aniso"]), 0, 4))
	var sh: Array = SHADOWS[clampi(int(g["shadows"]), 0, 4)]
	if int(sh[0]) > 0:
		RenderingServer.directional_shadow_atlas_set_size(int(sh[0]), true)
		RenderingServer.directional_soft_shadow_filter_set_quality(int(sh[1]))
		RenderingServer.positional_soft_shadow_filter_set_quality(int(sh[1]))
	var ssao := clampi(int(g["ssao"]), 0, 4)
	if ssao > 0:
		RenderingServer.environment_set_ssao_quality(SSAO_Q[ssao], ssao < 3,
			float(ProjectSettings.get_setting("rendering/environment/ssao/adaptive_target", 0.5)),
			int(ProjectSettings.get_setting("rendering/environment/ssao/blur_passes", 2)),
			float(ProjectSettings.get_setting("rendering/environment/ssao/fadeout_from", 50.0)),
			float(ProjectSettings.get_setting("rendering/environment/ssao/fadeout_to", 300.0)))
	var ssil := clampi(int(g["ssil"]), 0, 2)
	if ssil > 0:
		RenderingServer.environment_set_ssil_quality(
			RenderingServer.ENV_SSIL_QUALITY_LOW if ssil == 1 else RenderingServer.ENV_SSIL_QUALITY_MEDIUM, true,
			float(ProjectSettings.get_setting("rendering/environment/ssil/adaptive_target", 0.5)),
			int(ProjectSettings.get_setting("rendering/environment/ssil/blur_passes", 4)),
			float(ProjectSettings.get_setting("rendering/environment/ssil/fadeout_from", 50.0)),
			float(ProjectSettings.get_setting("rendering/environment/ssil/fadeout_to", 300.0)))
	var vfog := clampi(int(g["volumetric_fog"]), 0, 2)
	if vfog > 0:
		RenderingServer.environment_set_volumetric_fog_volume_size(40 if vfog == 1 else 64, 40 if vfog == 1 else 64)
		RenderingServer.environment_set_volumetric_fog_filter_active(vfog == 2)
	var scene := get_tree().current_scene
	if scene == null:
		return
	var moon := scene.get_node_or_null("Moon") as DirectionalLight3D
	if moon:
		moon.shadow_enabled = int(sh[0]) > 0
		moon.directional_shadow_max_distance = maxf(float(sh[2]), 1.0)
	var we := scene.get_node_or_null("WorldEnvironment") as WorldEnvironment
	if we and we.environment:
		var env := we.environment
		env.ssao_enabled = ssao > 0
		env.ssil_enabled = ssil > 0
		env.ssr_enabled = int(g["ssr"]) > 0
		env.ssr_max_steps = 24 if int(g["ssr"]) == 1 else 48
		env.volumetric_fog_enabled = vfog > 0
		env.glow_enabled = bool(g["glow"])
		var bright := float(get_value("display", "brightness"))
		env.adjustment_enabled = not is_equal_approx(bright, 1.0)
		env.adjustment_brightness = bright
		var lens := we.camera_attributes as CameraAttributesPractical
		if lens:
			lens.dof_blur_far_enabled = bool(g["dof"])
			lens.dof_blur_near_enabled = bool(g["dof"])
	var post := scene.get_node_or_null("PostFX")
	if post and post.has_method("set_effects"):
		post.set_effects(bool(g["contact_ao"]), bool(g["sharpen"]), bool(g["vignette"]))
	_apply_camera()


func _apply_camera() -> void:
	var scene := get_tree().current_scene
	var rig := scene.get_node_or_null("CameraRig") if scene else null
	if rig == null:
		return
	# The rig's option setters arrive with the native rebuild; older builds just keep their defaults.
	if rig.has_method("set_orbit_speed"):
		rig.set_orbit_speed(float(get_value("controls", "camera_speed")))
		rig.set_invert_orbit(bool(get_value("controls", "invert_camera")))
		rig.set_auto_follow(bool(get_value("gameplay", "auto_follow")))
		rig.set_shake_scale(float(get_value("gameplay", "shake")))
		rig.set_base_fov(float(get_value("gameplay", "fov")))


func _apply_controls() -> void:
	var dz := float(get_value("controls", "deadzone"))
	for action: String in ["move_left", "move_right", "move_forward", "move_back", "cam_left", "cam_right"]:
		if InputMap.has_action(action):
			InputMap.action_set_deadzone(action, dz)
	_apply_camera()


func _apply_audio() -> void:
	for pair in [["Master", "master"], ["Music", "music"], ["SFX", "sfx"]]:
		var idx := AudioServer.get_bus_index(pair[0])
		if idx < 0:
			continue
		var v := float(get_value("audio", pair[1])) / 100.0
		AudioServer.set_bus_mute(idx, v <= 0.001)
		AudioServer.set_bus_volume_db(idx, linear_to_db(maxf(v, 0.001)))


# ---------------------------------------------------------------- bindings
func _apply_bindings() -> void:
	if not _cfg.has_section("bindings"):
		return
	for key: String in _cfg.get_section_keys("bindings"):
		var parts := key.split(".")
		if parts.size() != 2 or not InputMap.has_action(parts[0]):
			continue
		var ev := _decode(_cfg.get_value("bindings", key))
		if ev:
			_replace(parts[0], parts[1], ev)


## Rebinds an action's keyboard ("kb": a key or mouse button) or gamepad ("pad": a button or
## trigger) input; sticks stay as they are.
func set_binding(action: String, kind: String, ev: InputEvent) -> void:
	_replace(action, kind, ev)
	_cfg.set_value("bindings", "%s.%s" % [action, kind], _encode(ev))
	_cfg.save(PATH)
	input_method_changed.emit()


func _is_kind(ev: InputEvent, kind: String) -> bool:
	if kind == "kb":
		return ev is InputEventKey or ev is InputEventMouseButton
	if ev is InputEventJoypadMotion:
		var axis := (ev as InputEventJoypadMotion).axis
		return axis == JOY_AXIS_TRIGGER_LEFT or axis == JOY_AXIS_TRIGGER_RIGHT
	return ev is InputEventJoypadButton


func _replace(action: String, kind: String, ev: InputEvent) -> void:
	for old in InputMap.action_get_events(action):
		if _is_kind(old, kind):
			InputMap.action_erase_event(action, old)
	InputMap.action_add_event(action, ev)


func _restore_default(action: String, kind: String) -> void:
	for old in InputMap.action_get_events(action):
		if _is_kind(old, kind):
			InputMap.action_erase_event(action, old)
	if kind == "kb":
		for key: int in BootScript.KEYS.get(action, []):
			var k := InputEventKey.new()
			k.physical_keycode = key
			InputMap.action_add_event(action, k)
		for button: int in BootScript.MOUSE.get(action, []):
			var m := InputEventMouseButton.new()
			m.button_index = button
			InputMap.action_add_event(action, m)
	else:
		for button: int in BootScript.BUTTONS.get(action, []):
			var b := InputEventJoypadButton.new()
			b.button_index = button
			InputMap.action_add_event(action, b)
		for axis: int in BootScript.TRIGGERS.get(action, []):
			var t := InputEventJoypadMotion.new()
			t.axis = axis
			t.axis_value = 1.0
			InputMap.action_add_event(action, t)


func _encode(ev: InputEvent) -> Dictionary:
	if ev is InputEventKey:
		var k := ev as InputEventKey
		return {"t": "key", "c": k.physical_keycode if k.physical_keycode != KEY_NONE else k.keycode}
	if ev is InputEventMouseButton:
		return {"t": "mouse", "b": (ev as InputEventMouseButton).button_index}
	if ev is InputEventJoypadMotion:
		return {"t": "axis", "a": (ev as InputEventJoypadMotion).axis}
	return {"t": "joy", "b": (ev as InputEventJoypadButton).button_index}


func _decode(d: Variant) -> InputEvent:
	if not d is Dictionary:
		return null
	match str(d.get("t", "")):
		"key":
			var k := InputEventKey.new()
			k.physical_keycode = int(d.get("c", 0))
			return k
		"mouse":
			var m := InputEventMouseButton.new()
			m.button_index = int(d.get("b", 1))
			return m
		"axis":
			var a := InputEventJoypadMotion.new()
			a.axis = int(d.get("a", JOY_AXIS_TRIGGER_RIGHT))
			a.axis_value = 1.0
			return a
		"joy":
			var b := InputEventJoypadButton.new()
			b.button_index = int(d.get("b", 0))
			return b
	return null


# ---------------------------------------------------------------- prompts
## "kb", "xbox" or "ps": what on-screen prompts show right now.
func prompt_style() -> String:
	match int(get_value("controls", "prompts")):
		1:
			return "kb"
		2:
			return "xbox"
		3:
			return "ps"
	return "kb" if method == "kb" else pad_style()


func pad_style() -> String:
	var n := _pad_name.to_lower()
	for tag in ["ps3", "ps4", "ps5", "playstation", "dualshock", "dualsense", "sony"]:
		if n.contains(tag):
			return "ps"
	var p := int(get_value("controls", "prompts"))
	return "ps" if p == 3 else "xbox"


## The key or button for an action in the current prompt style. Also "move" and "camera".
func glyph(action: String, style := "") -> String:
	if style == "":
		style = prompt_style()
	if style == "kb":
		if action == "move":
			var keys := []
			var short := true
			for a: String in ["move_forward", "move_left", "move_back", "move_right"]:
				var s := key_label(a)
				short = short and s.length() == 1
				keys.append(s)
			return "".join(keys) if short else "/".join(keys)
		if action == "camera":
			return "%s / %s" % [key_label("cam_left"), key_label("cam_right")]
		return key_label(action, true)
	if action == "move":
		return "Left stick"
	if action == "camera":
		return "Right stick"
	return pad_label(action, style)


## An action's keyboard binding: its first key (and, with `with_mouse`, its mouse button too).
func key_label(action: String, with_mouse := false) -> String:
	var key := ""
	var mouse := ""
	for ev in InputMap.action_get_events(action):
		if ev is InputEventKey and key == "":
			var k := ev as InputEventKey
			var code := k.keycode
			if k.physical_keycode != KEY_NONE and DisplayServer.get_name() != "headless":
				code = DisplayServer.keyboard_get_keycode_from_physical(k.physical_keycode)
			key = OS.get_keycode_string(code)
			key = {"Escape": "Esc", "PageUp": "Page Up", "PageDown": "Page Down"}.get(key, key)
		elif ev is InputEventMouseButton and mouse == "":
			mouse = {MOUSE_BUTTON_LEFT: "Click", MOUSE_BUTTON_RIGHT: "Right click", MOUSE_BUTTON_MIDDLE: "Middle click",
				MOUSE_BUTTON_XBUTTON1: "Mouse 4", MOUSE_BUTTON_XBUTTON2: "Mouse 5"}.get((ev as InputEventMouseButton).button_index, "Mouse")
	if key != "" and mouse != "":
		return "%s / %s" % [key, mouse] if with_mouse else key
	return key if key != "" else (mouse if mouse != "" else "—")


## An action's gamepad binding in a button style ("xbox" or "ps").
func pad_label(action: String, style := "") -> String:
	if style == "" or style == "kb":
		style = pad_style()
	var names: Dictionary = PLAYSTATION if style == "ps" else XBOX
	for ev in InputMap.action_get_events(action):
		if ev is InputEventJoypadButton:
			return str(names.get((ev as InputEventJoypadButton).button_index, "Button %d" % (ev as InputEventJoypadButton).button_index))
		if ev is InputEventJoypadMotion:
			var axis := (ev as InputEventJoypadMotion).axis
			if axis == JOY_AXIS_TRIGGER_LEFT:
				return "L2" if style == "ps" else "LT"
			if axis == JOY_AXIS_TRIGGER_RIGHT:
				return "R2" if style == "ps" else "RT"
	return "—"


## Fills {action} tokens in a line of text with the current keys or buttons, e.g. "Press {shop}", and
## picks {kb:keyboard words|pad:gamepad words} for whichever controller the prompts show right now,
## so hints never name both.
func prompts(text: String) -> String:
	if not text.contains("{"):
		return text
	var out := text
	var pick := RegEx.create_from_string("\\{kb:([^|}]*)\\|pad:([^}]*)\\}")
	var kb := prompt_style() == "kb"
	for m in pick.search_all(text):
		out = out.replace(m.get_string(0), m.get_string(1) if kb else m.get_string(2))
	var re := RegEx.create_from_string("\\{([a-z_]+)\\}")
	text = out
	for m in re.search_all(text):
		var action := m.get_string(1)
		if action == "move" or action == "camera" or InputMap.has_action(action):
			out = out.replace(m.get_string(0), glyph(action))
	return out
