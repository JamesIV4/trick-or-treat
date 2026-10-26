extends Node
## Plays the FM-rendered sound effects (art/tools/make_sfx.py) and the looping neighborhood music
## (art/tools/make_music.py). Any sound can be replaced without touching code: a file named after it
## in res://assets/sfx_custom/ (.ogg, .wav or .mp3; the music is music_neighborhood) wins over the
## generated one, and mix.json there holds per-sound volume trims in dB. The Sound Board editor
## panel (addons/sound_board) manages both.

const SFX := ["candy", "chocolate", "lollipop", "golden", "combo", "treats", "jump", "flap", "bounce",
	"land", "pound", "step_a", "step_b", "doorbell", "cleared", "night_bell", "thunder",
	"fire", "blast", "hit", "hit_kill", "fizzle", "hurt", "spill", "telegraph", "monster_pop",
	"nest_break", "vine_burn", "throw", "puff", "spawn", "freed", "menu", "denied", "purchase",
	"teleport", "car_summon", "car_on", "car_off", "car_bump"]
const CUSTOM_DIR := "res://assets/sfx_custom/"
# Voices: a shared pool. A new sound takes a free voice; with none free it replaces the least
# important, oldest sound still playing, and is dropped only if everything playing matters more.
# A sound that is already playing many times over reuses its own oldest copy instead of crowding
# out everything else (footsteps, hits and candy chimes in a busy moment).
const VOICES := 40
const PRIORITY := {
	"combo": 3, "freed": 3, "cleared": 3, "purchase": 3, "doorbell": 3, "treats": 3, "nest_break": 3,
	"hurt": 3, "teleport": 3, "golden": 3, "menu": 3, "denied": 3, "car_summon": 3, "night_bell": 3,
	"lollipop": 2, "chocolate": 2, "spill": 2, "blast": 2, "hit_kill": 2, "monster_pop": 2, "vine_burn": 2,
	"pound": 2, "car_on": 2, "car_off": 2,
	"step_a": 0, "step_b": 0, "telegraph": 0, "spawn": 0, "throw": 0, "fizzle": 0, "land": 0,
}
const MAX_SAME := {
	"step_a": 2, "step_b": 2, "land": 2, "jump": 2, "flap": 2, "spawn": 2, "telegraph": 3,
	"throw": 3, "puff": 3, "fizzle": 3, "bounce": 3, "thunder": 2, "combo": 3,
}
const DEFAULT_MAX_SAME := 5
const PICKUP_GAP := 0.07

var _streams := {}
var _pool: Array[AudioStreamPlayer] = []
var _voice_sound: Array[String] = [] # what each voice last started
var _voice_prio: Array[int] = []
var _voice_t: Array[float] = [] # when it started (seconds)
var _last_pickup := -1.0
var _last_rank := 0
var _music: AudioStreamPlayer
var _mix := {} # sound name -> volume trim (dB) from the Sound Board


func _ready() -> void:
	for name in SFX:
		var s := sound_stream(name)
		if s:
			_streams[name] = s
	if FileAccess.file_exists(CUSTOM_DIR + "mix.json"):
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(CUSTOM_DIR + "mix.json"))
		if parsed is Dictionary:
			_mix = parsed
	for i in VOICES:
		var p := AudioStreamPlayer.new()
		p.bus = "SFX" # the Options menu's volumes (scripts/settings.gd makes the buses)
		add_child(p)
		_pool.append(p)
		_voice_sound.append("")
		_voice_prio.append(0)
		_voice_t.append(0.0)
	# A limiter on the master bus keeps busy moments (fights, candy bursts) from clipping.
	if AudioServer.get_bus_effect_count(0) == 0:
		var limiter := AudioEffectHardLimiter.new()
		limiter.ceiling_db = -0.5
		AudioServer.add_bus_effect(0, limiter)
	_music = AudioStreamPlayer.new()
	_music.bus = "Music"
	_music.volume_db = -9.0
	add_child(_music)
	var m := sound_stream("music_neighborhood")
	if m is AudioStreamWAV:
		m.loop_mode = AudioStreamWAV.LOOP_FORWARD
		m.loop_begin = 0
		m.loop_end = int(m.get_length() * m.mix_rate)
	elif m is AudioStreamOggVorbis or m is AudioStreamMP3:
		m.loop = true
	_music.stream = m
	_music.volume_db = -9.0 + float(_mix.get("music_neighborhood", 0.0))


## The stream a sound plays: a custom file if one has been assigned, else the generated one.
static func sound_stream(name: String) -> AudioStream:
	for ext: String in ["ogg", "wav", "mp3"]:
		var custom := CUSTOM_DIR + name + "." + ext
		if ResourceLoader.exists(custom):
			return load(custom)
	var path := "res://assets/music/neighborhood.wav" if name == "music_neighborhood" else "res://assets/sfx/%s.wav" % name
	return load(path) if ResourceLoader.exists(path) else null


func _exit_tree() -> void:
	# Let go of every playing stream before teardown (otherwise they are reported as leaked at exit).
	for p in _pool + [_music]:
		p.stop()
		p.stream = null


func _play(name: String, pitch := 1.0, volume_db := 0.0) -> void:
	if not _streams.has(name):
		return
	var p := _voice_for(name)
	if p == null:
		return # every voice is busy with something more important
	p.stream = _streams[name]
	p.pitch_scale = pitch
	p.volume_db = volume_db + float(_mix.get(name, 0.0))
	p.play()


