// Shared textured toon material for every mesh in the neighborhood.
//   UV  : material tiling coordinates in metres (projected per face)
//   UV2 : x = material layer + palette cell u (fract), y = palette cell v
//   COLOR.r : baked ambient occlusion
// The painted material (texture array slice) modulates the palette tint, so one siding texture
// serves every house colour. Glow comes from palette_emit (alpha = flicker). Soft two-band
// diffuse so shadows read as the blue night ambient and lantern light reads orange. Occluders
// between camera and player dither away. Meshes without UV2 fall back to the flat palette path.
shader_type spatial;
render_mode cull_back, specular_schlick_ggx;

uniform sampler2D palette : source_color, filter_nearest;
uniform sampler2D palette_emit : source_color, filter_nearest;
uniform sampler2DArray albedo_array : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2DArray normal_array : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform float layer_scale[32];
uniform float layer_strength[32];
uniform float layer_normal[32];
uniform float emit_strength = 2.6;
uniform float ramp_soft = 0.2;
uniform vec3 shadow_tint : source_color = vec3(0.55, 0.65, 1.0);
uniform float ao_strength = 0.85;
uniform vec3 cast_shadow_tint : source_color = vec3(0.42, 0.58, 1.0);
uniform float cast_shadow_fill = 0.32;

instance uniform float rim_strength = 0.0;
instance uniform vec3 rim_color : source_color = vec3(0.62, 0.76, 1.0);
instance uniform float glow_boost = 1.0;
instance uniform float flash = 0.0;
instance uniform vec3 hit_flash : source_color = vec3(0.0); // monster hit / telegraph glow
// Monster rig hit / telegraph flash (Lane M): brightens the surface in proportion to its OWN colour, plus a floor
// shaded by AO and a soft top light (dark surfaces flash too) and a strong rim, so dark stripes, ink eyes and parts
// in front keep their contrast. A flat added glow (hit_flash) at partial strength is a uniform veil: parts in front
// of others read as seen through frosted glass. rgb = flash colour * strength (linear, no sRGB hint), a = the peak
// at the start of a hit: the whole body near white for 2 or 3 frames (a full-strength silhouette reads solid).
instance uniform vec4 body_flash = vec4(0.0);
instance uniform float wind = 0.0;
instance uniform float keep_visible = 0.0; // 1: never dithered away by the player cutout (monsters)
// A tail's swish (DragonController, the cat): x amplitude at the tip (m), y phase, z wavenumber (rad/m),
// w tail length (m); 0 length = off.
instance uniform vec4 tail_wave = vec4(0.0);
// Soft parts (kid rig and monster rig): one continuous mesh bent about its own origin (the pin).
// All four default to off. (13 of Godot's 16 per-instance uniform slots are used, with body_flash.)
//   soft_pin  xyz = direction in mesh space along which the part runs away from the pin (zero vector
//             = use the distance from the mesh origin), w = length over which the bend ramps to full
//             (0 = off).
//   soft_bend xyz = rotation vector (axis * radians, mesh space) reached at the tip, w = distance from
//             the pin that stays rigid. Vertices and normals rotate about the pin by bend * k, k
//             easing 0..1 along the part, so the part keeps its length.
instance uniform vec4 soft_pin = vec4(0.0);
instance uniform vec4 soft_bend = vec4(0.0);
// Cloth ripple: a wave running round the mesh's local Y axis, pushed out along the normal.
//   x amplitude (m, 0 = off), y phase (radians), z lobes round the axis, w = local height below which
//   it applies, reaching full strength 0.2 below that (0 = the whole mesh, growing with the distance
//   from the origin and full at soft_pin.w, or at 0.25 if that is unset). A negative z pushes the cloth
//   straight out from the Y axis instead of along the normal (|z| lobes).
instance uniform vec4 cloth_wave = vec4(0.0);
// Only gloss <= -2 does anything now: no highlight and no sky sheen (eyes, mouth cavities, sockets), so dark
// features stay dark. Every other value gets the original look: the soft painterly highlight plus the sky
// sheen. (User, 2026-11-17: "The sheen or specular response of the models was better before. It was less
// realistic but that fit the art style better." The tight catchlight for gloss > 0 and the dimmer highlight
// for gloss between -2 and 0 are gone; the rigs still pass their old values.)
instance uniform float gloss = 0.0;

