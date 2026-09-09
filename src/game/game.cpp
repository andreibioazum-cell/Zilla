// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "game/game.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace zilla {

namespace {

constexpr float PLAYER_SPEED = 195.0f;
constexpr float ATTACK_RANGE = 118.0f;
constexpr float ATTACK_DAMAGE = 34.0f;
constexpr float ATTACK_COOLDOWN = 0.42f;
constexpr float ATTACK_FLASH_TIME = 0.22f;
constexpr float INVULNERABLE_TIME = 0.55f;

constexpr float DIFFICULTY_SPEED[3] = { 0.78f, 1.0f, 1.28f };
constexpr float DIFFICULTY_DAMAGE[3] = { 0.7f, 1.0f, 1.45f };

std::string format(const char *p_format, ...) {
	char buffer[256];
	std::va_list args;
	va_start(args, p_format);
	std::vsnprintf(buffer, sizeof(buffer), p_format, args);
	va_end(args);
	return std::string(buffer);
}

} // namespace

const char *Game::difficulty_name(int p_index) {
	switch (p_index) {
		case 0:
			return "Легко";
		case 2:
			return "Тяжело";
		default:
			return "Нормально";
	}
}

Game::Game() = default;

// --- setup ------------------------------------------------------------------

void Game::start_new_run() {
	enemies_.clear();
	particles_.clear();
	player_hp_ = player_max_hp_;
	score_ = 0;
	kills_ = 0;
	wave_ = 1;
	spawn_timer_ = 0.5f;
	attack_cooldown_ = 0.0f;
	attack_flash_ = 0.0f;
	invulnerable_ = 0.0f;
	move_input_ = Vec2();
	pending_attack_ = false;
	player_pos_ = Vec2(-1000.0f, -1000.0f); // placed once the arena is known
	screen_ = Screen::Playing;
}

Rect Game::arena_rect(const Ui &p_ui) const {
	const float top = p_ui.theme().title_bar_height + 14.0f;
	const float bottom = p_ui.height() - p_ui.theme().status_bar_height - 14.0f;
	return Rect(16.0f, top, p_ui.width() - 32.0f, std::max(120.0f, bottom - top));
}

// --- frame ------------------------------------------------------------------

bool Game::frame(float p_delta, Ui &p_ui) {
	time_ += p_delta;

	switch (screen_) {
		case Screen::Lobby:
			draw_lobby(p_ui);
			break;
		case Screen::Playing:
			draw_playing(p_delta, p_ui);
			break;
		case Screen::Paused:
			draw_playing(0.0f, p_ui);
			draw_paused(p_ui);
			break;
		case Screen::GameOver:
			draw_playing(0.0f, p_ui);
			draw_game_over(p_ui);
			break;
	}

	return !quit_;
}

// --- lobby ------------------------------------------------------------------