## Picks the voice for a new sound (see the notes at VOICES).
func _voice_for(name: String) -> AudioStreamPlayer:
	var now := Time.get_ticks_msec() / 1000.0
	var prio: int = PRIORITY.get(name, 1)
	var cap: int = MAX_SAME.get(name, DEFAULT_MAX_SAME)
	var same := 0
	var oldest_same := -1
	var free := -1
	var victim := -1
	for i in VOICES:
		if not _pool[i].playing:
			if free < 0:
				free = i
			continue
		if _voice_sound[i] == name:
			same += 1
			if oldest_same < 0 or _voice_t[i] < _voice_t[oldest_same]:
				oldest_same = i
		if victim < 0 or _voice_prio[i] < _voice_prio[victim] or (_voice_prio[i] == _voice_prio[victim] and _voice_t[i] < _voice_t[victim]):
			victim = i
	var pick := -1
	if same >= cap:
		pick = oldest_same
	elif free >= 0:
		pick = free
	elif victim >= 0 and _voice_prio[victim] <= prio:
		pick = victim
	if pick < 0:
		return null
	_voice_sound[pick] = name
	_voice_prio[pick] = prio
	_voice_t[pick] = now
	return _pool[pick]

func start_music() -> void:
	# Trailer clips recorded one shot at a time get one continuous music bed laid under them later.
	if OS.get_cmdline_user_args().has("--cine-no-music"):
		return
	if _music.stream and not _music.playing:
		_music.volume_db = -9.0 + float(_mix.get("music_neighborhood", 0.0))
		_music.play()


func stop_music() -> void:
	var tw := create_tween()
	tw.tween_property(_music, "volume_db", -40.0, 1.5)
	tw.tween_callback(_music.stop)


## Pickups play on their own two voices, at most one every PICKUP_GAP seconds (a bigger treat
## still gets through), so a handful of candy arriving together reads as a quick run of chimes
## instead of a pile-up.
func play_pickup(tier: int) -> void:
	var now := Time.get_ticks_msec() / 1000.0
	var rank := clampi(tier, 0, 3) # 0 everyday, 1 chocolate, 2 lollipop, 3 golden
	if now - _last_pickup < PICKUP_GAP and rank <= _last_rank:
		return
	_last_pickup = now
	_last_rank = rank
	var name: String = ["candy", "chocolate", "lollipop", "golden"][rank]
	_play(name, 1.0, [-11.0, -8.0, -7.0, -5.0][rank])


## A whole candy run collected cleanly.
func play_combo(_size: int) -> void:
	_play("combo", 1.0, -7.0)


func play_treats() -> void:
	_play("treats")


func play_step(parity: int) -> void:
	_play("step_a" if parity == 0 else "step_b", randf_range(0.97, 1.03), -4.0)


func play_doorbell() -> void:
	_play("doorbell", 1.0, -1.0)


func play_cleared() -> void:
	_music.volume_db = -16.0
	create_tween().tween_property(_music, "volume_db", -9.0, 2.0).set_delay(1.0)
	_play("cleared")


func play_bell() -> void:
	stop_music()
	_play("night_bell")


func play_jump() -> void:
	_play("jump", randf_range(0.97, 1.04), -4.0)


func play_flap() -> void:
	_play("flap", randf_range(0.98, 1.03), -4.0)


func play_bounce() -> void:
	_play("bounce", randf_range(0.95, 1.05), -1.0)


func play_pound() -> void:
	_play("pound")


func play_land(_impact: float) -> void:
	_play("land", randf_range(0.95, 1.05), -6.0)


func play_thunder(_strength: float) -> void:
	_play("thunder", randf_range(0.85, 1.1), -6.0)


func play_fire() -> void:
	_play("fire", randf_range(0.95, 1.06), -3.0)


func play_blast() -> void:
	_play("blast", 1.0, 0.0)


func play_hit(destroyed: int) -> void:
	if destroyed:
		_play("hit_kill", randf_range(0.97, 1.03), -1.0)
	else:
		_play("hit", randf_range(0.92, 1.08), -3.0)


func play_fizzle() -> void:
	_play("fizzle", randf_range(0.9, 1.1), -10.0)


func play_hurt() -> void:
	_play("hurt", 1.0, 0.0)


func play_spill() -> void:
	_play("spill", 1.0, -2.0)


func play_telegraph() -> void:
	_play("telegraph", randf_range(0.97, 1.03), -9.0)


func play_monster_pop() -> void:
	_play("monster_pop", randf_range(0.95, 1.05), -2.0)


func play_nest_break() -> void:
	_play("nest_break", 1.0, 0.0)


func play_vine_burn() -> void:
	_play("vine_burn", 1.0, -1.0)


func play_throw() -> void:
	_play("throw", randf_range(0.9, 1.1), -8.0)


func play_puff() -> void:
	_play("puff", 1.0, -5.0)


func play_spawn() -> void:
	_play("spawn", randf_range(0.9, 1.1), -11.0)


func play_freed() -> void:
	_music.volume_db = -18.0
	create_tween().tween_property(_music, "volume_db", -9.0, 2.5).set_delay(1.2)
	_play("freed", 1.0, 0.0)


func play_menu() -> void:
	_play("menu", 1.0, -4.0)


func play_denied() -> void:
	_play("denied", 1.0, -4.0)


func play_purchase() -> void:
	_play("purchase", 1.0, -1.0)


func play_teleport() -> void:
	_play("teleport", 1.0, -1.0)


func play_car_summon() -> void:
	_play("car_summon", 1.0, -2.0)


func play_car_on() -> void:
	_play("car_on", 1.0, -4.0)


func play_car_off() -> void:
	_play("car_off", 1.0, -5.0)


func play_car_bump() -> void:
	_play("car_bump", randf_range(0.95, 1.05), -3.0)