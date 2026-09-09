// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Palette and metrics. The colours are taken from Godot's default dark editor
// theme (blue accent, dark grey panels, Inter as the interface font) so the
// game looks like a Godot window.
#ifndef ZILLA_UI_THEME_H
#define ZILLA_UI_THEME_H

#include "core/types.h"

namespace zilla {

struct Theme {
	// Surfaces.
	Color window_bg = Color::hex(0x1B1E24FF);
	Color panel_bg = Color::hex(0x23262DFF);
	Color panel_border = Color::hex(0x383D48FF);
	Color title_bg = Color::hex(0x191B21FF);
	Color sunken_bg = Color::hex(0x17191EFF);

	// Controls.
	Color button_bg = Color::hex(0x323842FF);
	Color button_hover = Color::hex(0x3D4553FF);
	Color button_pressed = Color::hex(0x262B33FF);
	Color button_border = Color::hex(0x464E5CFF);
	Color button_text = Color::hex(0xDCDFE5FF);
	Color button_disabled = Color::hex(0x2A2E36FF);

	// Godot blue accent.
	Color accent = Color::hex(0x699CE8FF);
	Color accent_hover = Color::hex(0x84B0F3FF);
	Color accent_pressed = Color::hex(0x5382C6FF);
	Color accent_text = Color::hex(0xF2F6FFFF);

	// Text.
	Color text = Color::hex(0xE3E6EBFF);
	Color text_dim = Color::hex(0x9DA3AEFF);
	Color text_disabled = Color::hex(0x6A707BFF);

	// Gameplay.
	Color health_bg = Color::hex(0x2A2024FF);
	Color health_fill = Color::hex(0x5FCC66FF);
	Color danger = Color::hex(0xE36A6AFF);
	Color warning = Color::hex(0xE8B25AFF);
	Color player_body = Color::hex(0x699CE8FF);
	Color player_ring = Color::hex(0x9CC0F5FF);
	Color enemy_body = Color::hex(0xE36A6AFF);
	Color enemy_ring = Color::hex(0xF09A9AFF);
	Color arena_bg = Color::hex(0x15171BFF);
	Color arena_border = Color::hex(0x3A404CFF);
	Color grid_minor = Color::hex(0x23272FFF);
	Color grid_major = Color::hex(0x2F343DFF);
	Color attack_flash = Color::hex(0x9CC0F5FF);

	Color joystick_base = Color::hex(0x1F232ACC);
	Color joystick_border = Color::hex(0x4C5566FF);
	Color joystick_knob = Color::hex(0x8FA6C8FF);

	// Metrics.
	float font_size = 16.0f;
	float font_size_small = 13.0f;
	float font_size_title = 30.0f;
	float title_bar_height = 36.0f;
	float status_bar_height = 26.0f;
	float radius = 5.0f;
	float padding = 14.0f;
};

} // namespace zilla

#endif // ZILLA_UI_THEME_H
