extends CanvasLayer
## Minimal, toggleable HUD: candy carried, the current area's infestation status, area banners,
## toasts and callouts, a hurt flash, the candy shop and the travel-gate menu, a controls hint and
## an F3 debug panel.

signal purchase(id: String)
signal start_mode(mode: int)
signal pause_choice(choice: String)
signal shop_tab(dir: int)
signal travel(index: int)
signal menu_closed

const NAVY := Color(0.07, 0.08, 0.2, 0.78)
const ORANGE := Color(1.0, 0.62, 0.25)
const CREAM := Color(1.0, 0.93, 0.8)
const TEAL := Color(0.45, 1.0, 0.85)
const CALLOUT_COLORS := [Color(1.0, 0.6, 0.85), Color(1.0, 0.78, 0.35), Color(0.55, 1.0, 0.8),
	Color(0.75, 0.6, 1.0), Color(1.0, 0.5, 0.45), Color(1.0, 0.9, 0.35), Color(0.6, 0.85, 1.0)]

var _root: Control
var _candy: Label
var _candy_panel: PanelContainer
var _combo: Label
var _combo_n := 0
var _area_name: Label
var _area_line: Label
var _area_candy: Label
var _area_panel: PanelContainer
var _toast: Label
var _toast_panel: PanelContainer
var _toast_t := 0.0
var _hint: Label
var _hint_t := 10.0
var _debug: Label
var _callout: Label
var _callout_tw: Tween
var _banner: Label
var _banner_sub: Label
var _hurt: ColorRect
var _fade: ColorRect
var _menu: Control # a PanelContainer, or the full-screen title screen
var _menu_dim: ColorRect # the backdrop behind a menu panel
var _menu_opened_frame := -1
var _menu_kind := ""
var _preview_pivot: Node3D
var _last_value := 0
var _seed := 0
var _prompt_nodes := [] # [Control, template or Callable]: texts that name keys or buttons
var _toast_template := ""


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS # menus keep working while the night is paused
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_root)
	var theme := Theme.new()
	var body := _font(BODY_PATH, 700)
	if body:
		theme.default_font = body
	_root.theme = theme

	_hurt = ColorRect.new()
	_hurt.color = Color(1.0, 0.25, 0.3, 0.0)
	_hurt.set_anchors_preset(Control.PRESET_FULL_RECT)
	_hurt.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(_hurt)

	# Candy carried (top left).
	_candy_panel = _pill(ORANGE)
	_candy_panel.position = Vector2(28, 22)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 14)
	_candy_panel.add_child(row)
	row.add_child(_icon(_candy_image()))
	var ccol := VBoxContainer.new()
	ccol.add_theme_constant_override("separation", 0)
	row.add_child(ccol)
	_candy = _display_label(32, CREAM)
	_candy.text = "0"
	ccol.add_child(_candy)
	var shop_hint := _label(15, Color(1.0, 0.8, 0.55, 0.85))
	_prompt(shop_hint, "Shop: {shop}")
	ccol.add_child(shop_hint)
	# Combo meter: candy collected in quick succession (x13), beside the counter.
	_combo = _display_label(40, Color(1.0, 0.86, 0.35))
	_combo.add_theme_constant_override("outline_size", 10)
	_combo.add_theme_color_override("font_outline_color", Color(0.35, 0.12, 0.05))
	_combo.modulate.a = 0.0
	row.add_child(_combo)

	# Area status (top right).
	_area_panel = _pill(TEAL)
	_area_panel.anchor_left = 1.0
	_area_panel.anchor_right = 1.0
	_area_panel.offset_left = -380
	_area_panel.offset_right = -28
	_area_panel.offset_top = 22
	_area_panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	var col := VBoxContainer.new()
	col.add_theme_constant_override("separation", 2)
	_area_panel.add_child(col)
	var arow := HBoxContainer.new()
	arow.add_theme_constant_override("separation", 10)
	col.add_child(arow)
	arow.add_child(_icon(_moon_image(), 34))
	_area_name = _display_label(26, Color(0.86, 0.95, 1.0))
	arow.add_child(_area_name)
	_area_line = _label(19, Color(1.0, 0.75, 0.6))
	col.add_child(_area_line)
	_area_candy = _label(19, Color(0.9, 0.85, 1.0))
	col.add_child(_area_candy)

	# Callout (milestones) and the area banner.
	_callout = _display_label(64, Color(1, 1, 1))
	_root.add_child(_callout)
	_callout.anchor_left = 0.0
	_callout.anchor_right = 1.0
	_callout.offset_top = 215
	_callout.offset_bottom = 305
	_callout.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_callout.add_theme_constant_override("outline_size", 14)
	_callout.modulate.a = 0.0
	_banner = _display_label(52, CREAM)
	_banner.add_theme_constant_override("outline_size", 12)
	_root.add_child(_banner)
	_banner.anchor_left = 0.0
	_banner.anchor_right = 1.0
	_banner.offset_top = 96
	_banner.offset_bottom = 160
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner.modulate.a = 0.0
	_banner_sub = _label(24, Color(1.0, 0.8, 0.6))
	_root.add_child(_banner_sub)
	_banner_sub.anchor_left = 0.0
	_banner_sub.anchor_right = 1.0
	_banner_sub.offset_top = 160
	_banner_sub.offset_bottom = 196
	_banner_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner_sub.modulate.a = 0.0

	# Toast (bottom centre).
	_toast_panel = _pill(ORANGE)
	_toast_panel.anchor_left = 0.5
	_toast_panel.anchor_right = 0.5
	_toast_panel.anchor_top = 1.0
	_toast_panel.anchor_bottom = 1.0
	_toast_panel.offset_top = -180
	_toast_panel.offset_bottom = -120
	_toast_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_toast = _label(26, CREAM)
	_toast.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_toast_panel.add_child(_toast)
	_toast_panel.modulate.a = 0.0

	_hint = _label(18, Color(0.85, 0.88, 1.0, 0.9))
	_root.add_child(_hint)
	_hint.anchor_left = 0.0
	_hint.anchor_right = 1.0
	_hint.anchor_top = 1.0
	_hint.anchor_bottom = 1.0
	_hint.offset_top = -64
	_hint.offset_bottom = -24
	_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_prompt(_hint, "Move {move}   Jump {jump} (again: flap, hold: glide)   Fire {attack}   Spin {spin}   Roll, pound {roll}   Ledge: push in or {jump} to climb, pull away or {drop} to drop   Shop {shop}   Car {summon_car}   Costume {next_costume}   Pause {pause}   Camera {camera}")

	_debug = _label(16, Color(0.8, 1.0, 0.8))
	_root.add_child(_debug)
	_debug.position = Vector2(30, 110)
	_debug.visible = false

	_fade = ColorRect.new()
	_fade.color = Color(0.02, 0.02, 0.08, 0.0)
	_fade.set_anchors_preset(Control.PRESET_FULL_RECT)
	_fade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(_fade)

	# Prompts follow the controller used last (and the Options menu's bindings).
	Settings.input_method_changed.connect(_refresh_prompts)
	Settings.changed.connect(func(section: String, key: String):
		if section == "gameplay" and (key == "hint" or key == "") and _candy_panel.visible:
			_hint.visible = bool(Settings.get_value("gameplay", "hint")))


## Sets a label's (or button's) text from a template with {action} prompts, and keeps it current.
## The source is a template string, or a Callable that returns the text.
func _prompt(node: Control, source: Variant) -> void:
	node.set("text", _prompt_text(source))
	if _prompt_nodes.size() > 160: # drop the closed menus' labels
		_prompt_nodes = _prompt_nodes.filter(func(e: Array) -> bool: return is_instance_valid(e[0]) and not (e[0] as Node).is_queued_for_deletion())
	_prompt_nodes.append([node, source])


func _prompt_text(source: Variant) -> String:
	return str(source.call()) if source is Callable else Settings.prompts(str(source))


func _refresh_prompts() -> void:
	var keep := []
	for entry in _prompt_nodes:
		if is_instance_valid(entry[0]):
			entry[0].set("text", _prompt_text(entry[1]))
			keep.append(entry)
	_prompt_nodes = keep
	if _toast_template != "":
		_toast.text = Settings.prompts(_toast_template)
	if _menu_kind == "options":
		_refresh_option_rows()
	elif _menu == null:
		if _candy_panel.visible and bool(Settings.get_value("gameplay", "hint")):
			_hint_t = maxf(_hint_t, 5.0) # show the line again in the new buttons
			_hint.modulate.a = 1.0


func _pill(border: Color, radius := 22) -> PanelContainer:
	var p := PanelContainer.new()
	p.add_theme_stylebox_override("panel", _box(border, radius))
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(p)
	return p


func _box(border: Color, radius := 22, bg := NAVY) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.set_corner_radius_all(radius)
	sb.border_color = border
	sb.set_border_width_all(3)
	sb.content_margin_left = 20
	sb.content_margin_right = 20
	sb.content_margin_top = 10
	sb.content_margin_bottom = 10
	sb.shadow_color = Color(0, 0, 0, 0.3)
	sb.shadow_size = 6
	return sb


func _label(size: int, color: Color) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_outline_color", Color(0.05, 0.04, 0.12, 0.9))
	l.add_theme_constant_override("outline_size", 6)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l


