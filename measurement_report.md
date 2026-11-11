# Hill Drive — Project Report

Hill Drive is a Raspberry Pi Pico 2 game with a 320×240 ST7789 SPI display.
The player drives over generated hills, controls the car’s pitch while
airborne, and tries to avoid hard landings and rollovers. Distance is scored
and the best score is saved in flash.

## Implementation

- RGB565 framebuffer, pixel-art car sprite, SPI display transfer using DMA.
- Dual-core design: input and physics on Core 0; rendering on Core 1.
- Four debounced buttons, with button combinations for pause and restart.
- PWM sound through a passive piezo on GP16 (physical pin 21).
- Pico SDK 2.3.0; built for Raspberry Pi Pico 2.

## Build measurements

Measured from the compiled firmware:

| Metric | Result |
|---|---:|
| ELF code (`.text`) | 37,900 bytes |
| Static RAM (`.bss`) | 157,372 bytes (29.56% of 520 KiB) |
| Framebuffer | 153,600 bytes (150 KiB) |
| Flash binary | 33,804 bytes |

At the requested 32 MHz SPI clock, sending one full framebuffer takes at
least 38.4 ms, giving a **calculated transfer-only ceiling of about 26 FPS**.
This is not a gameplay measurement.

## Physical measurements to complete

| Measurement | Result |
|---|---|
| Average gameplay FPS | Pending: read the on-screen counter while playing |
| Lowest gameplay FPS | Pending: observe during jumps and landings |
| Actual SPI clock | Pending: read while the game is running |
| USB voltage and current | Pending: measure with an inline USB meter |
| Energy for 10 minutes | Pending: voltage × current × 10/60 Wh |

The Pico was in BOOTSEL mode during report preparation, so live gameplay FPS
and power were not measured. The firmware includes an FPS counter.

## Rubric status

Framebuffer rendering, sprite, score display and storage, debounce logic,
dual-core rendering, DMA, and sound are implemented. The current build has
four physical buttons; the six-button baseline is not yet met. Enclosure,
GitHub publication, physical FPS and power measurements, and demo video remain
to be completed. See `docs/schematic.md` for wiring.
