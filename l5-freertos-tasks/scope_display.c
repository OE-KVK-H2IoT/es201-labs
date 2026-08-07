/* scope_display.c — see scope_display.h. A sweep-scope over the L4 ST7796 driver. */
#include "scope_display.h"
#include "st7796.h"

#define SCOPE_W   ST7796_WIDTH    /* 320 columns = the time axis */
#define SCOPE_H   ST7796_HEIGHT   /* 480 rows    = the amplitude axis */
#define BG        ST_BLACK
#define TRACE     ST_GREEN
#define GRID      RGB565(30, 30, 30)   /* dim baseline */
#define CURSOR    RGB565(60, 60, 60)   /* the "just-ahead" erase bar */

static uint16_t col = 0;   /* current sweep column, wraps at SCOPE_W */

void scope_display_init(void) {
    st7796_init();
    st7796_fill_screen(BG);
    st7796_fill_rect(0, SCOPE_H / 2, SCOPE_W, 1, GRID);   /* mid-scale baseline */
    col = 0;
}

void scope_draw_sample(uint16_t sample) {
    /* 12-bit ADC (0..4095) -> screen row. Invert so a higher voltage sits higher
     * on the panel (row 0 is the top). */
    uint32_t s = sample & 0x0FFF;
    uint16_t y = (uint16_t)((4095u - s) * (SCOPE_H - 1) / 4095u);

    /* Erase this column back to background, restore the one baseline pixel... */
    st7796_fill_rect(col, 0, 1, SCOPE_H, BG);
    st7796_draw_pixel(col, SCOPE_H / 2, GRID);

    /* ...then plot the sample as a 1x2 dot so a single point is visible. */
    st7796_fill_rect(col, y, 1, 2, TRACE);

    /* Advance the sweep and paint a faint cursor bar one column ahead, so you can
     * see where the trace is being overwritten (the classic analog-scope look). */
    col = (uint16_t)((col + 1) % SCOPE_W);
    st7796_fill_rect(col, 0, 1, SCOPE_H, CURSOR);
}
