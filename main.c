#include <stdbool.h>
#include <stdint.h>
#include "display_st7789.h"
#include "game.h"
#include "input.h"
#include "score_store.h"
#include "sound.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define SIMULATION_PERIOD_MS 1u
#define RENDER_PERIOD_MS 33u

static GameState game;
static GameState discarded_snapshot;
static queue_t render_queue;
static semaphore_t renderer_ready;

// Core 1 owns the framebuffer and display SPI/DMA. Core 0 can continue
// sampling buttons and advancing the game while a frame is being transferred.
static void display_worker(void) {
    flash_safe_execute_core_init();
    sem_release(&renderer_ready);

    GameState snapshot;
    uint32_t fps = 0;
    uint32_t frames = 0;
    uint32_t window_start = time_us_32();
    while (true) {
        queue_remove_blocking(&render_queue, &snapshot);
        display_render(&snapshot, fps);
        display_present_dma();

        ++frames;
        uint32_t now = time_us_32();
        uint32_t elapsed = now - window_start;
        if (elapsed >= 1000000u) {
            fps = (uint32_t)(((uint64_t)frames * 1000000u) / elapsed);
            frames = 0;
            window_start = now;
        }
    }
}

static void publish_latest_state(void) {
    if (!queue_try_add(&render_queue, &game)) {
        // Keep the freshest game state instead of building display latency.
        (void)queue_try_remove(&render_queue, &discarded_snapshot);
        (void)queue_try_add(&render_queue, &game);
    }
}

int main(void) {
    display_init();
    sound_init();
    input_init();

    game_init(&game, score_store_load(), time_us_32() ^ 0x92A76B31u);
    queue_init(&render_queue, sizeof(GameState), 1);
    sem_init(&renderer_ready, 0, 1);
    multicore_launch_core1(display_worker);
    sem_acquire_blocking(&renderer_ready);

    uint32_t last_render_publish = 0;
    bool score_save_attempted = false;
    publish_latest_state();

    while (true) {
        uint32_t now_ms = time_us_32() / 1000u;
        InputFrame input;
        input_poll(&input);
        if (input.restart && game.score_dirty) {
            (void)score_store_save(game.best_score);
            game.score_dirty = false;
        }
        GameEvent event = game_update(&game, &input, now_ms);
        sound_play_event(event);
        sound_update();

        if (game.status == GAME_OVER && game.score_dirty &&
            !score_save_attempted) {
            (void)score_store_save(game.best_score);
            game.score_dirty = false;
            score_save_attempted = true;
        }
        if (input.restart) score_save_attempted = false;

        uint32_t now_us = time_us_32();
        if ((uint32_t)(now_us - last_render_publish) >=
            RENDER_PERIOD_MS * 1000u) {
            publish_latest_state();
            last_render_publish = now_us;
        }

        sleep_ms(SIMULATION_PERIOD_MS);
    }
}
