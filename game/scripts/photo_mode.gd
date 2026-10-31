extends Node
## Position a camera while paused, then play through that fixed view.

signal finished(return_to_pause: bool)

const MAX_DISTANCE := 70.0
const PHOTO_FOLDER := "Halloween - Trick or Treat"

var _camera: Camera3D
var _previous: Camera3D
var _rig: Node
var _origin: Vector3
var _yaw: float
var _pitch: float
var _overlay: CanvasLayer
var _panel: PanelContainer
var _status: Label
var _state: Label
var _rows: Array[HBoxContainer] = []
var _active := false
var _capturing := false
var _opened_frame := -1
var _playing := false
var _hud_visible := false


func start() -> void:
	_previous = get_viewport().get_camera_3d()
	if not _previous:
		finished.emit(true)
		queue_free()
		return
	process_mode = Node.PROCESS_MODE_ALWAYS
	_rig = _previous.get_parent()
	_rig.set_process(false)
	_camera = Camera3D.new()
	_camera.process_mode = Node.PROCESS_MODE_ALWAYS
	get_tree().current_scene.add_child(_camera)
	_camera.global_transform = _previous.global_transform
	_camera.fov = _previous.fov
	_camera.near = _previous.near
	_camera.far = _previous.far
	_camera.cull_mask = _previous.cull_mask
	_camera.attributes = _previous.attributes
	_origin = _camera.global_position
	var rot := _camera.global_rotation
	_yaw = rot.y
	_pitch = rot.x
	RenderingServer.global_shader_parameter_set("cutout_radius", 0.0)
	_camera.make_current()
	_build_overlay()
	Settings.input_method_changed.connect(_refresh_controls)
	_active = true
	_opened_frame = Engine.get_process_frames()


func _build_overlay() -> void:
	_overlay = CanvasLayer.new()
	_overlay.layer = 20
	add_child(_overlay)
	_panel = PanelContainer.new()
	_panel.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	_panel.offset_left = 24
	_panel.offset_bottom = -24
	_panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_panel.custom_minimum_size.x = 560
	_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var hud := get_parent()
	_panel.theme = hud._root.theme
	_panel.add_theme_stylebox_override("panel", hud._box(Color(0.7, 0.55, 1.0)))
	_overlay.add_child(_panel)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 4)
	_panel.add_child(content)
	var header := HBoxContainer.new()
	content.add_child(header)
	var title: Label = hud._display_label(30, Color(1.0, 0.93, 0.8))
	title.text = "Photo mode"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	_state = hud._label(19, Color(0.7, 0.55, 1.0))
	_state.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	header.add_child(_state)
	content.add_child(HSeparator.new())
	for i in 9:
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 24)
		var action: Label = hud._label(19, Color(1.0, 0.93, 0.8))
		action.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(action)
		var input: Label = hud._label(19, Color(0.45, 1.0, 0.85))
		input.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		row.add_child(input)
		content.add_child(row)
		_rows.append(row)
	_status = hud._label(16, Color(0.55, 1.0, 0.8))
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size.x = 520
	_status.visible = false
	content.add_child(_status)
	_refresh_controls()


func _refresh_controls() -> void:
	if not _state:
		return
	_state.text = "PLAYING" if _playing else "PAUSED"
	var keyboard := Settings.method == "kb"
	var names: Dictionary = Settings.PLAYSTATION if Settings.pad_style() == "ps" else Settings.XBOX
	var entries: Array = []
	if not _playing:
		entries.append_array([
			["Move camera", "W A S D" if keyboard else "Left stick"],
			["Rise / lower", "E / Q" if keyboard else "%s / %s" % [names[JOY_BUTTON_RIGHT_SHOULDER], names[JOY_BUTTON_LEFT_SHOULDER]]],
			["Look around", "Arrows / right mouse drag" if keyboard else "Right stick"],
			["Field of view", "Z / X" if keyboard else "D-pad left / right"],
		])
	entries.append_array([
		["Pause and move camera" if _playing else "Play from this view", "F6" if keyboard else str(names[JOY_BUTTON_RIGHT_STICK])],
		["Hide game HUD" if _hud_visible else "Show game HUD", "H" if keyboard else "D-pad down"],
		["Hide photo UI", "G" if keyboard else "D-pad up"],
		["Save photo", "P" if keyboard else ("D-pad right" if _playing else str(names[JOY_BUTTON_X]))],
		["Return to pause menu", "Esc" if keyboard else str(names[JOY_BUTTON_BACK])],
	])
	for i in _rows.size():
		var row := _rows[i]
		row.visible = i < entries.size()
		if row.visible:
			(row.get_child(0) as Label).text = entries[i][0]
			(row.get_child(1) as Label).text = entries[i][1]


func _toggle_hud() -> void:
	_hud_visible = not _hud_visible
	get_parent()._set_photo_hud_visible(_hud_visible)
	_refresh_controls()


func _toggle_play() -> void:
	_playing = not _playing
	if _playing:
		# This is the same transition used when closing the normal pause menu.
		get_tree().current_scene._on_menu_closed()
	else:
		get_tree().paused = true
	_refresh_controls()


