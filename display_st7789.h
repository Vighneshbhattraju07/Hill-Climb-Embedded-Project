#pragma once

#include <stdint.h>
#include "game.h"

void display_init(void);
uint32_t display_actual_spi_hz(void);
void display_render(const GameState *game, uint32_t fps);
void display_present_dma(void);