global uniform vec3 cutout_from;
global uniform vec3 cutout_to;
global uniform float cutout_radius;

varying vec3 world_pos;
varying float seed;

float bayer4(vec2 p) {
	ivec2 i = ivec2(mod(p, 4.0));
	int idx = i.x + i.y * 4;
	const float m[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	return (m[idx] + 0.5) / 16.0;
}

void vertex() {
	vec3 origin = MODEL_MATRIX[3].xyz;
	seed = fract(sin(dot(origin, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
	if (wind > 0.0) {
		float h = max(VERTEX.y, 0.0);
		float sway = sin(TIME * 1.3 + seed * 6.28 + origin.x * 0.15) * 0.5 + sin(TIME * 2.1 + seed * 3.1) * 0.25;
		VERTEX.x += sway * wind * h * h * 0.012;
		VERTEX.z += cos(TIME * 1.1 + seed * 4.0) * wind * h * h * 0.008;
	}
	// Tail swish: a sideways wave travelling from the root (the mesh origin) to the tip, growing along
	// the tail, so the whole tail undulates in one smooth curve.
	if (tail_wave.w > 0.0) {
		float s = length(VERTEX);
		float k = clamp(s / tail_wave.w, 0.0, 1.0);
		VERTEX.x += tail_wave.x * k * k * sin(tail_wave.y - tail_wave.z * s);
	}
	// Cloth ripple (before the bend, so a bent hem still ripples along its own surface).
	if (cloth_wave.x != 0.0) {
		float m;
		if (cloth_wave.w != 0.0) {
			m = clamp((cloth_wave.w - VERTEX.y) * 5.0, 0.0, 1.0);
		} else {
			m = clamp(length(VERTEX) / (soft_pin.w > 0.0 ? soft_pin.w : 0.25), 0.0, 1.0);
		}
		float ang = atan(VERTEX.x, VERTEX.z);
		// z < 0 (Lane P sheets with an inside wall): |z| lobes, pushed straight out from the Y axis
		// instead of along the normal, so both faces of a thick cloth move together.
		vec3 dir = NORMAL;
		float lobes = abs(cloth_wave.z);
		if (cloth_wave.z < 0.0) {
			float rh = length(VERTEX.xz);
			dir = rh > 0.0001 ? vec3(VERTEX.x / rh, 0.0, VERTEX.z / rh) : vec3(0.0);
			// tilt the normal with the slope of the wave round the axis, so the moving folds shade
			float slope = cloth_wave.x * m * lobes * cos(cloth_wave.y + lobes * ang) / max(rh, 0.05);
			NORMAL = normalize(NORMAL - vec3(cos(ang), 0.0, -sin(ang)) * slope * sign(dot(NORMAL, dir)));
		}
		VERTEX += dir * (cloth_wave.x * m * sin(cloth_wave.y + lobes * ang));
	}
	// Soft bend: rotate about the pin (the mesh origin) by a fraction of soft_bend that grows along
	// the part. The depth and shadow passes run this too, so the shadow bends with the mesh.
	if (soft_pin.w > 0.0) {
		float ang = length(soft_bend.xyz);
		if (ang > 0.0001) {
			float along = dot(soft_pin.xyz, soft_pin.xyz) > 0.000001 ? dot(VERTEX, normalize(soft_pin.xyz)) : length(VERTEX);
			float k = clamp((along - soft_bend.w) / soft_pin.w, 0.0, 1.0);
			k = k * k * (3.0 - 2.0 * k);
			vec3 ax = soft_bend.xyz / ang;
			float sn = sin(ang * k);
			float cs = cos(ang * k);
			VERTEX = VERTEX * cs + cross(ax, VERTEX) * sn + ax * (dot(ax, VERTEX) * (1.0 - cs));
			NORMAL = NORMAL * cs + cross(ax, NORMAL) * sn + ax * (dot(ax, NORMAL) * (1.0 - cs));
		}
	}
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	// Only things standing up above the player's feet can occlude; the ground never cuts out.
	if (cutout_radius > 0.0 && keep_visible < 0.5 && world_pos.y > cutout_to.y - 0.25) {
		vec3 seg = cutout_to - cutout_from;
		float len = length(seg);
		vec3 dir = seg / max(len, 0.001);
		float t = dot(world_pos - cutout_from, dir);
		float fade = 0.0;
		if (t > 0.0 && t < len - 1.1) {
			float radius = cutout_radius + (len - t) * 0.22;
			float r = length(world_pos - (cutout_from + dir * t));
			fade = smoothstep(radius, radius * 0.55, r);
		}
		fade = max(fade, smoothstep(3.6, 2.2, distance(world_pos, cutout_from)));
		if (fade > bayer4(FRAGCOORD.xy)) {
			discard;
		}
	}
	float layer = 0.0;
	vec2 cell = UV;
	if (UV2.x != 0.0 || UV2.y != 0.0) {
		layer = floor(UV2.x);
		cell = vec2(fract(UV2.x), UV2.y);
	}
	int li = clamp(int(layer), 0, 31);
	vec3 tint = texture(palette, cell).rgb;
	vec2 tuv = UV / max(layer_scale[li], 0.01);
	vec3 det = texture(albedo_array, vec3(tuv, layer)).rgb;
	ALBEDO = tint * mix(vec3(1.0), det * 2.0, layer_strength[li]);
	if (layer > 0.5) {
		NORMAL_MAP = texture(normal_array, vec3(tuv, layer)).rgb;
		NORMAL_MAP_DEPTH = layer_normal[li] * 1.4;
	}
	AO = mix(1.0, COLOR.r, ao_strength);
	AO_LIGHT_AFFECT = 0.35;
	vec4 e = texture(palette_emit, cell);
	float flick = 1.0 - e.a * (0.3 + 0.3 * sin(TIME * 9.0 + seed * 40.0) * sin(TIME * 5.3 + seed * 13.0));
	EMISSION = e.rgb * emit_strength * flick * glow_boost;
	EMISSION += flash * vec3(0.55, 0.48, 0.95);
	EMISSION += hit_flash;
	float rim = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 3.0);
	EMISSION += rim_color * rim * rim_strength;
	// own colour plus a share of its own brightness (whitens toward the flash without lifting dark parts); AO keeps
	// creases and joints darker, so the form reads through the flash
	float flash_top = 0.55 + 0.45 * clamp((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).y, -1.0, 1.0); // a soft top light: form kept
	float flash_edge = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 2.0);
	// (mostly the surface's brightness, not its hue: the flash reads in its own colour, warm for the telegraph)
	EMISSION += body_flash.rgb * ((ALBEDO * 1.2 + vec3(dot(ALBEDO, vec3(0.3, 0.59, 0.11)) * 2.6 + 0.35)) * AO * flash_top + flash_edge * 2.0);
	ALBEDO = mix(ALBEDO, vec3(1.0), body_flash.a);
	EMISSION += body_flash.a * vec3(2.4, 2.3, 2.1);
	ROUGHNESS = 0.8;
	// gloss <= -2 (eyes, cavities): no sky / environment sheen, so a dark iris or mouth stays dark.
	SPECULAR = gloss <= -1.99 ? 0.0 : 0.3;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float ramp = smoothstep(-ramp_soft, ramp_soft, ndl);
	float lit = ramp * ATTENUATION;
	vec3 shade = LIGHT_COLOR * shadow_tint * 0.12 * (1.0 - ramp) * clamp(ATTENUATION + 0.4, 0.0, 1.0);
	// Cast shadows from the moon stay a soft blue rather than going dark: a cool fill where the surface
	// faces the light but is shadowed.
	if (LIGHT_IS_DIRECTIONAL) {
		shade += LIGHT_COLOR * cast_shadow_tint * cast_shadow_fill * ramp * (1.0 - ATTENUATION);
	}
	DIFFUSE_LIGHT += (LIGHT_COLOR * lit + shade) / PI;
	// Soft painterly highlight.
	vec3 h = normalize(LIGHT + VIEW);
	float spec = pow(max(dot(NORMAL, h), 0.0), 24.0) * 0.18 * ATTENUATION;
	SPECULAR_LIGHT += LIGHT_COLOR * spec * (gloss <= -1.99 ? 0.0 : 1.0); // gloss <= -2: eyes and cavities stay dark
}
