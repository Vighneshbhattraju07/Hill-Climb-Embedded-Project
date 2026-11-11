#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "console_config.h"
#include "input.h"

typedef enum {
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER
} GameStatus;

typedef enum {
    GAME_EVENT_NONE,
    GAME_EVENT_MILESTONE,
    GAME_EVENT_CRASH
} GameEvent;

typedef struct {
    float world_x;
    float y;
    float velocity_x;
    float velocity_y;
    float angle;
    float angular_velocity;
    float max_distance;
    float camera_y;
    uint32_t score;
    uint32_t best_score;
    uint32_t last_update_ms;
    GameStatus status;
    bool grounded;
    bool score_dirty;
} GameState;

void game_init(GameState *game, uint32_t saved_best, uint32_t random_seed);
GameEvent game_update(GameState *game, const InputFrame *input,
                      uint32_t now_ms);
float game_terrain_y(float world_x);
