# A0 — First Contact: Toolchain, C, and the PicoBot

Reference implementation for the [A0 lab](https://oe-kvk-h2iot.github.io/courses/Architecture%20Of%20Embedded%20Systems/Labs/01-picobot-first-contact/).
The lab guide builds this program step by step; this folder is the known-good result, used for
lecturer bench checks and regression builds.

**Pico SDK only** — no helper library. The blink, buzzer (PWM), motors (H-bridge) and the command
failsafe are all written in `main.c`, which is the point of the lab: the course's `picobot` library
is the reference for later labs, not a dependency here.

## Build and flash

```bash
picolab build a0_first_contact        # from the courses repo root
picolab flash a0_first_contact --uf2  # probe-free over plain USB (the lab's first path)
picolab flash a0_first_contact        # ...or SWD with the Debug Probe
picolab monitor usb
```

## Safety

The program commands the motors. Run it with the **wheels up**, on the robot's own supply, and keep
the failsafe armed — the last section deliberately stops feeding it and proves the motors stop
about 500 ms later.
