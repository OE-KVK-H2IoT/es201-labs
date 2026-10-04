/*
 * picobot.c — the PicoBot driver implementation (our own, SDK only).
 *
 * Buzzer: a PWM square wave at the requested frequency, 50% duty.
 * Motors: one H-bridge per side, two control pins each — drive one with PWM,
 * hold the other low; swap them to reverse.
 * Failsafe: a timestamp the application must "feed"; if it goes stale, stop.
 */
#include "picobot.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

/* ---- buzzer: a PWM square wave ----------------------------------------- */
void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(slice, 64.0f);              /* 150 MHz / 64 = 2.34375 MHz */
    pwm_set_enabled(slice, false);
}

void buzzer_tone(uint32_t freq_hz) {
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint32_t wrap = 2343750u / freq_hz - 1u;   /* period in counter ticks */
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(BUZZER_PIN, wrap / 2u); /* 50% duty */
    pwm_set_enabled(slice, true);
}

void buzzer_off(void) {
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER_PIN), false);
}

/* ---- motors: one H-bridge per side, two pins each ---------------------- */
static void motor_pin_low(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

static void motor_drive(uint pin_fwd, uint pin_rev, int16_t speed) {
    uint slice = pwm_gpio_to_slice_num(pin_fwd);
    uint ch_fwd = pwm_gpio_to_channel(pin_fwd);
    uint ch_rev = pwm_gpio_to_channel(pin_rev);

    if (speed == 0) {
        motor_pin_low(pin_fwd);
        motor_pin_low(pin_rev);
        pwm_set_chan_level(slice, ch_fwd, 0);
        pwm_set_chan_level(slice, ch_rev, 0);
        return;
    }

    uint16_t duty = (uint16_t)((uint32_t)(speed < 0 ? -speed : speed) * 65535u / 255u);

    if (speed > 0) {                 /* forward: PWM on A, B held low */
        gpio_set_function(pin_fwd, GPIO_FUNC_PWM);
        motor_pin_low(pin_rev);
        pwm_set_chan_level(slice, ch_fwd, duty);
        pwm_set_chan_level(slice, ch_rev, 0);
    } else {                         /* backward: PWM on B, A held low */
        gpio_set_function(pin_rev, GPIO_FUNC_PWM);
        motor_pin_low(pin_fwd);
        pwm_set_chan_level(slice, ch_rev, duty);
        pwm_set_chan_level(slice, ch_fwd, 0);
    }
    pwm_set_enabled(slice, true);
}

void motors_init(void) {
    uint slices[2] = {
        pwm_gpio_to_slice_num(MOTOR_L_FWD),
        pwm_gpio_to_slice_num(MOTOR_R_FWD),
    };
    for (int i = 0; i < 2; i++) {
        pwm_set_wrap(slices[i], 65535u);
        /* wrap 65535 -> freq = clk / (clkdiv * 65536) */
        pwm_set_clkdiv(slices[i],
                       (float)clock_get_hz(clk_sys) / ((float)MOTOR_PWM_HZ * 65536.0f));
        pwm_set_enabled(slices[i], true);
    }
    motor_drive(MOTOR_L_FWD, MOTOR_L_REV, 0);
    motor_drive(MOTOR_R_FWD, MOTOR_R_REV, 0);
}

void motors_set(int16_t left, int16_t right) {
    if (left > 255) left = 255;
    if (left < -255) left = -255;
    if (right > 255) right = 255;
    if (right < -255) right = -255;
    motor_drive(MOTOR_L_FWD, MOTOR_L_REV, left);
    motor_drive(MOTOR_R_FWD, MOTOR_R_REV, right);
}

/* ---- failsafe: no feed for 500 ms -> motors stop ----------------------- */
static uint32_t fs_last_ms;

void failsafe_arm(void) {
    fs_last_ms = to_ms_since_boot(get_absolute_time());
}

void failsafe_feed(void) {
    fs_last_ms = to_ms_since_boot(get_absolute_time());
}

uint32_t failsafe_age_ms(void) {
    return to_ms_since_boot(get_absolute_time()) - fs_last_ms;
}
