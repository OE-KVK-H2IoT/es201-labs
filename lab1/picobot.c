/*
 * picobot.c — the PicoBot line-sensor driver implementation (our own, SDK only).
 *
 * Each channel is an IR emitter + phototransistor pair followed by a voltage
 * comparator: the comparator hands the RP2350 a clean digital level, so the
 * sensor is plain GPIO — no ADC needed. This robot reads 0 on black (line).
 */
#include "picobot.h"

#include "pico/stdlib.h"

#define LINE_LEFT   2u
#define LINE_MIDL   3u
#define LINE_MIDR   4u
#define LINE_RIGHT  5u

static const uint LINE_PINS[LINE_SENSOR_COUNT] = { LINE_LEFT, LINE_MIDL, LINE_MIDR, LINE_RIGHT };

void line_sensors_init(void) {
    for (int i = 0; i < LINE_SENSOR_COUNT; i++) {
        gpio_init(LINE_PINS[i]);
        gpio_set_dir(LINE_PINS[i], GPIO_IN);
    }
}

void line_sensors_read(uint8_t values[LINE_SENSOR_COUNT]) {
    for (int i = 0; i < LINE_SENSOR_COUNT; i++) {
        values[i] = (uint8_t)gpio_get(LINE_PINS[i]);
    }
}

bool line_detected(void) {
    for (int i = 0; i < LINE_SENSOR_COUNT; i++) {
        if (!gpio_get(LINE_PINS[i])) {   /* 0 = black */
            return true;
        }
    }
    return false;
}
