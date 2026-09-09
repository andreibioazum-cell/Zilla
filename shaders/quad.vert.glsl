#version 450
// Zilla - one vertex shader for every 2D primitive.
//
// Panels, circles and glyphs are all "a quad plus parameters": the fragment
// shader decides whether to shade a rounded box (SDF) or to sample the font
// atlas. That keeps the whole interface in a single draw call.

layout(location = 0) in vec2 aPos;    // position in UI pixels
layout(location = 1) in vec2 aUV;     // font atlas coordinates
layout(location = 2) in vec2 aLocal;  // position relative to the quad centre
layout(location = 3) in vec4 aFill;   // fill colour
layout(location = 4) in vec4 aBorder; // border colour
layout(location = 5) in vec4 aParams; // half size, corner radius (or SDF scale), border width
layout(location = 6) in float aTex;   // 1.0 => sample the font atlas

// 2 * content_scale / framebuffer_size: turns UI pixels into clip space.
layout(push_constant) uniform PushConstants {
	vec2 scale;
};

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vFill;
layout(location = 2) out vec4 vBorder;
layout(location = 3) out vec4 vParams;
layout(location = 4) out vec2 vLocal;
layout(location = 5) out flat float vTex;

void main() {
	vUV = aUV;
	vFill = aFill;
	vBorder = aBorder;
	vParams = aParams;
	vLocal = aLocal;
	vTex = aTex;

	gl_Position = vec4(aPos * scale - vec2(1.0), 0.0, 1.0);
}
