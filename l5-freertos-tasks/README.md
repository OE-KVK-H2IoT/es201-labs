# L5 — FreeRTOS Tasks (preemptive multitasking on two cores)

Two standalone targets that back the [L5 tutorial](../../../docs/Architecture%20Of%20Embedded%20Systems/Tutorials/06-freertos-tasks.md):

| Target | Source | What it is |
|---|---|---|
| `l5_rtos_hello` | `main_hello.c` | **The fall-back reference.** Two tasks, two priorities, `vTaskDelay` vs `vTaskDelayUntil`, all pinned to core 0. Self-contained (kernel only). Flash this first to prove the toolchain. |
| `l5_rtos_scope` | `main_scope.c` + `scope_display.c` | **The mini-scope as a system:** `task_sampling → queue → task_display → ST7796`, with a button ISR handing a trigger over a binary semaphore. Reuses the L4 `st7796` driver. This is the *complete/working* version — the tutorial hands students the same file with four `// TODO` gaps; here each is filled and marked `[solves TODO n]`. |

## Get the kernel (once)

FreeRTOS is **not** part of the Pico SDK — it is source you build in. Either add it as a
submodule beside `lib/lvgl`:

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git ../lib/FreeRTOS-Kernel
```

…or point the build at a checkout you already have:

```bash
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
```

If neither is present the CMake configure prints a warning and **skips** these targets
(so `../run.sh build-all` still succeeds for the rest of the labs).

## Build & flash

```bash
../run.sh l5_rtos_hello        # build, flash over SWD, open the serial monitor
../run.sh build l5_rtos_scope  # build only
# or the standard standalone path:
mkdir build && cd build && cmake .. -G Ninja && make
```

Board is `pico2_w`. `l5_rtos_scope` wants the EP-0172 ST7796 panel (SPI0) and a signal on
ADC0/GP26; `l5_rtos_hello` needs nothing but the board and USB serial.

## Notes for the RP2350 SMP port (things that bite)

The `FreeRTOSConfig.h` here is complete for the **RP2350 ARM non-TrustZone (NTZ) SMP port**.
Three groups of macros are easy to miss and each is *mandatory* for this port:

- `configENABLE_FPU / MPU / TRUSTZONE` — the ARMv8-M port `#error`s without them.
- `configMAX_SYSCALL_INTERRUPT_PRIORITY` — required, and it defines the ISR-priority ceiling
  for `FromISR` calls.
- `configUSE_TIMERS` + `INCLUDE_xTimerPendFunctionCall` — the port's Pico-sync interop wakes
  tasks via an event group set *FromISR*, which only exists when the timer daemon is enabled
  (otherwise you get `undefined reference to xEventGroupSetBitsFromISR` at link time).

The kernel port path (`portable/ThirdParty/GCC/RP2350_ARM_NTZ/…`) has moved between releases;
the CMake here probes the known layouts. If it can't find the import file, run
`find <kernel> -name FreeRTOS_Kernel_import.cmake`.

See the [tutorial](../../../docs/Architecture%20Of%20Embedded%20Systems/Tutorials/06-freertos-tasks.md)
for the lab tasks (queue sizing, forced race, priority inversion, SMP) and the
[RTOS Fundamentals](../../../docs/Architecture%20Of%20Embedded%20Systems/Reference/rtos-fundamentals.md)
reference for the theory.
