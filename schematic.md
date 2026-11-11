# Wiring schematic

This wiring uses the Raspberry Pi Pico 2, ST7789 SPI TFT, four normally-open
tactile switches, and one passive piezo for Hill Drive. GP16 means GPIO 16,
which is physical header pin 21. Physical pin 16 is GP12 (the Down button).
Display orientation is 320×240 landscape, rotated 180° in software.

## Connection table

| Pico 2 pin | Connect to | Notes |
|---|---|---|
| 14 / GP10 | Up button | Internal pull-up; switch to GND |
| 15 / GP11 | Left button | Internal pull-up; switch to GND |
| 16 / GP12 | Down button | Internal pull-up; switch to GND |
| 17 / GP13 | Right button | Internal pull-up; switch to GND |
| 18 / GND | Common ground rail | Shared by buttons, piezo, and TFT |
| 21 / GP16 | Piezo + / signal | PWM sound output |
| 20 / GP15 | TFT RESET | Controller reset |
| 22 / GP17 | TFT CS | SPI chip select |
| 24 / GP18 | TFT SCK | SPI clock |
| 25 / GP19 | TFT SDI / MOSI | SPI data from Pico to TFT |
| 26 / GP20 | TFT DC | Data/command select |
| 36 / 3V3(OUT) | TFT VCC and LED/BL | Existing display power wiring |
| 38 / GND | TFT GND | Common ground |
| 38 / GND | Piezo − / other lead | Passive piezo return |

## Simplified diagram

```text
Pico 2                                  ST7789 TFT
GP18 (pin 24) ------------------------- SCK
GP19 (pin 25) ------------------------- SDI / MOSI
GP17 (pin 22) ------------------------- CS
GP20 (pin 26) ------------------------- DC
GP15 (pin 20) ------------------------- RESET
3V3  (pin 36) ------------------------- VCC, LED/BL
GND  (pin 38) ------------------------- GND

Pico 2                                  Four momentary buttons
GP10 (pin 14) ----[ UP switch ]----+
GP11 (pin 15) ----[ LEFT switch ]--+
GP12 (pin 16) ----[ DOWN switch ]--+---- GND (pin 18 or 38)
GP13 (pin 17) ----[ RIGHT switch ]-+

Pico 2                                  Passive piezo
GP16 (pin 21) ------------------------- + / signal
GND  (pin 18 or 38) ------------------- − / other lead
```

For each four-leg tactile switch, its two legs on one electrical side are
already joined. Put the GPIO on one switch side and common GND on the other
side; do not use two legs from the same internally connected pair as the two
connections. All switch grounds can share the same ground rail.

## Controls with four switches

Hold GP13/RIGHT to accelerate. GP11/LEFT brakes, then reverses when the car
slows down. Hold GP10/UP or GP12/DOWN to pitch the car while airborne. Holding
UP+DOWN for about half a second toggles pause; holding LEFT+RIGHT for about
half a second restarts the run. The input module filters contact bounce and
keeps these button chords available while reporting held controls.