func _input(event: InputEvent) -> void:
	if not _active or Engine.get_process_frames() == _opened_frame or _capturing:
		return
	if not _playing and event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		_yaw -= event.relative.x * 0.003
		_pitch = clampf(_pitch - event.relative.y * 0.003, -1.55, 1.55)
		get_viewport().set_input_as_handled()
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_ESCAPE:
				_exit_photo()
			KEY_F6:
				_toggle_play()
			KEY_H:
				_toggle_hud()
			KEY_G:
				_panel.visible = not _panel.visible
			KEY_P:
				_save_photo()
			KEY_N:
				if _playing:
					_exit_photo(false)
	elif event is InputEventJoypadButton and event.pressed:
		match event.button_index:
			JOY_BUTTON_BACK:
				_exit_photo()
			JOY_BUTTON_RIGHT_STICK:
				_toggle_play()
			JOY_BUTTON_DPAD_DOWN:
				_toggle_hud()
			JOY_BUTTON_DPAD_UP:
				_panel.visible = not _panel.visible
			JOY_BUTTON_DPAD_RIGHT:
				if _playing:
					_save_photo()
			JOY_BUTTON_X:
				if not _playing:
					_save_photo()


func _process(delta: float) -> void:
	if not _active or _capturing or _playing:
		return
	var pads := Input.get_connected_joypads()
	var pad: int = pads[0] if not pads.is_empty() else -1
	var deadzone := float(Settings.get_value("controls", "deadzone"))
	var move := Vector2(float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)), float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W)))
	var look := Vector2(float(Input.is_physical_key_pressed(KEY_RIGHT)) - float(Input.is_physical_key_pressed(KEY_LEFT)), float(Input.is_physical_key_pressed(KEY_DOWN)) - float(Input.is_physical_key_pressed(KEY_UP)))
	var up := float(Input.is_physical_key_pressed(KEY_E)) - float(Input.is_physical_key_pressed(KEY_Q))
	var zoom := float(Input.is_physical_key_pressed(KEY_X)) - float(Input.is_physical_key_pressed(KEY_Z))
	if pad >= 0:
		var stick := Vector2(Input.get_joy_axis(pad, JOY_AXIS_LEFT_X), Input.get_joy_axis(pad, JOY_AXIS_LEFT_Y))
		var right := Vector2(Input.get_joy_axis(pad, JOY_AXIS_RIGHT_X), Input.get_joy_axis(pad, JOY_AXIS_RIGHT_Y))
		if stick.length() > deadzone:
			move += stick
		if right.length() > deadzone:
			look += right
		up += float(Input.is_joy_button_pressed(pad, JOY_BUTTON_RIGHT_SHOULDER)) - float(Input.is_joy_button_pressed(pad, JOY_BUTTON_LEFT_SHOULDER))
		zoom += float(Input.is_joy_button_pressed(pad, JOY_BUTTON_DPAD_RIGHT)) - float(Input.is_joy_button_pressed(pad, JOY_BUTTON_DPAD_LEFT))
	_yaw -= look.x * delta * 1.8
	_pitch = clampf(_pitch - look.y * delta * 1.8, -1.55, 1.55)
	_camera.rotation = Vector3(_pitch, _yaw, 0.0)
	var direction := _camera.global_basis.x * move.x + _camera.global_basis.z * move.y + Vector3.UP * up
	if direction.length() > 1.0:
		direction = direction.normalized()
	var pos := _camera.global_position + direction * 8.0 * delta
	_camera.global_position = _origin + (pos - _origin).limit_length(MAX_DISTANCE)
	_camera.fov = clampf(_camera.fov + zoom * 30.0 * delta, 20.0, 100.0)


func _save_photo() -> void:
	if _capturing:
		return
	_capturing = true
	var was_visible := _panel.visible
	_panel.hide()
	get_parent()._set_photo_hud_visible(false)
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	var pictures := OS.get_system_dir(OS.SYSTEM_DIR_PICTURES)
	var dir := pictures.path_join(PHOTO_FOLDER) if not pictures.is_empty() else ""
	var err := DirAccess.make_dir_recursive_absolute(dir) if not dir.is_empty() else ERR_CANT_OPEN
	var path := ""
	if err == OK:
		var stamp := Time.get_datetime_string_from_system().replace(":", "-")
		path = dir.path_join("Photo-%s-%d.png" % [stamp, Time.get_ticks_msec()])
		err = image.save_png(path)
	_panel.visible = was_visible
	get_parent()._set_photo_hud_visible(_hud_visible)
	_status.text = "Saved: " + path if err == OK else "Could not save photo in Pictures (error %d)." % err
	_status.visible = true
	_capturing = false


func _exit_photo(return_to_pause := true) -> void:
	_active = false
	if return_to_pause and _playing:
		# Debounce the exit key and return to the paused menu.
		get_tree().current_scene._on_pause_choice("resume")
		get_tree().paused = true
	if is_instance_valid(_rig):
		_rig.set_process(true)
	if is_instance_valid(_previous):
		_previous.make_current()
	_camera.queue_free()
	finished.emit(return_to_pause)
	queue_free()
