#version 450
// Zilla - rounded boxes (SDF) and signed distance field text.

layout(binding = 0) uniform sampler2D uAtlas;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vFill;
layout(location = 2) in vec4 vBorder;
layout(location = 3) in vec4 vParams;
layout(location = 4) in vec2 vLocal;
layout(location = 5) in flat float vTex;

layout(location = 0) out vec4 oColor;

// Keep in sync with Font::SDF_ONEDGE / Font::SDF_DIST_SCALE in src/gfx/font.h.
const float SDF_ONEDGE = 128.0;
const float SDF_DIST_SCALE = 25.6; // SDF_ONEDGE / SDF_PADDING (5)

float rounded_box_sdf(vec2 p, vec2 half_size, float radius) {
	vec2 q = abs(p) - half_size + radius;
	return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main() {
	if (vTex > 0.5) {
		// Glyph: convert the atlas value back to a distance in screen pixels.
		float distance = (texture(uAtlas, vUV).r * 255.0 - SDF_ONEDGE) / SDF_DIST_SCALE;
		float pixels = distance * vParams.z;
		float coverage = clamp(pixels + 0.5, 0.0, 1.0);
		oColor = vec4(vFill.rgb, vFill.a * coverage);
		return;
	}

	// Shape: rounded box, optionally with a border of `vParams.w` pixels.
	float d = rounded_box_sdf(vLocal, vParams.xy, vParams.z);
	float coverage = 1.0 - smoothstep(-0.75, 0.75, d);
	float border_width = vParams.w;
			// `inner` is 0 in the border ring (d > -border_width) and 1 in the fill.
		float inner = border_width > 0.0
				? 1.0 - smoothstep(-border_width - 0.75, -border_width + 0.75, d)
				: 1.0;

	oColor = vec4(mix(vBorder.rgb, vFill.rgb, inner),
			mix(vBorder.a, vFill.a, inner) * coverage);
}
