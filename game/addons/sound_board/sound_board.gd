@tool
extends HSplitContainer
## The Sound Board: every sound in the game (from assets/sfx/sounds.json), grouped, with a preview
## of the selected one (play it, its waveform and file details, the code that plays it in
## scripts/audio.gd and the generator code that made it in art/tools/make_sfx.py), and tools to
## replace it: assign a .wav/.ogg/.mp3 (button, or drop a file from the FileSystem dock or the
## desktop onto the panel), revert to the generated sound, and trim its volume.
##
## Replacements are copied to res://assets/sfx_custom/<name>.<ext> and trims saved to
## res://assets/sfx_custom/mix.json; scripts/audio.gd picks both up the next time the game runs.

const REGISTRY := "res://assets/sfx/sounds.json"
const CUSTOM_DIR := "res://assets/sfx_custom/"
const MIX_PATH := CUSTOM_DIR + "mix.json"
const AUDIO_SCRIPT := "res://scripts/audio.gd"
const EXTS := ["ogg", "wav", "mp3"]

var _sounds: Array = [] # [{name, group, when}]
var _mix := {}
var _current := ""

var _search: LineEdit
var _tree: Tree
var _title: Label
var _when: Label
var _file_info: Label
var _wave: Control
var _code: CodeEdit
var _trim: HSlider
var _trim_label: Label
var _revert: Button
var _player: AudioStreamPlayer
var _dialog: EditorFileDialog
var _peaks := PackedFloat32Array()
var _playhead := -1.0


func _ready() -> void:
	split_offset = 300
	_player = AudioStreamPlayer.new()
	add_child(_player)

	# Left: search and the grouped list.
	var left := VBoxContainer.new()
	left.custom_minimum_size = Vector2(260, 0)
	add_child(left)
	_search = LineEdit.new()
	_search.placeholder_text = "Filter sounds..."
	_search.clear_button_enabled = true
	_search.text_changed.connect(func(_t): _fill_tree())
	left.add_child(_search)
	_tree = Tree.new()
	_tree.hide_root = true
	_tree.columns = 2
	_tree.set_column_expand(1, false)
	_tree.set_column_custom_minimum_width(1, 80)
	_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tree.item_selected.connect(_on_selected)
	_tree.item_activated.connect(_play)
	left.add_child(_tree)
	var refresh := Button.new()
	refresh.text = "Reload list"
	refresh.pressed.connect(_reload)
	left.add_child(refresh)

	# Right: the selected sound.
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	add_child(scroll)
	var right := VBoxContainer.new()
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.add_theme_constant_override("separation", 6)
	scroll.add_child(right)
	_title = Label.new()
	_title.add_theme_font_size_override("font_size", 22)
	right.add_child(_title)
	_when = Label.new()
	_when.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	right.add_child(_when)

	var bar := HBoxContainer.new()
	right.add_child(bar)
	bar.add_child(_button("Play", _play))
	bar.add_child(_button("Stop", func(): _player.stop()))
	bar.add_child(VSeparator.new())
	bar.add_child(_button("Assign file...", _open_dialog))
	_revert = _button("Revert to generated", _revert_custom)
	bar.add_child(_revert)
	bar.add_child(_button("Open replacements folder", func(): OS.shell_open(ProjectSettings.globalize_path(CUSTOM_DIR))))
	bar.add_child(VSeparator.new())
	var tl := Label.new()
	tl.text = "Volume trim"
	bar.add_child(tl)
	_trim = HSlider.new()
	_trim.min_value = -24.0
	_trim.max_value = 12.0
	_trim.step = 0.5
	_trim.custom_minimum_size = Vector2(160, 0)
	_trim.value_changed.connect(_on_trim)
	bar.add_child(_trim)
	_trim_label = Label.new()
	_trim_label.custom_minimum_size = Vector2(64, 0)
	bar.add_child(_trim_label)

	var hint := Label.new()
	hint.text = "Drop a .wav / .ogg / .mp3 here (from the FileSystem dock or the desktop) to assign it to the selected sound."
	hint.modulate = Color(1, 1, 1, 0.6)
	right.add_child(hint)

	_wave = Control.new()
	_wave.custom_minimum_size = Vector2(0, 90)
	_wave.draw.connect(_draw_wave)
	_wave.gui_input.connect(func(e):
		if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
			_play_from(clampf(e.position.x / maxf(_wave.size.x, 1.0), 0.0, 1.0)))
	right.add_child(_wave)
	_file_info = Label.new()
	_file_info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	right.add_child(_file_info)

	var code_title := Label.new()
	code_title.text = "Code"
	code_title.add_theme_font_size_override("font_size", 16)
	right.add_child(code_title)
	_code = CodeEdit.new()
	_code.editable = false
	_code.custom_minimum_size = Vector2(0, 260)
	_code.gutters_draw_line_numbers = false
	_code.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	right.add_child(_code)

	_dialog = EditorFileDialog.new()
	_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
	_dialog.access = EditorFileDialog.ACCESS_FILESYSTEM
	_dialog.filters = PackedStringArray(["*.wav, *.ogg, *.mp3 ; Audio"])
	_dialog.title = "Choose a sound file"
	_dialog.file_selected.connect(_assign)
	add_child(_dialog)

	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().filesystem_changed.connect(_on_fs_changed)
		get_tree().root.files_dropped.connect(_on_os_files_dropped)
	_reload()


