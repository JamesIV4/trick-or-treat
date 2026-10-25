extends Node
## Plays the FM-rendered sound effects (art/tools/make_sfx.py) and the looping neighborhood music
## (art/tools/make_music.py).

const SFX := ["candy", "chocolate", "lollipop", "golden", "combo", "treats", "jump", "flap", "bounce",
	"land", "pound", "step_a", "step_b", "doorbell", "cleared", "night_bell", "thunder",
	"fire", "blast", "hit", "hit_kill", "fizzle", "hurt", "spill", "telegraph", "monster_pop",
	"nest_break", "vine_burn", "throw", "puff", "spawn", "freed", "menu", "denied", "purchase",
	"teleport", "car_summon", "car_on", "car_off", "car_bump"]
const VOICES := 16
const PICKUP_GAP := 0.07

var _streams := {}
var _pool: Array[AudioStreamPlayer] = []
var _next := 0
var _pickup_voices: Array[AudioStreamPlayer] = []
var _next_pickup := 0
var _last_pickup := -1.0
var _last_rank := 0
var _music: AudioStreamPlayer


func _ready() -> void:
	for name in SFX:
		var path := "res://assets/sfx/%s.wav" % name
		if ResourceLoader.exists(path):
			_streams[name] = load(path)
	for i in VOICES:
		var p := AudioStreamPlayer.new()
		add_child(p)
		_pool.append(p)
	for i in 2:
		var p := AudioStreamPlayer.new()
		add_child(p)
		_pickup_voices.append(p)
	# A limiter on the master bus keeps busy moments (fights, candy bursts) from clipping.
	if AudioServer.get_bus_effect_count(0) == 0:
		var limiter := AudioEffectHardLimiter.new()
		limiter.ceiling_db = -0.5
		AudioServer.add_bus_effect(0, limiter)
	_music = AudioStreamPlayer.new()
	_music.volume_db = -9.0
	add_child(_music)
	var music_path := "res://assets/music/neighborhood.wav"
	if ResourceLoader.exists(music_path):
		var m: AudioStreamWAV = load(music_path)
		m.loop_mode = AudioStreamWAV.LOOP_FORWARD
		m.loop_begin = 0
		m.loop_end = int(m.get_length() * m.mix_rate)
		_music.stream = m


func _exit_tree() -> void:
	# Let go of every playing stream before teardown (otherwise they are reported as leaked at exit).
	for p in _pool + _pickup_voices + [_music]:
		p.stop()
		p.stream = null


func _play(name: String, pitch := 1.0, volume_db := 0.0) -> void:
	if not _streams.has(name):
		return
	var p := _pool[_next]
	_next = (_next + 1) % VOICES
	p.stream = _streams[name]
	p.pitch_scale = pitch
	p.volume_db = volume_db
	p.play()


func start_music() -> void:
	if _music.stream and not _music.playing:
		_music.volume_db = -9.0
		_music.play()


func stop_music() -> void:
	var tw := create_tween()
	tw.tween_property(_music, "volume_db", -40.0, 1.5)
	tw.tween_callback(_music.stop)


## Pickups play on their own two voices, at most one every PICKUP_GAP seconds (a bigger treat
## still gets through), so a handful of candy arriving together reads as a quick run of chimes
## instead of a pile-up.
func play_pickup(kind: int) -> void:
	var now := Time.get_ticks_msec() / 1000.0
	var rank := 0 if kind < 4 else kind - 3
	if now - _last_pickup < PICKUP_GAP and rank <= _last_rank:
		return
	_last_pickup = now
	_last_rank = rank
	var p := _pickup_voices[_next_pickup]
	_next_pickup = (_next_pickup + 1) % _pickup_voices.size()
	var name: String = ["candy", "chocolate", "lollipop", "golden"][rank]
	if not _streams.has(name):
		return
	p.stream = _streams[name]
	p.pitch_scale = 1.0
	p.volume_db = [-11.0, -8.0, -7.0, -5.0][rank]
	p.play()


## A whole candy run collected cleanly.
func play_combo(_size: int) -> void:
	_play("combo", 1.0, 1.0)


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