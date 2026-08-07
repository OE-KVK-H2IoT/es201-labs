// L4 (graphics primitives) — "the cost of a pixel", and roll-your-own drawing.
// ===========================================================================
// WHY THIS DEMO EXISTS
//   The display driver hands you st7796_fill_rect()/fill_screen(): they open ONE
//   window on the panel and stream pixels into it. But what is a single pixel
//   really worth? Here we draw the cheapest possible way — one pixel at a time —
//   and TIME it against the driver's windowed fill. The gap is enormous, and the
//   reason is pure architecture: every put_pixel() below is a *whole* SPI
//   transaction (set the column window, set the row window, "memory write", push
//   ONE 16-bit colour). The maths of "which pixel" is free; the per-pixel SPI
//   handshake is not. That is exactly why real drivers stream into a window, why
//   you redraw only dirty rectangles, and ultimately why graphics libraries like
//   LVGL exist. You measure it once, you never forget it.
//
//   CORE TASK (everyone): run it, read slow_ms vs fast_ms, explain the ratio.
//   SIDE TRACK (optional): fill in draw_line() (and go further — circles, text)
//   and feel how a real graphics layer is built on put_pixel(). The point is not
//   to finish a graphics library; it is to feel why you'd rather not write one.
// ===========================================================================
#include <stdio.h>
#include "pico/stdlib.h"
#include "st7796.h"

// Our one primitive: write a single pixel through the driver's honest
// st7796_draw_pixel() — a 1x1 window plus two bytes of colour, blocking SPI, no
// DMA. Everything below (lines, the naive fill) is built ONLY from this, so the
// timing really measures per-pixel command overhead. (A thin int-arg wrapper so
// the drawing code can pass signed coordinates.)
static inline void put_pixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0) return;
    st7796_draw_pixel((uint16_t)x, (uint16_t)y, color);
}

// A horizontal line is the simplest thing built FROM put_pixel: walk x, stamp a
// pixel each step. (Given as the worked example so the TODO below has a pattern.)
static void draw_hline(int x, int y, int len, uint16_t color) {
    for (int i = 0; i < len; i++) put_pixel(x + i, y, color);
}

// SIDE TRACK — fill this in. A general line is the first genuinely interesting
// primitive: it must step the "fast" axis one pixel at a time while advancing the
// "slow" axis fractionally, using only integers (no float on the hot path). That
// algorithm is Bresenham's. Until you implement it, this draws nothing.
//   Reference solution is in the tutorial (05-display-dma.md, the Side Track box).
static void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
    // TODO (side track): Bresenham. Stamp put_pixel() along the line, integer-only.
}

int main(void) {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(2000);
    printf("\n=== L4 Graphics Primitives — the cost of a pixel ===\n");

    st7796_init();
    // Knowing the SPI clock makes the numbers below predictable, not magic: the
    // per-pixel cost is dominated by the command bytes clocked at this rate.
    printf("SPI granted = %lu Hz\n", (unsigned long)st7796_spi_hz());

    const uint32_t pixels = (uint32_t)ST7796_WIDTH * ST7796_HEIGHT;

    // --- MEASUREMENT 1: fill the screen the naive way, one pixel at a time ---
    // This is deliberately slow. Narrate it so nobody thinks the board hung —
    // the WAIT is the lesson. ~150k separate SPI transactions are about to happen.
    printf("filling %lu pixels ONE AT A TIME (put_pixel) — this takes a few seconds...\n",
           (unsigned long)pixels);
    uint64_t t0 = time_us_64();
    for (int y = 0; y < ST7796_HEIGHT; y++)
        for (int x = 0; x < ST7796_WIDTH; x++)
            put_pixel(x, y, ST_BLUE);
    uint64_t slow_us = time_us_64() - t0;

    // --- MEASUREMENT 2: the same pixels via the driver's ONE windowed fill ---
    // One window opened, every pixel streamed back-to-back: no per-pixel handshake.
    t0 = time_us_64();
    st7796_fill_screen(ST_BLACK);
    uint64_t fast_us = time_us_64() - t0;

    // --- the result: read these two numbers and the ratio between them ---
    double slow_ms = slow_us / 1000.0, fast_ms = fast_us / 1000.0;
    printf("pixels=%lu slow_ms=%.1f fast_ms=%.1f ratio=%.0f\n",
           (unsigned long)pixels, slow_ms, fast_ms,
           fast_us ? (double)slow_us / (double)fast_us : 0.0);
    // Pixels/second is the honest throughput of each path — the windowed fill is
    // bus-bound (near the SPI ceiling); put_pixel is transaction-bound (nowhere near it).
    printf("slow_pps=%.0f fast_pps=%.0f\n",
           slow_us ? pixels * 1e6 / slow_us : 0.0,
           fast_us ? pixels * 1e6 / fast_us : 0.0);
    printf("=> a pixel through the command interface costs ~%.0fx a streamed pixel.\n",
           (slow_us && fast_us) ? (double)slow_us / (double)fast_us : 0.0);
    printf("   That overhead is WHY drivers stream to a window, why we redraw only\n");
    printf("   dirty rectangles, and why graphics libraries (LVGL) exist.\n\n");

    // --- SIDE TRACK showcase: once draw_line() works, this draws a fan of lines.
    //     Until then the screen just stays black after the fast fill. ---
    printf("side track: implement draw_line() to see a fan of lines here.\n");
    draw_hline(20, 20, 280, ST_WHITE);                 // the worked example, always visible
    for (int i = 0; i <= 280; i += 20)
        draw_line(160, 240, i + 20, 460, ST_YELLOW);   // your code lights this up

    while (true) tight_loop_contents();
}
