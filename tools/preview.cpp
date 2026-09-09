// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Offline preview renderer: rasterises the exact same DrawList the Vulkan
// backend would draw, on the CPU, and writes PNG screenshots. Useful to check
// the interface (and the Cyrillic text) on a machine without a GPU, and to keep
// a visual reference in the repository.
//
// The shading here mirrors shaders/quad.frag.glsl - keep the two in sync.

#include "core/log.h"
#include "core/types.h"
#include "game/game.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/input.h"
#include "ui/theme.h"
#include "ui/ui.h"

// stb_image_write is a single header library with its own warnings; keep them
// out of the build log.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace zilla;

namespace {

Color unpack_color(uint32_t p_packed) {
	return Color(((p_packed >> 24) & 0xFF) / 255.0f,
			((p_packed >> 16) & 0xFF) / 255.0f,
			((p_packed >> 8) & 0xFF) / 255.0f,
			(p_packed & 0xFF) / 255.0f);
}

struct Varying {
	float lx = 0.0f, ly = 0.0f;
	float u = 0.0f, v = 0.0f;
	float fr = 0.0f, fg = 0.0f, fb = 0.0f, fa = 1.0f;
	float br = 0.0f, bg = 0.0f, bb = 0.0f, ba = 1.0f;
	float hw = 0.0f, hh = 0.0f, radius = 0.0f, border = 0.0f;
	float tex = 0.0f;
};

Varying lerp3(const Varying &p_a, const Varying &p_b, const Varying &p_c,
		float p_wa, float p_wb, float p_wc) {
#define Z_INTERP(field) p_a.field *p_wa + p_b.field *p_wb + p_c.field *p_wc
	Varying out;
	out.lx = Z_INTERP(lx);
	out.ly = Z_INTERP(ly);
	out.u = Z_INTERP(u);
	out.v = Z_INTERP(v);
	out.fr = Z_INTERP(fr);
	out.fg = Z_INTERP(fg);
	out.fb = Z_INTERP(fb);
	out.fa = Z_INTERP(fa);
	out.br = Z_INTERP(br);
	out.bg = Z_INTERP(bg);
	out.bb = Z_INTERP(bb);
	out.ba = Z_INTERP(ba);
	out.hw = Z_INTERP(hw);
	out.hh = Z_INTERP(hh);
	out.radius = Z_INTERP(radius);
	out.border = Z_INTERP(border);
	out.tex = Z_INTERP(tex);
#undef Z_INTERP
	return out;
}

float rounded_box_sdf(const Vec2 &p_point, const Vec2 &p_half_size, float p_radius) {
	const Vec2 q(std::fabs(p_point.x) - p_half_size.x + p_radius,
			std::fabs(p_point.y) - p_half_size.y + p_radius);
	return std::min(std::max(q.x, q.y), 0.0f) + std::sqrt(std::max(q.x, 0.0f) * std::max(q.x, 0.0f) +
														 std::max(q.y, 0.0f) * std::max(q.y, 0.0f)) - p_radius;
}

class Rasterizer {
public:
	Rasterizer(int p_width, int p_height, const Color &p_clear);

	void draw(const DrawList &p_list, const Font &p_font);
	bool save_png(const char *p_path) const;

private:
	void triangle(const DrawVertex &p_a, const DrawVertex &p_b, const DrawVertex &p_c,
			const Rect &p_clip, const Font &p_font);
	float sample_atlas(const Font &p_font, float p_u, float p_v) const;
	void blend(int p_x, int p_y, const Color &p_color);