func _icon(img: Image, size := 44) -> TextureRect:
	var t := TextureRect.new()
	t.texture = ImageTexture.create_from_image(img)
	t.custom_minimum_size = Vector2(size, size)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	return t


func _candy_image() -> Image:
	# A wrapped candy: round body with stripes and two twisted ends.
	var s := 64
	var img := Image.create(s, s, false, Image.FORMAT_RGBA8)
	for y in s:
		for x in s:
			var p := Vector2(x, y) - Vector2(s, s) * 0.5
			var c := Color(0, 0, 0, 0)
			var body := p.length() / 15.0
			var wing := (absf(p.x) - 17.0) / 12.0
			if wing > 0.0 and wing < 1.0 and absf(p.y) < 4.0 + wing * 11.0:
				c = Color(1.0, 0.55, 0.75)
			if body < 1.0:
				var stripe := int((p.x + p.y + 64.0) / 7.0) % 2 == 0
				c = Color(1.0, 0.45, 0.68) if stripe else Color(1.0, 0.95, 0.92)
				c = c.lightened(0.25 * (1.0 - body)) if p.y < 0 else c
			elif body < 1.18 and c.a == 0.0:
				c = Color(0.4, 0.1, 0.25)
			img.set_pixel(x, y, c)
	return img


func _moon_image() -> Image:
	var s := 64
	var img := Image.create(s, s, false, Image.FORMAT_RGBA8)
	for y in s:
		for x in s:
			var p := Vector2(x, y) - Vector2(s, s) * 0.5
			var d := p.length() / 24.0
			var c := Color(0, 0, 0, 0)
			if d < 1.0:
				c = Color(1.0, 0.95, 0.78)
				if (p - Vector2(-7, -6)).length() < 6.0 or (p - Vector2(8, 5)).length() < 4.0:
					c = Color(0.9, 0.83, 0.65)
			elif d < 1.35:
				c = Color(0.8, 0.82, 1.0, (1.35 - d) / 0.35 * 0.5)
			img.set_pixel(x, y, c)
	return img


# ---------------------------------------------------------------- game -> HUD
func world_started(seed: int) -> void:
	_seed = seed
	_last_value = 0
	_hint_t = 10.0
	_hint.modulate.a = 1.0
	_close_menu(false)
	_debug.text = "world seed %d" % seed
	# Review aid: --options-tab=N opens that Options tab (with --capture= for a screenshot).
	for a: String in OS.get_cmdline_user_args():
		if a.begins_with("--options-tab="):
			_options_tab = clampi(a.substr(14).to_int(), 0, OPTION_TABS.size() - 1)
			_options_from = "pause"
			_build_options.call_deferred()


func set_candy(value: int) -> void:
	_candy.text = "%d" % value
	if value > _last_value and _candy_panel:
		# Punch the counter so every pickup registers.
		_candy_panel.pivot_offset = _candy_panel.size * 0.5
		var tw := create_tween()
		tw.tween_property(_candy_panel, "scale", Vector2(1.12, 1.12), 0.06)
		tw.tween_property(_candy_panel, "scale", Vector2.ONE, 0.18).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_last_value = value


func set_combo(n: int) -> void:
	if n < 2:
		if _combo_n >= 2:
			var fade := create_tween()
			fade.tween_property(_combo, "modulate:a", 0.0, 0.35)
		_combo_n = n
		return
	_combo_n = n
	_combo.text = "x%d" % n
	_combo.modulate.a = 1.0
	_combo.pivot_offset = _combo.size * 0.5
	var tw := create_tween()
	tw.tween_property(_combo, "scale", Vector2(1.3, 1.3), 0.05)
	tw.tween_property(_combo, "scale", Vector2.ONE, 0.2).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)


func set_area(d: Dictionary) -> void:
	_area_name.text = d.get("name", "")
	if not d.get("settlement", false):
		_area_line.text = "Open country"
		_area_line.add_theme_color_override("font_color", Color(0.75, 0.85, 1.0))
		_area_candy.text = ""
		return
	if d.get("peaceful", false):
		var left: int = int(d.get("houses_total", 0)) - int(d.get("houses", 0))
		_area_line.text = ("%d doorbells left to ring" % left) if left > 0 else "Every doorbell rung!"
		_area_line.add_theme_color_override("font_color", ORANGE if left > 0 else TEAL)
	elif d.get("freed", false):
		_area_line.text = "Freed!  The travel gate is awake."
		_area_line.add_theme_color_override("font_color", TEAL)
	else:
		_area_line.text = "Monster nests %d/%d   Thorn nests %d/%d" % [d.get("nests_left", 0), d.get("nests", 0), d.get("hearts_left", 0), d.get("hearts", 0)]
		_area_line.add_theme_color_override("font_color", Color(1.0, 0.72, 0.55))
	var places := "Shops" if d.get("town", false) else "Houses"
	_area_candy.text = "%s trick-or-treated %d/%d   Candy %d/%d" % [places, d.get("houses", 0), d.get("houses_total", 0), d.get("candy", 0), d.get("candy_total", 0)]


func banner(title: String, sub: String) -> void:
	_banner.text = Settings.prompts(title)
	_banner_sub.text = Settings.prompts(sub)
	var tw := create_tween()
	tw.tween_property(_banner, "modulate:a", 1.0, 0.4)
	tw.parallel().tween_property(_banner_sub, "modulate:a", 1.0 if sub != "" else 0.0, 0.4)
	tw.tween_interval(2.4)
	tw.tween_property(_banner, "modulate:a", 0.0, 0.8)
	tw.parallel().tween_property(_banner_sub, "modulate:a", 0.0, 0.8)


func callout(text: String, level: int) -> void:
	_callout.text = text
	_callout.add_theme_color_override("font_color", CALLOUT_COLORS[level % CALLOUT_COLORS.size()])
	_callout.pivot_offset = Vector2(_callout.size.x * 0.5, 45)
	_callout.modulate.a = 1.0
	_callout.scale = Vector2(0.3, 0.3)
	_callout.rotation = -0.12
	# A new callout restarts the pop and the fade (the last one's fade must not cut this one short).
	if _callout_tw:
		_callout_tw.kill()
	_callout_tw = create_tween().set_parallel(true)
	_callout_tw.tween_property(_callout, "scale", Vector2(1.0, 1.0), 0.35).set_trans(Tween.TRANS_ELASTIC).set_ease(Tween.EASE_OUT)
	_callout_tw.tween_property(_callout, "rotation", 0.0, 0.4).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_callout_tw.chain().tween_property(_callout, "modulate:a", 0.0, 0.5).set_delay(1.0)


func toast(text: String) -> void:
	_toast_template = text
	_toast.text = Settings.prompts(text)
	_toast_t = 3.0
	_toast_panel.modulate.a = 1.0
	_toast_panel.offset_left = 0
	_toast_panel.offset_right = 0


func hurt() -> void:
	_hurt.color.a = 0.32
	create_tween().tween_property(_hurt, "color:a", 0.0, 0.45)


func fade(on: bool) -> void:
	create_tween().tween_property(_fade, "color:a", 1.0 if on else 0.0, 0.35)


## items: {id, name, status, desc, enabled}, shown as a grid of tiles on every tab. previews: a
## Node3D (or null) per item, turning on a little stage while its tile has focus; stage is the stage
## camera's distance (0: no stage).
func open_shop(items: Array, wallet: int, tab := 0, tabs: Array = [], previews: Array = [], stage := 0.0, focus_id := "") -> void:
	var list := _open_menu("Candy Shop", "You have %d candy" % wallet, ORANGE)
	_menu_kind = "shop"
	_menu.offset_left = -530
	_menu.offset_right = 530
	if tabs.size() > 1:
		list.add_child(_tab_row(tabs, tab))
	# Fit the whole menu on screen: the stage and the tiles share what's left after the title, tabs
	# and close button (about 290 px with margins).
	var screen_h := get_viewport().get_visible_rect().size.y
	var stage_h := 0.0
	var pivot: Node3D = null
	if stage > 0.0:
		stage_h = clampf(screen_h * 0.24, 140.0, 250.0)
		pivot = _preview_stage(list, stage, stage_h)
	const COLS := 3
	const TILE := Vector2(316, 132)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.follow_focus = true
	var rows_n := int(ceil(items.size() / float(COLS)))
	var room := maxf(TILE.y + 12.0, screen_h - stage_h - 290.0)
	scroll.custom_minimum_size = Vector2(0, minf(rows_n * (TILE.y + 12.0), room))
	list.add_child(scroll)
	var center := CenterContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(center)
	var grid := GridContainer.new()
	grid.columns = COLS
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 12)
	center.add_child(grid)
	var first: Button = null
	var wanted: Button = null
	var tiles: Array[Button] = []
	for i in items.size():
		var item: Dictionary = items[i]
		var b := _shop_tile(item, TILE, wallet)
		var id: String = item["id"]
		b.pressed.connect(func(): purchase.emit(id))
		grid.add_child(b)
		tiles.append(b)
		var prev: Node3D = previews[i] if i < previews.size() else null
		if pivot and prev:
			prev.visible = false
			pivot.add_child(prev)
		b.focus_entered.connect(func():
			scroll.ensure_control_visible.call_deferred(b)
			if pivot:
				for c in pivot.get_children():
					c.visible = c == prev)
		if id == focus_id:
			wanted = b
		if first == null and not b.disabled:
			first = b
	if first == null and not tiles.is_empty():
		first = tiles[0]
	var close := _finish_menu(list, wanted if wanted else first)
	# Explicit grid navigation: left to itself, focus jumps from the last visible row to the close
	# button below the grid, skipping the rows still scrolled out of view.
	var n := tiles.size()
	for i in n:
		var b := tiles[i]
		var row := floori(i / float(COLS))
		var last_row := floori((n - 1) / float(COLS))
		if i % COLS > 0:
			b.focus_neighbor_left = b.get_path_to(tiles[i - 1])
		if i % COLS < COLS - 1 and i + 1 < n:
			b.focus_neighbor_right = b.get_path_to(tiles[i + 1])
		if row > 0:
			b.focus_neighbor_top = b.get_path_to(tiles[i - COLS])
		var down: Control = close
		if i + COLS < n:
			down = tiles[i + COLS]
		elif row < last_row:
			down = tiles[n - 1] # a short last row: drop to its final tile
		b.focus_neighbor_bottom = b.get_path_to(down)
		b.focus_next = b.get_path_to(tiles[i + 1] if i + 1 < n else close)
		if i > 0:
			b.focus_previous = b.get_path_to(tiles[i - 1])
	if n > 0:
		close.focus_neighbor_top = close.get_path_to(tiles[n - 1])
		close.focus_previous = close.get_path_to(tiles[n - 1])


