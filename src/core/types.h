// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Small value types used everywhere: vectors, rectangles and colours.
#ifndef ZILLA_CORE_TYPES_H
#define ZILLA_CORE_TYPES_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace zilla {

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;

	Vec2() = default;
	Vec2(float p_x, float p_y) :
			x(p_x), y(p_y) {}

	Vec2 operator+(const Vec2 &p_v) const { return Vec2(x + p_v.x, y + p_v.y); }
	Vec2 operator-(const Vec2 &p_v) const { return Vec2(x - p_v.x, y - p_v.y); }
	Vec2 operator*(float p_s) const { return Vec2(x * p_s, y * p_s); }
	Vec2 operator/(float p_s) const { return Vec2(x / p_s, y / p_s); }
	Vec2 operator-() const { return Vec2(-x, -y); }

	Vec2 &operator+=(const Vec2 &p_v) {
		x += p_v.x;
		y += p_v.y;
		return *this;
	}
	Vec2 &operator-=(const Vec2 &p_v) {
		x -= p_v.x;
		y -= p_v.y;
		return *this;
	}
	Vec2 &operator*=(float p_s) {
		x *= p_s;
		y *= p_s;
		return *this;
	}

	float length() const { return std::sqrt(x * x + y * y); }
	float length_squared() const { return x * x + y * y; }
	float distance_to(const Vec2 &p_v) const { return (*this - p_v).length(); }

	Vec2 normalized() const {
		const float l = length();
		return l > 0.000001f ? Vec2(x / l, y / l) : Vec2();
	}

	Vec2 limited(float p_max) const {
		const float l = length();
		return (l > p_max && l > 0.000001f) ? Vec2(x / l * p_max, y / l * p_max) : *this;
	}
};

inline Vec2 operator*(float p_s, const Vec2 &p_v) { return p_v * p_s; }

struct Rect {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	Rect() = default;
	Rect(float p_x, float p_y, float p_w, float p_h) :
			x(p_x), y(p_y), w(p_w), h(p_h) {}
	Rect(Vec2 p_pos, Vec2 p_size) :
			x(p_pos.x), y(p_pos.y), w(p_size.x), h(p_size.y) {}

	static Rect from_center(Vec2 p_center, Vec2 p_size) {
		return Rect(p_center.x - p_size.x * 0.5f, p_center.y - p_size.y * 0.5f, p_size.x, p_size.y);
	}

	float left() const { return x; }
	float top() const { return y; }
	float right() const { return x + w; }
	float bottom() const { return y + h; }

	Vec2 position() const { return Vec2(x, y); }
	Vec2 size() const { return Vec2(w, h); }
	Vec2 center() const { return Vec2(x + w * 0.5f, y + h * 0.5f); }

	bool is_empty() const { return w <= 0.0f || h <= 0.0f; }
	bool contains(Vec2 p_point) const {
		return p_point.x >= x && p_point.x < right() && p_point.y >= y && p_point.y < bottom();
	}

	Rect grow(float p_margin) const { return Rect(x - p_margin, y - p_margin, w + p_margin * 2.0f, h + p_margin * 2.0f); }
	Rect shrink(float p_margin) const { return grow(-p_margin); }

	Rect intersection(const Rect &p_other) const {
		const float x0 = std::max(x, p_other.x);
		const float y0 = std::max(y, p_other.y);
		const float x1 = std::min(right(), p_other.right());
		const float y1 = std::min(bottom(), p_other.bottom());
		return Rect(x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0));
	}

	bool operator==(const Rect &p_other) const {
		return x == p_other.x && y == p_other.y && w == p_other.w && h == p_other.h;
	}
	bool operator!=(const Rect &p_other) const { return !(*this == p_other); }
};

struct Color {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;

	Color() = default;
	Color(float p_r, float p_g, float p_b, float p_a = 1.0f) :
			r(p_r), g(p_g), b(p_b), a(p_a) {}

	// Builds a colour from 0xRRGGBBAA.
	static Color hex(uint32_t p_hex) {
		return Color(((p_hex >> 24) & 0xFF) / 255.0f,
				((p_hex >> 16) & 0xFF) / 255.0f,
				((p_hex >> 8) & 0xFF) / 255.0f,
				(p_hex & 0xFF) / 255.0f);
	}

	Color with_alpha(float p_alpha) const { return Color(r, g, b, p_alpha); }
	Color scaled(float p_factor) const { return Color(r * p_factor, g * p_factor, b * p_factor, a); }
	Color lightened(float p_amount) const {
		return Color(std::min(1.0f, r + p_amount), std::min(1.0f, g + p_amount), std::min(1.0f, b + p_amount), a);
	}

	Color lerp(const Color &p_to, float p_t) const {
		return Color(r + (p_to.r - r) * p_t,
				g + (p_to.g - g) * p_t,
				b + (p_to.b - b) * p_t,
				a + (p_to.a - a) * p_t);
	}

	uint32_t pack() const {
		const auto to8 = [](float v) { return uint32_t(std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
		return (to8(r) << 24) | (to8(g) << 16) | (to8(b) << 8) | to8(a);
	}
};

inline float clamp(float p_value, float p_min, float p_max) {
	return p_value < p_min ? p_min : (p_value > p_max ? p_max : p_value);
}

inline float lerp(float p_a, float p_b, float p_t) { return p_a + (p_b - p_a) * p_t; }

inline float smoothstep(float p_edge0, float p_edge1, float p_x) {
	const float t = clamp((p_x - p_edge0) / (p_edge1 - p_edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

inline float approach(float p_value, float p_target, float p_delta) {
	if (p_value < p_target) {
		return std::min(p_value + p_delta, p_target);
	}
	return std::max(p_value - p_delta, p_target);
}

// Deterministic little xorshift generator - keeps gameplay reproducible.
class Rng {
public:
	explicit Rng(uint32_t p_seed = 0x5EED1234u) :
			state(p_seed ? p_seed : 1u) {}

	float next_float() { return next_uint32() / 4294967296.0f; }
	float range(float p_min, float p_max) { return p_min + (p_max - p_min) * next_float(); }
	int range_int(int p_min, int p_max) { return p_min + int(next_uint32() % uint32_t(p_max - p_min + 1)); }

private:
	uint32_t next_uint32() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	uint32_t state = 1;
};

} // namespace zilla

#endif // ZILLA_CORE_TYPES_H
