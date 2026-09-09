// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Backend independent input snapshot. The window backend (GLFW) fills it once
// per frame, the UI layer consumes it.
#ifndef ZILLA_PLATFORM_INPUT_H
#define ZILLA_PLATFORM_INPUT_H

#include "core/types.h"

namespace zilla {

enum class Key : int {
	Unknown = 0,
	Escape,
	Space,
	Enter,
	Tab,
	W,
	A,
	S,
	D,
	R,
	P,
	F,
	Count,
};

struct InputState {
	// Pointer / touch position in UI pixels (origin: top-left).
	Vec2 pointer;
	bool pointer_down = false;
	bool pointer_pressed = false;
	bool pointer_released = false;

	bool key_down[int(Key::Count)] = {};
	bool key_pressed[int(Key::Count)] = {};

	bool is_down(Key p_key) const { return key_down[int(p_key)]; }
	bool is_pressed(Key p_key) const { return key_pressed[int(p_key)]; }
};

} // namespace zilla

#endif // ZILLA_PLATFORM_INPUT_H
