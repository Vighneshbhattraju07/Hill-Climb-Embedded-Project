#include <math.h>
#include <stddef.h>
#include "console_config.h"
#include "display_st7789.h"
#include "hardware/dma.h"
#include "hardware/spi.h"

#define DISPLAY_SPI spi0

// RGB565 pixels are stored in wire byte order (high byte, then low byte).
// One 320x240 framebuffer occupies 153,600 bytes of Pico 2 SRAM.
static uint8_t framebuffer[FRAMEBUFFER_BYTES] __attribute__((aligned(4)));
static int dma_channel;
static uint32_t actual_spi_hz;

static const uint8_t digits_3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}
};

static const uint8_t letters_3x5[26][5] = {
    {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3},
    {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
    {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7},
    {1, 1, 1, 5, 2}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
    {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {7, 5, 5, 5, 7},
    {6, 5, 6, 4, 4}, {7, 5, 5, 7, 1}, {6, 5, 6, 5, 5},
    {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7},
    {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
    {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}
};

static void command_data(uint8_t command, const uint8_t *data, size_t size) {
    gpio_put(TFT_PIN_CS, 0);
    gpio_put(TFT_PIN_DC, 0);
    spi_write_blocking(DISPLAY_SPI, &command, 1);
    if (size != 0) {
        gpio_put(TFT_PIN_DC, 1);
        spi_write_blocking(DISPLAY_SPI, data, size);
    }
    gpio_put(TFT_PIN_CS, 1);
}

static void command(uint8_t value) {
    command_data(value, NULL, 0);
}

static void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x1 = x + w - 1;
    uint16_t y1 = y + h - 1;
    const uint8_t columns[] = {
        (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1
    };
    const uint8_t rows[] = {
        (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y1 >> 8), (uint8_t)y1
    };
    command_data(0x2A, columns, sizeof columns); // CASET
    command_data(0x2B, rows, sizeof rows);       // RASET
    command(0x2C);                               // RAMWR
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || w == 0 || h == 0) return;
    if ((uint32_t)x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if ((uint32_t)y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    for (uint16_t row = y; row < y + h; ++row) {
        size_t offset = ((size_t)row * SCREEN_WIDTH + x) * 2u;
        for (uint16_t col = 0; col < w; ++col) {
            framebuffer[offset++] = (uint8_t)(color >> 8);
            framebuffer[offset++] = (uint8_t)color;
        }
    }
}

static const uint8_t *glyph_for(char ch) {
    static const uint8_t colon[5] = {0, 2, 0, 2, 0};
    static const uint8_t dash[5] = {0, 0, 7, 0, 0};
    static const uint8_t plus[5] = {0, 2, 7, 2, 0};
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    if (ch >= '0' && ch <= '9') return digits_3x5[ch - '0'];
    if (ch >= 'A' && ch <= 'Z') return letters_3x5[ch - 'A'];
    if (ch == ':') return colon;
    if (ch == '-') return dash;
    if (ch == '+') return plus;
    return blank;
}

static void draw_text(uint16_t x, uint16_t y, const char *text,
                      uint16_t color, uint8_t scale) {
    while (*text != '\0') {
        const uint8_t *glyph = glyph_for(*text++);
        for (uint8_t row = 0; row < 5; ++row) {
            for (uint8_t col = 0; col < 3; ++col) {
                if (glyph[row] & (1u << (2u - col))) {
                    fill_rect(x + col * scale, y + row * scale,
                              scale, scale, color);
                }
            }
        }
        x += 4u * scale;
    }
}

static void draw_number(uint16_t x, uint16_t y, uint32_t value,
                        uint16_t color, uint8_t scale) {
    char reversed[10];
    uint8_t count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof reversed);
    while (count != 0) {
        char digit[2] = {reversed[--count], '\0'};
        draw_text(x, y, digit, color, scale);
        x += 4u * scale;
    }
}

static void draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < (int16_t)GAME_HUD_HEIGHT ||
        x >= (int16_t)SCREEN_WIDTH || y >= (int16_t)SCREEN_HEIGHT) return;
    size_t offset = ((size_t)y * SCREEN_WIDTH + (uint16_t)x) * 2u;
    framebuffer[offset] = (uint8_t)(color >> 8);
    framebuffer[offset + 1] = (uint8_t)color;
}

static void draw_disc(int16_t cx, int16_t cy, int16_t radius,
                      uint16_t color) {
    int16_t r2 = radius * radius;
    for (int16_t y = -radius; y <= radius; ++y) {
        for (int16_t x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= r2) draw_pixel(cx + x, cy + y, color);
        }
    }
}

