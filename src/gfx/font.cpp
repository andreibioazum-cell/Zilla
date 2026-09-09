// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "gfx/font.h"

#include "core/log.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace zilla {

namespace {

constexpr int ATLAS_WIDTH = 1024;
constexpr int ATLAS_MAX_HEIGHT = 1024;

// Latin + Cyrillic + the handful of symbols the interface uses. Keeping the
// charset small keeps the atlas (and the startup bake) cheap.
std::vector<uint32_t> default_charset() {
	std::vector<uint32_t> cps;
	for (uint32_t c = 0x20; c < 0x7F; ++c) {
		cps.push_back(c);
	}
	for (uint32_t c = 0x400; c <= 0x45F; ++c) { // Cyrillic, including Ё/ё
		cps.push_back(c);
	}
	for (uint32_t c : { 0x00A0u, 0x00ABu, 0x00B7u, 0x00BBu, 0x00D7u, 0x2013u, 0x2014u, 0x2018u,
							0x2019u, 0x201Cu, 0x201Du, 0x2022u, 0x2026u, 0x2192u, 0x2116u,
							0x25AAu, 0x25A0u, 0x25B8u, 0x25CBu, 0x25CFu, 0x2715u }) {
		cps.push_back(c);
	}
	return cps;
}

} // namespace

uint32_t utf8_decode(const std::string &p_text, size_t &p_index) {
	if (p_index >= p_text.size()) {
		return 0;
	}
	const unsigned char c = static_cast<unsigned char>(p_text[p_index]);
	if (c < 0x80) {
		++p_index;
		return c;
	}
	int extra = 0;
	uint32_t cp = 0;
	if ((c & 0xE0) == 0xC0) {
		extra = 1;
		cp = c & 0x1F;
	} else if ((c & 0xF0) == 0xE0) {
		extra = 2;
		cp = c & 0x0F;
	} else if ((c & 0xF8) == 0xF0) {
		extra = 3;
		cp = c & 0x07;
	} else {
		++p_index;
		return 0;
	}
	if (p_index + size_t(extra) >= p_text.size()) {
		++p_index;
		return 0;
	}
	for (int i = 1; i <= extra; ++i) {
		cp = (cp << 6) | (static_cast<unsigned char>(p_text[p_index + size_t(i)]) & 0x3F);
	}
	p_index += size_t(extra) + 1;
	return cp;
}

Font::Font() = default;

Font::~Font() {
	delete info_;
	info_ = nullptr;
}

bool Font::load(const uint8_t *p_data, size_t p_size) {
	ttf_.assign(p_data, p_data + p_size);
	delete info_;
	info_ = new stbtt_fontinfo();
	if (!stbtt_InitFont(info_, ttf_.data(), 0)) {
		ZILLA_LOG_ERROR("Font: stb_truetype could not parse the font data.");
		delete info_;
		info_ = nullptr;
		return false;
	}
	stbtt_GetFontVMetrics(info_, &ascent_, &descent_, &line_gap_);
	build_atlas();
	loaded_ = true;
	return true;
}

void Font::build_atlas() {
	const float bake_scale = stbtt_ScaleForPixelHeight(info_, SDF_PIXEL_HEIGHT);
	const std::vector<uint32_t> charset = default_charset();

	atlas_width_ = ATLAS_WIDTH;
	atlas_.assign(size_t(ATLAS_WIDTH) * ATLAS_MAX_HEIGHT, 0);

	int pen_x = 0;
	int pen_y = 0;
	int row_height = 0;
	int baked = 0;

	for (uint32_t cp : charset) {
		Glyph glyph;
		int advance = 0;
		int lsb = 0;
		stbtt_GetCodepointHMetrics(info_, int(cp), &advance, &lsb);
		glyph.advance = float(advance);

		int w = 0, h = 0, xoff = 0, yoff = 0;
		unsigned char *bitmap = stbtt_GetCodepointSDF(info_, bake_scale, int(cp), SDF_PADDING,
				static_cast<unsigned char>(SDF_ONEDGE), SDF_DIST_SCALE, &w, &h, &xoff, &yoff);
		if (bitmap != nullptr && w > 0 && h > 0) {
			if (pen_x + w > ATLAS_WIDTH) {
				pen_x = 0;
				pen_y += row_height;
				row_height = 0;
			}
			if (pen_y + h > ATLAS_MAX_HEIGHT) {
				ZILLA_LOG_WARN("Font: atlas is full, dropping U+%04X.", unsigned(cp));
				stbtt_FreeSDF(bitmap, nullptr);
				glyphs_[cp] = glyph;
				continue;
			}
			for (int y = 0; y < h; ++y) {
				std::memcpy(&atlas_[size_t(pen_y + y) * ATLAS_WIDTH + pen_x], bitmap + size_t(y) * w, size_t(w));
			}
			glyph.has_bitmap = true;
			glyph.bw = float(w);
			glyph.bh = float(h);
			glyph.off_x = float(xoff);
			glyph.off_y = float(yoff);
			glyph.px = pen_x;
			glyph.py = pen_y;
			glyph.pw = w;
			glyph.ph = h;
			pen_x += w;
			row_height = std::max(row_height, h);
			++baked;
			glyphs_[cp] = glyph;
			stbtt_FreeSDF(bitmap, nullptr);
		} else {
			glyphs_[cp] = glyph;
		}
	}

	atlas_height_ = std::max(1, pen_y + row_height);

	// Second pass: turn the packed pixel rectangles into UV coordinates now that
	// the final atlas height is known.
	for (auto &entry : glyphs_) {
		Glyph &glyph = entry.second;
		if (!glyph.has_bitmap) {
			continue;
		}
		glyph.u0 = float(glyph.px) / float(atlas_width_);
		glyph.u1 = float(glyph.px + glyph.pw) / float(atlas_width_);
		glyph.v0 = float(glyph.py) / float(atlas_height_);
		glyph.v1 = float(glyph.py + glyph.ph) / float(atlas_height_);
	}

	atlas_.resize(size_t(atlas_width_) * size_t(atlas_height_));
	ZILLA_LOG_INFO("Font: baked %d glyphs into a %dx%d SDF atlas.", baked, atlas_width_, atlas_height_);
}

