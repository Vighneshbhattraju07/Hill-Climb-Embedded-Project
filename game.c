#include <math.h>
#include <string.h>
#include "game.h"

#define TERRAIN_SAMPLE_SPACING 96.0f
#define TERRAIN_BASE_Y 190.0f
#define PLAYER_SCREEN_X 88.0f
#define WHEELBASE 38.0f
#define WHEEL_HALFBASE (WHEELBASE * 0.5f)
#define WHEEL_LOCAL_Y 11.0f
#define WHEEL_RADIUS 7.0f
#define GRAVITY 520.0f
#define ENGINE_ACCEL 190.0f
#define BRAKE_ACCEL 430.0f
#define REVERSE_ACCEL 100.0f
#define MAX_FORWARD_SPEED 240.0f
#define MAX_REVERSE_SPEED 70.0f
#define SPRING_STIFFNESS 90.0f
#define SUSPENSION_DAMPING 25.0f
#define LANDING_CRASH_SPEED 390.0f

static uint32_t terrain_seed;

static float terrain_node_height(int32_t index) {
    uint32_t value = (uint32_t)index + terrain_seed + 0x9E3779B9u;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return (float)((int32_t)(value % 57u) - 28);
}

float game_terrain_y(float world_x) {
    float position = world_x / TERRAIN_SAMPLE_SPACING;
    int32_t index = (int32_t)position;
    if (position < (float)index) --index;
    float t = position - (float)index;
    // Smooth interpolation makes a continuous sequence of rounded hill crests.
    t = t * t * (3.0f - 2.0f * t);
    float h0 = terrain_node_height(index);
    float h1 = terrain_node_height(index + 1);
    float hill_height = h0 + (h1 - h0) * t;
    return TERRAIN_BASE_Y - hill_height;
}

static void update_best_score(GameState *game) {
    if (game->score > game->best_score) {
        game->best_score = game->score;
        game->score_dirty = true;
    }
}

static void start_round(GameState *game) {
    uint32_t best = game->best_score;
    memset(game, 0, sizeof *game);
    game->best_score = best;
    game->world_x = 0.0f;
    float rear_ground = game_terrain_y(-WHEEL_HALFBASE);
    float front_ground = game_terrain_y(WHEEL_HALFBASE);
    game->y = (rear_ground + front_ground) * 0.5f -
              WHEEL_LOCAL_Y - WHEEL_RADIUS;
    game->angle = atanf((front_ground - rear_ground) / WHEELBASE);
    game->camera_y = game->y - 145.0f;
    game->status = GAME_PLAYING;
}

void game_init(GameState *game, uint32_t saved_best, uint32_t random_seed) {
    memset(game, 0, sizeof *game);
    game->best_score = saved_best;
    terrain_seed = random_seed ? random_seed : 0x1F123BB5u;
    start_round(game);
}

static void crash(GameState *game) {
    game->status = GAME_OVER;
}

