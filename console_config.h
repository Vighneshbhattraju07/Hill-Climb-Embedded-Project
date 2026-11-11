#pragma once

#include <stdint.h>

// Raspberry Pi Pico 2 and the existing ST7789/button/buzzer wiring.
#define TFT_SPI spi0
#define TFT_PIN_SCK 18       // physical pin 24
#define TFT_PIN_MOSI 19      // physical pin 25
#define TFT_PIN_CS 17        // physical pin 22
#define TFT_PIN_DC 20        // physical pin 26
#define TFT_PIN_RESET 15     // GPIO 15, physical pin 20

#define BUTTON_PIN_UP 10     // physical pin 14
#define BUTTON_PIN_LEFT 11   // physical pin 15
#define BUTTON_PIN_DOWN 12   // physical pin 16
#define BUTTON_PIN_RIGHT 13  // physical pin 17
#define BUZZER_PIN 16        // GPIO 16, physical pin 21

#define SCREEN_WIDTH 320u
#define SCREEN_HEIGHT 240u
#define FRAMEBUFFER_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT * 2u)
#define DISPLAY_SPI_HZ 32000000u

#define GAME_HUD_HEIGHT 20u

#define COLOR_BACKGROUND 0x0841u
#define COLOR_PANEL 0x0000u
#define COLOR_TEXT 0xFFFFu
#define COLOR_ACCENT 0xFFE0u
#define COLOR_PLAYER_CAR 0x07E0u
#define COLOR_WARNING 0xF800u

#define INPUT_DEBOUNCE_US 25000u
#define INPUT_CHORD_GRACE_US 80000u
#define INPUT_CHORD_HOLD_US 500000u

