extends Node
## Post-processing ported from the OpenXcom-Extended-3D battlescape's shipped render preset
## ("Latest 2", oxp/debug/shipped_render_defaults.gd): the depth-only small-radius AO compositor pass
## (contact_fx.gd) and the sharpen + vignette post shader (no film grain). The environment values
## (Filmic tonemap, glow levels, SSAO, SSIL) live in main.tscn; quality settings in project.godot.

const ContactFx := preload("res://scripts/contact_fx.gd")

const SHARPEN := 0.27
const VIGNETTE := 0.5
const VIGNETTE_RADIUS := 1.0
const VIGNETTE_SOFT := 0.45
const DEPTH_AO_RADIUS := 0.46
const DEPTH_AO_INTENSITY := 0.77

var contact_fx: CompositorEffect
var _sun: DirectionalLight3D
var _rect: ColorRect


func _ready() -> void:
	var env := get_parent().get_node_or_null("WorldEnvironment") as WorldEnvironment
	_sun = get_parent().get_node_or_null("Moon") as DirectionalLight3D
	if env and RenderingServer.get_current_rendering_method() == "forward_plus":
		contact_fx = ContactFx.new()
		contact_fx.set("ao_on", true)
		contact_fx.set("ao_radius", DEPTH_AO_RADIUS)
		contact_fx.set("ao_intensity", DEPTH_AO_INTENSITY)
		contact_fx.set("cs_on", false)
		contact_fx.set("half_res", false)
		var comp := Compositor.new()
		comp.compositor_effects = [contact_fx]
		env.compositor = comp
	# Above the 3D view, under the HUD (layer 1).
	var layer := CanvasLayer.new()
	layer.layer = 0
	add_child(layer)
	var rect := ColorRect.new()
	_rect = rect
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var mat := ShaderMaterial.new()
	mat.shader = preload("res://shaders/post_fx.gdshader")
	mat.set_shader_parameter("sharpen", SHARPEN)
	mat.set_shader_parameter("vignette_strength", VIGNETTE)
	mat.set_shader_parameter("vignette_radius", VIGNETTE_RADIUS)
	mat.set_shader_parameter("vignette_softness", VIGNETTE_SOFT)
	mat.set_shader_parameter("vignette_color", Color(0, 0, 0))
	rect.material = mat
	layer.add_child(rect)
	# Quality settings from the preset that are server-side (SSAO Ultra at full resolution).
	RenderingServer.environment_set_ssao_quality(RenderingServer.ENV_SSAO_QUALITY_ULTRA, false,
		float(ProjectSettings.get_setting("rendering/environment/ssao/adaptive_target", 0.5)),
		int(ProjectSettings.get_setting("rendering/environment/ssao/blur_passes", 2)),
		float(ProjectSettings.get_setting("rendering/environment/ssao/fadeout_from", 50.0)),
		float(ProjectSettings.get_setting("rendering/environment/ssao/fadeout_to", 300.0)))


## Graphics options (scripts/settings.gd): the depth AO pass, sharpen and vignette on or off.
func set_effects(contact_ao: bool, sharpen: bool, vignette: bool) -> void:
	if contact_fx:
		contact_fx.enabled = contact_ao
	if _rect:
		var mat := _rect.material as ShaderMaterial
		mat.set_shader_parameter("sharpen", SHARPEN if sharpen else 0.0)
		mat.set_shader_parameter("vignette_strength", VIGNETTE if vignette else 0.0)
		_rect.visible = sharpen or vignette


func _process(_delta: float) -> void:
	if contact_fx and _sun:
		contact_fx.set("sun_dir", _sun.global_transform.basis.z)
