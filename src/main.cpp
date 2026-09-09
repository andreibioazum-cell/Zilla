// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Zilla - a small Vulkan game: Godot-styled lobby, on-screen thumbstick,
// enemies that chase you. Entry point.

#include "core/log.h"
#include "game/game.h"
#include "gfx/font.h"
#include "gfx/vulkan.h"
#include "platform/window.h"
#include "ui/theme.h"
#include "ui/ui.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

using namespace zilla;

namespace {

std::string executable_dir() {
#if defined(_WIN32)
	char buffer[MAX_PATH];
	const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	const std::string path(buffer, size_t(length));
#elif defined(__linux__)
	char buffer[4096];
	const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0) {
		return std::string();
	}
	const std::string path(buffer, size_t(length));
#else
	return std::string();
#endif
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool file_exists(const std::string &p_path) {
	std::ifstream file(p_path, std::ios::binary);
	return file.good();
}

std::string asset_path(const char *p_name) {
	if (const char *env = std::getenv("ZILLA_ASSETS")) {
		if (*env != '\0') {
			return std::string(env) + "/" + p_name;
		}
	}
	const std::string dir = executable_dir();
	const std::vector<std::string> candidates = {
		dir + "/assets/" + p_name,
		dir + "/../assets/" + p_name,
		dir + "/../../assets/" + p_name,
		std::string("assets/") + p_name,
	};
	for (const std::string &candidate : candidates) {
		if (file_exists(candidate)) {
			return candidate;
		}
	}
	return std::string("assets/") + p_name;
}

std::vector<uint8_t> read_file(const std::string &p_path) {
	std::ifstream file(p_path, std::ios::binary | std::ios::ate);
	if (!file) {
		ZILLA_LOG_ERROR("Could not open '%s' (run the game from the project root or set ZILLA_ASSETS).",
				p_path.c_str());
		return {};
	}
	const std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(static_cast<size_t>(size));
	file.read(reinterpret_cast<char *>(data.data()), size);
	return data;
}

bool load_font(Font &p_font, const char *p_name) {
	const std::string path = asset_path(p_name);
	const std::vector<uint8_t> data = read_file(path);
	if (data.empty()) {
		return false;
	}
	if (!p_font.load(data.data(), data.size())) {
		ZILLA_LOG_ERROR("Could not parse the font '%s'.", path.c_str());
		return false;
	}
	return true;
}

void print_usage(const char *p_program) {
	ZILLA_LOG_INFO("Usage: %s [--validation] [--width 1280] [--height 760]", p_program);
	ZILLA_LOG_INFO("  --validation  enable the Vulkan validation layers (needs the Vulkan SDK)");
}

} // namespace

int main(int argc, char **argv) {
	int width = 1280;
	int height = 760;
	bool validation = false;

	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];
		if (argument == "--validation") {
			validation = true;
		} else if (argument == "--width" && i + 1 < argc) {
			width = std::atoi(argv[++i]);
		} else if (argument == "--height" && i + 1 < argc) {
			height = std::atoi(argv[++i]);
		} else if (argument == "--help" || argument == "-h") {
			print_usage(argv[0]);
			return 0;
		} else {
			ZILLA_LOG_ERROR("Unknown argument: %s", argument.c_str());
			print_usage(argv[0]);
			return 1;
		}
	}

	Window window;
	if (!window.create(width, height, "Zilla — Vulkan")) {
		return 1;
	}

	VulkanRenderer renderer;
	if (!renderer.init(window.handle(), validation)) {
		window.destroy();
		return 1;
	}

	Font font;
	Font font_bold;
	if (!load_font(font, "fonts/Inter_Regular.ttf") ||
			!load_font(font_bold, "fonts/Inter_Bold.ttf")) {
		renderer.shutdown();
		window.destroy();
		return 1;
	}
	if (!renderer.upload_font_atlas(font.atlas().data(), font.atlas_width(), font.atlas_height())) {
		renderer.shutdown();
		window.destroy();
		return 1;
	}

	Theme theme;
	DrawList draw_list;
	Ui ui(draw_list, font, font_bold);
	Game game;
	game.set_device_name(renderer.device_name());

	using clock = std::chrono::steady_clock;
	clock::time_point previous = clock::now();
	float fps = 60.0f;
	float fps_timer = 0.0f;
	int fps_frames = 0;

	while (!window.should_close()) {
		window.poll_events();

		const clock::time_point now = clock::now();
		const float delta = std::min(0.1f, std::chrono::duration<float>(now - previous).count());
		previous = now;

		fps_timer += delta;
		++fps_frames;
		if (fps_timer >= 0.5f) {
			fps = float(fps_frames) / fps_timer;
			fps_timer = 0.0f;
			fps_frames = 0;
		}
		game.set_fps(fps);

		ui.begin(float(window.width()), float(window.height()), window.input(), theme);
		const bool running = game.frame(delta, ui);
		ui.end();

		if (!running) {
			window.request_close();
		}

		renderer.frame(draw_list, theme.window_bg, window.content_scale());
	}

	renderer.shutdown();
	window.destroy();
	return 0;
}
