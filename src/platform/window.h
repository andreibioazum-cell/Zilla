// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// GLFW window + input. Nothing else in the game knows about GLFW.
#ifndef ZILLA_PLATFORM_WINDOW_H
#define ZILLA_PLATFORM_WINDOW_H

#include "core/types.h"
#include "platform/input.h"

struct GLFWwindow;

namespace zilla {

class Window {
public:
	Window() = default;
	~Window();

	Window(const Window &) = delete;
	Window &operator=(const Window &) = delete;

	bool create(int p_width, int p_height, const char *p_title);
	void destroy();

	bool should_close() const;
	void request_close();

	// Fills the input snapshot for this frame. Call once per frame.
	void poll_events();

	// Called by the GLFW callbacks.
	void input_pointer_move(float p_x, float p_y) { input_.pointer = Vec2(p_x, p_y); }
	void input_pointer_button(bool p_down) { input_.pointer_down = p_down; }
	void input_key(Key p_key, bool p_down) { input_.key_down[int(p_key)] = p_down; }

	const InputState &input() const { return input_; }

	// Logical (UI) size.
	int width() const { return width_; }
	int height() const { return height_; }
	// Framebuffer size in device pixels.
	int framebuffer_width() const { return framebuffer_width_; }
	int framebuffer_height() const { return framebuffer_height_; }
	// Device pixels per UI pixel (HiDPI).
	float content_scale() const { return content_scale_; }

	bool framebuffer_changed() const { return framebuffer_changed_; }
	void clear_framebuffer_changed() { framebuffer_changed_ = false; }

	GLFWwindow *handle() const { return handle_; }

	// Used by the GLFW callbacks below.
	static Key map_key(int p_glfw_key);
	void update_size();

private:

	GLFWwindow *handle_ = nullptr;
	InputState input_;
	int width_ = 0;
	int height_ = 0;
	int framebuffer_width_ = 0;
	int framebuffer_height_ = 0;
	float content_scale_ = 1.0f;
	bool framebuffer_changed_ = false;
};

} // namespace zilla

#endif // ZILLA_PLATFORM_WINDOW_H
