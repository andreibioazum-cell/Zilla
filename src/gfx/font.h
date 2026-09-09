// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// TrueType font rasterised once into a signed distance field atlas
// (stb_truetype). The atlas is uploaded to Vulkan as an R8 texture; the values
// in `SDF_*` below must stay in sync with shaders/quad.frag.glsl.
#ifndef ZILLA_GFX_FONT_H
#define ZILLA_GFX_FONT_H

#include "core/types.h"
#include "gfx/draw_list.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct stbtt_fontinfo;

namespace zilla {

class Font {
public:
	// SDF generation parameters (mirrored by the fragment shader).
	static constexpr float SDF_PIXEL_HEIGHT = 32.0f; // glyphs are baked at this size
	static constexpr int SDF_PADDING = 5; // distance field margin, in baked pixels
	static constexpr float SDF_ONEDGE = 128.0f; // atlas value that sits on the outline
	static constexpr float SDF_DIST_SCALE = SDF_ONEDGE / float(SDF_PADDING);

	enum class Align {
		Left,
		Center,
		Right,
	};

	Font();
	~Font();

	Font(const Font &) = delete;
	Font &operator=(const Font &) = delete;

	// `p_data` is kept alive by the font, so it may be freed after the call.
	bool load(const uint8_t *p_data, size_t p_size);
	bool is_loaded() const { return loaded_; }

	int atlas_width() const { return atlas_width_; }
	int atlas_height() const { return atlas_height_; }
	const std::vector<uint8_t> &atlas() const { return atlas_; }

	float ascent(float p_size) const;
	float descent(float p_size) const;
	float line_height(float p_size) const;
	float cap_height(float p_size) const { return ascent(p_size) - descent(p_size); }

	float measure(const std::string &p_text, float p_size, float p_letter_spacing = 0.0f) const;

	// `p_pos` is the top-left corner of the text box (for Center/Right the x
	// coordinate is treated as the centre / right edge).
	void draw(DrawList &p_dl, const std::string &p_text, Vec2 p_pos, float p_size,
			const Color &p_color, Align p_align = Align::Left, float p_letter_spacing = 0.0f) const;

	// Same, but vertically centred inside `p_rect`.
	void draw_in_rect(DrawList &p_dl, const std::string &p_text, const Rect &p_rect, float p_size,
			const Color &p_color, Align p_align = Align::Left, float p_letter_spacing = 0.0f) const;

	int glyph_count() const { return int(glyphs_.size()); }

private:
	struct Glyph {
		float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
		int px = 0, py = 0, pw = 0, ph = 0; // packed rectangle in the atlas
		float advance = 0.0f; // font units
		float off_x = 0.0f, off_y = 0.0f; // bitmap offset from the pen position (baked px)
		float bw = 0.0f, bh = 0.0f; // bitmap size (baked px)
		bool has_bitmap = false;
	};

	void build_atlas();
	const Glyph *find(uint32_t p_codepoint) const;

	stbtt_fontinfo *info_ = nullptr;
	std::vector<uint8_t> ttf_;
	std::unordered_map<uint32_t, Glyph> glyphs_;
	std::vector<uint8_t> atlas_;
	int atlas_width_ = 0;
	int atlas_height_ = 0;
	int ascent_ = 0;
	int descent_ = 0;
	int line_gap_ = 0;
	bool loaded_ = false;
};

// Decodes one UTF-8 codepoint, advancing `p_index`. Returns 0 on invalid input.
uint32_t utf8_decode(const std::string &p_text, size_t &p_index);

} // namespace zilla

#endif // ZILLA_GFX_FONT_H
