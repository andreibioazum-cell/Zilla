// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "gfx/draw_list.h"

namespace zilla {

void DrawList::clear() {
	vertices_.clear();
	indices_.clear();
	commands_.clear();
	clip = Rect();
	clip_enabled = false;
}

void DrawList::set_clip(const Rect &p_rect) {
	clip = p_rect;
	clip_enabled = true;
}

void DrawList::clear_clip() {
	clip = Rect();
	clip_enabled = false;
}

void DrawList::ensure_command() {
	const bool needs_new = commands_.empty() || commands_.back().has_clip != clip_enabled ||
			(commands_.back().has_clip && commands_.back().clip != clip) ||
			commands_.back().index_count > 0 &&
					commands_.back().first_index + commands_.back().index_count != uint32_t(indices_.size());

	if (needs_new) {
		DrawCommand cmd;
		cmd.clip = clip;
		cmd.has_clip = clip_enabled;
		cmd.first_index = uint32_t(indices_.size());
		cmd.index_count = 0;
		commands_.push_back(cmd);
	}
}

void DrawList::push_quad(const DrawVertex &p_a, const DrawVertex &p_b, const DrawVertex &p_c, const DrawVertex &p_d) {
	ensure_command();

	const uint32_t base = uint32_t(vertices_.size());
	vertices_.push_back(p_a);
	vertices_.push_back(p_b);
	vertices_.push_back(p_c);
	vertices_.push_back(p_d);

	indices_.push_back(base + 0);
	indices_.push_back(base + 1);
	indices_.push_back(base + 2);
	indices_.push_back(base + 0);
	indices_.push_back(base + 2);
	indices_.push_back(base + 3);

	commands_.back().index_count += 6;
}

void DrawList::add_rect(const Rect &p_rect, const Color &p_fill, float p_radius,
		const Color &p_border, float p_border_width) {
	if (p_rect.is_empty() || (p_fill.a <= 0.0f && p_border.a <= 0.0f)) {
		return;
	}

	const float half_w = p_rect.w * 0.5f;
	const float half_h = p_rect.h * 0.5f;
	const float radius = std::min(p_radius, std::min(half_w, half_h));

	DrawVertex base;
	base.fill = p_fill.pack();
	base.border = p_border.pack();
	base.half_w = half_w;
	base.half_h = half_h;
	base.radius = radius;
	base.border_w = p_border_width;
	base.tex = 0.0f;

	const Vec2 center = p_rect.center();

	auto corner = [&](float p_sx, float p_sy) {
		DrawVertex vtx = base;
		vtx.x = center.x + p_sx * half_w;
		vtx.y = center.y + p_sy * half_h;
		vtx.lx = p_sx * half_w;
		vtx.ly = p_sy * half_h;
		return vtx;
	};

	push_quad(corner(-1.0f, -1.0f), corner(1.0f, -1.0f), corner(1.0f, 1.0f), corner(-1.0f, 1.0f));
}

void DrawList::add_circle(const Vec2 &p_center, float p_radius, const Color &p_fill,
		const Color &p_border, float p_border_width) {
	add_rect(Rect::from_center(p_center, Vec2(p_radius * 2.0f, p_radius * 2.0f)),
			p_fill, p_radius, p_border, p_border_width);
}

void DrawList::add_glyph(const Rect &p_rect, const Rect &p_uv, const Color &p_color, float p_sdf_scale) {
	if (p_rect.is_empty() || p_color.a <= 0.0f) {
		return;
	}

	const float half_w = p_rect.w * 0.5f;
	const float half_h = p_rect.h * 0.5f;
	const Vec2 center = p_rect.center();

	DrawVertex base;
	base.fill = p_color.pack();
	base.border = p_color.pack();
	base.half_w = half_w;
	base.half_h = half_h;
	base.radius = p_sdf_scale; // reused as the SDF scale on the shader side
	base.border_w = 0.0f;
	base.tex = 1.0f;

	auto corner = [&](float p_sx, float p_sy) {
		DrawVertex vtx = base;
		vtx.x = center.x + p_sx * half_w;
		vtx.y = center.y + p_sy * half_h;
		vtx.lx = p_sx * half_w;
		vtx.ly = p_sy * half_h;
		vtx.u = p_sx < 0.0f ? p_uv.x : p_uv.right();
		vtx.v = p_sy < 0.0f ? p_uv.y : p_uv.bottom();
		return vtx;
	};

	push_quad(corner(-1.0f, -1.0f), corner(1.0f, -1.0f), corner(1.0f, 1.0f), corner(-1.0f, 1.0f));
}

} // namespace zilla
