extends Node
## Autoload: sets up the input map in code so keyboard and gamepad bindings live in one place.

const KEYS := {
	"move_left": [KEY_A],
	"move_right": [KEY_D],
	"move_forward": [KEY_W],
	"move_back": [KEY_S],
	"jump": [KEY_SPACE],
	"spin": [KEY_E, KEY_J],
	"roll": [KEY_SHIFT, KEY_K],
	"drop": [KEY_CTRL], # let go of a ledge (B on the gamepad, shared with roll)
	"attack": [KEY_F, KEY_R],
	"summon_car": [KEY_V],
	"next_costume": [KEY_T],
	"shop": [KEY_TAB],
	"cam_left": [KEY_Q, KEY_LEFT],
	"cam_right": [KEY_C, KEY_RIGHT],
	"new_night": [KEY_N],
	"toggle_hud": [KEY_H],
	"toggle_debug": [KEY_F3],
	"toggle_lightning": [KEY_L],
	# Shop tabs (the bumpers on the gamepad).
	"pause": [KEY_ESCAPE],
	"shop_tab_prev": [KEY_Q, KEY_PAGEUP],
	"shop_tab_next": [KEY_E, KEY_PAGEDOWN],
}

const BUTTONS := {
	"jump": [JOY_BUTTON_A],
	"roll": [JOY_BUTTON_B],
	"drop": [JOY_BUTTON_B],
	"spin": [JOY_BUTTON_X],
	"attack": [JOY_BUTTON_Y], # and RT (TRIGGERS)
	"summon_car": [JOY_BUTTON_LEFT_SHOULDER],
	"next_costume": [JOY_BUTTON_RIGHT_SHOULDER],
	"shop": [JOY_BUTTON_START],
	"pause": [JOY_BUTTON_BACK],
	"shop_tab_prev": [JOY_BUTTON_LEFT_SHOULDER],
	"shop_tab_next": [JOY_BUTTON_RIGHT_SHOULDER],
}

# Menu navigation on the gamepad (Godot's default ui_* actions are keyboard only).
const UI_BUTTONS := {
	"ui_accept": [JOY_BUTTON_A],
	"ui_cancel": [JOY_BUTTON_B],
	"ui_up": [JOY_BUTTON_DPAD_UP],
	"ui_down": [JOY_BUTTON_DPAD_DOWN],
	"ui_left": [JOY_BUTTON_DPAD_LEFT],
	"ui_right": [JOY_BUTTON_DPAD_RIGHT],
}
const UI_AXES := {
	"ui_up": [JOY_AXIS_LEFT_Y, -1.0],
	"ui_down": [JOY_AXIS_LEFT_Y, 1.0],
	"ui_left": [JOY_AXIS_LEFT_X, -1.0],
	"ui_right": [JOY_AXIS_LEFT_X, 1.0],
}

const MOUSE := {
	"attack": [MOUSE_BUTTON_LEFT],
}

# Triggers pressed like buttons.
const TRIGGERS := {
	"attack": [JOY_AXIS_TRIGGER_RIGHT],
}

const AXES := {
	"move_left": [JOY_AXIS_LEFT_X, -1.0],
	"move_right": [JOY_AXIS_LEFT_X, 1.0],
	"move_forward": [JOY_AXIS_LEFT_Y, -1.0],
	"move_back": [JOY_AXIS_LEFT_Y, 1.0],
	# Right stick orbit is inverted horizontally (push right to swing the view left).
	"cam_left": [JOY_AXIS_RIGHT_X, 1.0],
	"cam_right": [JOY_AXIS_RIGHT_X, -1.0],
}


func _enter_tree() -> void:
	for action in KEYS.keys() + BUTTONS.keys() + AXES.keys() + MOUSE.keys() + TRIGGERS.keys():
		if not InputMap.has_action(action):
			InputMap.add_action(action, 0.2)
	for action in KEYS:
		for key in KEYS[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = key
			InputMap.action_add_event(action, ev)
	for action in BUTTONS:
		for button in BUTTONS[action]:
			var ev := InputEventJoypadButton.new()
			ev.button_index = button
			InputMap.action_add_event(action, ev)
	for action in MOUSE:
		for button in MOUSE[action]:
			var ev := InputEventMouseButton.new()
			ev.button_index = button
			InputMap.action_add_event(action, ev)
	for action in TRIGGERS:
		for axis in TRIGGERS[action]:
			var ev := InputEventJoypadMotion.new()
			ev.axis = axis
			ev.axis_value = 1.0
			InputMap.action_add_event(action, ev)
	for action in AXES:
		var ev := InputEventJoypadMotion.new()
		ev.axis = AXES[action][0]
		ev.axis_value = AXES[action][1]
		InputMap.action_add_event(action, ev)
	for action in UI_BUTTONS:
		for button in UI_BUTTONS[action]:
			var ev := InputEventJoypadButton.new()
			ev.button_index = button
			if not InputMap.action_has_event(action, ev):
				InputMap.action_add_event(action, ev)
	for action in UI_AXES:
		var ev := InputEventJoypadMotion.new()
		ev.axis = UI_AXES[action][0]
		ev.axis_value = UI_AXES[action][1]
		if not InputMap.action_has_event(action, ev):
			InputMap.action_add_event(action, ev)