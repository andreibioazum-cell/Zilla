// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "platform/window.h"

#include "core/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace zilla {

namespace {

Window *window_from(GLFWwindow *p_handle) {
	return static_cast<Window *>(glfwGetWindowUserPointer(p_handle));
}

void cursor_position_callback(GLFWwindow *p_handle, double p_x, double p_y) {
	Window *window = window_from(p_handle);
	if (window == nullptr) {
		return;
	}
	window->input_pointer_move(float(p_x), float(p_y));
}

void mouse_button_callback(GLFWwindow *p_handle, int p_button, int p_action, int) {
	Window *window = window_from(p_handle);
	if (window == nullptr || p_button != GLFW_MOUSE_BUTTON_LEFT) {
		return;
	}
	window->input_pointer_button(p_action == GLFW_PRESS);
}

void key_callback(GLFWwindow *p_handle, int p_key, int, int p_action, int) {
	Window *window = window_from(p_handle);
	if (window == nullptr) {
		return;
	}
	const Key key = Window::map_key(p_key);
	if (key != Key::Unknown) {
		window->input_key(key, p_action == GLFW_PRESS || p_action == GLFW_REPEAT);
	}
}

void framebuffer_size_callback(GLFWwindow *p_handle, int, int) {
	Window *window = window_from(p_handle);
	if (window != nullptr) {
		window->update_size();
	}
}

} // namespace

Key Window::map_key(int p_glfw_key) {
	switch (p_glfw_key) {
		case GLFW_KEY_ESCAPE:
			return Key::Escape;
		case GLFW_KEY_SPACE:
			return Key::Space;
		case GLFW_KEY_ENTER:
			return Key::Enter;
		case GLFW_KEY_TAB:
			return Key::Tab;
		case GLFW_KEY_W:
			return Key::W;
		case GLFW_KEY_A:
			return Key::A;
		case GLFW_KEY_S:
			return Key::S;
		case GLFW_KEY_D:
			return Key::D;
		case GLFW_KEY_R:
			return Key::R;
		case GLFW_KEY_P:
			return Key::P;
		case GLFW_KEY_F:
			return Key::F;
		default:
			return Key::Unknown;
	}
}

Window::~Window() {
	destroy();
}

bool Window::create(int p_width, int p_height, const char *p_title) {
	if (!glfwInit()) {
		ZILLA_LOG_ERROR("Window: glfwInit() failed.");
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we drive Vulkan ourselves
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

	handle_ = glfwCreateWindow(p_width, p_height, p_title, nullptr, nullptr);
	if (handle_ == nullptr) {
		ZILLA_LOG_ERROR("Window: glfwCreateWindow() failed.");
		glfwTerminate();
		return false;
	}

	glfwSetWindowUserPointer(handle_, this);
	glfwSetCursorPosCallback(handle_, cursor_position_callback);
	glfwSetMouseButtonCallback(handle_, mouse_button_callback);
	glfwSetKeyCallback(handle_, key_callback);
	glfwSetFramebufferSizeCallback(handle_, framebuffer_size_callback);

	update_size();

	double mouse_x = 0.0;
	double mouse_y = 0.0;
	glfwGetCursorPos(handle_, &mouse_x, &mouse_y);
	input_.pointer = Vec2(float(mouse_x), float(mouse_y));

	return true;
}

void Window::destroy() {
	if (handle_ != nullptr) {
		glfwDestroyWindow(handle_);
		handle_ = nullptr;
		glfwTerminate();
	}
}

bool Window::should_close() const {
	return handle_ == nullptr || glfwWindowShouldClose(handle_) != 0;
}

void Window::request_close() {
	if (handle_ != nullptr) {
		glfwSetWindowShouldClose(handle_, GLFW_TRUE);
	}
}

void Window::poll_events() {
	if (handle_ == nullptr) {
		return;
	}

	bool previous_keys[int(Key::Count)];
	std::copy(std::begin(input_.key_down), std::end(input_.key_down), std::begin(previous_keys));
	const bool previous_down = input_.pointer_down;

	input_.pointer_pressed = false;
	input_.pointer_released = false;
	for (bool &pressed : input_.key_pressed) {
		pressed = false;
	}

	glfwPollEvents();

	input_.pointer_pressed = input_.pointer_down && !previous_down;
	input_.pointer_released = !input_.pointer_down && previous_down;
	for (int i = 0; i < int(Key::Count); ++i) {
		input_.key_pressed[i] = input_.key_down[i] && !previous_keys[i];
	}
}

void Window::update_size() {
	if (handle_ == nullptr) {
		return;
	}
	int window_width = 0;
	int window_height = 0;
	glfwGetWindowSize(handle_, &window_width, &window_height);
	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(handle_, &fb_width, &fb_height);

	if (fb_width != framebuffer_width_ || fb_height != framebuffer_height_) {
		framebuffer_changed_ = true;
	}
	framebuffer_width_ = std::max(1, fb_width);
	framebuffer_height_ = std::max(1, fb_height);
	width_ = std::max(1, window_width);
	height_ = std::max(1, window_height);
	content_scale_ = float(framebuffer_width_) / float(width_);
}

} // namespace zilla
