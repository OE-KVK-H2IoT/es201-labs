# Board Context — Pico 2 W on the EP-0172 carrier

*Hardware facts the tutor must respect. A small model left to guess will invent pins and
SDK calls; this file is what keeps its answers on *this* board. Loaded alongside the
[contract](contract.md) on every surface.*

## Board

- MCU: **RP2350** (Cortex-M33), on a **Raspberry Pi Pico 2 W**.
- Carrier: **EP-0172**. The labs use the carrier's peripherals, not a bare Pico.

## Pins that matter (and the traps)

- **User LEDs: GP16 / GP17** (EP-0172). Use these for any "blink an LED" task.
- **The onboard LED is NOT usable as a plain GPIO.** On the Pico 2 W it hangs off the
  **CYW43 wireless chip** — so both hard-coding `25` *and* using `PICO_DEFAULT_LED_PIN`
  fail. Driving it needs `cyw43_arch_init()` then
  `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, …)`. For a simple blink, prefer GP16/GP17.
- **Do NOT use GP23 / GP24 / GP25 / GP29 as GPIO** on the Pico 2 W — they are tied to the
  wireless chip.
- Display (L4/L5): ST7796 over SPI — SCK=GP2, MOSI=GP3, CS=GP5, DC=GP6, RST=GP7.
  Touch (GT911) on I2C0. ADC: GP26=ADC0, GP27=ADC1, GP28=ADC2 (see L2).

## SDK conventions

- Prefer official headers: `pico/stdlib.h`, `hardware/gpio.h`, `hardware/timer.h`,
  `hardware/irq.h`, `hardware/adc.h`, `hardware/pwm.h`, `hardware/spi.h`,
  `hardware/i2c.h`, `hardware/dma.h`.
- Use `GPIO_OUT` / `GPIO_IN` for direction — not magic `0`/`1`.
- For non-blocking behaviour suggest `repeating_timer`, `absolute_time_t` deadlines, IRQs,
  DMA, or a state machine — never a busy-wait.

## What this model is known to get wrong (so it should hedge)

Small local models are good at C syntax and **bad at this board**. Expect — and warn about —
wrong LED pin (`25` / `PICO_DEFAULT_LED_PIN`), non-existent SDK functions or wrong
signatures, a busy-wait called "non-blocking," and occasional Arduino/PIC answers. **None
of these survive a measurement** — which is exactly why the contract ends every answer with
a measure-next step. Trust the bench, not the model.