GameEvent game_update(GameState *game, const InputFrame *input,
                      uint32_t now_ms) {
    if (input->restart) {
        start_round(game);
        game->last_update_ms = now_ms;
        return GAME_EVENT_NONE;
    }

    if (input->pause_toggle &&
        (game->status == GAME_PLAYING || game->status == GAME_PAUSED)) {
        game->status = game->status == GAME_PLAYING ? GAME_PAUSED : GAME_PLAYING;
        game->last_update_ms = now_ms;
        return GAME_EVENT_NONE;
    }
    if (game->status != GAME_PLAYING) {
        game->last_update_ms = now_ms;
        return GAME_EVENT_NONE;
    }

    if (game->last_update_ms == 0) {
        game->last_update_ms = now_ms;
        return GAME_EVENT_NONE;
    }
    uint32_t elapsed_ms = now_ms - game->last_update_ms;
    game->last_update_ms = now_ms;
    if (elapsed_ms == 0) return GAME_EVENT_NONE;
    if (elapsed_ms > 25u) elapsed_ms = 25u;
    float dt = (float)elapsed_ms * 0.001f;

    float slope = (game_terrain_y(game->world_x + 8.0f) -
                   game_terrain_y(game->world_x - 8.0f)) / 16.0f;
    float acceleration_x =
        GRAVITY * slope / (1.0f + slope * slope) * 0.55f -
        game->velocity_x * 0.22f;
    if (input->right) acceleration_x += ENGINE_ACCEL;
    if (input->left) {
        acceleration_x -= game->velocity_x > 4.0f
                              ? BRAKE_ACCEL : REVERSE_ACCEL;
    }
    game->velocity_x += acceleration_x * dt;
    if (game->velocity_x > MAX_FORWARD_SPEED)
        game->velocity_x = MAX_FORWARD_SPEED;
    if (game->velocity_x < -MAX_REVERSE_SPEED)
        game->velocity_x = -MAX_REVERSE_SPEED;
    game->world_x += game->velocity_x * dt;
    if (game->world_x < 0.0f) {
        game->world_x = 0.0f;
        if (game->velocity_x < 0.0f) game->velocity_x = 0.0f;
    }

    float sin_angle = sinf(game->angle);
    float cos_angle = cosf(game->angle);
    float acceleration_y = GRAVITY;
    float acceleration_angle = -2.1f * game->angular_velocity;
    if (!game->grounded) {
        if (input->up) acceleration_angle -= 9.0f;
        if (input->down) acceleration_angle += 9.0f;
    }

    bool touching_ground = false;
    float old_velocity_y = game->velocity_y;
    const float wheel_dx[2] = {-WHEEL_HALFBASE, WHEEL_HALFBASE};
    for (uint8_t i = 0; i < 2; ++i) {
        float dx = wheel_dx[i];
        float wheel_x = game->world_x + cos_angle * dx;
        float wheel_center_y = game->y + sin_angle * dx +
                               cos_angle * WHEEL_LOCAL_Y;
        float wheel_bottom = wheel_center_y + WHEEL_RADIUS;
        float penetration = wheel_bottom - game_terrain_y(wheel_x);
        if (penetration <= 0.0f) continue;

        touching_ground = true;
        float wheel_velocity_y = game->velocity_y +
                                 game->angular_velocity * cos_angle * dx;
        float spring_accel = -penetration * SPRING_STIFFNESS -
                             fmaxf(wheel_velocity_y, 0.0f) * SUSPENSION_DAMPING;
        if (spring_accel < -2400.0f) spring_accel = -2400.0f;
        acceleration_y += spring_accel * 0.5f;
        acceleration_angle += dx * spring_accel / 820.0f;
    }

    if (touching_ground && !game->grounded &&
        old_velocity_y > LANDING_CRASH_SPEED) {
        crash(game);
        return GAME_EVENT_CRASH;
    }
    game->grounded = touching_ground;
    game->velocity_y += acceleration_y * dt;
    game->y += game->velocity_y * dt;
    game->angular_velocity += acceleration_angle * dt;
    if (game->angular_velocity > 3.5f) game->angular_velocity = 3.5f;
    if (game->angular_velocity < -3.5f) game->angular_velocity = -3.5f;
    game->angle += game->angular_velocity * dt;

    if (touching_ground && fabsf(game->angle) > 1.55f) {
        crash(game);
        return GAME_EVENT_CRASH;
    }
    if (fabsf(game->angle) > 2.45f) {
        crash(game);
        return GAME_EVENT_CRASH;
    }

    // Camera easing follows jumps without letting the car leave the screen.
    float target_camera_y = game->y - 145.0f;
    float camera_blend = dt * 3.0f;
    if (camera_blend > 1.0f) camera_blend = 1.0f;
    game->camera_y += (target_camera_y - game->camera_y) * camera_blend;

    float distance = game->world_x > game->max_distance
                         ? game->world_x : game->max_distance;
    uint32_t old_score = game->score;
    game->max_distance = distance;
    game->score = (uint32_t)(distance / 10.0f);
    update_best_score(game);
    return game->score / 100u > old_score / 100u
               ? GAME_EVENT_MILESTONE : GAME_EVENT_NONE;
}
