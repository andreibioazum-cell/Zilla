// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// The whole game: a Godot-styled lobby, an arena, a thumbstick you drag with
// the mouse or a finger, and enemies that chase you.
#ifndef ZILLA_GAME_GAME_H
#define ZILLA_GAME_GAME_H

#include "core/types.h"
#include "platform/input.h"
#include "ui/ui.h"

#include <string>
#include <vector>

namespace zilla {

enum class Screen {
	Lobby,
	Playing,
	Paused,
	GameOver,
};

class Game {
public:
	Game();

	void set_device_name(const std::string &p_name) { device_name_ = p_name; }

	// Jumps straight into a run (what the "Играть" button does, also used by the
	// offline preview renderer).
	void start_new_run();
	void set_fps(float p_fps) { fps_ = p_fps; }

	Screen screen() const { return screen_; }

	// Advances the simulation and draws one frame. Returns false when the user
	// asked to close the application.
	bool frame(float p_delta, Ui &p_ui);

	// Difficulty names shown in the lobby.
	static const char *difficulty_name(int p_index);

private:
	struct Enemy {
		Vec2 pos;
		Vec2 vel;
		float radius = 15.0f;
		float hp = 60.0f;
		float max_hp = 60.0f;
		float speed = 70.0f;
		float damage = 9.0f;
		float hit_flash = 0.0f;
		float spawn_anim = 1.0f;
	};

	// What the on-screen controls produced during this frame.
	struct Controls {
		Vec2 move;
		bool attack = false;
	};

	struct Particle {
		Vec2 pos;
		Vec2 vel;
		float life = 0.0f;
		float max_life = 1.0f;
		float radius = 3.0f;
		Color color;
	};

	Rect arena_rect(const Ui &p_ui) const;
	Rect hud_rect(const Ui &p_ui) const;

	void draw_lobby(Ui &p_ui);
	void draw_playing(float p_delta, Ui &p_ui);
	void draw_paused(Ui &p_ui);
	void draw_game_over(Ui &p_ui);

	void draw_arena(Ui &p_ui, const Rect &p_arena);
	void draw_grid(Ui &p_ui, const Rect &p_arena);
	void draw_hud(Ui &p_ui, const Rect &p_arena);
	Controls draw_controls(Ui &p_ui, const Rect &p_arena, float p_delta);
	void draw_dim(Ui &p_ui, float p_alpha);
	void draw_window_chrome(Ui &p_ui, const std::string &p_title);

	void spawn_enemy(const Rect &p_arena);
	void update_enemies(float p_delta, const Rect &p_arena);
	void update_particles(float p_delta);
	void attack();
	void damage_player(float p_amount);
	void burst(const Vec2 &p_pos, const Color &p_color, int p_count, float p_speed);

	Screen screen_ = Screen::Lobby;
	int difficulty_ = 1;

	Vec2 player_pos_;
	Vec2 player_vel_;
	float player_hp_ = 100.0f;
	float player_max_hp_ = 100.0f;
	float player_radius_ = 18.0f;
	float attack_cooldown_ = 0.0f;
	float attack_flash_ = 0.0f;
	float invulnerable_ = 0.0f;

	int score_ = 0;
	int best_score_ = 0;
	int kills_ = 0;
	int wave_ = 1;
	float spawn_timer_ = 0.0f;
	float time_ = 0.0f;

	std::vector<Enemy> enemies_;
	std::vector<Particle> particles_;
	Rng rng_;

	Vec2 move_input_;
	bool pending_attack_ = false;
	bool quit_ = false;

	std::string device_name_ = "Vulkan";
	float fps_ = 0.0f;
};

} // namespace zilla

#endif // ZILLA_GAME_GAME_H
