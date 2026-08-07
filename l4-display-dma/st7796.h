// Minimal ST7796 (320x480 SPI TFT) driver for the 52Pi EP-0172 carrier.
// Pins (confirmed on the 52Pi EP-0172 wiki): SPI0  SCK=GP2 MOSI=GP3
//   CS=GP5  DC=GP6  RST=GP7.  Backlight is hardwired on (no GPIO).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define ST7796_WIDTH  320
#define ST7796_HEIGHT 480

// RGB565 (5-6-5). The panel is initialised BGR (MADCTL 0x48); if red and blue
// look swapped on your unit, flip the BGR bit in st7796_init (0x48 <-> 0x40).
#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3)))
#define ST_BLACK   0x0000
#define ST_WHITE   0xFFFF
#define ST_RED     0xF800
#define ST_GREEN   0x07E0
#define ST_BLUE    0x001F
#define ST_YELLOW  0xFFE0
#define ST_CYAN    0x07FF
#define ST_MAGENTA 0xF81F

void st7796_init(void);
// The smallest real panel operation: open a 1x1 window (CASET/RASET/RAMWR) and
// push one 16-bit pixel over blocking SPI — no DMA, no rectangle. This is the
// honest "cost of a pixel" through the command interface; build line/rect/etc.
// on top of it (see gfx_primitives.c) and compare against the windowed fills below.
void st7796_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void st7796_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7796_fill_screen(uint16_t color);
// Push an arbitrary w*h RGB565 buffer into a window (a "blit") — used to send a
// whole host-side framebuffer at once (double buffering).
void st7796_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf);
uint32_t st7796_spi_hz(void);   // the ACTUAL SPI clock granted (often < requested)
