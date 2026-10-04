# A0 — First Contact: Toolchain, C, and the PicoBot

Reference implementation for the [A0 lab](https://oe-kvk-h2iot.github.io/courses/Architecture%20Of%20Embedded%20Systems/Labs/01-picobot-first-contact/).
The lab guide builds this program step by step; this folder is the known-good result, used for
lecturer bench checks and regression builds.

## Build and flash

```bash
picolab build a0_first_contact     # from the courses repo root
picolab flash a0_first_contact     # SWD via the Debug Probe
picolab flash a0_first_contact --uf2   # or probe-free over plain USB
picolab monitor
```

The helper library comes from the sibling submodule `src/picobot/c`. If your checkout differs:

```bash
picolab build a0_first_contact PICOBOT_DIR=/path/to/picobot/c
```

## Safety

The program commands the motors. Run it with the **wheels up**, on the robot's own supply, and
keep the failsafe armed — the last section deliberately stops feeding it and proves the motors
stop about 500 ms later.