	int width_ = 0;
	int height_ = 0;
	std::vector<uint8_t> pixels_;
};

Rasterizer::Rasterizer(int p_width, int p_height, const Color &p_clear) :
		width_(p_width), height_(p_height), pixels_(size_t(p_width) * p_height * 4, 0) {
	for (size_t i = 0; i < size_t(p_width) * p_height; ++i) {
		pixels_[i * 4 + 0] = uint8_t(p_clear.r * 255.0f);
		pixels_[i * 4 + 1] = uint8_t(p_clear.g * 255.0f);
		pixels_[i * 4 + 2] = uint8_t(p_clear.b * 255.0f);
		pixels_[i * 4 + 3] = 255;
	}
}

float Rasterizer::sample_atlas(const Font &p_font, float p_u, float p_v) const {
	const std::vector<uint8_t> &atlas = p_font.atlas();
	if (atlas.empty()) {
		return 0.0f;
	}
	const float x = p_u * float(p_font.atlas_width()) - 0.5f;
	const float y = p_v * float(p_font.atlas_height()) - 0.5f;
	const int x0 = int(std::floor(x));
	const int y0 = int(std::floor(y));
	const float fx = x - float(x0);
	const float fy = y - float(y0);

	auto fetch = [&](int px, int py) {
		const int cx = std::clamp(px, 0, p_font.atlas_width() - 1);
		const int cy = std::clamp(py, 0, p_font.atlas_height() - 1);
		return atlas[size_t(cy) * p_font.atlas_width() + size_t(cx)] / 255.0f;
	};

	const float top = zilla::lerp(fetch(x0, y0), fetch(x0 + 1, y0), fx);
	const float bottom = zilla::lerp(fetch(x0, y0 + 1), fetch(x0 + 1, y0 + 1), fx);
	return zilla::lerp(top, bottom, fy);
}

void Rasterizer::blend(int p_x, int p_y, const Color &p_color) {
	uint8_t *pixel = &pixels_[(size_t(p_y) * width_ + size_t(p_x)) * 4];
	const float alpha = zilla::clamp(p_color.a, 0.0f, 1.0f);
	for (int i = 0; i < 3; ++i) {
		const float src = (&p_color.r)[i] * alpha;
		const float dst = pixel[i] / 255.0f * (1.0f - alpha);
		pixel[i] = uint8_t(zilla::clamp(src + dst, 0.0f, 1.0f) * 255.0f);
	}
}

void Rasterizer::triangle(const DrawVertex &p_a, const DrawVertex &p_b, const DrawVertex &p_c,
		const Rect &p_clip, const Font &p_font) {
	Varying varyings[3];
	const DrawVertex *vertices[3] = { &p_a, &p_b, &p_c };
	for (int i = 0; i < 3; ++i) {
		const Color fill = unpack_color(vertices[i]->fill);
		const Color border = unpack_color(vertices[i]->border);
		Varying &out = varyings[i];
		out.lx = vertices[i]->lx;
		out.ly = vertices[i]->ly;
		out.u = vertices[i]->u;
		out.v = vertices[i]->v;
		out.fr = fill.r;
		out.fg = fill.g;
		out.fb = fill.b;
		out.fa = fill.a;
		out.br = border.r;
		out.bg = border.g;
		out.bb = border.b;
		out.ba = border.a;
		out.hw = vertices[i]->half_w;
		out.hh = vertices[i]->half_h;
		out.radius = vertices[i]->radius;
		out.border = vertices[i]->border_w;
		out.tex = vertices[i]->tex;
	}

	const Vec2 points[3] = { Vec2(p_a.x, p_a.y), Vec2(p_b.x, p_b.y), Vec2(p_c.x, p_c.y) };
	auto edge = [](const Vec2 &a, const Vec2 &b, const Vec2 &p) {
		return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
	};

	const float area = edge(points[0], points[1], points[2]);
	if (std::fabs(area) < 0.000001f) {
		return;
	}

	const int min_x = int(std::max(0.0f, std::floor(std::min({ points[0].x, points[1].x, points[2].x }))));
	const int max_x = int(std::min(float(width_ - 1), std::ceil(std::max({ points[0].x, points[1].x, points[2].x }))));
	const int min_y = int(std::max(0.0f, std::floor(std::min({ points[0].y, points[1].y, points[2].y }))));
	const int max_y = int(std::min(float(height_ - 1), std::ceil(std::max({ points[0].y, points[1].y, points[2].y }))));

	for (int y = min_y; y <= max_y; ++y) {
		for (int x = min_x; x <= max_x; ++x) {
			const Vec2 pixel(float(x) + 0.5f, float(y) + 0.5f);
			if (!p_clip.contains(pixel)) {
				continue;
			}
			const float w0 = edge(points[1], points[2], pixel);
			const float w1 = edge(points[2], points[0], pixel);
			const float w2 = edge(points[0], points[1], pixel);
			const bool positive = area > 0.0f;
			if ((positive && (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)) ||
					(!positive && (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f))) {
				continue;
			}

			const Varying varying = lerp3(varyings[0], varyings[1], varyings[2],
					w0 / area, w1 / area, w2 / area);

			Color color;
			if (varying.tex > 0.5f) {
				const float sampled = sample_atlas(p_font, varying.u, varying.v);
				const float distance = (sampled * 255.0f - Font::SDF_ONEDGE) / Font::SDF_DIST_SCALE;
				const float pixels = distance * varying.radius;
				color = Color(varying.fr, varying.fg, varying.fb,
						varying.fa * zilla::clamp(pixels + 0.5f, 0.0f, 1.0f));
			} else {
				const float d = rounded_box_sdf(Vec2(varying.lx, varying.ly),
						Vec2(varying.hw, varying.hh), varying.radius);
				const float coverage = 1.0f - zilla::smoothstep(-0.75f, 0.75f, d);
				// `inner` is 0 in the border ring (d > -border) and 1 in the fill.
				const float inner = varying.border > 0.0f
						? 1.0f - zilla::smoothstep(-varying.border - 0.75f, -varying.border + 0.75f, d)
						: 1.0f;
				color = Color(zilla::lerp(varying.br, varying.fr, inner),
						zilla::lerp(varying.bg, varying.fg, inner),
						zilla::lerp(varying.bb, varying.fb, inner),
						zilla::lerp(varying.ba, varying.fa, inner) * coverage);
			}

			if (color.a > 0.0f) {
				blend(x, y, color);
			}
		}
	}
}

void Rasterizer::draw(const DrawList &p_list, const Font &p_font) {
	const Rect canvas(0.0f, 0.0f, float(width_), float(height_));
	for (const DrawCommand &command : p_list.commands()) {
		Rect clip = command.has_clip ? command.clip : canvas;
		clip = clip.intersection(canvas);
		if (clip.is_empty()) {
			continue;
		}
		for (uint32_t i = command.first_index; i + 2 < command.first_index + command.index_count; i += 3) {
			triangle(p_list.vertices()[p_list.indices()[i]],
					p_list.vertices()[p_list.indices()[i + 1]],
					p_list.vertices()[p_list.indices()[i + 2]],
					clip, p_font);
		}
	}
}

bool Rasterizer::save_png(const char *p_path) const {
	if (stbi_write_png(p_path, width_, height_, 4, pixels_.data(), width_ * 4) == 0) {
		ZILLA_LOG_ERROR("Preview: could not write '%s'.", p_path);
		return false;
	}
	return true;
}

std::vector<uint8_t> read_file(const std::string &p_path) {
	std::ifstream file(p_path, std::ios::binary | std::ios::ate);
	if (!file) {
		ZILLA_LOG_ERROR("Preview: could not open '%s'.", p_path.c_str());
		return {};
	}
	const std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(static_cast<size_t>(size));
	file.read(reinterpret_cast<char *>(data.data()), size);
	return data;
}

bool load_font(Font &p_font, const std::string &p_path) {
	const std::vector<uint8_t> data = read_file(p_path);
	return !data.empty() && p_font.load(data.data(), data.size());
}

} // namespace