void Game::draw_lobby(Ui &p_ui) {
	const Theme &th = p_ui.theme();
	p_ui.window_background();
	draw_window_chrome(p_ui, "Zilla — Лобби");

	const float top = th.title_bar_height + 16.0f;
	const float bottom = p_ui.height() - th.status_bar_height - 16.0f;

	// Left dock, the way the Godot editor lists scenes and nodes.
	const Rect dock(16.0f, top, 214.0f, bottom - top);
	p_ui.panel(dock);

	float y = dock.y + 12.0f;
	p_ui.label(Rect(dock.x + 12.0f, y, dock.w - 24.0f, 20.0f), "СЦЕНЫ",
			th.font_size_small, th.text_dim, Font::Align::Left, true);
	y += 26.0f;

	static const char *scenes[] = { "Лобби", "Арена" };
	for (int i = 0; i < 2; ++i) {
		const Rect item(dock.x + 8.0f, y, dock.w - 16.0f, 26.0f);
		if (i == 0) {
			p_ui.fill_rect(item, th.button_bg, 4.0f);
		}
		p_ui.circle(Vec2(item.x + 14.0f, item.center().y), 3.5f,
				i == 0 ? th.accent : th.text_disabled);
		p_ui.label(Rect(item.x + 26.0f, item.y, item.w - 30.0f, item.h), scenes[i],
				th.font_size_small, i == 0 ? th.text : th.text_dim);
		y += 30.0f;
	}

	y += 14.0f;
	p_ui.label(Rect(dock.x + 12.0f, y, dock.w - 24.0f, 20.0f), "УЗЛЫ",
			th.font_size_small, th.text_dim, Font::Align::Left, true);
	y += 26.0f;

	static const char *nodes[] = { "Игрок", "Враг × 3+", "Джойстик", "HUD" };
	for (int i = 0; i < 4; ++i) {
		const Rect item(dock.x + 8.0f, y, dock.w - 16.0f, 24.0f);
		p_ui.circle(Vec2(item.x + 14.0f, item.center().y), 3.0f, th.text_disabled);
		p_ui.label(Rect(item.x + 26.0f, item.y, item.w - 30.0f, item.h), nodes[i],
				th.font_size_small, th.text_dim);
		y += 26.0f;
	}

	// Main area.
	const Rect center(dock.right() + 16.0f, top, p_ui.width() - dock.right() - 32.0f, bottom - top);
	p_ui.panel(center);

	const float title_y = center.y + std::min(70.0f, center.h * 0.16f);
	p_ui.label(Rect(center.x, title_y, center.w, 62.0f), "ZILLA", 54.0f, th.text,
			Font::Align::Center, true);
	p_ui.label(Rect(center.x, title_y + 66.0f, center.w, 26.0f),
			"Арена на Vulkan с виртуальным джойстиком", th.font_size, th.text_dim,
			Font::Align::Center);

	const float button_width = std::min(280.0f, center.w - 80.0f);
	const float button_x = center.center().x - button_width * 0.5f;
	float button_y = title_y + 120.0f;

	const Rect play_rect(button_x, button_y, button_width, 46.0f);
	button_y += 58.0f;
	const Rect diff_rect(button_x, button_y, button_width, 40.0f);
	button_y += 50.0f;
	const Rect quit_rect(button_x, button_y, button_width, 40.0f);

	if (p_ui.button("lobby/play", play_rect, "Играть", true, true)) {
		start_new_run();
		screen_ = Screen::Playing;
	}
	if (p_ui.button("lobby/difficulty", diff_rect,
				std::string("Сложность: ") + difficulty_name(difficulty_))) {
		difficulty_ = (difficulty_ + 1) % 3;
	}
	if (p_ui.button("lobby/quit", quit_rect, "Выход")) {
		quit_ = true;
	}

	p_ui.label(Rect(center.x, center.bottom() - 84.0f, center.w, 24.0f),
			format("Лучший счёт: %d", best_score_), th.font_size, th.text_dim,
			Font::Align::Center);
	p_ui.label(Rect(center.x, center.bottom() - 58.0f, center.w, 22.0f),
			"Джойстик — движение • УДАР — атака • ESC — пауза",
			th.font_size_small, th.text_disabled, Font::Align::Center);

	p_ui.status_bar("Vulkan • " + device_name_, format("FPS: %.0f • Zilla 0.1", fps_));
}

// --- gameplay ---------------------------------------------------------------

