// L3 (PWM, variant) — a "breathing" LED: PWM brightness ramped up and down on a
// smooth sine curve, with GAMMA CORRECTION so the fade looks linear to your eye.
//
// Two ideas beyond the buzzer-tone demo:
//   * PWM as a *brightness* knob, not a frequency: pwm_set_gpio_level() sets the duty
//     cycle (0..wrap), and the LED's average brightness follows the duty cycle.
//   * The eye is NON-linear: perceived brightness ≈ (duty)^(1/gamma). So a linear PWM
//     ramp looks like it "jumps on" early and lingers near full. We pre-distort the
//     table by duty = brightness^gamma (~2.4) so the *perceived* fade is smooth.
//
// GP16 and GP17 breathe in ANTI-PHASE (one fades up while the other fades down) — easy
// to see both channels working. Put a scope on GP16 to watch the duty cycle change.
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define LED_PIN1          16      // user LED 1 on the EP-0172 carrier
#define LED_PIN2          17      // user LED 2 — driven in anti-phase
#define RESOLUTION        256     // steps in a half-breath (fade up or fade down)
#define BREATH_PERIOD_MS  2000    // one full up+down breath
#define LED_GAMMA         2.4f    // 2.0 softer … 2.8 stronger (blue LEDs like ~2.6)

// Precomputed once at startup: a sine ramp (0..1) run through gamma correction and
// scaled to the 16-bit PWM range. (You could instead generate this table offline with a
// script and paste it as a `static const` array — zero runtime math, ~512 B of flash.)
static uint16_t brightness_table[RESOLUTION];

static void build_brightness_table(void) {
    for (int i = 0; i < RESOLUTION; ++i) {
        // theta sweeps -pi/2 .. +pi/2, so sin(theta) sweeps -1 .. +1 -> map to 0 .. 1.
        float theta  = ((float)i / (RESOLUTION - 1)) * (float)M_PI - (float)M_PI_2;
        float linear = (sinf(theta) + 1.0f) * 0.5f;      // perceived brightness we want
        float duty   = powf(linear, LED_GAMMA);          // pre-distort for the eye
        brightness_table[i] = (uint16_t)(duty * 65535.0f);
    }
}

int main(void) {
    stdio_init_all();
    build_brightness_table();

    // Hand both pins to the PWM block, then configure their slice for a full 16-bit
    // range (wrap = 65535). pwm_gpio_to_slice_num() is the correct way to find a pin's
    // slice — never compute it as pin/2.
    gpio_set_function(LED_PIN1, GPIO_FUNC_PWM);
    gpio_set_function(LED_PIN2, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 65535);
    pwm_init(pwm_gpio_to_slice_num(LED_PIN1), &cfg, true);
    pwm_init(pwm_gpio_to_slice_num(LED_PIN2), &cfg, true);

    const uint32_t step_delay_us = (BREATH_PERIOD_MS * 1000u) / (2u * RESOLUTION);
    printf("\nL3 breathe — gamma-corrected PWM breathing LED on GP%d/GP%d (anti-phase).\n"
           "Try LED_GAMMA=1.0 (no correction) and watch the fade look uneven.\n",
           LED_PIN1, LED_PIN2);

    while (true) {
        for (int i = 0; i < RESOLUTION; ++i) {           // GP16 fades up, GP17 fades down
            pwm_set_gpio_level(LED_PIN1, brightness_table[i]);
            pwm_set_gpio_level(LED_PIN2, brightness_table[RESOLUTION - 1 - i]);
            sleep_us(step_delay_us);
        }
        for (int i = RESOLUTION - 1; i >= 0; --i) {       // and back
            pwm_set_gpio_level(LED_PIN1, brightness_table[i]);
            pwm_set_gpio_level(LED_PIN2, brightness_table[RESOLUTION - 1 - i]);
            sleep_us(step_delay_us);
        }
    }
}
