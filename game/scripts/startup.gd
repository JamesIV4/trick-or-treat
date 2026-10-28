extends Control
## The built-in boot screen has no proportional size control, so show the same logo and height as
## the main menu in a short first scene while the game scene loads.

const Hud := preload("res://scripts/hud.gd")
const MAIN_SCENE := "res://scenes/main.tscn"
const MINIMUM_DISPLAY_MS := 750

var _started_ms := 0
var _capture_path := ""
var _capture_frames := 0


func _ready() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-splash="):
			_capture_path = arg.trim_prefix("--capture-splash=")
	var background := ColorRect.new()
	background.color = Color(0.025, 0.03, 0.075)
	background.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var logo := TextureRect.new()
	logo.texture = load(Hud.LOGO_PATH)
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	logo.anchor_right = 1.0
	logo.anchor_top = 0.5
	logo.anchor_bottom = 0.5
	logo.offset_top = -Hud.LOGO_HEIGHT / 2.0
	logo.offset_bottom = Hud.LOGO_HEIGHT / 2.0
	add_child(logo)
	_started_ms = Time.get_ticks_msec()
	if _capture_path.is_empty():
		var error := ResourceLoader.load_threaded_request(MAIN_SCENE)
		if error != OK:
			_show_error()


func _process(_delta: float) -> void:
	if not _capture_path.is_empty():
		_capture_frames += 1
		if _capture_frames >= 3:
			var error := get_viewport().get_texture().get_image().save_png(_capture_path)
			if error != OK:
				push_error("Could not capture the loading screen: ", error)
			else:
				print("CAPTURED ", _capture_path)
			get_tree().quit(0 if error == OK else 1)
		return
	var status := ResourceLoader.load_threaded_get_status(MAIN_SCENE)
	if status == ResourceLoader.THREAD_LOAD_FAILED or status == ResourceLoader.THREAD_LOAD_INVALID_RESOURCE:
		_show_error()
	elif status == ResourceLoader.THREAD_LOAD_LOADED:
		if DisplayServer.get_name() == "headless" or Time.get_ticks_msec() - _started_ms >= MINIMUM_DISPLAY_MS:
			var scene := ResourceLoader.load_threaded_get(MAIN_SCENE) as PackedScene
			if scene == null or get_tree().change_scene_to_packed(scene) != OK:
				_show_error()
			else:
				set_process(false)


func _show_error() -> void:
	set_process(false)
	push_error("Could not load the game scene.")
	var message := Label.new()
	message.text = "Could not load the game. Please try copying it again."
	message.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	message.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	message.offset_top = -100
	add_child(message)