void Game::draw_playing(float p_delta, Ui &p_ui) {
	const Theme &th = p_ui.theme();
	const Rect arena = arena_rect(p_ui);
	const InputState &input = p_ui.input();

	if (player_pos_.x < -500.0f) {
		player_pos_ = arena.center();
	}

	if (screen_ == Screen::Playing && input.is_pressed(Key::Escape)) {
		screen_ = Screen::Paused;
	}

	// --- simulation (uses the controls gathered during the previous frame) ---
	if (p_delta > 0.0f) {
		attack_cooldown_ = std::max(0.0f, attack_cooldown_ - p_delta);
		attack_flash_ = std::max(0.0f, attack_flash_ - p_delta);
		invulnerable_ = std::max(0.0f, invulnerable_ - p_delta);

		if (pending_attack_ && attack_cooldown_ <= 0.0f) {
			attack();
		}

		player_pos_ += move_input_ * PLAYER_SPEED * p_delta;
		player_pos_.x = clamp(player_pos_.x, arena.x + player_radius_, arena.right() - player_radius_);
		player_pos_.y = clamp(player_pos_.y, arena.y + player_radius_, arena.bottom() - player_radius_);

		spawn_timer_ -= p_delta;
		const int target_enemies = 2 + wave_;
		if (spawn_timer_ <= 0.0f && int(enemies_.size()) < target_enemies) {
			spawn_enemy(arena);
			spawn_timer_ = std::max(0.35f, 1.5f - wave_ * 0.09f);
		}

		update_enemies(p_delta, arena);
		update_particles(p_delta);

		if (player_hp_ <= 0.0f) {
			best_score_ = std::max(best_score_, score_);
			screen_ = Screen::GameOver;
		}
	}

	// --- drawing ---
	p_ui.window_background();
	draw_window_chrome(p_ui, "Zilla — Арена");
	draw_arena(p_ui, arena);

	p_ui.set_clip(arena.shrink(2.0f));
	draw_grid(p_ui, arena);

	for (const Particle &particle : particles_) {
		const float t = clamp(particle.life / particle.max_life, 0.0f, 1.0f);
		p_ui.circle(particle.pos, particle.radius * t, particle.color.with_alpha(t * 0.9f));
	}

	for (const Enemy &enemy : enemies_) {
		const float grow = 1.0f - enemy.spawn_anim * 0.65f;
		Color body = th.enemy_body;
		if (enemy.hit_flash > 0.0f) {
			body = body.lightened(0.45f);
		}
		p_ui.circle(enemy.pos, enemy.radius * grow, body.with_alpha(1.0f - enemy.spawn_anim * 0.5f),
				th.enemy_ring.with_alpha(0.85f), 2.0f);
		const Rect bar(enemy.pos.x - 19.0f, enemy.pos.y - enemy.radius - 11.0f, 38.0f, 4.0f);
		p_ui.progress_bar(bar, enemy.hp / enemy.max_hp, th.danger, th.health_bg, 2.0f);
	}

	// Player.
	Color player_body = th.player_body;
	if (invulnerable_ > 0.0f && std::fmod(time_, 0.14f) < 0.07f) {
		player_body = player_body.lightened(0.45f);
	}
	if (attack_flash_ > 0.0f) {
		const float t = attack_flash_ / ATTACK_FLASH_TIME;
		p_ui.circle(player_pos_, ATTACK_RANGE * (1.0f - 0.22f * t), Color(0.0f, 0.0f, 0.0f, 0.0f),
				th.attack_flash.with_alpha(0.15f + 0.55f * t), 2.0f + 4.0f * t);
	}
	p_ui.circle(player_pos_, player_radius_, player_body, th.player_ring, 2.5f);
	p_ui.circle(player_pos_, player_radius_ * 0.40f, th.player_ring.with_alpha(0.9f));
	p_ui.clear_clip();

	draw_hud(p_ui, arena);

	const Controls controls = draw_controls(p_ui, arena, p_delta);
	move_input_ = controls.move;
	pending_attack_ = controls.attack;

	p_ui.status_bar("Vulkan • " + device_name_,
			format("FPS: %.0f • Счёт: %d • Волна: %d", fps_, score_, wave_));
}

void Game::draw_arena(Ui &p_ui, const Rect &p_arena) {
	p_ui.bordered_rect(p_arena, p_ui.theme().sunken_bg, p_ui.theme().arena_border, 6.0f, 1.0f);
}

void Game::draw_grid(Ui &p_ui, const Rect &p_arena) {
	const Theme &th = p_ui.theme();
	for (float x = p_arena.x + 64.0f; x < p_arena.right(); x += 64.0f) {
		const bool major = int(std::round((x - p_arena.x) / 64.0f)) % 4 == 0;
		p_ui.fill_rect(Rect(std::round(x), p_arena.y + 1.0f, 1.0f, p_arena.h - 2.0f),
				major ? th.grid_major : th.grid_minor);
	}
	for (float y = p_arena.y + 64.0f; y < p_arena.bottom(); y += 64.0f) {
		const bool major = int(std::round((y - p_arena.y) / 64.0f)) % 4 == 0;
		p_ui.fill_rect(Rect(p_arena.x + 1.0f, std::round(y), p_arena.w - 2.0f, 1.0f),
				major ? th.grid_major : th.grid_minor);
	}
}

