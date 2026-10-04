/*
 * a0_first_contact.c — A0: first PicoBot program, Pico SDK only (reference implementation).
 *
 *   1. blinks the onboard LED (which lives on the CYW43 wireless chip)
 *   2. plays a short tone on the buzzer (PWM we configure ourselves)
 *   3. ramps the motors on the wheels-up stand, under our own failsafe
 *   4. stops feeding and lets the failsafe prove itself
 *
 * Board: PicoBot (RP2350 / Pico 2 W). SAFETY: run with the wheels clear of the
 * desk, on the robot's own supply, and keep the failsafe armed.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

/* ---- PicoBot pin map (verify against your unit) ------------------------ */
#define BUZZER_PIN   22u
#define MOTOR_L_FWD  13u
#define MOTOR_L_REV  12u
#define MOTOR_R_FWD  10u
#define MOTOR_R_REV  11u
#define MOTOR_PWM_HZ 1000u

/* ---- buzzer: a PWM square wave ----------------------------------------- */
static void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(slice, 64.0f);              /* 150 MHz / 64 = 2.34375 MHz */
    pwm_set_enabled(slice, false);
}

static void buzzer_tone(uint32_t freq_hz) {
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint32_t wrap = 2343750u / freq_hz - 1u;   /* period in counter ticks */
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(BUZZER_PIN, wrap / 2u); /* 50% duty */
    pwm_set_enabled(slice, true);
}

static void buzzer_off(void) {
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

static void motors_init(void) {
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

static void motors_set(int16_t left, int16_t right) {
    if (left > 255) left = 255;
    if (left < -255) left = -255;
    if (right > 255) right = 255;
    if (right < -255) right = -255;
    motor_drive(MOTOR_L_FWD, MOTOR_L_REV, left);
    motor_drive(MOTOR_R_FWD, MOTOR_R_REV, right);
}

/* ---- failsafe: no feed for 500 ms -> motors stop ----------------------- */
static uint32_t fs_last_ms;

static void failsafe_arm(void) {
    fs_last_ms = to_ms_since_boot(get_absolute_time());
}

static void failsafe_feed(void) {
    fs_last_ms = to_ms_since_boot(get_absolute_time());
}

static uint32_t failsafe_age_ms(void) {
    return to_ms_since_boot(get_absolute_time()) - fs_last_ms;
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);                 /* let USB-CDC attach before the first print */

    if (cyw43_arch_init()) {        /* the LED lives on the wireless chip */
        printf("cyw43 init failed\n");
        return 1;
    }

    printf("\n=== PicoBot A0 first contact ===\n");
    printf("clock: %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    /* 1. blink the onboard LED (twice, as a hello) */
    for (int i = 0; i < 2; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(250);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(250);
    }
    printf("blink: done\n");

    /* 2. buzzer */
    buzzer_init();
    buzzer_tone(440);               /* A4 */
    sleep_ms(300);
    buzzer_tone(523);               /* C5 */
    sleep_ms(300);
    buzzer_off();
    printf("buzzer: done\n");

    /* 3. motors, under our failsafe */
    motors_init();
    failsafe_arm();

    for (int i = 0; i < 5; i++) {
        int16_t speed = (int16_t)(60 + i * 30);
        motors_set(speed, speed);
        failsafe_feed();
        printf("motors: %d (age %u ms)\n", speed, (unsigned)failsafe_age_ms());
        sleep_ms(400);
    }

    /* 4. stop feeding: the failsafe must trip by itself */
    while (failsafe_age_ms() <= 500u) {
        printf("waiting for failsafe... age %u ms\n", (unsigned)failsafe_age_ms());
        sleep_ms(200);
    }
    motors_set(0, 0);
    printf("failsafe tripped: motors stopped\n");

    for (;;) {
        tight_loop_contents();
    }
}
