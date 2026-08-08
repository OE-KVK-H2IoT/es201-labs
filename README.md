# ES201 — Architecture of Embedded Systems: lab demo sources

Buildable demo programs for each lab of the **Architecture of Embedded Systems** (MSc) course, RP2350 / Raspberry Pi Pico 2 W. Two jobs:

1. **Instructor test harness** — flash each demo to a real board to verify the hardware and the lab steps before teaching them.
2. **Single source of truth** — the course docs transclude code from here (via MkDocs snippets), so a lab's code lives in exactly one place that actually compiles.

> Environment setup (toolchain, SDK, OpenOCD, udev) is documented once in the course docs:
> *Architecture of Embedded Systems → [Environment Setup (Linux)](../../docs/Architecture%20Of%20Embedded%20Systems/setup.md)*. Do that first.

## Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk    # the SDK, with submodules initialised
cd src/es201
cmake -B build                            # configure once (defaults to PICO_BOARD=pico2)
cmake --build build                       # build every lab demo
cmake --build build --target l3_pwm_tone  # ...or just one
```

Each demo produces a `.uf2` (drag-and-drop flashing) and `.elf` (SWD/debugger) under `build/<lab>/`.

## `run.sh` — build, flash, monitor in one command

```bash
./run.sh list                 # show buildable targets
./run.sh build                # build everything (or: build l3_pwm_tone)
./run.sh flash l3_pwm_tone    # build that target + flash it over SWD (Debug Probe)
./run.sh monitor              # open the USB-serial console (/dev/ttyACM0)
./run.sh l3_pwm_tone          # shorthand: flash, then monitor
```

`flash` uses OpenOCD's one-shot `program … verify reset exit` (no GDB needed) and
auto-finds the Pi-fork OpenOCD scripts under `~/.pico-sdk` if `OPENOCD_SCRIPTS`
isn't set. Remember the Pico needs its **own USB power** in addition to the probe.

**Serial:** programs print to *both* the Pico's USB-CDC **and** UART0 (GP0/GP1 →
the Debug Probe's UART bridge). `run.sh monitor` prefers the **probe UART bridge**
because USB-CDC can be flaky on some RP2350 setups. The probe's UART pins must be
wired to the Pico's GP0/GP1. Override the port with `SERIAL=/dev/ttyACMx ./run.sh monitor`.

Manual SWD flash (equivalent, for debugging):
```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000"
# second terminal:
arm-none-eabi-gdb build/l0-getting-started/l0_blink.elf
(gdb) target extended-remote :3333
(gdb) load
(gdb) monitor reset init
(gdb) continue
```

## Labs

| Dir | Lab | Demos | Status |
|---|---|---|---|
| `l0-getting-started/` | L0 | `l0_blink`, `l0_buttons` | ✅ builds & runs |
| `l1-gpio-timers-interrupts/` | L1 | `l1_timer_blink` | ✅ builds (verify on board) |
| `l2-adc-characterization/` | L2 | `l2_adc_read` | ✅ joystick X/Y raw readout + live range |
| `l3-pwm-signal-generation/` | L3 | `l3_pwm_tone` | ✅ builds (verify on board) |
| `l4-display-dma/` | L4 | `l4_display`, `l4_touch`, `l4_optimize`, `l4_joybox`, `l4_touchbox` | ✅ panel self-test + GT911 touch paint + full vs dirty vs double-buffer fps demo |
| `l5-freertos-tasks/` | L5 | — | 🚧 needs FreeRTOS-Kernel |
| `l6-capstone-mini-scope/` | L6 | — | 🚧 integration (ADC + DMA + display) |

The 🚧 labs have folder READMEs noting what they need; they're commented out in the top-level `CMakeLists.txt` until wired. Uncomment as each is built and tested.

## Carrier pin map (52Pi EP-0172)

| Peripheral | Pin |
|---|---|
| User LEDs | GP16, GP17 |
| Buttons K1/K2 (active-low) | GP14, GP15 |
| RGB LED (WS2812) | GP12 |
| Buzzer (passive) | GP13 |
| Joystick X/Y | GP26/ADC0, GP27/ADC1 |

## License

MIT — see [LICENSE](LICENSE). The course documentation this code accompanies is CC BY 4.0 (see the courses repo).