func _exit_tree() -> void:
	if _player:
		_player.stop()
		_player.stream = null


func _button(text: String, cb: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.pressed.connect(cb)
	return b


# ---------------------------------------------------------------- list
func _reload() -> void:
	_sounds.clear()
	if FileAccess.file_exists(REGISTRY):
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(REGISTRY))
		if parsed is Array:
			_sounds = parsed
	_mix = {}
	if FileAccess.file_exists(MIX_PATH):
		var m = JSON.parse_string(FileAccess.get_file_as_string(MIX_PATH))
		if m is Dictionary:
			_mix = m
	_fill_tree()


func _fill_tree() -> void:
	_tree.clear()
	var root := _tree.create_item()
	var groups := {}
	var filter := _search.text.strip_edges().to_lower()
	var select_item: TreeItem = null
	for s in _sounds:
		var name: String = s["name"]
		if filter != "" and not (name.to_lower().contains(filter) or str(s.get("when", "")).to_lower().contains(filter)):
			continue
		var group: String = s.get("group", "Other")
		if not groups.has(group):
			var g := _tree.create_item(root)
			g.set_text(0, group)
			g.set_selectable(0, false)
			g.set_selectable(1, false)
			g.set_custom_color(0, Color(1.0, 0.75, 0.45))
			groups[group] = g
		var it := _tree.create_item(groups[group])
		it.set_text(0, name)
		it.set_metadata(0, name)
		var custom := _custom_path(name)
		it.set_text(1, "custom" if custom != "" else "generated")
		it.set_custom_color(1, Color(0.5, 1.0, 0.8) if custom != "" else Color(0.7, 0.7, 0.8))
		it.set_tooltip_text(0, str(s.get("when", "")))
		if name == _current:
			select_item = it
	if select_item:
		select_item.select(0)
	elif _current == "" and _sounds.size() > 0:
		_show(_sounds[0]["name"])


func _on_selected() -> void:
	var it := _tree.get_selected()
	if it and it.get_metadata(0) != null:
		_show(str(it.get_metadata(0)))


func _on_fs_changed() -> void:
	_fill_tree()
	if _current != "":
		_show(_current)


# ---------------------------------------------------------------- the selected sound
func _entry(name: String) -> Dictionary:
	for s in _sounds:
		if s["name"] == name:
			return s
	return {}


func _custom_path(name: String) -> String:
	for ext: String in EXTS:
		var p := CUSTOM_DIR + name + "." + ext
		if FileAccess.file_exists(p):
			return p
	return ""


func _generated_path(name: String) -> String:
	return "res://assets/music/neighborhood.wav" if name == "music_neighborhood" else "res://assets/sfx/%s.wav" % name


func _active_path(name: String) -> String:
	var c := _custom_path(name)
	return c if c != "" else _generated_path(name)


func _show(name: String) -> void:
	_current = name
	var e := _entry(name)
	_title.text = name
	_when.text = "%s  -  %s" % [e.get("group", ""), e.get("when", "")]
	var trim := float(_mix.get(name, 0.0))
	_trim.set_value_no_signal(trim)
	_trim_label.text = "%+.1f dB" % trim
	var custom := _custom_path(name)
	_revert.disabled = custom == ""
	var path := _active_path(name)
	var stream: AudioStream = load(path) if ResourceLoader.exists(path) else null
	_file_info.text = _describe(path, stream, custom != "")
	_peaks = _wave_peaks(stream)
	_playhead = -1.0
	_wave.queue_redraw()
	_code.text = _code_for(name, custom)