void Game::draw_hud(Ui &p_ui, const Rect &p_arena) {
	const Theme &th = p_ui.theme();

	const Rect left_panel(p_arena.x + 12.0f, p_arena.y + 12.0f, 246.0f, 62.0f);
	p_ui.panel(left_panel);
	p_ui.label(Rect(left_panel.x + 12.0f, left_panel.y + 8.0f, 120.0f, 18.0f), "Здоровье",
			th.font_size_small, th.text_dim);
	const Rect bar(left_panel.x + 12.0f, left_panel.y + 30.0f, left_panel.w - 24.0f, 14.0f);
	p_ui.progress_bar(bar, player_hp_ / player_max_hp_, th.health_fill, th.health_bg);
	p_ui.label(bar, format("%.0f / %.0f", player_hp_, player_max_hp_), th.font_size_small,
			th.text, Font::Align::Center);

	const Rect right_panel(p_arena.right() - 12.0f - 168.0f, p_arena.y + 12.0f, 168.0f, 62.0f);
	p_ui.panel(right_panel);
	p_ui.label(Rect(right_panel.x, right_panel.y + 8.0f, right_panel.w - 12.0f, 20.0f),
			format("Счёт: %d", score_), th.font_size, th.text, Font::Align::Right);
	p_ui.label(Rect(right_panel.x, right_panel.y + 32.0f, right_panel.w - 12.0f, 18.0f),
			format("Волна: %d • Врагов: %d", wave_, int(enemies_.size())),
			th.font_size_small, th.text_dim, Font::Align::Right);
}

Game::Controls Game::draw_controls(Ui &p_ui, const Rect &p_arena, float) {
	const Theme &th = p_ui.theme();
	const InputState &input = p_ui.input();
	Controls controls;

	// Thumbstick, bottom-left.
	const float stick_size = 152.0f;
	const Rect stick_rect(p_arena.x + 22.0f, p_arena.bottom() - 22.0f - stick_size,
			stick_size, stick_size);
	controls.move = p_ui.joystick("game/stick", stick_rect);
	p_ui.label(Rect(stick_rect.x - 20.0f, stick_rect.bottom() + 2.0f, stick_rect.w + 40.0f, 18.0f),
			"Джойстик", th.font_size_small, th.text_disabled, Font::Align::Center);

	// Keyboard fallback so the game is playable without touching the stick.
	Vec2 keys;
	if (input.is_down(Key::A)) {
		keys.x -= 1.0f;
	}
	if (input.is_down(Key::D)) {
		keys.x += 1.0f;
	}
	if (input.is_down(Key::W)) {
		keys.y -= 1.0f;
	}
	if (input.is_down(Key::S)) {
		keys.y += 1.0f;
	}
	if (keys.length_squared() > 0.0f) {
		controls.move = (controls.move + keys.normalized()).limited(1.0f);
	}

	// Round attack button, bottom-right.
	const float button_size = 118.0f;
	const Rect attack_rect(p_arena.right() - 22.0f - button_size,
			p_arena.bottom() - 22.0f - button_size, button_size, button_size);
	const bool ready = attack_cooldown_ <= 0.0f;
	const std::string label = ready ? "УДАР" : format("%.1f", attack_cooldown_);
	if (p_ui.button("game/attack", attack_rect, label, ready, ready)) {
		controls.attack = true;
	}
	if (input.is_down(Key::Space)) {
		controls.attack = true;
	}

	// Pause button, top-right of the arena.
	const Rect pause_rect(p_arena.right() - 12.0f - 34.0f, p_arena.bottom() - 12.0f - 34.0f, 34.0f, 34.0f);
	if (p_ui.button("game/pause", pause_rect, "")) {
		screen_ = Screen::Paused;
	}
	p_ui.fill_rect(Rect(pause_rect.center().x - 5.0f, pause_rect.center().y - 6.0f, 3.5f, 12.0f),
			th.button_text);
	p_ui.fill_rect(Rect(pause_rect.center().x + 1.5f, pause_rect.center().y - 6.0f, 3.5f, 12.0f),
			th.button_text);

	return controls;
}

// --- overlays ---------------------------------------------------------------