static void draw_rotated_rect(float cx, float cy, float angle,
                              int16_t offset_x, int16_t offset_y,
                              int16_t width, int16_t height,
                              uint16_t color) {
    float sine = sinf(angle);
    float cosine = cosf(angle);
    for (int16_t y = 0; y < height; ++y) {
        for (int16_t x = 0; x < width; ++x) {
            float local_x = (float)offset_x + x - (float)width * 0.5f;
            float local_y = (float)offset_y + y - (float)height * 0.5f;
            int16_t screen_x = (int16_t)(cx + cosine * local_x - sine * local_y);
            int16_t screen_y = (int16_t)(cy + sine * local_x + cosine * local_y);
            draw_pixel(screen_x, screen_y, color);
        }
    }
}

static void draw_hill_terrain(const GameState *game) {
    fill_rect(0, GAME_HUD_HEIGHT, SCREEN_WIDTH,
              SCREEN_HEIGHT - GAME_HUD_HEIGHT, 0x2D7Fu);
    draw_disc(278, 48, 13, COLOR_ACCENT);

    for (int16_t x = 0; x < (int16_t)SCREEN_WIDTH; ++x) {
        float world_x = game->world_x + (float)x - 88.0f;
        int16_t surface_y = (int16_t)(game_terrain_y(world_x) - game->camera_y);
        if (surface_y < (int16_t)GAME_HUD_HEIGHT)
            surface_y = (int16_t)GAME_HUD_HEIGHT;
        if (surface_y >= (int16_t)SCREEN_HEIGHT) continue;
        fill_rect((uint16_t)x, (uint16_t)surface_y, 1,
                  SCREEN_HEIGHT - (uint16_t)surface_y, 0x8A22u);
        fill_rect((uint16_t)x, (uint16_t)surface_y, 1, 4, 0x46A0u);
    }
}

static void draw_hill_car(const GameState *game) {
    float cx = 88.0f;
    float cy = game->y - game->camera_y;
    float sine = sinf(game->angle);
    float cosine = cosf(game->angle);

    draw_rotated_rect(cx, cy, game->angle, 0, 19, 48, 5, 0x1082u);
    for (int8_t side = -1; side <= 1; side += 2) {
        float local_x = (float)side * 19.0f;
        int16_t wx = (int16_t)(cx + cosine * local_x - sine * 11.0f);
        int16_t wy = (int16_t)(cy + sine * local_x + cosine * 11.0f);
        draw_disc(wx, wy, 7, COLOR_PANEL);
        draw_disc(wx, wy, 3, 0xBDF7u);
    }

    // 2x transparent pixel-art sprite, rotated with the simulated chassis.
    // The wide chassis, raised cabin, and two visible wheels read clearly at
    // the small TFT size while retaining the same car physics and controls.
    // Palette: G body, Y cabin, B glass, W lamps, R rear lamp.
    static const char sprite[11][25] = {
        "........................",
        "........YYYYYYYY........",
        "......YYYYBBBBYYYY......",
        ".....YYYBBBBBBYYY.......",
        "....YYYYYYYYYYYYYY......",
        "..GGGGGGGGGGGGGGGGGGGG..",
        ".GGGGGGGGGGGGGGGGGGGGGG.",
        "GGGGGGGGGGGGGGGGGGGGGGGG",
        "GGGGGGGGGGGGGGGGGGGGGGGG",
        ".RR.GGGGGGGGGGGGGGGGGG.W.",
        "........................"
    };
    for (int16_t sy = 0; sy < 11; ++sy) {
        for (int16_t sx = 0; sx < 24; ++sx) {
            uint16_t color;
            switch (sprite[sy][sx]) {
                case 'G': color = COLOR_PLAYER_CAR; break;
                case 'Y': color = 0xFFE0u; break;
                case 'B': color = 0x7DFFu; break;
                case 'W': color = COLOR_TEXT; break;
                case 'R': color = COLOR_WARNING; break;
                default: continue;
            }
            float lx = ((float)sx - 12.0f) * 2.0f;
            float ly = ((float)sy - 5.5f) * 2.0f;
            for (int8_t py = 0; py < 2; ++py) {
                for (int8_t px = 0; px < 2; ++px) {
                    float local_x = lx + px;
                    float local_y = ly + py;
                    draw_pixel((int16_t)(cx + cosine * local_x - sine * local_y),
                               (int16_t)(cy + sine * local_x + cosine * local_y),
                               color);
                }
            }
        }
    }

    float nose_x = cx + cosine * 25.0f - sine * -2.0f;
    float nose_y = cy + sine * 25.0f + cosine * -2.0f;
    draw_disc((int16_t)nose_x, (int16_t)nose_y, 2, COLOR_TEXT);
}

