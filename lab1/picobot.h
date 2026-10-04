/* picobot.h — the PicoBot hardware map and driver interface (our own). */
#ifndef PICOBOT_H
#define PICOBOT_H

#include <stdint.h>

/* ---- pin map (verify against your unit) -------------------------------- */
#define BUZZER_PIN   22u
#define MOTOR_L_FWD  13u
#define MOTOR_L_REV  12u
#define MOTOR_R_FWD  10u
#define MOTOR_R_REV  11u
#define MOTOR_PWM_HZ 1000u

/* ---- what main.c may call ---------------------------------------------- */
void buzzer_init(void);
void buzzer_tone(uint32_t freq_hz);
void buzzer_off(void);

void motors_init(void);
void motors_set(int16_t left, int16_t right);

void failsafe_arm(void);
void failsafe_feed(void);
uint32_t failsafe_age_ms(void);

#endif /* PICOBOT_H */
