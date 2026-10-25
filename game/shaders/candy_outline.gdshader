// Glowing outline around every candy (second pass of candy.gdshader): an inflated back-face shell
// that follows the same spin and bob, tinted per candy kind (INSTANCE_CUSTOM.g), pulsing softly.
shader_type spatial;
render_mode cull_front, unshaded, shadows_disabled, fog_disabled;

uniform float thickness = 0.03;
uniform float glow = 2.6;

varying vec3 tint;
varying float pulse;

vec3 kind_color(int k) {
	if (k == 0) return vec3(1.0, 0.42, 0.72);
	if (k == 1) return vec3(0.35, 1.0, 0.78);
	if (k == 2) return vec3(0.72, 0.5, 1.0);
	if (k == 3) return vec3(1.0, 0.9, 0.3);
	if (k == 4) return vec3(1.0, 0.58, 0.3);
	if (k == 5) return vec3(1.0, 0.38, 0.48);
	return vec3(1.0, 0.82, 0.25);
}

void vertex() {
	float phase = INSTANCE_CUSTOM.r;
	tint = kind_color(int(INSTANCE_CUSTOM.g + 0.5));
	pulse = 0.8 + 0.2 * sin(TIME * 3.2 + phase * 2.0);
	float a = TIME * 1.7 + phase;
	mat2 r = mat2(vec2(cos(a), sin(a)), vec2(-sin(a), cos(a)));
	VERTEX.xz = r * VERTEX.xz;
	NORMAL.xz = r * NORMAL.xz;
	float s = max(length(MODEL_MATRIX[0].xyz), 0.01);
	vec3 dir = normalize(mix(normalize(VERTEX + vec3(0.0001)), NORMAL, 0.35));
	VERTEX += dir * thickness / s;
	VERTEX.y += sin(TIME * 2.3 + phase) * 0.09;
}

void fragment() {
	ALBEDO = tint;
	EMISSION = tint * glow * pulse;
}
