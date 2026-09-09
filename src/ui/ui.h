// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Immediate mode interface layer. No retained widget tree and no callbacks:
// every frame the game asks `Ui` to draw a widget and gets the interaction
// result back, Godot-style panels and buttons included.
#ifndef ZILLA_UI_UI_H
#define ZILLA_UI_UI_H

#include "core/types.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/input.h"
#include "ui/theme.h"

#include <string>

namespace zilla {

class Ui {
public:
	Ui(DrawList &p_draw_list, const Font &p_font, const Font &p_font_bold);

	void begin(float p_width, float p_height, const InputState &p_input, const Theme &p_theme);
	void end();

	float width() const { return width_; }
	float height() const { return height_; }
	const Theme &theme() const { return theme_; }
	const InputState &input() const { return input_; }

	// --- primitives ---------------------------------------------------------
	void fill_rect(const Rect &p_rect, const Color &p_color, float p_radius = 0.0f);
	void bordered_rect(const Rect &p_rect, const Color &p_fill, const Color &p_border,
			float p_radius = -1.0f, float p_border_width = 1.0f);
	void text(Vec2 p_pos, const std::string &p_text, float p_size, const Color &p_color,
			Font::Align p_align = Font::Align::Left, bool p_bold = false);
	void text_in_rect(const Rect &p_rect, const std::string &p_text, float p_size,
			const Color &p_color, Font::Align p_align = Font::Align::Left, bool p_bold = false);
	void circle(const Vec2 &p_center, float p_radius, const Color &p_fill,
			const Color &p_border = Color(), float p_border_width = 0.0f);

	void set_clip(const Rect &p_rect) { draw_list_.set_clip(p_rect); }
	void clear_clip() { draw_list_.clear_clip(); }

	// --- widgets ------------------------------------------------------------
	// Window chrome.
	void window_background();
	void title_bar(const std::string &p_title);
	void status_bar(const std::string &p_left, const std::string &p_right);

	void panel(const Rect &p_rect, bool p_sunken = false);
	void label(const Rect &p_rect, const std::string &p_text, float p_size, const Color &p_color,
			Font::Align p_align = Font::Align::Left, bool p_bold = false);

	// Returns true on the frame the button is released inside its rectangle.
	bool button(const char *p_id, const Rect &p_rect, const std::string &p_label,
			bool p_enabled = true, bool p_primary = false);

	void progress_bar(const Rect &p_rect, float p_value, const Color &p_fill,
			const Color &p_background, float p_radius = 3.0f);

	// On-screen thumbstick: drag anywhere inside to steer. Returns -1..1 per axis.
	Vec2 joystick(const char *p_id, const Rect &p_rect);

	bool is_hovered(const Rect &p_rect);

private:
	static uint32_t hash_id(const char *p_id);

	DrawList &draw_list_;
	const Font &font_;
	const Font &font_bold_;
	Theme theme_;
	InputState input_;
	float width_ = 0.0f;
	float height_ = 0.0f;
	uint32_t hot_ = 0;
	uint32_t active_ = 0;
	Vec2 joystick_origin_;
};

} // namespace zilla

#endif // ZILLA_UI_UI_H
