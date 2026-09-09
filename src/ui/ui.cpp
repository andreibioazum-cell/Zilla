// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "ui/ui.h"

#include <algorithm>
#include <cmath>

namespace zilla {

namespace {
constexpr uint32_t FNV_OFFSET = 2166136261u;
constexpr uint32_t FNV_PRIME = 16777619u;
} // namespace

Ui::Ui(DrawList &p_draw_list, const Font &p_font, const Font &p_font_bold) :
		draw_list_(p_draw_list), font_(p_font), font_bold_(p_font_bold) {}

uint32_t Ui::hash_id(const char *p_id) {
	uint32_t hash = FNV_OFFSET;
	for (const char *c = p_id; *c != '\0'; ++c) {
		hash ^= uint32_t(static_cast<unsigned char>(*c));
		hash *= FNV_PRIME;
	}
	return hash;
}

void Ui::begin(float p_width, float p_height, const InputState &p_input, const Theme &p_theme) {
	draw_list_.clear();
	width_ = p_width;
	height_ = p_height;
	input_ = p_input;
	theme_ = p_theme;
	hot_ = 0;
}

void Ui::end() {
	draw_list_.clear_clip();
	if (input_.pointer_released) {
		active_ = 0;
	}
}

// --- primitives -------------------------------------------------------------

void Ui::fill_rect(const Rect &p_rect, const Color &p_color, float p_radius) {
	draw_list_.add_rect(p_rect, p_color, p_radius < 0.0f ? theme_.radius : p_radius);
}

void Ui::bordered_rect(const Rect &p_rect, const Color &p_fill, const Color &p_border,
		float p_radius, float p_border_width) {
	draw_list_.add_rect(p_rect, p_fill, p_radius < 0.0f ? theme_.radius : p_radius, p_border, p_border_width);
}

void Ui::circle(const Vec2 &p_center, float p_radius, const Color &p_fill,
		const Color &p_border, float p_border_width) {
	draw_list_.add_circle(p_center, p_radius, p_fill, p_border, p_border_width);
}

void Ui::text(Vec2 p_pos, const std::string &p_text, float p_size, const Color &p_color,
		Font::Align p_align, bool p_bold) {
	(p_bold ? font_bold_ : font_).draw(draw_list_, p_text, p_pos, p_size, p_color, p_align);
}

void Ui::text_in_rect(const Rect &p_rect, const std::string &p_text, float p_size,
		const Color &p_color, Font::Align p_align, bool p_bold) {
	(p_bold ? font_bold_ : font_).draw_in_rect(draw_list_, p_text, p_rect, p_size, p_color, p_align);
}

void Ui::window_background() {
	fill_rect(Rect(0.0f, 0.0f, width_, height_), theme_.window_bg, 0.0f);
}

void Ui::title_bar(const std::string &p_title) {
	const Rect bar(0.0f, 0.0f, width_, theme_.title_bar_height);
	fill_rect(bar, theme_.title_bg, 0.0f);
	fill_rect(Rect(0.0f, bar.bottom() - 1.0f, width_, 1.0f), theme_.panel_border, 0.0f);
	text_in_rect(Rect(14.0f, 0.0f, width_ - 28.0f, bar.h), p_title, theme_.font_size, theme_.text);
}

void Ui::status_bar(const std::string &p_left, const std::string &p_right) {
	const Rect bar(0.0f, height_ - theme_.status_bar_height, width_, theme_.status_bar_height);
	fill_rect(bar, theme_.title_bg, 0.0f);
	fill_rect(Rect(0.0f, bar.y, width_, 1.0f), theme_.panel_border, 0.0f);
	text_in_rect(Rect(12.0f, bar.y, width_ * 0.7f - 12.0f, bar.h), p_left,
			theme_.font_size_small, theme_.text_dim);
	text_in_rect(Rect(width_ * 0.3f, bar.y, width_ * 0.7f - 12.0f, bar.h), p_right,
			theme_.font_size_small, theme_.text_dim, Font::Align::Right);
}

void Ui::panel(const Rect &p_rect, bool p_sunken) {
	bordered_rect(p_rect, p_sunken ? theme_.sunken_bg : theme_.panel_bg, theme_.panel_border);
}

void Ui::label(const Rect &p_rect, const std::string &p_text, float p_size, const Color &p_color,
		Font::Align p_align, bool p_bold) {
	text_in_rect(p_rect, p_text, p_size, p_color, p_align, p_bold);
}

// --- widgets ----------------------------------------------------------------

bool Ui::button(const char *p_id, const Rect &p_rect, const std::string &p_label,
		bool p_enabled, bool p_primary) {
	const uint32_t id = hash_id(p_id);
	const bool can_interact = p_enabled && (active_ == 0 || active_ == id);
	const bool hovered = can_interact && p_rect.contains(input_.pointer);
	if (hovered) {
		hot_ = id;
	}
	if (can_interact && input_.pointer_pressed && hovered) {
		active_ = id;
	}
	const bool held = active_ == id && input_.pointer_down;
	const bool clicked = active_ == id && input_.pointer_released && p_rect.contains(input_.pointer);

	Color background;
	Color border;
	Color foreground;
	if (!p_enabled) {
		background = theme_.button_disabled;
		border = theme_.panel_border;
		foreground = theme_.text_disabled;
	} else if (p_primary) {
		background = held ? theme_.accent_pressed : (hovered ? theme_.accent_hover : theme_.accent);
		border = theme_.accent_pressed;
		foreground = theme_.accent_text;
	} else {
		background = held ? theme_.button_pressed : (hovered ? theme_.button_hover : theme_.button_bg);
		border = hovered ? theme_.accent.with_alpha(0.55f) : theme_.button_border;
		foreground = theme_.button_text;
	}

	bordered_rect(p_rect, background, border, theme_.radius, 1.0f);
	text_in_rect(p_rect, p_label, theme_.font_size, foreground, Font::Align::Center);
	return clicked;
}

void Ui::progress_bar(const Rect &p_rect, float p_value, const Color &p_fill,
		const Color &p_background, float p_radius) {
	bordered_rect(p_rect, p_background, theme_.panel_border, p_radius, 1.0f);
	const float value = clamp(p_value, 0.0f, 1.0f);
	if (value <= 0.0f) {
		return;
	}
	const Rect inner = p_rect.shrink(2.0f);
	draw_list_.add_rect(Rect(inner.x, inner.y, inner.w * value, inner.h), p_fill, p_radius);
}

Vec2 Ui::joystick(const char *p_id, const Rect &p_rect) {
	const uint32_t id = hash_id(p_id);
	const Vec2 center = p_rect.center();
	const float radius = std::min(p_rect.w, p_rect.h) * 0.5f;

	draw_list_.add_circle(center, radius, theme_.joystick_base, theme_.joystick_border, 1.5f);
	draw_list_.add_circle(center, radius * 0.62f, Color(0.0f, 0.0f, 0.0f, 0.0f),
			theme_.joystick_border.with_alpha(0.35f), 1.0f);

	const bool can_grab = active_ == 0 || active_ == id;
	if (input_.pointer_pressed && can_grab && p_rect.contains(input_.pointer)) {
		active_ = id;
		joystick_origin_ = input_.pointer;
	}

	Vec2 value;
	if (active_ == id && input_.pointer_down) {
		Vec2 delta = input_.pointer - joystick_origin_;
		const float length = delta.length();
		if (length > radius) {
			delta = delta * (radius / length);
		}
		value = delta / (radius * 0.75f);
		if (value.length() > 1.0f) {
			value = value.normalized();
		}
		constexpr float DEAD_ZONE = 0.12f;
		if (value.length() < DEAD_ZONE) {
			value = Vec2();
		}
	}

	const Vec2 knob = center + value * (radius * 0.45f);
	draw_list_.add_circle(knob, radius * 0.30f, theme_.joystick_knob, theme_.joystick_border.with_alpha(0.8f), 1.0f);
	return value;
}

bool Ui::is_hovered(const Rect &p_rect) {
	return p_rect.contains(input_.pointer);
}

} // namespace zilla