## One shop tile: the item's name, its price or state, and a line or two about it.
func _shop_tile(item: Dictionary, tile_size: Vector2, wallet: int) -> Button:
	var b := _menu_button()
	b.custom_minimum_size = tile_size
	b.disabled = not bool(item["enabled"])
	b.focus_mode = Control.FOCUS_ALL # unaffordable and locked tiles stay browsable (hints, previews)
	b.mouse_entered.connect(b.grab_focus)
	var v := VBoxContainer.new()
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.offset_left = 16
	v.offset_right = -16
	v.offset_top = 12
	v.offset_bottom = -10
	v.add_theme_constant_override("separation", 4)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(v)
	var title := _display_label(24, CREAM if not b.disabled else Color(0.75, 0.75, 0.85))
	title.text = str(item["name"])
	title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	v.add_child(title)
	var status := str(item["status"])
	var priced := status.ends_with("candy")
	var st := _label(20, ORANGE if (priced and not b.disabled) else (Color(0.6, 0.6, 0.72) if priced or status == "not found yet" else TEAL))
	st.text = status
	v.add_child(st)
	if str(item["desc"]) != "":
		var d := _label(15, Color(0.82, 0.82, 1.0, 0.85))
		_prompt(d, str(item["desc"]))
		d.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		d.custom_minimum_size = Vector2(tile_size.x - 32.0, 0)
		d.max_lines_visible = 2
		v.add_child(d)
	return b

## The tab strip: the current tab bright, the others dim, with the bumper hints at the ends.
## With `pick`, a click on a tab calls pick(index).
func _tab_row(tabs: Array, current: int, pick := Callable()) -> Control:
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 22)
	var lb := _label(18, Color(0.75, 0.75, 0.95, 0.8))
	_prompt(lb, "< {shop_tab_prev}")
	row.add_child(lb)
	for i in tabs.size():
		var t := _display_label(26 if i == current else 22, ORANGE if i == current else Color(0.7, 0.7, 0.85, 0.75))
		t.text = str(tabs[i])
		row.add_child(t)
		if pick.is_valid():
			t.mouse_filter = Control.MOUSE_FILTER_STOP
			t.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
			var idx := i
			t.gui_input.connect(func(ev: InputEvent):
				if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
					pick.call(idx))
	var rb := _label(18, Color(0.75, 0.75, 0.95, 0.8))
	_prompt(rb, "{shop_tab_next} >")
	row.add_child(rb)
	return row


## A small 3D stage in its own world: a turntable pivot, a key light and a camera `dist` away.
func _preview_stage(parent: Control, dist: float, height := 250.0) -> Node3D:
	var box := SubViewportContainer.new()
	box.stretch = true
	box.custom_minimum_size = Vector2(0, height)
	parent.add_child(box)
	var vp := SubViewport.new()
	vp.own_world_3d = true
	vp.transparent_bg = true
	vp.msaa_3d = Viewport.MSAA_4X
	box.add_child(vp)
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_CLEAR_COLOR
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.55, 0.55, 0.75)
	env.environment.ambient_light_energy = 0.8
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	vp.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_color = Color(1.0, 0.92, 0.82)
	key.light_energy = 1.3
	key.rotation = Vector3(-0.7, 0.6, 0.0)
	vp.add_child(key)
	var cam := Camera3D.new()
	cam.fov = 40.0
	vp.add_child(cam)
	var look := Vector3(0, dist * 0.2, 0)
	cam.position = look + Vector3(0, dist * 0.32, dist)
	cam.look_at_from_position(cam.position, look)
	var pivot := Node3D.new()
	vp.add_child(pivot)
	_preview_pivot = pivot
	return pivot


func open_travel(destinations: Array) -> void:
	var list := _open_menu("Lantern Gate", "Travel to a freed place", TEAL)
	var first: Button = null
	if destinations.is_empty():
		var l := _label(22, CREAM)
		l.text = "No other gates are awake yet.\nFree more neighborhoods and towns to link them."
		list.add_child(l)
	for i in destinations.size():
		var d: Dictionary = destinations[i]
		var b := _menu_button()
		b.text = "%s   (%d m away)" % [d["name"], d["distance"]]
		var idx := i
		b.pressed.connect(func():
			_close_menu(false)
			travel.emit(idx))
		list.add_child(b)
		if first == null:
			first = b
	_finish_menu(list, first)


# ---------------------------------------------------------------- trailer
const LOGO_PATH := "res://assets/ui/logo.png"
const FONT_PATH := "res://assets/fonts/Fredoka-Variable.ttf"
var _cine_title: Label
var _cine_sub: Label
var _cine_logo: TextureRect
var _logo_tex: Texture2D
var _fonts := {}
## Body text everywhere else: Nunito, a rounded sans that sits well beside Fredoka (warmer than
## the engine's default face) and stays readable small.
const BODY_PATH := "res://assets/fonts/Nunito-Variable.ttf"


## Trailer and screenshot shots: the play HUD goes away.
func cine_mode(on: bool) -> void:
	_set_play_hud(not on)
	_close_menu(false)


## A caption over a trailer shot: a lower-third line, or the big title when there is a subtitle.
func cine_caption(text: String, sub: String, duration: float, top := false) -> void:
	if _cine_title == null:
		_cine_title = _display_label(64, CREAM)
		_cine_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		_cine_title.anchor_left = 0.0
		_cine_title.anchor_right = 1.0
		_root.add_child(_cine_title)
		_cine_sub = _display_label(42, CREAM)
		_cine_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		_cine_sub.anchor_left = 0.0
		_cine_sub.anchor_right = 1.0
		_root.add_child(_cine_sub)
		_cine_logo = TextureRect.new()
		_cine_logo.texture = _logo()
		_cine_logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		_cine_logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		_cine_logo.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_cine_logo.anchor_left = 0.18
		_cine_logo.anchor_right = 0.82
		_cine_logo.anchor_top = 0.08
		_cine_logo.anchor_bottom = 0.6
		_root.add_child(_cine_logo)
	# The title card is the logo itself; other shots get a lower-third line in the logo's lettering.
	var big := sub != ""
	_cine_title.visible = not big
	_cine_logo.visible = big
	_cine_title.anchor_top = 0.09 if top else 0.8
	_cine_title.anchor_bottom = _cine_title.anchor_top
	_cine_title.offset_top = 0
	_cine_title.offset_bottom = 0
	_cine_title.grow_vertical = Control.GROW_DIRECTION_BOTH
	_cine_sub.anchor_top = 0.62
	_cine_sub.anchor_bottom = 0.62
	_cine_sub.offset_top = 0
	_cine_sub.offset_bottom = 0
	_cine_sub.grow_vertical = Control.GROW_DIRECTION_BOTH
	# Above any menu a shot opens (the shop in the upgrades shot).
	for c: Control in [_cine_title, _cine_sub, _cine_logo]:
		c.z_index = 10
	_cine_title.text = text
	_cine_sub.text = sub
	_cine_title.modulate.a = 0.0
	_cine_sub.modulate.a = 0.0
	_cine_logo.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_interval(0.5 if not big else 1.6)
	if big:
		_cine_logo.pivot_offset = _cine_logo.size * 0.5
		_cine_logo.scale = Vector2(0.85, 0.85)
		tw.tween_property(_cine_logo, "modulate:a", 1.0, 0.7)
		tw.parallel().tween_property(_cine_logo, "scale", Vector2.ONE, 0.9).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		tw.tween_property(_cine_sub, "modulate:a", 1.0, 0.8)
	else:
		tw.tween_property(_cine_title, "modulate:a", 1.0, 0.6)
		tw.tween_interval(maxf(0.5, duration - 2.2))
		tw.tween_property(_cine_title, "modulate:a", 0.0, 0.5)


