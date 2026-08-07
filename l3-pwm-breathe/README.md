# L3 — Breathing LED, built piece by piece

`breathe.c` is the *destination*: a two-LED, gamma-corrected breathing demo. But the point of
this example isn't the finished code — it's the four small steps that get you there, and the
**one nuance each step forces you to notice**. Build a rung, run it, *measure it*, understand
*why* it's not right yet, then add the next piece. (Start by cutting `breathe.c` down to Rung 1.)

This is the pattern for every example in the course: don't paste the finished thing — build it
small, measure, and dig into the nuance before you move on.

## Rung 1 — a plain linear fade (one LED, GP16)
The simplest thing that could work: ramp the PWM duty 0 → max → 0 in equal steps.
```c
for (uint16_t d = 0;     d < 65535; d += 256) { pwm_set_gpio_level(16, d); sleep_us(100); }
for (uint16_t d = 65535; d > 0;     d -= 256) { pwm_set_gpio_level(16, d); sleep_us(100); }
```
- **Build, run, watch** → it *snaps* bright early and lingers near full; the low end flies past.
- **Measure** → at what duty % does the LED first look "on"? (Far below 50%.)
- **Nuance** → PWM duty is *electrical on-time*, not *perceived brightness*. They are not the same thing.

## Rung 2 — shape the ramp with a sine
Replace the constant step with a sine ease-in/ease-out (soft at the turnarounds):
```c
float b = (sinf(theta) + 1.0f) * 0.5f;   // theta: -pi/2..+pi/2  ->  b: 0..1
pwm_set_gpio_level(16, (uint16_t)(b * 65535));
```
- **Build, watch** → the *turnarounds* are smoother, but the brightness still crushes at the bottom.
- **Nuance** → shaping *when* the duty changes ≠ fixing *how bright it looks*. Two different problems.

## Rung 3 — gamma correction (the real fix)
Pre-distort: send `duty = brightness^gamma` (γ ≈ 2.4) so the *perceived* fade is even.
```c
float duty = powf(b, 2.4f);
pwm_set_gpio_level(16, (uint16_t)(duty * 65535));
```
- **Build, watch** → *now* it breathes evenly — no early snap, no lingering.
- **Measure** → print raw `b` vs corrected `duty`; see how much of the range lives in the bottom.
- **Nuance** → the eye is non-linear (γ ≈ 2.2–2.8; blue LEDs ~2.6). Gamma spends more PWM resolution where you can actually see it.
- **Find out** → set γ = 1.0 (no correction) and γ = 3.0. Which looks right? Why?

## Rung 4 — measure it on the bench
Put a scope / logic analyzer on GP16 and watch the duty cycle sweep. Confirm the table you
*computed* is the signal you actually *get*. This is the "trust the bench, not the model" step.

## Rung 5 — extend (pick one)
- **Two LEDs anti-phase** — drive GP17 with `table[N-1-i]` so it fades opposite GP16. (This is what `breathe.c` does.)
- **Compile-time table** — generate the 256-entry table offline (a small Python script) and paste it as a
  `static const uint16_t[]`. Zero runtime `sinf`/`powf`, ~512 B of flash, deterministic — and you can drop
  `m` from `target_link_libraries`. **Nuance:** precompute-vs-compute is *the* embedded trade-off.

---
Each rung = one build, one observation, one measured fact, one nuance. Reach `breathe.c` by earning it.
