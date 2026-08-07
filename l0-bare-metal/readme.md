# Lab 0 — Bare-Metal Blink (no SDK, no CMake)

Three files, an LED, and nothing else — the floor under the whole course. This
directory is **deliberately not part of the CMake build**: you compile it by hand
so that nothing is hidden. Full walkthrough: the course tutorial
*[Lab 0 — From Scratch](../../../docs/Architecture%20Of%20Embedded%20Systems/Tutorials/00-bare-metal-from-scratch.md)*.

Files:

- `blink.c` — the whole program: direct writes to GPIO registers (carrier LED, GP16)
- `startup.s` — vector table (initial SP + reset vector) **+ the RP2350 boot block**
- `pico2.ld` — linker script: where the bytes go in flash

Build and flash by hand (toolchain + OpenOCD from `~/.pico-sdk`; see the Debug /
ELF Cheat Sheet for the PATH setup):

```bash
arm-none-eabi-as  -mcpu=cortex-m33 startup.s -o startup.o
arm-none-eabi-gcc -c -Os -std=c11 -ffreestanding -nostartfiles -mcpu=cortex-m33 blink.c -o blink.o
arm-none-eabi-gcc -Os -ffreestanding -nostartfiles -nostdlib -mcpu=cortex-m33 -T pico2.ld startup.o blink.o -o blink.elf
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "program blink.elf verify reset exit"
```

The whole image is ~84 bytes. Read it: `arm-none-eabi-objdump -d blink.elf`.

Adapted for the EP-0172 carrier (LED on GP16) from the from-scratch example at
<https://github.com/OE-KVK-H2IoT/pico2> (`0_bare_blinking_LED`).