func _describe(path: String, stream: AudioStream, custom: bool) -> String:
	if stream == null:
		return "File: %s (missing or not imported yet)" % path
	var bytes := 0
	var f := FileAccess.open(path, FileAccess.READ)
	if f:
		bytes = f.get_length()
	var lines := ["File: %s   (%s)" % [path, "custom replacement" if custom else "generated"]]
	var detail := "%s, %.2f s, %.1f KB" % [stream.get_class(), stream.get_length(), bytes / 1024.0]
	if stream is AudioStreamWAV:
		var w: AudioStreamWAV = stream
		detail += ", %d Hz, %s, %s" % [w.mix_rate, "stereo" if w.stereo else "mono", ["8-bit", "16-bit", "IMA ADPCM", "QOA"][clampi(w.format, 0, 3)]]
	lines.append(detail)
	return "\n".join(lines)


# ---------------------------------------------------------------- waveform
func _wave_peaks(stream: AudioStream) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	if not (stream is AudioStreamWAV):
		return out
	var w: AudioStreamWAV = stream
	if w.format != AudioStreamWAV.FORMAT_16_BITS or w.data.is_empty():
		return out
	var frames := w.data.size() / (4 if w.stereo else 2)
	var cols := 400
	out.resize(cols)
	var per := maxi(1, frames / cols)
	for c in cols:
		var peak := 0.0
		var start := c * per
		var step := maxi(1, per / 64)
		var i := start
		while i < mini(start + per, frames):
			var v := absf(w.data.decode_s16(i * (4 if w.stereo else 2)) / 32768.0)
			peak = maxf(peak, v)
			i += step
		out[c] = peak
	return out


func _draw_wave() -> void:
	var r := Rect2(Vector2.ZERO, _wave.size)
	_wave.draw_rect(r, Color(0.08, 0.08, 0.14))
	var mid := r.size.y * 0.5
	_wave.draw_line(Vector2(0, mid), Vector2(r.size.x, mid), Color(1, 1, 1, 0.12))
	if _peaks.is_empty():
		_wave.draw_string(get_theme_default_font(), Vector2(10, mid + 5), "Waveform preview is shown for WAV files (click to play from a point)", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.5))
	else:
		var n := _peaks.size()
		for c in n:
			var x := (c + 0.5) * r.size.x / n
			var h := _peaks[c] * mid * 0.95
			_wave.draw_line(Vector2(x, mid - h), Vector2(x, mid + h), Color(1.0, 0.65, 0.3), maxf(1.0, r.size.x / n))
	if _playhead >= 0.0:
		var px := _playhead * r.size.x
		_wave.draw_line(Vector2(px, 0), Vector2(px, r.size.y), Color(0.5, 1.0, 0.85), 2.0)


func _process(_delta: float) -> void:
	if _player and _player.playing and _player.stream:
		_playhead = _player.get_playback_position() / maxf(_player.stream.get_length(), 0.001)
		_wave.queue_redraw()
	elif _playhead >= 0.0:
		_playhead = -1.0
		_wave.queue_redraw()


# ---------------------------------------------------------------- code
func _code_for(name: String, custom: String) -> String:
	var parts := PackedStringArray()
	var played := _audio_blocks(name)
	parts.append("# How the game plays it (%s)" % AUDIO_SCRIPT)
	parts.append(played if played != "" else "# (not referenced directly in audio.gd)")
	parts.append("")
	if custom != "":
		parts.append("# Replaced by a custom file: %s" % custom)
		parts.append("# The generator below is no longer used for this sound (Revert to go back to it).")
		parts.append("")
	if name == "music_neighborhood":
		parts.append("# How it was generated (art/tools/make_music.py)")
		parts.append(_head_of(_art_tool("make_music.py"), 30))
	else:
		parts.append("# How it was generated (art/tools/make_sfx.py, rendered with art/tools/fm_synth.py)")
		var gen := _sfx_definition(name)
		parts.append(gen if gen != "" else "# (no generator entry found)")
	return "\n".join(parts)


func _audio_blocks(name: String) -> String:
	var text := FileAccess.get_file_as_string(AUDIO_SCRIPT)
	if text == "":
		return ""
	var lines := text.split("\n")
	var blocks := PackedStringArray()
	var i := 0
	while i < lines.size():
		if lines[i].begins_with("func "):
			var start := i
			while start > 0 and lines[start - 1].begins_with("##"):
				start -= 1
			var end := i + 1
			while end < lines.size() and not lines[end].begins_with("func ") and not (lines[end] == "" and end + 1 < lines.size() and lines[end + 1] == ""):
				end += 1
			var block := "\n".join(lines.slice(start, end)).strip_edges()
			if block.contains("\"%s\"" % name) and not block.begins_with("static func sound_stream"):
				blocks.append(block)
			i = end
		else:
			i += 1
	return "\n\n".join(blocks)