## The logo (title screen and trailer title card).
func _logo() -> Texture2D:
	if _logo_tex == null:
		if ResourceLoader.exists(LOGO_PATH):
			_logo_tex = load(LOGO_PATH)
		else:
			var img := Image.load_from_file(ProjectSettings.globalize_path(LOGO_PATH))
			if img:
				_logo_tex = ImageTexture.create_from_image(img)
	return _logo_tex


## Lettering to match the logo: chunky rounded Fredoka in bold, a thick plum outline and a soft
## purple glow under it.
func _display_label(size: int, color: Color) -> Label:
	var l := _label(size, color)
	var f := _display_font()
	if f:
		l.add_theme_font_override("font", f)
	l.add_theme_color_override("font_outline_color", Color(0.17, 0.06, 0.3))
	l.add_theme_constant_override("outline_size", maxi(8, int(size * 0.3)))
	l.add_theme_color_override("font_shadow_color", Color(0.5, 0.25, 0.95, 0.6))
	l.add_theme_constant_override("shadow_offset_x", 0)
	l.add_theme_constant_override("shadow_offset_y", maxi(3, int(size * 0.08)))
	l.add_theme_constant_override("shadow_outline_size", maxi(10, int(size * 0.34)))
	return l


## Headers, titles and announcements: Fredoka, the logo's chunky rounded lettering.
func _display_font() -> Font:
	return _font(FONT_PATH, 650)


## A font at a weight (both faces are variable fonts), loaded once.
func _font(path: String, weight: int) -> Font:
	var key := "%s@%d" % [path, weight]
	if _fonts.has(key):
		return _fonts[key]
	var base: Font = null
	if ResourceLoader.exists(path):
		base = load(path)
	else:
		var ff := FontFile.new()
		if ff.load_dynamic_font(ProjectSettings.globalize_path(path)) == OK:
			base = ff
	var fv: FontVariation = null
	if base:
		fv = FontVariation.new()
		fv.base_font = base
		fv.variation_opentype = {TextServerManager.get_primary_interface().name_to_tag("wght"): weight}
	_fonts[key] = fv
	return fv


# ---------------------------------------------------------------- main menu and pause
## The title screen over the live world: the game's name, the two modes as big cards, and quit.
func show_main_menu(focus_id := "") -> void:
	_close_menu(false)
	_set_play_hud(false)
	_menu_kind = "main"
	var screen := Control.new()
	screen.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.add_child(screen)
	_menu = screen
	# A soft night vignette so the text reads over the world.
	var shade := TextureRect.new()
	shade.set_anchors_preset(Control.PRESET_FULL_RECT)
	shade.texture = _vignette_texture()
	shade.stretch_mode = TextureRect.STRETCH_SCALE
	shade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	screen.add_child(shade)
	var col := VBoxContainer.new()
	col.set_anchors_preset(Control.PRESET_FULL_RECT)
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_theme_constant_override("separation", 18)
	screen.add_child(col)
	var title := TextureRect.new()
	title.texture = _logo()
	title.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	title.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	title.custom_minimum_size = Vector2(0, 380)
	title.mouse_filter = Control.MOUSE_FILTER_IGNORE
	col.add_child(title)
	var sub := _display_label(32, CREAM)
	sub.text = "A Halloween night adventure"
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col.add_child(sub)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 36)
	col.add_child(gap)
	var cards := HBoxContainer.new()
	cards.alignment = BoxContainer.ALIGNMENT_CENTER
	cards.add_theme_constant_override("separation", 48)
	col.add_child(cards)
	var treat := _mode_card("Trick or Treat", "A cozy candy hunt. No monsters, no thorns:\njust doorbells, treats and trampolines.", _candy_image(), ORANGE, 0)
	var save := _mode_card("Save the Town", "Monsters have overrun the neighborhoods.\nDestroy their nests and free every town!", _moon_image(), Color(0.7, 0.55, 1.0), 1)
	cards.add_child(treat)
	cards.add_child(save)
	treat.focus_neighbor_right = treat.get_path_to(save)
	save.focus_neighbor_left = save.get_path_to(treat)
	var gap2 := Control.new()
	gap2.custom_minimum_size = Vector2(0, 26)
	col.add_child(gap2)
	# Continue (the newest save), Load game, Options and Quit in a row under the cards.
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 20)
	col.add_child(row)
	var latest := Saves.latest()
	var buttons: Array[Button] = []
	for entry in [["continue", "Continue"], ["load", "Load game"], ["options", "Options"], ["quit", "Quit game"]]:
		var id: String = entry[0]
		if id == "continue" and latest == "":
			continue
		var b := _menu_button()
		b.text = entry[1]
		b.alignment = HORIZONTAL_ALIGNMENT_CENTER
		b.custom_minimum_size = Vector2(230, 0)
		b.mouse_entered.connect(b.grab_focus)
		b.set_meta("id", id)
		row.add_child(b)
		buttons.append(b)
		match id:
			"continue":
				b.pressed.connect(func(): _load_and_play(latest))
			"load":
				b.disabled = latest == ""
				b.focus_mode = Control.FOCUS_NONE if b.disabled else Control.FOCUS_ALL
				b.pressed.connect(func(): open_load("main"))
			"options":
				b.pressed.connect(func(): open_options("main"))
			"quit":
				b.pressed.connect(func(): get_tree().quit())
	var nav := buttons.filter(func(b: Button) -> bool: return not b.disabled)
	for i in nav.size():
		var b: Button = nav[i]
		b.focus_neighbor_top = b.get_path_to(treat if i < nav.size() / 2 else save)
		b.focus_neighbor_bottom = b.get_path_to(b)
		b.focus_neighbor_left = b.get_path_to(nav[i - 1] if i > 0 else b)
		b.focus_neighbor_right = b.get_path_to(nav[i + 1] if i + 1 < nav.size() else b)
	treat.focus_neighbor_bottom = treat.get_path_to(nav[0])
	save.focus_neighbor_bottom = save.get_path_to(nav[nav.size() - 1] if nav.size() < 3 else nav[nav.size() - 2])
	var help := _label(18, Color(0.8, 0.82, 1.0, 0.8))
	_prompt(help, func() -> String:
		if Settings.prompt_style() == "kb":
			return "Choose with the arrow keys or the mouse, then Enter"
		return "Choose with the stick, then %s" % Settings.pad_label("ui_accept"))
	help.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col.add_child(help)
	var focus: Button = treat
	if focus_id == "" and latest != "":
		focus_id = "continue" # picking up where you left off is the likely choice
	for b: Button in nav:
		if b.get_meta("id", "") == focus_id:
			focus = b
	focus.grab_focus()
	_menu_opened_frame = Engine.get_process_frames()
	# Ease in, then let the title float gently.
	screen.modulate.a = 0.0
	create_tween().tween_property(screen, "modulate:a", 1.0, 0.8)
	title.resized.connect(func(): title.pivot_offset = title.size * 0.5)
	var bob := title.create_tween().set_loops() # bound to the logo, so it ends when the menu closes
	bob.tween_property(title, "scale", Vector2(1.025, 1.025), 1.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	bob.tween_property(title, "scale", Vector2.ONE, 1.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)


## A big mode card: icon, name and a short pitch; it lifts and glows while focused.
func _mode_card(title: String, pitch: String, icon: Image, accent: Color, mode: int) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(470, 300)
	b.add_theme_stylebox_override("normal", _box(Color(accent, 0.55), 30, Color(0.07, 0.06, 0.18, 0.88)))
	b.add_theme_stylebox_override("hover", _box(accent, 30, Color(0.14, 0.1, 0.28, 0.94)))
	b.add_theme_stylebox_override("focus", _box(accent, 30, Color(0.16, 0.11, 0.3, 0.96)))
	b.add_theme_stylebox_override("pressed", _box(TEAL, 30, Color(0.12, 0.2, 0.28, 0.96)))
	var v := VBoxContainer.new()
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.alignment = BoxContainer.ALIGNMENT_CENTER
	v.add_theme_constant_override("separation", 12)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(v)
	var ic := _icon(icon, 88)
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ic.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	v.add_child(ic)
	var t := _display_label(44, accent)
	t.text = title
	t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	t.add_theme_constant_override("outline_size", 10)
	v.add_child(t)
	var p := _label(20, CREAM)
	p.text = pitch
	p.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(p)
	b.pivot_offset = b.custom_minimum_size * 0.5
	b.focus_entered.connect(func():
		create_tween().tween_property(b, "scale", Vector2(1.06, 1.06), 0.15).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT))
	b.focus_exited.connect(func(): create_tween().tween_property(b, "scale", Vector2.ONE, 0.15))
	b.mouse_entered.connect(b.grab_focus)
	b.pressed.connect(func():
		_close_menu(false)
		_set_play_hud(true)
		Saves.new_game()
		start_mode.emit(mode))
	return b


