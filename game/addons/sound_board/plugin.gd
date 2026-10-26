@tool
extends EditorPlugin
## Adds the Sound Board as a tab in the editor's bottom panel.

const SoundBoard := preload("res://addons/sound_board/sound_board.gd")

var _board: Control


func _enter_tree() -> void:
	_board = SoundBoard.new()
	_board.name = "SoundBoard"
	_board.custom_minimum_size = Vector2(0, 320)
	add_control_to_bottom_panel(_board, "Sound Board")


func _exit_tree() -> void:
	if _board:
		remove_control_from_bottom_panel(_board)
		_board.queue_free()
		_board = null
