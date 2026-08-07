// L4 (graphics, the right way) — draw into a RAM framebuffer with the CPU, then
// blit it once. This is how real graphics stacks (and LVGL) work, and it splits
// the work into two costs you can measure separately:
// ===========================================================================
//   1. RENDER cost  — the CPU draws shapes into a buffer in SRAM. Here a "pixel"
//      is just a memory store (fb[y*W+x] = colour) — cheap, no panel transaction.
//      But you still pay one store per pixel you touch, so a full redraw costs
//      CPU time proportional to the work. THIS is what a GPU would offload.
//   2. TRANSFER cost — one DMA blit ships the finished buffer to the panel over
//      SPI. The CPU is free during it; the SPI bus sets the ceiling (the L4 lab).
//
//   Contrast with l4_gfx, where each pixel was a whole SPI command transaction:
//   there the per-pixel cost was the *bus*; here the per-pixel cost is the *CPU*,
//   and the bus is paid once. Drawing a moving sine wave makes both visible: the
//   live [fps] line shows render_ms and blit_ms side by side every frame.
// ===========================================================================
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "st7796.h"

// A PARTIAL framebuffer — a band, not the whole screen. A full 320x480x16bpp
// frame is 300 KB and won't sit comfortably in 520 KB of SRAM (the memory lesson
// from Section 1), so we render a strip. LVGL uses exactly this "partial buffer"
// trick for the same reason. Override the height: ./run.sh flash l4_fb BAND_H=120
#ifndef BAND_H
#define BAND_H 200
#endif
#define FB_W   ST7796_WIDTH          // full width, partial height
#define FB_H   BAND_H
#define CYCLES 2.5f                  // sine cycles across the width

static uint16_t fb[FB_W * FB_H];     // the software framebuffer (in SRAM)

// A pixel in the buffer is a plain memory write — no command, no bus. Compare
// this single store to st7796_draw_pixel()'s whole 1x1-window transaction.
static inline void fb_pixel(int x, int y, uint16_t c) {
    if ((unsigned)x < FB_W && (unsigned)y < FB_H) fb[y * FB_W + x] = c;
}
static void fb_clear(uint16_t c) {
    for (int i = 0; i < FB_W * FB_H; i++) fb[i] = c;
}
static void fb_vline(int x, int y0, int y1, uint16_t c) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) fb_pixel(x, y, c);
}

// Render one frame of a scrolling sine into the buffer. We connect each column to
// the previous one with a short vertical run, so a steep part of the wave is a
// continuous line and not a dotted one (the same idea as draw_line's interpolation).
static void render_sine(float phase) {
    fb_clear(ST_BLACK);
    for (int x = 0; x < FB_W; x++) fb_pixel(x, FB_H / 2, 0x2104);   // dim centre axis
    int prev_y = FB_H / 2;
    for (int x = 0; x < FB_W; x++) {
        float t = (x / (float)FB_W) * 2.0f * (float)M_PI * CYCLES + phase;
        int   y = (int)(FB_H / 2 - sinf(t) * (FB_H / 2 - 4));
        fb_vline(x, prev_y, y, ST_GREEN);
        prev_y = y;
    }
}

int main(void) {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(2000);
    printf("\n=== L4 Framebuffer — render in RAM, blit once ===\n");

    st7796_init();
    printf("SPI granted = %lu Hz   buffer = %dx%d (%u KB in SRAM)\n",
           (unsigned long)st7796_spi_hz(), FB_W, FB_H,
           (unsigned)(sizeof fb / 1024));
    st7796_fill_screen(ST_BLACK);
    const int band_y = (ST7796_HEIGHT - FB_H) / 2;   // centre the band on the panel

    // --- one-shot measurement: the two costs, separated ---
    uint64_t t0 = time_us_64();
    render_sine(0.0f);
    uint64_t render_us = time_us_64() - t0;          // CPU drawing into SRAM

    t0 = time_us_64();
    st7796_blit(0, band_y, FB_W, FB_H, fb);          // DMA the finished buffer out
    uint64_t blit_us = time_us_64() - t0;

    uint32_t pixels = (uint32_t)FB_W * FB_H;
    printf("pixels=%lu render_ms=%.2f blit_ms=%.2f\n",
           (unsigned long)pixels, render_us / 1000.0, blit_us / 1000.0);
    printf("=> RENDER is CPU time (one memory store per pixel — a GPU would do these\n");
    printf("   in parallel); BLIT is bus time (DMA, CPU free). Two different costs.\n\n");

    // --- animate: scroll the wave; print the per-frame split live ---
    printf("animating — watch render_ms vs blit_ms, and fps = 1000/(render+blit):\n");
    float phase = 0.0f;
    while (true) {
        t0 = time_us_64(); render_sine(phase);        render_us = time_us_64() - t0;
        t0 = time_us_64(); st7796_blit(0, band_y, FB_W, FB_H, fb); blit_us = time_us_64() - t0;
        double frame_ms = (render_us + blit_us) / 1000.0;
        printf("[fps] render_ms=%.2f blit_ms=%.2f frame_ms=%.2f fps=%.0f\n",
               render_us / 1000.0, blit_us / 1000.0, frame_ms, frame_ms ? 1000.0 / frame_ms : 0.0);
        phase += 0.20f;     // scroll speed
    }
}
