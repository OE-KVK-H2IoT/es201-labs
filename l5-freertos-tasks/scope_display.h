/* scope_display — a tiny sweep-scope renderer over the L4 ST7796 driver.
 *
 * This is the "your L4 display path" the mini-scope split (main_scope.c) draws to.
 * It is deliberately thin: init the panel, then plot one sample per call, sweeping
 * left-to-right and erasing just ahead of the cursor like an analog scope. The
 * point of L5 is the *task/queue architecture* feeding this, not the graphics —
 * so the drawing stays simple and lives behind these two calls.
 */
#pragma once
#include <stdint.h>

void scope_display_init(void);          /* ST7796 init + clear + baseline grid */
void scope_draw_sample(uint16_t sample); /* plot one 12-bit ADC sample (0..4095) */