func _vignette_texture() -> Texture2D:
	var g := Gradient.new()
	g.offsets = PackedFloat32Array([0.0, 0.55, 1.0])
	g.colors = PackedColorArray([Color(0.03, 0.02, 0.1, 0.25), Color(0.03, 0.02, 0.1, 0.45), Color(0.02, 0.01, 0.08, 0.85)])
	var t := GradientTexture2D.new()
	t.gradient = g
	t.fill = GradientTexture2D.FILL_RADIAL
	t.fill_from = Vector2(0.5, 0.45)
	t.fill_to = Vector2(1.1, 1.1)
	t.width = 256
	t.height = 256
	return t


## The pause menu (Esc / Back): the night stands still underneath.
func open_pause(focus_id := "resume") -> void:
	if _menu == null:
		Saves.remember_frame() # the moment play stopped, for a save's thumbnail
	var list := _open_menu("Paused", "", Color(0.7, 0.55, 1.0))
	_menu_kind = "pause"
	_menu.offset_left = -270
	_menu.offset_right = 270
	var focus: Button = null
	for entry in [["resume", "Resume"], ["save", "Save game"], ["load", "Load game"], ["options", "Options"], ["new_night", "New night"], ["main_menu", "Main menu"], ["quit", "Quit game"]]:
		var b := _menu_button()
		b.text = entry[1]
		b.alignment = HORIZONTAL_ALIGNMENT_CENTER
		var id: String = entry[0]
		# Save, load and options open over the pause (the night stays paused underneath).
		if id == "save":
			b.disabled = not Saves.can_save()
			b.focus_mode = Control.FOCUS_NONE if b.disabled else Control.FOCUS_ALL
			b.pressed.connect(func(): open_save())
		elif id == "load":
			b.disabled = Saves.latest() == ""
			b.focus_mode = Control.FOCUS_NONE if b.disabled else Control.FOCUS_ALL
			b.pressed.connect(func(): open_load("pause"))
		elif id == "options":
			b.pressed.connect(func(): open_options("pause"))
		elif id == "main_menu" or id == "quit":
			b.pressed.connect(func():
				Saves.save_slot("autosave", "Autosave") # leaving the night: keep it
				_close_menu(false)
				pause_choice.emit(id))
		else:
			b.pressed.connect(func():
				_close_menu(false)
				pause_choice.emit(id))
		b.mouse_entered.connect(b.grab_focus)
		list.add_child(b)
		if id == focus_id or focus == null:
			focus = b
	focus.grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


func _set_play_hud(on: bool) -> void:
	for c in [_candy_panel, _area_panel, _hint, _toast_panel]:
		if c:
			c.visible = on
	# No controls line in trailer and screenshot runs (scripted): it clutters the gameplay clips.
	_hint.visible = on and bool(Settings.get_value("gameplay", "hint")) and not Settings.scripted
	if on:
		_hint_t = 10.0
		_hint.modulate.a = 1.0


# ---------------------------------------------------------------- options
## From the title screen or the pause menu: tabs of settings (scripts/settings.gd applies and saves
## them). Rows change with left / right (or a click); the Controls tab also rebinds keys and buttons.
const OPTION_TABS := ["Gameplay", "Controls", "Display", "Graphics", "Audio"]
const TAB_SECTION := {"Gameplay": "gameplay", "Controls": "controls", "Display": "display", "Graphics": "graphics", "Audio": "audio"}
var _options_from := "" # "main" or "pause": where Back goes
var _options_tab := 0
var _options_desc: Label
var _desc_src := ""
var _row_refresh: Array[Callable] = []
var _capture := {} # a rebind waiting for its key or button: {action, kind, button, t}
var _capture_end_frame := -1
var _hold_dir := 0
var _hold_t := 0.0


func open_options(from: String) -> void:
	_options_from = from
	_options_tab = 0
	_build_options()


func _build_options() -> void:
	var tab: String = OPTION_TABS[_options_tab]
	_capture = {}
	var list := _open_menu("Options", "", Color(0.7, 0.55, 1.0))
	_menu_kind = "options"
	_menu.offset_left = -640
	_menu.offset_right = 640
	list.get_child(1).visible = false # no subtitle
	_row_refresh.clear()
	list.add_child(_tab_row(OPTION_TABS, _options_tab, func(i: int):
		_options_tab = i
		_build_options()))
	var screen_h := get_viewport().get_visible_rect().size.y
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.follow_focus = true
	scroll.custom_minimum_size = Vector2(0, clampf(screen_h - 400.0, 260.0, 640.0))
	list.add_child(scroll)
	var box := VBoxContainer.new()
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override("separation", 8)
	scroll.add_child(_scroll_gutter(scroll, box))
	# Each entry: [left control, right control or null], for the explicit focus wiring below.
	var nav: Array = []
	for row: Dictionary in Settings.rows(tab):
		var b := _option_row(row)
		box.add_child(b)
		nav.append([b, null])
	if tab == "Controls":
		var head := HBoxContainer.new()
		head.add_theme_constant_override("separation", 12)
		var head_gap := Control.new()
		head_gap.custom_minimum_size = Vector2(8, 58)
		head.add_child(head_gap)
		var t := _display_label(26, ORANGE)
		t.text = "Bindings"
		t.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		head.add_child(t)
		for col_name: String in ["Keyboard / mouse", "Gamepad"]:
			var c := _label(18, Color(0.8, 0.8, 1.0, 0.85))
			c.text = col_name
			c.custom_minimum_size = Vector2(280, 0)
			c.size_flags_vertical = Control.SIZE_SHRINK_END
			c.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			c.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
			head.add_child(c)
		box.add_child(head)
		for r: Array in Settings.REBIND:
			var h := HBoxContainer.new()
			h.add_theme_constant_override("separation", 12)
			var inset := Control.new()
			inset.custom_minimum_size = Vector2(8, 0) # with the separation: an option row's name inset
			h.add_child(inset)
			var name := _label(21, CREAM)
			name.text = r[1]
			name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			name.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			h.add_child(name)
			var kb := _bind_button(r[0], "kb", r[2])
			var pad := _bind_button(r[0], "pad", r[3])
			h.add_child(kb)
			h.add_child(pad)
			box.add_child(h)
			nav.append([kb, pad if r[3] else null])
	_options_desc = _label(18, Color(0.85, 0.85, 1.0, 0.9))
	_options_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_options_desc.custom_minimum_size = Vector2(0, 56)
	_options_desc.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	list.add_child(_options_desc)
	var bottom := HBoxContainer.new()
	bottom.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom.add_theme_constant_override("separation", 24)
	list.add_child(bottom)
	var reset := _menu_button()
	reset.text = "Reset %s to defaults" % tab.to_lower()
	var back := _menu_button()
	_prompt(back, "Back  ({ui_cancel})")
	for b: Button in [reset, back]:
		b.alignment = HORIZONTAL_ALIGNMENT_CENTER
		b.custom_minimum_size = Vector2(320, 0)
		b.mouse_entered.connect(b.grab_focus)
		bottom.add_child(b)
	reset.pressed.connect(func():
		Settings.reset_section(TAB_SECTION[tab])
		_build_options())
	back.pressed.connect(_options_back)
	reset.focus_entered.connect(func(): _set_desc("Puts every %s setting back the way the game starts." % tab.to_lower()))
	back.focus_entered.connect(func(): _set_desc(""))
	reset.focus_neighbor_right = reset.get_path_to(back)
	back.focus_neighbor_left = back.get_path_to(reset)
	# Explicit focus: straight down the rows (never jumping to the buttons past rows still scrolled
	# out of view), left / right only between the two binding columns.
	for i in nav.size():
		var a: Control = nav[i][0]
		var p: Control = nav[i][1]
		var up: Array = nav[i - 1] if i > 0 else []
		var down: Array = nav[i + 1] if i + 1 < nav.size() else []
		for c: Control in ([a, p] if p else [a]):
			var right_col := c == p
			var up_c: Control = (up[1] if right_col and up[1] else up[0]) if not up.is_empty() else c
			var down_c: Control = ((down[1] if right_col and down[1] else down[0]) if not down.is_empty() else (back if right_col else reset))
			c.focus_neighbor_top = c.get_path_to(up_c)
			c.focus_neighbor_bottom = c.get_path_to(down_c)
		if p:
			a.focus_neighbor_right = a.get_path_to(p)
			p.focus_neighbor_left = p.get_path_to(a)
			p.focus_neighbor_right = p.get_path_to(p)
		if not p:
			a.focus_neighbor_right = a.get_path_to(a) # left / right change the value instead
		a.focus_neighbor_left = a.get_path_to(a)
	if not nav.is_empty():
		var last: Array = nav[nav.size() - 1]
		reset.focus_neighbor_top = reset.get_path_to(last[0])
		back.focus_neighbor_top = back.get_path_to(last[1] if last[1] else last[0])
		(nav[0][0] as Control).grab_focus()
	else:
		back.grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


