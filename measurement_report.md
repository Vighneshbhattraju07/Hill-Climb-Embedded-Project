# Hill Drive project report and measurement worksheet

**Hardware:** Raspberry Pi Pico 2, existing 320×240 ST7789 SPI TFT, four
normally-open tactile switches, passive piezo, USB power.

**Status:** The updated source builds for Pico 2. The build uses a 320x240
framebuffer and includes the GP16 piezo output. Physical gameplay, FPS, and
USB power have not been measured. Record these after flashing; do not present
estimates as measured results.

## Design summary

Hill Drive is a side-view driving game over generated rolling terrain. The
player uses GP13 to accelerate, GP11 to brake and reverse, and GP10/GP12 to
pitch the car in the air. Two spring-damped wheels respond to terrain height;
gravity pulls the car down while airborne, and slope force changes horizontal
acceleration. Hard landings and rollovers end the run. Distance is scored in
meters, with a persistent best-distance record. UP+DOWN pauses/resumes and
LEFT+RIGHT restarts.

Core 0 samples/debounces the held controls and advances physics at a 1 ms loop
rate. It publishes the latest state to a one-entry queue. Core 1 draws the
RGB565 framebuffer and transfers it to the ST7789 using SPI TX DMA. Old queued
states are replaced so input is not blocked by display refresh. The displayed
FPS is measured from completed render-and-transfer frames on core 1.

The car is a 2x pixel-art sprite rotated with its physics angle.

Distance milestones and crashes use PWM tones on GP16 (physical pin 21). The design is USB
powered and does not use a battery.

## Memory and throughput estimates

- One 320×240 RGB565 framebuffer: `320 × 240 × 2 = 153,600 bytes` (150 KiB),
  about 29% of the Pico 2's 520 KiB SRAM. This is the largest static RAM item.
- Game state and the latest render snapshot are copied through a one-entry
  queue between the two cores.
- Read exact static RAM and flash usage from `build/pico_hill_drive.map` or
  the ELF size report after the SDK build.
- The SPI clock is requested at 32 MHz. The pause overlay shows the actual
  configured clock; the raw full-frame transfer ceiling is
  `actual_SPI_bits_per_second ÷ (153,600 × 8)`.
- The on-screen FPS counter includes drawing and DMA transfer, so it will be
  below the raw SPI ceiling.

## Measurements to complete on the physical console

| Measurement | Result | Method |
|---|---|---|
| Build / SDK version | Pending | Record the local Pico SDK version and build result |
| Actual SPI clock | Pending | Pause and read the SPI MHz overlay |
| Average gameplay FPS | Pending | Read the on-screen FPS after 30 seconds of driving |
| Lowest observed FPS | Pending | Observe the counter during jumps and landing |
| ELF flash usage | Pending | `arm-none-eabi-size build/pico_hill_drive.elf` |
| Static RAM usage | Pending | ELF size or linker map; include framebuffer |
| USB voltage/current | Not measured | Requires an existing inline USB meter |
| Energy for 10 minutes | Pending | `Wh = average_voltage × average_current × 10/60` |
| Control bounce/missed input | Pending | 100 presses for each control and 20 chord presses |

The setup has no USB current sensor or charging/measurement IC, so software
cannot produce an honest current or energy reading.

## Baseline and scope limits

- The baseline asks for six physical buttons. This build has four; pause and
  restart are button chords rather than independent switches.
- A handheld enclosure is not present in the available hardware.
- Sleep-on-close needs a lid sensor or close switch; neither is present.
- Multiconsole play needs a second console and communications link.
- Sound and DMA are implemented; physical sound quality and high-speed SPI
  signal integrity still need bench verification.

## Demo video checklist

Record boot, acceleration, braking/reversing, pitching the car in the air,
hill landings, the distance/FPS display, pause/resume with UP+DOWN,
restart with LEFT+RIGHT, and the saved best distance after a power cycle.
Report measured FPS, memory totals, and power readings only after recording
them above.
