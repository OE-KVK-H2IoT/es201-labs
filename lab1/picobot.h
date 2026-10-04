/* picobot.h — the PicoBot line-sensor driver interface (our own). */
#ifndef PICOBOT_H
#define PICOBOT_H

#include <stdint.h>
#include <stdbool.h>

#define LINE_SENSOR_COUNT 4

/* Read all four sensors, left to right. 0 = black (line), 1 = white. */
void line_sensors_init(void);
void line_sensors_read(uint8_t values[LINE_SENSOR_COUNT]);
bool line_detected(void);

#endif /* PICOBOT_H */
