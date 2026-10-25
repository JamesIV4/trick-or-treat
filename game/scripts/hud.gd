extends CanvasLayer
## Minimal, toggleable HUD: candy carried, the current area's infestation status, area banners,
## toasts and callouts, a hurt flash, the candy shop and the travel-gate menu, a controls hint and
## an F3 debug panel.

signal purchase(id: String)
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
var _banner: Label
var _banner_sub: Label
var _hurt: ColorRect
var _fade: ColorRect
var _menu: PanelContainer
var _menu_opened_frame := -1
var _last_value := 0
var _seed := 0


func _ready() -> void:
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_root)

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
	_candy = _label(32, CREAM)
	_candy.text = "0"
	ccol.add_child(_candy)
	var shop_hint := _label(15, Color(1.0, 0.8, 0.55, 0.85))
	shop_hint.text = "Shop: Tab / Start"
	ccol.add_child(shop_hint)

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
	_area_name = _label(26, Color(0.86, 0.95, 1.0))
	arow.add_child(_area_name)
	_area_line = _label(19, Color(1.0, 0.75, 0.6))
	col.add_child(_area_line)
	_area_candy = _label(19, Color(0.9, 0.85, 1.0))
	col.add_child(_area_candy)

	# Callout (milestones) and the area banner.
	_callout = _label(64, Color(1, 1, 1))
	_root.add_child(_callout)
	_callout.anchor_left = 0.0
	_callout.anchor_right = 1.0
	_callout.offset_top = 215
	_callout.offset_bottom = 305
	_callout.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_callout.add_theme_constant_override("outline_size", 14)
	_callout.modulate.a = 0.0
	_banner = _label(52, CREAM)
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
	_hint.text = "Move WASD/stick   Jump Space/A (again: flap, hold: glide)   Fire F/click/Y   Tail spin E/X   Roll, pound Shift/B   Shop Tab/Start   Car V/LB   Camera Q C/right stick"

	_debug = _label(16, Color(0.8, 1.0, 0.8))
	_root.add_child(_debug)
	_debug.position = Vector2(30, 110)
	_debug.visible = false

	_fade = ColorRect.new()
	_fade.color = Color(0.02, 0.02, 0.08, 0.0)
	_fade.set_anchors_preset(Control.PRESET_FULL_RECT)
	_fade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(_fade)


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
	sb.content_margin_left = 18
	sb.content_margin_right = 22
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


func set_candy(value: int) -> void:
	_candy.text = "%d" % value
	if value > _last_value and _candy_panel:
		# Punch the counter so every pickup registers.
		_candy_panel.pivot_offset = _candy_panel.size * 0.5
		var tw := create_tween()
		tw.tween_property(_candy_panel, "scale", Vector2(1.12, 1.12), 0.06)
		tw.tween_property(_candy_panel, "scale", Vector2.ONE, 0.18).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_last_value = value


func set_area(d: Dictionary) -> void:
	_area_name.text = d.get("name", "")
	if not d.get("settlement", false):
		_area_line.text = "Open country"
		_area_line.add_theme_color_override("font_color", Color(0.75, 0.85, 1.0))
		_area_candy.text = ""
		return
	if d.get("freed", false):
		_area_line.text = "Freed!  The travel gate is awake."
		_area_line.add_theme_color_override("font_color", TEAL)
	else:
		_area_line.text = "Monster nests %d/%d   Thorn nests %d/%d" % [d.get("nests_left", 0), d.get("nests", 0), d.get("hearts_left", 0), d.get("hearts", 0)]
		_area_line.add_theme_color_override("font_color", Color(1.0, 0.72, 0.55))
	_area_candy.text = "Candy found here %d / %d" % [d.get("candy", 0), d.get("candy_total", 0)]


func banner(title: String, sub: String) -> void:
	_banner.text = title
	_banner_sub.text = sub
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
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_callout, "scale", Vector2(1.0, 1.0), 0.35).set_trans(Tween.TRANS_ELASTIC).set_ease(Tween.EASE_OUT)
	tw.tween_property(_callout, "rotation", 0.0, 0.4).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.chain().tween_property(_callout, "modulate:a", 0.0, 0.5).set_delay(1.0)


func toast(text: String) -> void:
	_toast.text = text
	_toast_t = 3.0
	_toast_panel.modulate.a = 1.0
	_toast_panel.offset_left = 0
	_toast_panel.offset_right = 0


func hurt() -> void:
	_hurt.color.a = 0.32
	create_tween().tween_property(_hurt, "color:a", 0.0, 0.45)


func fade(on: bool) -> void:
	create_tween().tween_property(_fade, "color:a", 1.0 if on else 0.0, 0.35)


func open_shop(items: Array, wallet: int) -> void:
	var list := _open_menu("Candy Shop", "You have %d candy" % wallet, ORANGE)
	var first: Button = null
	for item in items:
		var lvl: int = item["level"]
		var mx: int = item["max"]
		var b := _menu_button()
		var steps := ("Lv %d/%d" % [lvl, mx]) if mx > 1 else ("owned" if lvl > 0 else "")
		if lvl >= mx:
			b.text = "%s   %s   (done)" % [item["name"], steps]
			b.disabled = true
		else:
			b.text = "%s   %s   %d candy" % [item["name"], steps, item["cost"]]
			b.disabled = int(item["cost"]) > wallet
		var id: String = item["id"]
		b.pressed.connect(func(): purchase.emit(id))
		list.add_child(b)
		var desc := _label(17, Color(0.85, 0.85, 1.0, 0.85))
		desc.text = "      " + str(item["desc"])
		list.add_child(desc)
		if first == null and not b.disabled:
			first = b
	_finish_menu(list, first)


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


# ---------------------------------------------------------------- menus
func _open_menu(title: String, sub: String, border: Color) -> VBoxContainer:
	_close_menu(false)
	_menu = PanelContainer.new()
	_menu.add_theme_stylebox_override("panel", _box(border, 26, Color(0.06, 0.06, 0.16, 0.94)))
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
	v.add_theme_constant_override("separation", 6)
	_menu.add_child(v)
	var t := _label(40, border)
	t.text = title
	t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(t)
	var s := _label(22, CREAM)
	s.text = sub
	s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(s)
	v.add_child(HSeparator.new())
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
	b.add_theme_color_override("font_disabled_color", Color(0.6, 0.6, 0.7))
	return b


func _finish_menu(list: VBoxContainer, focus: Button) -> void:
	var close := _menu_button()
	close.text = "Close  (Tab / Start / Esc)"
	close.pressed.connect(func(): _close_menu(true))
	list.add_child(close)
	(focus if focus else close).grab_focus()
	_menu_opened_frame = Engine.get_process_frames()


func _close_menu(notify: bool) -> void:
	if _menu:
		_menu.queue_free()
		_menu = null
		if notify:
			menu_closed.emit()


func _process(delta: float) -> void:
	if Input.is_action_just_pressed("toggle_hud"):
		_root.visible = not _root.visible
	if Input.is_action_just_pressed("toggle_debug"):
		_debug.visible = not _debug.visible
	if _menu and Engine.get_process_frames() != _menu_opened_frame:
		if Input.is_action_just_pressed("shop") or Input.is_action_just_pressed("ui_cancel"):
			_close_menu(true)
	if _toast_t > 0.0:
		_toast_t -= delta
		_toast_panel.modulate.a = clampf(_toast_t / 0.6, 0.0, 1.0)
	if _hint_t > 0.0:
		_hint_t -= delta
		_hint.modulate.a = clampf(_hint_t / 1.5, 0.0, 1.0)