void display_init(void) {
    actual_spi_hz = spi_init(DISPLAY_SPI, DISPLAY_SPI_HZ);
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(TFT_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(TFT_PIN_CS);
    gpio_set_dir(TFT_PIN_CS, GPIO_OUT);
    gpio_put(TFT_PIN_CS, 1);
    gpio_init(TFT_PIN_DC);
    gpio_set_dir(TFT_PIN_DC, GPIO_OUT);
    gpio_put(TFT_PIN_DC, 1);
    gpio_init(TFT_PIN_RESET);
    gpio_set_dir(TFT_PIN_RESET, GPIO_OUT);
    gpio_put(TFT_PIN_RESET, 1);

    gpio_put(TFT_PIN_RESET, 0);
    sleep_ms(20);
    gpio_put(TFT_PIN_RESET, 1);
    sleep_ms(150);
    command(0x01); // SWRESET
    sleep_ms(150);
    command(0x11); // SLPOUT
    sleep_ms(120);
    const uint8_t rgb565[] = {0x55};
    command_data(0x3A, rgb565, sizeof rgb565); // COLMOD
    const uint8_t landscape_180[] = {0xA0};
    command_data(0x36, landscape_180, sizeof landscape_180); // MADCTL
    command(0x20); // INVOFF, matching the working pink test configuration
    command(0x13); // NORON
    sleep_ms(10);
    command(0x29); // DISPON
    sleep_ms(100);

    dma_channel = dma_claim_unused_channel(true);
}

uint32_t display_actual_spi_hz(void) {
    return actual_spi_hz;
}

void display_render(const GameState *game, uint32_t fps) {
    draw_hill_terrain(game);
    draw_hill_car(game);

    uint32_t speed_kmh = game->velocity_x > 0.0f
                             ? (uint32_t)(game->velocity_x * 0.36f) : 0u;
    fill_rect(0, 0, SCREEN_WIDTH, GAME_HUD_HEIGHT, COLOR_PANEL);
    draw_text(2, 5, "DIST", COLOR_TEXT, 2);
    draw_number(36, 5, game->score, COLOR_TEXT, 2);
    draw_text(90, 5, "SPD", COLOR_ACCENT, 2);
    draw_number(116, 5, speed_kmh, COLOR_ACCENT, 2);
    draw_text(157, 5, "BEST", COLOR_TEXT, 2);
    draw_number(193, 5, game->best_score, COLOR_TEXT, 2);
    draw_text(254, 5, "FPS", COLOR_ACCENT, 2);
    draw_number(282, 5, fps, COLOR_ACCENT, 2);

    if (game->status == GAME_PAUSED) {
        fill_rect(54, 88, 212, 66, COLOR_PANEL);
        draw_text(100, 96, "PAUSED", COLOR_ACCENT, 3);
        draw_text(91, 128, "HOLD UD RESUME", COLOR_TEXT, 1);
        draw_text(116, 144, "SPI", COLOR_TEXT, 1);
        draw_number(136, 144, display_actual_spi_hz() / 1000000u,
                    COLOR_ACCENT, 1);
        draw_text(153, 144, "MHZ", COLOR_TEXT, 1);
    } else if (game->status == GAME_OVER) {
        fill_rect(44, 91, 232, 62, COLOR_PANEL);
        draw_text(92, 98, "WRECK", COLOR_WARNING, 3);
        draw_text(94, 130, "HOLD LR RESTART", COLOR_TEXT, 1);
    }
}

void display_present_dma(void) {
    set_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gpio_put(TFT_PIN_CS, 0);
    gpio_put(TFT_PIN_DC, 1);

    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, spi_get_dreq(DISPLAY_SPI, true));
    dma_channel_configure(dma_channel, &config,
                          &spi_get_hw(DISPLAY_SPI)->dr,
                          framebuffer, FRAMEBUFFER_BYTES, true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    while (spi_is_busy(DISPLAY_SPI)) tight_loop_contents();
    gpio_put(TFT_PIN_CS, 1);
}