func _art_tool(file: String) -> String:
	var p := ProjectSettings.globalize_path("res://").path_join("../art/tools/" + file).simplify_path()
	return FileAccess.get_file_as_string(p)


func _head_of(text: String, n: int) -> String:
	var lines := text.split("\n")
	return "\n".join(lines.slice(0, mini(n, lines.size())))


func _sfx_definition(name: String) -> String:
	var lines := _art_tool("make_sfx.py").split("\n")
	for i in lines.size():
		if lines[i].begins_with("    \"%s\":" % name):
			var start := i
			while start > 0 and lines[start - 1].strip_edges().begins_with("#"):
				start -= 1
			var end := i + 1
			while end < lines.size() and not lines[end].begins_with("    \"") and not lines[end].strip_edges().begins_with("#") and not lines[end].begins_with("}"):
				end += 1
			var out := "\n".join(lines.slice(start, end))
			# Pull in any instrument or helper the definition uses (e.g. CHIME = F.Inst(...), def whoosh).
			var extra := PackedStringArray()
			for j in lines.size():
				var l := lines[j]
				var m := l.split(" = ")[0].strip_edges() if l.contains(" = F.Inst(") else ""
				if m != "" and out.contains(m + ",") or (m != "" and out.contains(m + ")")):
					extra.append(l)
				if l.begins_with("def "):
					var fn := l.substr(4).split("(")[0]
					if fn not in ["seq", "mix", "layer", "main"] and out.contains(fn + "("):
						var k := j + 1
						while k < lines.size() and (lines[k] == "" or lines[k].begins_with(" ")):
							k += 1
						extra.append("\n".join(lines.slice(j, k)).strip_edges())
			if not extra.is_empty():
				out = "\n".join(extra) + "\n\n" + out
			return out
	return ""


# ---------------------------------------------------------------- actions
func _play() -> void:
	_play_from(0.0)


func _play_from(t: float) -> void:
	if _current == "":
		return
	var path := _active_path(_current)
	if not ResourceLoader.exists(path):
		return
	_player.stream = load(path)
	_player.volume_db = float(_mix.get(_current, 0.0))
	_player.play(t * _player.stream.get_length())


func _on_trim(v: float) -> void:
	if _current == "":
		return
	if is_zero_approx(v):
		_mix.erase(_current)
	else:
		_mix[_current] = v
	_trim_label.text = "%+.1f dB" % v
	_player.volume_db = v
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(CUSTOM_DIR))
	var f := FileAccess.open(MIX_PATH, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(_mix, "  ", true))


func _open_dialog() -> void:
	if _current != "":
		_dialog.popup_file_dialog()


func _assign(src: String) -> void:
	if _current == "":
		return
	var ext := src.get_extension().to_lower()
	if ext not in EXTS:
		push_warning("Sound Board: only .wav, .ogg and .mp3 files can be assigned.")
		return
	var src_abs := ProjectSettings.globalize_path(src) if src.begins_with("res://") else src
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(CUSTOM_DIR))
	_remove_custom(_current)
	var dst := CUSTOM_DIR + _current + "." + ext
	var err := DirAccess.copy_absolute(src_abs, ProjectSettings.globalize_path(dst))
	if err != OK:
		push_error("Sound Board: could not copy %s (%s)" % [src, error_string(err)])
		return
	print("Sound Board: %s now plays %s" % [_current, src])
	EditorInterface.get_resource_filesystem().scan()


func _revert_custom() -> void:
	if _current == "":
		return
	_remove_custom(_current)
	print("Sound Board: %s is back to the generated sound" % _current)
	EditorInterface.get_resource_filesystem().scan()


func _remove_custom(name: String) -> void:
	for ext: String in EXTS:
		var p := ProjectSettings.globalize_path(CUSTOM_DIR + name + "." + ext)
		for q in [p, p + ".import"]:
			if FileAccess.file_exists(q):
				DirAccess.remove_absolute(q)


# Drops: files dragged from the FileSystem dock, or from the desktop onto this panel.
func _can_drop_data(_at: Vector2, data: Variant) -> bool:
	return _current != "" and data is Dictionary and data.get("type", "") == "files" and data["files"].size() > 0 \
		and str(data["files"][0]).get_extension().to_lower() in EXTS


func _drop_data(_at: Vector2, data: Variant) -> void:
	_assign(str(data["files"][0]))


func _on_os_files_dropped(files: PackedStringArray) -> void:
	if not is_visible_in_tree() or files.is_empty():
		return
	if Rect2(Vector2.ZERO, size).has_point(get_local_mouse_position()):
		_assign(files[0])