## A setting row: its name on the left, the value (with a bar for sliders) between arrows on the right.
func _option_row(row: Dictionary) -> Button:
	var b := _menu_button()
	b.custom_minimum_size = Vector2(0, 56)
	var h := HBoxContainer.new()
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 20
	h.offset_right = -16
	h.add_theme_constant_override("separation", 12)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(h)
	var name := _label(22, CREAM)
	name.text = str(row["label"])
	name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	name.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	h.add_child(name)
	var left := _label(26, ORANGE)
	left.text = "<"
	left.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	h.add_child(left)
	var mid := VBoxContainer.new()
	mid.custom_minimum_size = Vector2(340, 0)
	mid.alignment = BoxContainer.ALIGNMENT_CENTER
	mid.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(mid)
	var val := _label(22, Color(1.0, 0.92, 0.65))
	val.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	mid.add_child(val)
	var slider := row.has("min")
	var bar: ProgressBar = null
	if slider:
		bar = ProgressBar.new()
		bar.show_percentage = false
		bar.min_value = float(row["min"])
		bar.max_value = float(row["max"])
		bar.step = 0.0
		bar.custom_minimum_size = Vector2(260, 8)
		bar.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var bg := StyleBoxFlat.new()
		bg.bg_color = Color(0.2, 0.18, 0.36)
		bg.set_corner_radius_all(4)
		var fill := StyleBoxFlat.new()
		fill.bg_color = ORANGE
		fill.set_corner_radius_all(4)
		bar.add_theme_stylebox_override("background", bg)
		bar.add_theme_stylebox_override("fill", fill)
		mid.add_child(bar)
	var right := _label(26, ORANGE)
	right.text = ">"
	right.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	h.add_child(right)
	var section: String = row["section"]
	var key: String = row["key"]
	var refresh := func():
		var v: Variant = Settings.get_value(section, key)
		if slider:
			var shown := float(v) * float(row["scale"])
			var fmt: String = row["fmt"]
			val.text = fmt % roundi(shown) if fmt.contains("%d") else fmt % shown
			bar.value = float(v)
		else:
			var i: int = (row["values"] as Array).find(v)
			val.text = str(row["labels"][i]) if i >= 0 else str(v)
	var step := func(dir: int):
		var v: Variant = Settings.get_value(section, key)
		if slider:
			var st := float(row["step"])
			var nv := clampf(snappedf(float(v) + dir * st, st), float(row["min"]), float(row["max"]))
			if is_equal_approx(nv, float(v)):
				return
			Settings.set_value(section, key, roundi(nv) if Settings.DEFAULTS[section][key] is int else nv)
		else:
			var vals: Array = row["values"]
			var i := vals.find(v)
			Settings.set_value(section, key, vals[posmod((i if i >= 0 else 0) + dir, vals.size())])
		_refresh_option_rows()
	b.set_meta("step", step)
	if slider:
		b.set_meta("slider", true)
	_row_refresh.append(refresh)
	refresh.call()
	b.focus_entered.connect(func(): _set_desc(str(row["desc"])))
	b.mouse_entered.connect(b.grab_focus)
	b.gui_input.connect(func(ev: InputEvent):
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
			step.call(-1 if left.get_global_rect().grow(16).has_point(ev.global_position) else 1)
			b.accept_event())
	return b


## One binding: the action's key (or mouse button) or gamepad button; press it, then the new one.
func _bind_button(action: String, kind: String, enabled: bool) -> Button:
	var b := _menu_button()
	b.custom_minimum_size = Vector2(280, 48)
	b.alignment = HORIZONTAL_ALIGNMENT_CENTER
	var refresh := func():
		if not enabled:
			b.text = "Left stick" if action.begins_with("move") else "Right stick"
		elif not _capture.is_empty() and _capture["button"] == b:
			b.text = "Press a key or mouse button..." if kind == "kb" else "Press a button..."
		else:
			b.text = Settings.key_label(action, true) if kind == "kb" else Settings.pad_label(action)
	_row_refresh.append(refresh)
	refresh.call()
	if not enabled:
		b.disabled = true
		b.focus_mode = Control.FOCUS_NONE
		return b
	b.pressed.connect(func():
		_capture = {"action": action, "kind": kind, "button": b, "t": 5.0}
		_set_desc("Press the new key or mouse button (Esc cancels)." if kind == "kb" else "Press the new button or trigger (wait 5 seconds to cancel{kb:, or press Esc|pad:}).")
		_refresh_option_rows())
	b.mouse_entered.connect(b.grab_focus)
	b.focus_entered.connect(func():
		_set_desc("Select to change. %s" % ("Keys and mouse buttons both work." if kind == "kb" else "Buttons and triggers both work.")))
	return b


func _refresh_option_rows() -> void:
	for r in _row_refresh:
		r.call()
	if _options_desc and is_instance_valid(_options_desc):
		_options_desc.text = Settings.prompts(_desc_src)


## The Options help line (templates with prompts and {kb:...|pad:...} choices, kept current).
func _set_desc(src: String) -> void:
	_desc_src = src
	if _options_desc:
		_options_desc.text = Settings.prompts(src)


func _end_capture() -> void:
	var b: Button = _capture.get("button")
	_capture = {}
	_capture_end_frame = Engine.get_process_frames()
	_refresh_option_rows()
	if is_instance_valid(b):
		b.grab_focus()


func _options_back() -> void:
	_capture = {}
	if _options_from == "main":
		show_main_menu("options")
	else:
		open_pause("options")


## A rebind takes the very next key / mouse button (keyboard column) or button / trigger (gamepad).
func _input(event: InputEvent) -> void:
	if _capture.is_empty() or event is InputEventMouseMotion:
		return
	get_viewport().set_input_as_handled() # nothing else (menu navigation) sees it
	var kind: String = _capture["kind"]
	var action: String = _capture["action"]
	var bind: InputEvent = null
	var cancel := false
	if event is InputEventKey:
		var k := event as InputEventKey
		if not k.pressed or k.echo:
			return
		if k.keycode == KEY_ESCAPE and not (kind == "kb" and action == "pause"):
			cancel = true
		elif kind == "kb":
			var nk := InputEventKey.new()
			nk.physical_keycode = k.physical_keycode if k.physical_keycode != KEY_NONE else k.keycode
			bind = nk
	elif event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if kind == "kb" and mb.pressed and mb.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_XBUTTON1, MOUSE_BUTTON_XBUTTON2]:
			var nm := InputEventMouseButton.new()
			nm.button_index = mb.button_index
			bind = nm
	elif event is InputEventJoypadButton:
		if not event.is_pressed():
			return
		if kind == "pad":
			var nb := InputEventJoypadButton.new()
			nb.button_index = (event as InputEventJoypadButton).button_index
			bind = nb
		else:
			cancel = true
	elif event is InputEventJoypadMotion and kind == "pad":
		var jm := event as InputEventJoypadMotion
		if (jm.axis == JOY_AXIS_TRIGGER_LEFT or jm.axis == JOY_AXIS_TRIGGER_RIGHT) and jm.axis_value > 0.6:
			var na := InputEventJoypadMotion.new()
			na.axis = jm.axis
			na.axis_value = 1.0
			bind = na
	if bind:
		Settings.set_binding(action, kind, bind)
	if bind or cancel:
		_end_capture()


# ---------------------------------------------------------------- save and load
## The save screens (scripts/saves.gd keeps the files): Save game from the pause menu (pick a slot,
## then name it), Load game from the title screen or the pause menu, a delete button per save and
## a confirm step before anything is lost.
var _menu_back := Callable() # what Back / Esc / B does on the save, load, name and confirm screens
var _load_from := ""
const SLOT_ROW_H := 148.0


func open_save(focus_id := "") -> void:
	var infos := []
	for id: String in Saves.slot_ids():
		if id != "autosave":
			infos.append(Saves.info(id))
	_slot_list("Save game", "Pick a slot for this night.", ORANGE, infos, func(info: Dictionary): _save_form(info),
		func(): open_pause("save"), focus_id, func(): open_save())


func open_load(from: String, focus_id := "") -> void:
	_load_from = from
	var infos := []
	for id: String in Saves.slot_ids():
		if Saves.has_save(id):
			infos.append(Saves.info(id))
	infos.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return float(a["time"]) > float(b["time"]))
	var back := func():
		if from == "main":
			show_main_menu("load")
		else:
			open_pause("load")
	var pick := func(info: Dictionary):
		if from == "main":
			_load_and_play(str(info["id"]))
		else:
			_confirm("Load this save?", "\"%s\"\nAnything since your last save will be lost." % info["name"], "Load", true,
				func(): _load_and_play(str(info["id"])), func(): open_load(from, str(info["id"])))
	_slot_list("Load game", "Newest first.", TEAL, infos, pick, back, focus_id, func(): open_load(from))
	if infos.is_empty():
		back.call()


func _load_and_play(id: String) -> void:
	var name := str(Saves.info(id).get("name", ""))
	_close_menu(false)
	_menu_kind = ""
	_menu_back = Callable()
	if Saves.load_slot(id):
		_set_play_hud(true)
		toast("Loaded \"%s\"" % name)
	elif _load_from == "main":
		show_main_menu("load")
		toast("That save couldn't be loaded.")
	else:
		open_pause("load")
		toast("That save couldn't be loaded.")


