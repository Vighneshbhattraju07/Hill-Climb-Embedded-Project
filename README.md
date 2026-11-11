# Pico 2 Hill Drive

A self-contained Raspberry Pi Pico 2 driving game for the existing ST7789 SPI
TFT, four tactile switches, and passive piezo. Drive across generated hills,
build speed downhill, control the car in the air, and try to land on its
wheels. The project keeps the RGB565 framebuffer, SPI DMA, dual-core renderer,
FPS counter, debounced controls, PWM sound, and persistent distance record.

## Build and flash

Set `PICO_SDK_PATH` to your installed `pico-sdk` folder and make sure CMake,
GNU Make, and the ARM GCC toolchain are available. From this `spi_display`
folder, run:

```text
make
```

Flash `build/pico_hill_drive.uf2` to the Pico 2. CMake also writes
`build/pico_hill_drive.map` for the memory report.

## Controls

| Control | Button wiring / action |
|---|---|
| Up | GP10, physical pin 14; pitch the nose up while airborne |
| Left | GP11, physical pin 15; brake, then reverse |
| Down | GP12, physical pin 16; pitch the nose down while airborne |
| Right | GP13, physical pin 17; accelerate |
| Pause/resume | Hold Up + Down together for 0.5 seconds |
| Restart | Hold Left + Right together for 0.5 seconds |

Each switch connects its GPIO to common GND when pressed. Internal pull-ups are
enabled, so no button resistors are needed. Pause and restart are button
chords because the current setup has four physical switches.

## Display, controls, and buzzer wiring

| Device signal | Pico 2 GPIO | Physical pin |
|---|---:|---:|
| Up / pitch up | GP10 | 14 |
| Left / brake | GP11 | 15 |
| Down / pitch down | GP12 | 16 |
| Right / accelerator | GP13 | 17 |
| Common ground | GND | 18 or 38 |
| Passive piezo signal/+ | GP16 | 21 |
| TFT RESET | GP15 | 20 |
| TFT CS | GP17 | 22 |
| TFT SCK | GP18 | 24 |
| TFT MOSI / SDI | GP19 | 25 |
| TFT DC | GP20 | 26 |
| TFT VCC and LED/BL | 3V3(OUT) | 36 |
| TFT GND and piezo − | GND | 38 |

See [docs/schematic.md](docs/schematic.md) for the connection diagram. TFT
RESET connects to GP15 (physical pin 20).

## Driving and physics

Hold Right to apply engine force; release it and the car coasts. Left brakes
while moving forward and reverses once the car slows down. Hills affect the
car's acceleration. The chassis has damped wheels, gravity pulls it down in
the air, and Up/Down apply stronger pitch torque while airborne to help level
the car for landing. Hard landings and rollovers end the run; falling below
the visible terrain height alone does not. The terrain is generated from
smooth, seeded hills.
Distance is the score, and the best distance survives power-off.

## Implementation layout

- `main.c` runs the physics/input loop on core 0 and the renderer on core 1.
- `src/game.c` generates terrain and simulates throttle, slope force, gravity,
  suspension, pitch, landing, and distance scoring.
- `src/input.c` debounces controls and recognizes pause/restart chords.
- `src/display_st7789.c` draws the terrain and larger rotated pixel-art car
  sprite into the framebuffer and transfers it using SPI TX DMA.
- `src/sound.c` plays distance milestone and crash tones using PWM.
- `src/score_store.c` stores a checksummed high distance in a reserved flash
  sector.
