extends Node
## Autoload: saved games in user://saves. Each slot is three files: <id>.save (the game's state from
## FestivalGame.save_state, as a binary Variant), <id>.png (a thumbnail of the moment) and <id>.cfg
## (a summary for the Save / Load screens in scripts/hud.gd). Also keeps the play-time clock and
## autosaves every few minutes of play.

const DIR := "user://saves/"
const SLOTS := 8 # manual slots, plus the autosave
const AUTOSAVE_EVERY := 180.0 # seconds of play
const THUMB := Vector2i(320, 180)
const MODE_NAMES := ["Trick or Treat", "Save the Town"]

var play_time := 0.0 # seconds played in the current game
var _auto_t := 0.0
var _shot: Image # the last frame of play, taken as the pause menu opens (not the menu itself)


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS


func _process(delta: float) -> void:
	var g := game()
	if g == null or Settings.scripted or not g.has_method("is_playing") or get_tree().paused:
		return
	if g.is_playing():
		play_time += delta
		_auto_t += delta
		if _auto_t >= AUTOSAVE_EVERY:
			_auto_t = 0.0
			save_slot("autosave", "Autosave")


## The running game (the main scene), if it can save (the native rebuild adds save_state).
func game() -> Node:
	var s := get_tree().current_scene
	return s if s and s.has_method("save_state") else null


func can_save() -> bool:
	return game() != null


## A new game from the title screen: the clock starts again.
func new_game() -> void:
	play_time = 0.0
	_auto_t = 0.0


## Keeps the current frame for the next save's thumbnail.
func remember_frame() -> void:
	var tex := get_viewport().get_texture()
	_shot = tex.get_image() if tex else null


func slot_ids() -> Array:
	var ids := ["autosave"]
	for i in SLOTS:
		ids.append("slot%d" % (i + 1))
	return ids


func slot_label(id: String) -> String:
	return "Autosave" if id == "autosave" else "Slot %s" % id.trim_prefix("slot")


func has_save(id: String) -> bool:
	return FileAccess.file_exists(DIR + id + ".save")


## The slot's summary: {id, empty} plus, when there is a save, {name, time, mode, candy, area,
## play_time, houses, freed, costumes, thumb (Texture2D or null)}.
func info(id: String) -> Dictionary:
	var d := {"id": id, "empty": not has_save(id)}
	if d["empty"]:
		return d
	var cfg := ConfigFile.new()
	cfg.load(DIR + id + ".cfg")
	for key: String in ["name", "time", "mode", "candy", "area", "play_time", "houses", "freed", "costumes"]:
		d[key] = cfg.get_value("save", key, null)
	if d["name"] == null:
		d["name"] = slot_label(id)
	var img := Image.load_from_file(ProjectSettings.globalize_path(DIR + id + ".png")) if FileAccess.file_exists(DIR + id + ".png") else null
	d["thumb"] = ImageTexture.create_from_image(img) if img else null
	return d


## The most recently written save, or "".
func latest() -> String:
	var best := ""
	var best_t := -1.0
	for id: String in slot_ids():
		if has_save(id):
			var cfg := ConfigFile.new()
			cfg.load(DIR + id + ".cfg")
			var t := float(cfg.get_value("save", "time", 0.0))
			if t > best_t:
				best_t = t
				best = id
	return best


func save_slot(id: String, name: String) -> bool:
	var g := game()
	if g == null:
		return false
	var state: Dictionary = g.save_state()
	if state.is_empty():
		return false
	DirAccess.make_dir_recursive_absolute(DIR)
	var f := FileAccess.open(DIR + id + ".save", FileAccess.WRITE)
	if f == null:
		return false
	f.store_var(state)
	f.close()
	var img: Image = _shot if _shot and get_tree().paused else null
	if img == null and DisplayServer.get_name() != "headless":
		var tex := get_viewport().get_texture()
		img = tex.get_image() if tex else null
	if img:
		img = img.duplicate()
		img.resize(THUMB.x, THUMB.y, Image.INTERPOLATE_BILINEAR)
		img.save_png(DIR + id + ".png")
	var cfg := ConfigFile.new()
	cfg.set_value("save", "name", name.strip_edges() if name.strip_edges() != "" else slot_label(id))
	cfg.set_value("save", "time", Time.get_unix_time_from_system())
	cfg.set_value("save", "mode", int(state.get("mode", 1)))
	cfg.set_value("save", "candy", int(state.get("wallet", 0)))
	cfg.set_value("save", "area", str(state.get("area_name", "")))
	cfg.set_value("save", "play_time", play_time)
	cfg.set_value("save", "houses", int(state.get("houses", 0)))
	cfg.set_value("save", "freed", int(state.get("areas_freed", 0)))
	cfg.set_value("save", "costumes", int(state.get("costumes_found", 1)))
	cfg.save(DIR + id + ".cfg")
	if id != "autosave":
		_auto_t = 0.0
	return true


func load_slot(id: String) -> bool:
	var g := game()
	if g == null or not has_save(id):
		return false
	var f := FileAccess.open(DIR + id + ".save", FileAccess.READ)
	if f == null:
		return false
	var state: Variant = f.get_var()
	f.close()
	if not state is Dictionary or not g.load_state(state):
		return false
	var cfg := ConfigFile.new()
	cfg.load(DIR + id + ".cfg")
	play_time = float(cfg.get_value("save", "play_time", 0.0))
	_auto_t = 0.0
	return true


func delete_slot(id: String) -> void:
	for ext: String in [".save", ".png", ".cfg"]:
		if FileAccess.file_exists(DIR + id + ext):
			DirAccess.remove_absolute(DIR + id + ext)


## "1h 05m" / "12m".
static func play_time_text(seconds: float) -> String:
	var m := int(seconds / 60.0)
	return "%dh %02dm" % [m / 60, m % 60] if m >= 60 else "%dm" % m


## A save's date and time in local time, e.g. "Oct 26, 9:41 PM".
static func date_text(unix: float) -> String:
	var bias := int(Time.get_time_zone_from_system().get("bias", 0))
	var dt := Time.get_datetime_dict_from_unix_time(int(unix) + bias * 60)
	var months := ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
	var h := int(dt["hour"])
	return "%s %d, %d:%02d %s" % [months[int(dt["month"]) - 1], dt["day"], 12 if h % 12 == 0 else h % 12, dt["minute"], "AM" if h < 12 else "PM"]