## Name the save (the slot's old name, or the place the kid is in), then save.
func _save_form(info: Dictionary) -> void:
	var id: String = info["id"]
	var overwrite := not bool(info["empty"])
	var list := _open_menu("Save game", ("Replaces \"%s\" in %s." % [info["name"], Saves.slot_label(id)]) if overwrite else Saves.slot_label(id), ORANGE)
	_menu_kind = "sub"
	_menu_back = func(): open_save(id)
	var l := _label(20, Color(0.85, 0.85, 1.0, 0.9))
	l.text = "Name"
	list.add_child(l)
	var edit := LineEdit.new()
	edit.max_length = 40
	edit.text = str(info["name"]) if overwrite else (_area_name.text if _area_name.text != "" else Saves.slot_label(id))
	edit.add_theme_font_size_override("font_size", 24)
	edit.add_theme_stylebox_override("normal", _box(Color(0.4, 0.4, 0.7), 14, Color(0.1, 0.1, 0.22, 0.95)))
	edit.add_theme_stylebox_override("focus", _box(ORANGE, 14, Color(0.14, 0.12, 0.28, 0.95)))
	edit.add_theme_color_override("font_color", CREAM)
	edit.custom_minimum_size = Vector2(0, 54)
	edit.select_all_on_focus = true
	list.add_child(edit)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 8)
	list.add_child(gap)
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 20)
	list.add_child(row)
	var ok := _menu_button()
	ok.text = "Overwrite" if overwrite else "Save"
	var cancel := _menu_button()
	_prompt(cancel, "Cancel  ({ui_cancel})")
	for b: Button in [ok, cancel]:
		b.alignment = HORIZONTAL_ALIGNMENT_CENTER
		b.custom_minimum_size = Vector2(250, 0)
		b.mouse_entered.connect(b.grab_focus)
		row.add_child(b)
	var do_save := func(_t := ""):
		if Saves.save_slot(id, edit.text):
			open_pause("resume")
			toast("Game saved")
		else:
			open_save(id)
			toast("Couldn't save the game.")
	ok.pressed.connect(do_save)
	edit.text_submitted.connect(do_save)
	cancel.pressed.connect(_menu_back)
	edit.focus_neighbor_bottom = edit.get_path_to(ok)
	ok.focus_neighbor_top = ok.get_path_to(edit)
	cancel.focus_neighbor_top = cancel.get_path_to(edit)
	ok.focus_neighbor_right = ok.get_path_to(cancel)
	cancel.focus_neighbor_left = cancel.get_path_to(ok)
	# Typing on a keyboard starts in the name; on a gamepad, on Save (the name is already filled in).
	if Settings.prompt_style() == "kb":
		edit.grab_focus()
		edit.caret_column = edit.text.length()
	else:
		ok.grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


## A yes / no question. `focus_yes` false puts the cursor on No (for deleting).
func _confirm(title: String, text: String, yes: String, focus_yes: bool, on_yes: Callable, on_no: Callable) -> void:
	var list := _open_menu(title, text, Color(1.0, 0.6, 0.5) if not focus_yes else TEAL)
	_menu_kind = "sub"
	_menu_back = on_no
	_menu.offset_left = -360
	_menu.offset_right = 360
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 20)
	list.add_child(row)
	var y := _menu_button()
	y.text = yes
	var n := _menu_button()
	_prompt(n, "Cancel  ({ui_cancel})")
	for b: Button in [y, n]:
		b.alignment = HORIZONTAL_ALIGNMENT_CENTER
		b.custom_minimum_size = Vector2(250, 0)
		b.mouse_entered.connect(b.grab_focus)
		row.add_child(b)
	y.pressed.connect(on_yes)
	n.pressed.connect(on_no)
	y.focus_neighbor_right = y.get_path_to(n)
	n.focus_neighbor_left = n.get_path_to(y)
	(y if focus_yes else n).grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


## A list of save slots: a big button per slot (thumbnail and summary) with Delete beside it.
func _slot_list(title: String, sub: String, border: Color, infos: Array, on_pick: Callable, back: Callable, focus_id: String, reopen: Callable) -> void:
	var list := _open_menu(title, sub, border)
	_menu_kind = "sub"
	_menu_back = back
	_menu.offset_left = -600
	_menu.offset_right = 600
	var screen_h := get_viewport().get_visible_rect().size.y
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.follow_focus = true
	# Whole rows only (no sliver of a half-hidden slot above the Back button).
	var fit := maxi(1, floori((clampf(screen_h - 330.0, 280.0, 760.0) + 12.0) / (SLOT_ROW_H + 12.0)))
	scroll.custom_minimum_size = Vector2(0, mini(infos.size(), fit) * (SLOT_ROW_H + 12.0) - 12.0 + 8.0)
	list.add_child(scroll)
	var box := VBoxContainer.new()
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override("separation", 12)
	scroll.add_child(_scroll_gutter(scroll, box))
	var nav: Array = [] # [slot button, delete button or null]
	var focus: Control = null
	for info: Dictionary in infos:
		var h := HBoxContainer.new()
		h.add_theme_constant_override("separation", 12)
		box.add_child(h)
		var b := _slot_button(info)
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		b.pressed.connect(func(): on_pick.call(info))
		h.add_child(b)
		var del: Button = null
		if not bool(info["empty"]):
			del = _menu_button()
			del.text = "Delete"
			del.alignment = HORIZONTAL_ALIGNMENT_CENTER
			del.custom_minimum_size = Vector2(130, 0)
			del.mouse_entered.connect(del.grab_focus)
			var id: String = info["id"]
			var really := func():
				Saves.delete_slot(id)
				reopen.call()
			del.pressed.connect(func():
				_confirm("Delete this save?", "\"%s\"\nThis can't be undone." % info["name"], "Delete", false, really, reopen))
			h.add_child(del)
		else:
			var hole := Control.new()
			hole.custom_minimum_size = Vector2(130, 0)
			h.add_child(hole)
		nav.append([b, del])
		if info["id"] == focus_id:
			focus = b
	var close := _menu_button()
	_prompt(close, "Back  ({ui_cancel})")
	close.alignment = HORIZONTAL_ALIGNMENT_CENTER
	close.custom_minimum_size = Vector2(320, 0)
	close.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	close.mouse_entered.connect(close.grab_focus)
	close.pressed.connect(back)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 6)
	list.add_child(gap)
	list.add_child(close)
	# Explicit focus: down the slots (never past the ones still scrolled out of view), across to Delete.
	for i in nav.size():
		var b: Button = nav[i][0]
		var del: Button = nav[i][1]
		var up: Button = nav[i - 1][0] if i > 0 else b
		var down: Button = nav[i + 1][0] if i + 1 < nav.size() else close
		b.focus_neighbor_top = b.get_path_to(up)
		b.focus_neighbor_bottom = b.get_path_to(down)
		b.focus_neighbor_left = b.get_path_to(b)
		b.focus_neighbor_right = b.get_path_to(del if del else b)
		if del:
			del.focus_neighbor_left = del.get_path_to(b)
			del.focus_neighbor_right = del.get_path_to(del)
			del.focus_neighbor_top = del.get_path_to(nav[i - 1][1] if i > 0 and nav[i - 1][1] else up)
			del.focus_neighbor_bottom = del.get_path_to(nav[i + 1][1] if i + 1 < nav.size() and nav[i + 1][1] else down)
	if not nav.is_empty():
		close.focus_neighbor_top = close.get_path_to(nav[nav.size() - 1][0])
		(focus if focus else nav[0][0] as Control).grab_focus()
	else:
		close.grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


func _slot_button(info: Dictionary) -> Button:
	var b := _menu_button()
	# Laid out inside a 14 px margin (the button's own stylebox padding doesn't reach anchored
	# children); the text block (about 114 px) and the 16:9 thumbnail both fit with room to spare.
	b.custom_minimum_size = Vector2(0, SLOT_ROW_H)
	var pad := MarginContainer.new()
	pad.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side: String in ["left", "top", "bottom"]:
		pad.add_theme_constant_override("margin_" + side, 14)
	pad.add_theme_constant_override("margin_right", 20)
	pad.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(pad)
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", 22)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	pad.add_child(h)
	var frame := PanelContainer.new()
	var fb := StyleBoxFlat.new()
	fb.bg_color = Color(0.05, 0.05, 0.12)
	fb.set_corner_radius_all(10)
	frame.add_theme_stylebox_override("panel", fb)
	frame.custom_minimum_size = Vector2(192, 108)
	frame.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	frame.clip_children = CanvasItem.CLIP_CHILDREN_AND_DRAW # rounded corners on the thumbnail too
	frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(frame)
	var empty := bool(info["empty"])
	if not empty and info.get("thumb"):
		var t := TextureRect.new()
		t.texture = info["thumb"]
		t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		frame.add_child(t)
	else:
		var e := _icon(_candy_image() if empty else _moon_image(), 54)
		e.modulate.a = 0.35
		e.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		e.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		e.mouse_filter = Control.MOUSE_FILTER_IGNORE
		frame.add_child(e)
	var v := VBoxContainer.new()
	v.alignment = BoxContainer.ALIGNMENT_CENTER
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	v.add_theme_constant_override("separation", 2)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(v)
	var title := _display_label(25, CREAM if not empty else Color(0.7, 0.7, 0.85))
	title.text = "Empty slot" if empty else str(info["name"])
	title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	v.add_child(title)
	var when := _label(16, Color(0.8, 0.8, 1.0, 0.75))
	v.add_child(when)
	if empty:
		when.text = Saves.slot_label(str(info["id"]))
		return b
	var mode: int = clampi(int(info.get("mode", 1)), 0, 1)
	var line1 := _label(18, ORANGE if mode == 0 else Color(0.78, 0.65, 1.0))
	var area := str(info.get("area", ""))
	line1.text = Saves.MODE_NAMES[mode] + ("   ·   " + area if area != "" else "")
	v.add_child(line1)
	v.move_child(line1, 1)
	var bits := ["%s candy" % _thousands(int(info.get("candy", 0))), "%d houses" % int(info.get("houses", 0))]
	if mode == 1:
		bits.append("%d freed" % int(info.get("freed", 0)))
	bits.append("%d/6 costumes" % int(info.get("costumes", 1)))
	bits.append(Saves.play_time_text(float(info.get("play_time", 0.0))) + " played")
	var line2 := _label(17, CREAM)
	line2.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	line2.text = "   ·   ".join(bits)
	v.add_child(line2)
	v.move_child(line2, 2)
	when.text = "%s   ·   %s" % [Saves.slot_label(str(info["id"])), Saves.date_text(float(info.get("time", 0.0)))]
	return b