void Game::draw_dim(Ui &p_ui, float p_alpha) {
	p_ui.fill_rect(Rect(0.0f, 0.0f, p_ui.width(), p_ui.height()),
			Color(0.04f, 0.04f, 0.06f, p_alpha));
}

void Game::draw_paused(Ui &p_ui) {
	const Theme &th = p_ui.theme();
	draw_dim(p_ui, 0.6f);

	const Rect panel = Rect::from_center(Vec2(p_ui.width() * 0.5f, p_ui.height() * 0.5f),
			Vec2(380.0f, 220.0f));
	p_ui.panel(panel);
	p_ui.label(Rect(panel.x, panel.y + 24.0f, panel.w, 34.0f), "Пауза", 26.0f, th.text,
			Font::Align::Center, true);
	p_ui.label(Rect(panel.x, panel.y + 66.0f, panel.w, 22.0f),
			format("Счёт: %d • Волна: %d", score_, wave_), th.font_size_small, th.text_dim,
			Font::Align::Center);

	const float width = 220.0f;
	const float x = panel.center().x - width * 0.5f;
	if (p_ui.button("pause/resume", Rect(x, panel.y + 104.0f, width, 42.0f), "Продолжить", true, true)) {
		screen_ = Screen::Playing;
	}
	if (p_ui.button("pause/lobby", Rect(x, panel.y + 154.0f, width, 40.0f), "В лобби")) {
		screen_ = Screen::Lobby;
	}
	if (p_ui.input().is_pressed(Key::Escape)) {
		screen_ = Screen::Playing;
	}
}

void Game::draw_game_over(Ui &p_ui) {
	const Theme &th = p_ui.theme();
	draw_dim(p_ui, 0.62f);

	const Rect panel = Rect::from_center(Vec2(p_ui.width() * 0.5f, p_ui.height() * 0.5f),
			Vec2(420.0f, 268.0f));
	p_ui.panel(panel);
	p_ui.label(Rect(panel.x, panel.y + 24.0f, panel.w, 34.0f), "Игра окончена", 26.0f, th.text,
			Font::Align::Center, true);
	p_ui.label(Rect(panel.x, panel.y + 70.0f, panel.w, 28.0f), format("Счёт: %d", score_),
			th.font_size + 2.0f, th.accent, Font::Align::Center, true);
	p_ui.label(Rect(panel.x, panel.y + 104.0f, panel.w, 22.0f),
			format("Лучший: %d • Волна: %d", best_score_, wave_), th.font_size_small, th.text_dim,
			Font::Align::Center);

	const float width = 230.0f;
	const float x = panel.center().x - width * 0.5f;
	if (p_ui.button("over/retry", Rect(x, panel.y + 146.0f, width, 42.0f), "Ещё раз", true, true)) {
		start_new_run();
		screen_ = Screen::Playing;
	}
	if (p_ui.button("over/lobby", Rect(x, panel.y + 196.0f, width, 40.0f), "В лобби")) {
		screen_ = Screen::Lobby;
	}
}

void Game::draw_window_chrome(Ui &p_ui, const std::string &p_title) {
	p_ui.title_bar(p_title);
}

// --- simulation details -----------------------------------------------------

void Game::spawn_enemy(const Rect &p_arena) {
	Enemy enemy;
	enemy.radius = 14.0f + rng_.range(0.0f, 6.0f);
	enemy.max_hp = 46.0f + wave_ * 9.0f;
	enemy.hp = enemy.max_hp;
	enemy.speed = (58.0f + wave_ * 4.5f + rng_.range(-5.0f, 10.0f)) * DIFFICULTY_SPEED[difficulty_];
	enemy.damage = (8.0f + wave_ * 0.8f) * DIFFICULTY_DAMAGE[difficulty_];
	enemy.spawn_anim = 1.0f;

	const float inset = enemy.radius + 4.0f;
	switch (rng_.range_int(0, 3)) {
		case 0:
			enemy.pos = Vec2(rng_.range(p_arena.x + inset, p_arena.right() - inset), p_arena.y + inset);
			break;
		case 1:
			enemy.pos = Vec2(rng_.range(p_arena.x + inset, p_arena.right() - inset), p_arena.bottom() - inset);
			break;
		case 2:
			enemy.pos = Vec2(p_arena.x + inset, rng_.range(p_arena.y + inset, p_arena.bottom() - inset));
			break;
		default:
			enemy.pos = Vec2(p_arena.right() - inset, rng_.range(p_arena.y + inset, p_arena.bottom() - inset));
			break;
	}
	enemies_.push_back(enemy);
}

