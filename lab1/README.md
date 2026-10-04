# Lab 1 — First Contact: Toolchain, C, and the PicoBot

Reference implementation for the [Lab 1 guide](https://oe-kvk-h2iot.github.io/courses/Architecture%20Of%20Embedded%20Systems/Labs/01-picobot-first-contact/).
The lab guide builds this program step by step; this folder is the known-good result, used for
lecturer bench checks and regression builds.

**Pico SDK only** — and split the way the lab teaches it:

| File | Role |
|---|---|
| `main.c` | the application: blink, banner, beep, ramp, trip |
| `picobot.h` | the driver **interface** (prototypes, pin map) |
| `picobot.c` | the driver **implementation** (PWM, H-bridge, failsafe) |

## Build and flash

```bash
picolab build lab1        # from the courses repo root
picolab flash lab1 --uf2  # probe-free over plain USB (the lab's first path)
picolab flash lab1        # ...or SWD with the Debug Probe
picolab monitor usb
```

## Safety

The program commands the motors. Run it with the **wheels up**, on the robot's own supply, and keep
the failsafe armed — the last section deliberately stops feeding it and proves the motors stop
about 500 ms later.