const Font::Glyph *Font::find(uint32_t p_codepoint) const {
	const auto it = glyphs_.find(p_codepoint);
	return it == glyphs_.end() ? nullptr : &it->second;
}

float Font::ascent(float p_size) const {
	return float(ascent_) * stbtt_ScaleForPixelHeight(info_, p_size);
}

float Font::descent(float p_size) const {
	return float(descent_) * stbtt_ScaleForPixelHeight(info_, p_size);
}

float Font::line_height(float p_size) const {
	return float(ascent_ - descent_ + line_gap_) * stbtt_ScaleForPixelHeight(info_, p_size);
}

float Font::measure(const std::string &p_text, float p_size, float p_letter_spacing) const {
	if (!loaded_ || p_text.empty()) {
		return 0.0f;
	}
	const float scale = stbtt_ScaleForPixelHeight(info_, p_size);
	float width = 0.0f;
	uint32_t previous = 0;
	size_t index = 0;
	while (index < p_text.size()) {
		const uint32_t cp = utf8_decode(p_text, index);
		const Glyph *glyph = find(cp);
		if (glyph != nullptr) {
			const float kern = float(stbtt_GetCodepointKernAdvance(info_, int(previous), int(cp)));
			width += (glyph->advance + kern) * scale + p_letter_spacing;
		}
		previous = cp;
	}
	return width;
}

void Font::draw(DrawList &p_dl, const std::string &p_text, Vec2 p_pos, float p_size,
		const Color &p_color, Align p_align, float p_letter_spacing) const {
	if (!loaded_ || p_text.empty() || p_color.a <= 0.0f) {
		return;
	}

	const float metric_scale = stbtt_ScaleForPixelHeight(info_, p_size);
	const float quad_scale = p_size / SDF_PIXEL_HEIGHT;
	const float total = measure(p_text, p_size, p_letter_spacing);

	float pen_x = p_pos.x;
	if (p_align == Align::Center) {
		pen_x -= total * 0.5f;
	} else if (p_align == Align::Right) {
		pen_x -= total;
	}
	const float baseline = std::round(p_pos.y + ascent(p_size));

	uint32_t previous = 0;
	size_t index = 0;
	while (index < p_text.size()) {
		const uint32_t cp = utf8_decode(p_text, index);
		const Glyph *glyph = find(cp);
		if (glyph != nullptr) {
			if (glyph->has_bitmap) {
				const float x = std::round(pen_x + glyph->off_x * quad_scale);
				const float y = std::round(baseline + glyph->off_y * quad_scale);
				const Rect dest(x, y, glyph->bw * quad_scale, glyph->bh * quad_scale);
				const Rect uv(glyph->u0, glyph->v0, glyph->u1 - glyph->u0, glyph->v1 - glyph->v0);
				p_dl.add_glyph(dest, uv, p_color, quad_scale);
			}
			const float kern = float(stbtt_GetCodepointKernAdvance(info_, int(previous), int(cp)));
			pen_x += (glyph->advance + kern) * metric_scale + p_letter_spacing;
		}
		previous = cp;
	}
}

void Font::draw_in_rect(DrawList &p_dl, const std::string &p_text, const Rect &p_rect, float p_size,
		const Color &p_color, Align p_align, float p_letter_spacing) const {
	Vec2 pos = p_rect.position();
	pos.y = std::round(p_rect.center().y - cap_height(p_size) * 0.5f - 1.0f);
	if (p_align == Align::Center) {
		pos.x = p_rect.center().x;
	} else if (p_align == Align::Right) {
		pos.x = p_rect.right();
	}
	draw(p_dl, p_text, pos, p_size, p_color, p_align, p_letter_spacing);
}

} // namespace zilla