int main(int argc, char **argv) {
	const std::string asset_dir = argc > 1 ? argv[1] : "assets";
	const std::string output_dir = argc > 2 ? argv[2] : "build";

	Font font;
	Font font_bold;
	if (!load_font(font, asset_dir + "/fonts/Inter_Regular.ttf") ||
			!load_font(font_bold, asset_dir + "/fonts/Inter_Bold.ttf")) {
		return 1;
	}

	const int width = 1280;
	const int height = 760;
	Theme theme;
	DrawList draw_list;
	Ui ui(draw_list, font, font_bold);

	// --- lobby -------------------------------------------------------------
	{
		Game game;
		game.set_device_name("CPU Preview");
		game.set_fps(60.0f);
		InputState input;
		input.pointer = Vec2(900.0f, 430.0f); // hover over the "Играть" button

		ui.begin(float(width), float(height), input, theme);
		game.frame(1.0f / 60.0f, ui);
		ui.end();

		Rasterizer rasterizer(width, height, theme.window_bg);
		rasterizer.draw(draw_list, font);
		rasterizer.save_png((output_dir + "/preview_lobby.png").c_str());
		ZILLA_LOG_INFO("Preview: wrote %s/preview_lobby.png (%d primitives).",
				output_dir.c_str(), int(draw_list.primitive_count()));
	}

	// --- arena -------------------------------------------------------------
	{
		Game game;
		game.set_device_name("CPU Preview");
		game.start_new_run();

		InputState input;
		for (int i = 0; i < 420; ++i) {
			// Wander around with WASD and swing every now and then.
			const float t = float(i) / 60.0f;
			input.key_down[int(Key::D)] = std::sin(t) > 0.0f;
			input.key_down[int(Key::A)] = std::sin(t) <= 0.0f;
			input.key_down[int(Key::W)] = std::cos(t * 0.7f) > 0.0f;
			input.key_down[int(Key::S)] = std::cos(t * 0.7f) <= 0.0f;
			input.key_down[int(Key::Space)] = (i % 26 < 2);
			input.pointer = Vec2(140.0f, 640.0f); // parked on the thumbstick
			input.pointer_down = true;
			input.pointer_pressed = (i == 0);
			input.pointer_released = false;

			ui.begin(float(width), float(height), input, theme);
			game.frame(1.0f / 60.0f, ui);
			ui.end();
		}

		// One clean frame for the screenshot: stick pushed up-right.
		input.key_down[int(Key::D)] = true;
		input.key_down[int(Key::W)] = true;
		input.key_down[int(Key::A)] = false;
		input.key_down[int(Key::S)] = false;
		input.key_down[int(Key::Space)] = false;
		ui.begin(float(width), float(height), input, theme);
		game.frame(1.0f / 60.0f, ui);
		ui.end();

		Rasterizer rasterizer(width, height, theme.window_bg);
		rasterizer.draw(draw_list, font);
		rasterizer.save_png((output_dir + "/preview_arena.png").c_str());
		ZILLA_LOG_INFO("Preview: wrote %s/preview_arena.png (%d primitives).",
				output_dir.c_str(), int(draw_list.primitive_count()));
	}

	// --- font card ---------------------------------------------------------
	// A simple card showing the whole charset the atlas bakes - handy to check
	// that Cyrillic (and the SDF pipeline) look right.
	{
		const int card_width = 900;
		const int card_height = 230;

		static const char *lines[] = {
			"АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ",
			"абвгдежзийклмнопрстуфхцчшщъыьэюя",
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789",
			"Джойстик • УДАР • Здоровье • Счёт 128",
		};

		InputState input;
		ui.begin(float(card_width), float(card_height), input, theme);
		ui.panel(Rect(0.0f, 0.0f, float(card_width), float(card_height)));
		float y = 22.0f;
		for (const char *line : lines) {
			ui.text(Vec2(24.0f, y), line, 24.0f, theme.text);
			y += 34.0f;
		}
		ui.label(Rect(24.0f, y + 6.0f, float(card_width) - 48.0f, 24.0f),
				"Zilla 0.1 • Inter • SDF atlas", theme.font_size_small, theme.text_dim,
				Font::Align::Left, true);
		ui.end();

		Rasterizer rasterizer(card_width, card_height, theme.window_bg);
		rasterizer.draw(draw_list, font);
		rasterizer.save_png((output_dir + "/preview_fonts.png").c_str());
		ZILLA_LOG_INFO("Preview: wrote %s/preview_fonts.png (%d primitives).",
				output_dir.c_str(), int(draw_list.primitive_count()));
	}

	return 0;
}
