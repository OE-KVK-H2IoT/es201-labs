// ST7796 driver: SPI0 for the bus, a dedicated DMA channel to stream pixels so
// the CPU is free during fills (this is the "DMA" half of the L4 lab).
#include "st7796.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#define LCD_SPI   spi0
#define PIN_SCK   2
#define PIN_MOSI  3
#define PIN_CS    5
#define PIN_DC    6
#define PIN_RST   7
#ifndef LCD_MHZ
#define LCD_MHZ  62        // requested SPI clock in MHz (ST7796 tolerates ~62-80).
#endif                     // Override: ./run.sh flash l4_optimize LCD_MHZ=75
#define LCD_BAUD  (LCD_MHZ * 1000000u)  // the hardware grants the nearest achievable rate <= this

static int      dma_chan;
static uint32_t granted_hz;     // the SPI clock the hardware actually gave us
static uint8_t  colour2[2] __attribute__((aligned(2)));  // one pixel, big-endian, repeated by DMA

uint32_t st7796_spi_hz(void) { return granted_hz; }

static void wait_spi(void) { while (spi_is_busy(LCD_SPI)) tight_loop_contents(); }

// Send one command byte (DC low), then leave DC high for the data that follows.
static void cmd(uint8_t c) {
    wait_spi();
    gpio_put(PIN_DC, 0);
    spi_write_blocking(LCD_SPI, &c, 1);
    wait_spi();
    gpio_put(PIN_DC, 1);
}
static void dat(const uint8_t *d, size_t n) { spi_write_blocking(LCD_SPI, d, n); }
static void dat1(uint8_t d) { spi_write_blocking(LCD_SPI, &d, 1); }

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    cmd(0x2A);
    uint8_t cax[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF }; dat(cax, 4);
    cmd(0x2B);
    uint8_t ray[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF }; dat(ray, 4);
    cmd(0x2C);   // RAM write; pixel data follows (DC already high)
}

// Fill `count` pixels with one colour in a SINGLE DMA transfer, paced by the SPI
// DREQ. The trick that makes it fast: a read-address RING that wraps every 2
// bytes, so the DMA re-reads the same 2-byte colour over and over. We don't have
// to build a big source buffer, and there is no per-chunk setup/wait overhead —
// the whole fill is one streamed transfer, so we get close to the SPI's ceiling.
static void push_pixels(uint16_t color, uint32_t count) {
    colour2[0] = color >> 8;            // big-endian: ST7796 wants the high byte first
    colour2[1] = color & 0xFF;
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(LCD_SPI, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, 1);   // wrap the READ address every 2^1 = 2 bytes
    dma_channel_configure(dma_chan, &c, &spi_get_hw(LCD_SPI)->dr, colour2, count * 2, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    wait_spi();
}

// One pixel, the honest way: a 1x1 window, then two bytes of colour pushed by
// blocking SPI. No DMA — a single pixel isn't worth a DMA setup, and the point
// of this primitive is to expose the *per-pixel command overhead* (three commands
// + eight address bytes for two bytes of pixel). Streaming fills below amortise
// that one window-open over thousands of pixels; this pays it for every dot.
void st7796_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= ST7796_WIDTH || y >= ST7796_HEIGHT) return;
    set_window(x, y, x, y);
    uint8_t px[2] = { color >> 8, color & 0xFF };   // big-endian RGB565 on the wire
    dat(px, 2);
    wait_spi();
}

void st7796_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0 || h == 0) return;
    set_window(x, y, x + w - 1, y + h - 1);
    push_pixels(color, (uint32_t)w * h);
}

void st7796_fill_screen(uint16_t color) {
    st7796_fill_rect(0, 0, ST7796_WIDTH, ST7796_HEIGHT, color);
}

// Blit a host RGB565 buffer into a window. We switch the SPI to 16-bit frames so
// each uint16_t pixel goes out most-significant-byte first (= big-endian RGB565
// on the wire, which is what the ST7796 expects), DMA the whole buffer, then
// switch back to 8-bit for the command path.
void st7796_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf) {
    set_window(x, y, x + w - 1, y + h - 1);     // commands are 8-bit
    wait_spi();
    spi_set_format(LCD_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(LCD_SPI, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(dma_chan, &c, &spi_get_hw(LCD_SPI)->dr, buf, (uint32_t)w * h, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    wait_spi();
    spi_set_format(LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void st7796_init(void) {
    granted_hz = spi_init(LCD_SPI, LCD_BAUD);   // returns the ACTUAL baud granted
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC,  GPIO_OUT); gpio_put(PIN_DC,  1);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);
    dma_chan = dma_claim_unused_channel(true);

    // Hardware reset pulse.
    gpio_put(PIN_RST, 1); sleep_ms(10);
    gpio_put(PIN_RST, 0); sleep_ms(20);
    gpio_put(PIN_RST, 1); sleep_ms(120);

    gpio_put(PIN_CS, 0);   // single device on the bus: hold CS asserted

    cmd(0x01); sleep_ms(120);                 // software reset
    cmd(0x11); sleep_ms(120);                 // sleep out
    cmd(0xF0); dat1(0xC3);                     // command-set control (unlock)
    cmd(0xF0); dat1(0x96);
    cmd(0x36); dat1(0x48);                     // MADCTL: MX + BGR (portrait 320x480)
    cmd(0x3A); dat1(0x55);                     // COLMOD: 16 bits/pixel
    cmd(0xB4); dat1(0x01);                     // column inversion
    { uint8_t d[] = {0x80, 0x02, 0x3B};                          cmd(0xB6); dat(d, sizeof d); }
    { uint8_t d[] = {0x40,0x8A,0x00,0x00,0x29,0x19,0xA5,0x33};   cmd(0xE8); dat(d, sizeof d); }
    cmd(0xC1); dat1(0x06);
    cmd(0xC2); dat1(0xA7);
    cmd(0xC5); dat1(0x18); sleep_ms(120);
    { uint8_t d[] = {0xF0,0x09,0x0B,0x06,0x04,0x15,0x2F,0x54,0x42,0x3C,0x17,0x14,0x18,0x1B};
      cmd(0xE0); dat(d, sizeof d); }           // positive gamma
    { uint8_t d[] = {0xE0,0x09,0x0B,0x06,0x04,0x03,0x2B,0x43,0x42,0x3B,0x16,0x14,0x17,0x1B};
      cmd(0xE1); dat(d, sizeof d); }           // negative gamma
    sleep_ms(120);
    cmd(0xF0); dat1(0x3C);                      // command-set control (lock)
    cmd(0xF0); dat1(0x69);
    sleep_ms(120);
    cmd(0x29);                                  // display on
}