static func _thousands(n: int) -> String:
	var s := str(absi(n))
	var out := ""
	while s.length() > 3:
		out = "," + s.substr(s.length() - 3) + out
		s = s.substr(0, s.length() - 3)
	return ("-" if n < 0 else "") + s + out


## Wraps a ScrollContainer's content with a gutter on the right for its scrollbar (only while the
## scrollbar shows, so content that fits stays centred in the panel).
func _scroll_gutter(scroll: ScrollContainer, content: Control) -> MarginContainer:
	var m := MarginContainer.new()
	m.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	m.add_theme_constant_override("margin_left", 2)
	m.add_theme_constant_override("margin_right", 2)
	m.add_theme_constant_override("margin_top", 4)
	m.add_theme_constant_override("margin_bottom", 4)
	m.add_child(content)
	var bar := scroll.get_v_scroll_bar()
	bar.visibility_changed.connect(func(): m.add_theme_constant_override("margin_right", 20 if bar.visible else 2))
	return m


# ---------------------------------------------------------------- menus
func _open_menu(title: String, sub: String, border: Color) -> VBoxContainer:
	_close_menu(false)
	_menu_kind = ""
	_preview_pivot = null
	# Dim the world (and any area banner) behind the panel so the menu reads cleanly.
	_menu_dim = ColorRect.new()
	_menu_dim.color = Color(0.02, 0.01, 0.07, 0.55)
	_menu_dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.add_child(_menu_dim)
	_menu = PanelContainer.new()
	var panel := _box(border, 26, Color(0.06, 0.06, 0.16, 0.98))
	panel.content_margin_left = 32
	panel.content_margin_right = 32
	panel.content_margin_top = 22
	panel.content_margin_bottom = 28
	panel.shadow_size = 18
	panel.shadow_color = Color(0, 0, 0, 0.45)
	_menu.add_theme_stylebox_override("panel", panel)
	_menu.anchor_left = 0.5
	_menu.anchor_right = 0.5
	_menu.anchor_top = 0.5
	_menu.anchor_bottom = 0.5
	# Fixed width, height fits the content (grows both ways from the centre).
	_menu.offset_left = -380
	_menu.offset_right = 380
	_menu.offset_top = 0
	_menu.offset_bottom = 0
	_menu.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_menu.grow_vertical = Control.GROW_DIRECTION_BOTH
	_root.add_child(_menu)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	_menu.add_child(v)
	var t := _display_label(40, border)
	t.text = title
	t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(t)
	var s := _label(22, CREAM)
	s.text = sub
	s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	s.visible = sub != ""
	v.add_child(s)
	var sep := HSeparator.new()
	sep.add_theme_constant_override("separation", 14)
	v.add_child(sep)
	return v


func _menu_button() -> Button:
	var b := Button.new()
	b.add_theme_font_size_override("font_size", 22)
	b.alignment = HORIZONTAL_ALIGNMENT_LEFT
	b.add_theme_stylebox_override("normal", _box(Color(0.4, 0.4, 0.7), 14, Color(0.12, 0.12, 0.26, 0.95)))
	b.add_theme_stylebox_override("hover", _box(ORANGE, 14, Color(0.2, 0.15, 0.3, 0.95)))
	b.add_theme_stylebox_override("focus", _box(ORANGE, 14, Color(0.25, 0.17, 0.32, 0.95)))
	b.add_theme_stylebox_override("pressed", _box(TEAL, 14, Color(0.15, 0.25, 0.3, 0.95)))
	b.add_theme_stylebox_override("disabled", _box(Color(0.3, 0.3, 0.4), 14, Color(0.1, 0.1, 0.16, 0.8)))
	b.add_theme_color_override("font_color", CREAM)
	b.add_theme_color_override("font_focus_color", Color(1.0, 0.95, 0.7))
	b.add_theme_color_override("font_hover_color", Color(1.0, 0.95, 0.7))
	b.add_theme_color_override("font_pressed_color", Color(1.0, 0.95, 0.7))
	b.add_theme_color_override("font_hover_pressed_color", Color(1.0, 0.95, 0.7))
	b.add_theme_color_override("font_disabled_color", Color(0.6, 0.6, 0.7))
	return b


func _finish_menu(list: VBoxContainer, focus: Button) -> Button:
	var close := _menu_button()
	_prompt(close, "Close  ({shop} / {ui_cancel})")
	close.alignment = HORIZONTAL_ALIGNMENT_CENTER
	close.custom_minimum_size = Vector2(320, 0)
	close.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	close.mouse_entered.connect(close.grab_focus)
	close.pressed.connect(func(): _close_menu(true))
	list.add_child(close)
	(focus if focus else close).grab_focus()
	_menu_opened_frame = Engine.get_process_frames()
	return close


func _close_menu(notify: bool) -> void:
	if _menu_dim:
		_menu_dim.queue_free()
		_menu_dim = null
	if _menu:
		_menu.queue_free()
		_menu = null
		if notify:
			menu_closed.emit()


func _process(delta: float) -> void:
	var typing := get_viewport().gui_get_focus_owner() is LineEdit
	if Input.is_action_just_pressed("toggle_hud") and not typing:
		_root.visible = not _root.visible
	if Input.is_action_just_pressed("toggle_debug"):
		_debug.visible = not _debug.visible
	if _menu_kind == "options" and _menu:
		_options_input(delta)
	elif _menu and Engine.get_process_frames() != _menu_opened_frame:
		if _menu_kind == "sub":
			if (Input.is_action_just_pressed("ui_cancel") or Input.is_action_just_pressed("pause")) and _menu_back.is_valid():
				_menu_back.call()
		elif _menu_kind == "main":
			pass # the title screen only closes by picking a mode
		elif Input.is_action_just_pressed("shop") or Input.is_action_just_pressed("ui_cancel") or (_menu_kind == "pause" and Input.is_action_just_pressed("pause")):
			_close_menu(true)
		elif _menu_kind == "shop" and Input.is_action_just_pressed("shop_tab_prev"):
			shop_tab.emit(-1)
		elif _menu_kind == "shop" and Input.is_action_just_pressed("shop_tab_next"):
			shop_tab.emit(1)
	if _preview_pivot and is_instance_valid(_preview_pivot):
		_preview_pivot.rotate_y(delta * 0.8)
	if _toast_t > 0.0:
		_toast_t -= delta
		_toast_panel.modulate.a = clampf(_toast_t / 0.6, 0.0, 1.0)
	if _hint_t > 0.0:
		_hint_t -= delta
		_hint.modulate.a = clampf(_hint_t / 1.5, 0.0, 1.0)


## The Options screen: back, tab switching, and left / right (held: repeating) on the focused row.
func _options_input(delta: float) -> void:
	if not _capture.is_empty():
		_capture["t"] = float(_capture["t"]) - delta
		if _capture["kind"] == "pad" and float(_capture["t"]) <= 0.0:
			_end_capture()
		return
	var frame := Engine.get_process_frames()
	if frame == _menu_opened_frame or frame == _capture_end_frame:
		return
	if Input.is_action_just_pressed("ui_cancel") or Input.is_action_just_pressed("pause"):
		_options_back()
		return
	if Input.is_action_just_pressed("shop_tab_prev") or Input.is_action_just_pressed("shop_tab_next"):
		_options_tab = posmod(_options_tab + (-1 if Input.is_action_just_pressed("shop_tab_prev") else 1), OPTION_TABS.size())
		_build_options()
		return
	var f := get_viewport().gui_get_focus_owner()
	if f == null or not f.has_meta("step"):
		_hold_dir = 0
		return
	var step: Callable = f.get_meta("step")
	if Input.is_action_just_pressed("ui_accept"):
		step.call(1)
	var dir := 0
	if Input.is_action_pressed("ui_left"):
		dir = -1
	elif Input.is_action_pressed("ui_right"):
		dir = 1
	if dir == 0:
		_hold_dir = 0
	elif dir != _hold_dir:
		_hold_dir = dir
		_hold_t = 0.4
		step.call(dir)
	else:
		_hold_t -= delta
		if _hold_t <= 0.0:
			_hold_t = 0.08 if f.has_meta("slider") else 0.25
			step.call(dir)