void Game::update_enemies(float p_delta, const Rect &p_arena) {
	for (Enemy &enemy : enemies_) {
		enemy.hit_flash = std::max(0.0f, enemy.hit_flash - p_delta);
		enemy.spawn_anim = std::max(0.0f, enemy.spawn_anim - p_delta / 0.25f);

		const Vec2 to_player = player_pos_ - enemy.pos;
		const float distance = to_player.length();
		const Vec2 direction = distance > 0.001f ? to_player / distance : Vec2();

		// Steering with damping: steady state velocity is `speed`, knockback fades out.
		enemy.vel += direction * (enemy.speed * 6.0f * p_delta);
		enemy.vel *= std::exp(-6.0f * p_delta);
		enemy.pos += enemy.vel * p_delta;

		enemy.pos.x = clamp(enemy.pos.x, p_arena.x + enemy.radius, p_arena.right() - enemy.radius);
		enemy.pos.y = clamp(enemy.pos.y, p_arena.y + enemy.radius, p_arena.bottom() - enemy.radius);

		if (distance < enemy.radius + player_radius_) {
			damage_player(enemy.damage);
			enemy.vel -= direction * 240.0f;
		}
	}
}

void Game::update_particles(float p_delta) {
	for (Particle &particle : particles_) {
		particle.life -= p_delta;
		particle.pos += particle.vel * p_delta;
		particle.vel *= std::exp(-4.5f * p_delta);
	}
	particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
							 [](const Particle &p) { return p.life <= 0.0f; }),
			particles_.end());
}

void Game::attack() {
	attack_cooldown_ = ATTACK_COOLDOWN;
	attack_flash_ = ATTACK_FLASH_TIME;

	bool any_hit = false;
	for (Enemy &enemy : enemies_) {
		if (enemy.pos.distance_to(player_pos_) > ATTACK_RANGE + enemy.radius) {
			continue;
		}
		enemy.hp -= ATTACK_DAMAGE;
		enemy.hit_flash = 0.18f;
		enemy.vel += (enemy.pos - player_pos_).normalized() * 260.0f;
		burst(enemy.pos, Color::hex(0xF0A0A0FF), 7, 190.0f);
		any_hit = true;
	}

	if (!any_hit) {
		burst(player_pos_ + Vec2(0.0f, -8.0f), Color::hex(0x9CC0F5FF), 4, 110.0f);
	}

	int killed = 0;
	for (auto it = enemies_.begin(); it != enemies_.end();) {
		if (it->hp <= 0.0f) {
			burst(it->pos, Color::hex(0xE36A6AFF), 14, 230.0f);
			it = enemies_.erase(it);
			++killed;
		} else {
			++it;
		}
	}
	if (killed > 0) {
		kills_ += killed;
		score_ += killed * 10 * wave_;
		wave_ = kills_ / 6 + 1;
	}
}

void Game::damage_player(float p_amount) {
	if (invulnerable_ > 0.0f) {
		return;
	}
	player_hp_ = std::max(0.0f, player_hp_ - p_amount);
	invulnerable_ = INVULNERABLE_TIME;
	burst(player_pos_, Color::hex(0xE36A6AFF), 10, 170.0f);
}

void Game::burst(const Vec2 &p_pos, const Color &p_color, int p_count, float p_speed) {
	for (int i = 0; i < p_count; ++i) {
		Particle particle;
		particle.pos = p_pos;
		const float angle = rng_.range(0.0f, 6.2831853f);
		const float speed = rng_.range(p_speed * 0.35f, p_speed);
		particle.vel = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);
		particle.max_life = rng_.range(0.25f, 0.55f);
		particle.life = particle.max_life;
		particle.radius = rng_.range(2.0f, 4.5f);
		particle.color = p_color;
		particles_.push_back(particle);
	}
}

} // namespace zilla
