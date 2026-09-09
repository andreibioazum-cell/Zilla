// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Backend independent list of 2D primitives. The Vulkan renderer uploads it to
// the GPU; the offline preview renderer (tools/preview.cpp) rasterises exactly
// the same data on the CPU. Everything is expressed in UI pixels with the
// origin in the top-left corner.
//
// Each primitive is a textured-or-Shaped quad; panels, circles and text are all
// rounded boxes or glyphs resolved by the same shader, so a whole frame can be
// drawn with a handful of draw calls.
#ifndef ZILLA_GFX_DRAW_LIST_H
#define ZILLA_GFX_DRAW_LIST_H

#include "core/types.h"

#include <cstdint>
#include <vector>

namespace zilla {

// Vertex layout shared by the GPU pipeline (see shaders/quad.*.glsl) and by the
// CPU preview rasteriser.
struct DrawVertex {
	float x = 0.0f, y = 0.0f; // position (UI px)
	float u = 0.0f, v = 0.0f; // font atlas coordinates
	float lx = 0.0f, ly = 0.0f; // position relative to the quad centre (SDF space)
	uint32_t fill = 0; // packed RGBA8
	uint32_t border = 0; // packed RGBA8
	float half_w = 0.0f, half_h = 0.0f; // half size in px
	float radius = 0.0f; // corner radius (shapes) / SDF scale (glyphs)
	float border_w = 0.0f; // border width in px
	float tex = 0.0f; // 1.0 => sample the font atlas
};

struct DrawCommand {
	Rect clip; // only valid when has_clip is true
	uint32_t first_index = 0;
	uint32_t index_count = 0;
	bool has_clip = false;
};

class DrawList {
public:
	void clear();

	void set_clip(const Rect &p_rect);
	void clear_clip();
	const Rect &current_clip() const { return clip; }
	bool has_clip() const { return clip_enabled; }

	// Solid (optionally rounded and bordered) rectangle.
	void add_rect(const Rect &p_rect, const Color &p_fill, float p_radius = 0.0f,
			const Color &p_border = Color(), float p_border_width = 0.0f);
	void add_circle(const Vec2 &p_center, float p_radius, const Color &p_fill,
			const Color &p_border = Color(), float p_border_width = 0.0f);
	// A quad sampling the font atlas (one glyph).
	void add_glyph(const Rect &p_rect, const Rect &p_uv, const Color &p_color, float p_sdf_scale);

	const std::vector<DrawVertex> &vertices() const { return vertices_; }
	const std::vector<uint32_t> &indices() const { return indices_; }
	const std::vector<DrawCommand> &commands() const { return commands_; }

	bool is_empty() const { return indices_.empty(); }
	uint32_t primitive_count() const { return uint32_t(indices_.size() / 6); }

private:
	void push_quad(const DrawVertex &p_a, const DrawVertex &p_b, const DrawVertex &p_c, const DrawVertex &p_d);
	void ensure_command();

	std::vector<DrawVertex> vertices_;
	std::vector<uint32_t> indices_;
	std::vector<DrawCommand> commands_;
	Rect clip;
	bool clip_enabled = false;
};

} // namespace zilla

#endif // ZILLA_GFX_DRAW_LIST_H
