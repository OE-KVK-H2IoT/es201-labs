# Lab 1 — First Contact: Toolchain, C, and the PicoBot

Reference implementation for the [Lab 1 guide](https://oe-kvk-h2iot.github.io/courses/Architecture%20Of%20Embedded%20Systems/Labs/01-picobot-first-contact/).
The lab guide builds this program step by step; this folder is the known-good result, used for
lecturer bench checks and regression builds.

**Pico SDK only** — and split the way the lab teaches it:

| File | Role |
|---|---|
| `main.c` | the application: banner, sensor loop |
| `picobot.h` | the driver **interface** (line-sensor API, count) |
| `picobot.c` | the driver **implementation** (GPIO setup, read, line decision) |

The line sensors are four digital inputs on GP2–GP5 behind a voltage comparator — plain GPIO, no
ADC. This robot reads 0 on black (the line); verify the polarity and the left→right order on your
own unit.

## Build and flash

```bash
picolab build lab1        # from the courses repo root
picolab flash lab1 --uf2  # probe-free over plain USB (the lab's first path)
picolab flash lab1        # ...or SWD with the Debug Probe
picolab monitor usb
```
