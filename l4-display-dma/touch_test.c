// L4 (touch) — GT911 multi-finger touch paint that NARRATES what it does.
// ===========================================================================
// HOW A CAPACITIVE TOUCHSCREEN GIVES YOU DATA
//   The GT911 is an I2C chip. Each time you ask, it returns a SNAPSHOT:
//   "these 0..5 fingers are down right now, at these (x,y)", and crucially each
//   point carries a TRACK ID so you can follow one finger across samples. It
//   refreshes that snapshot at its own fixed rate — you can't poll faster.
//
//   This demo paints with ALL fingers at once. The track id is the key idea:
//   we keep a separate "last point" per id, so finger 0 and finger 1 each draw
//   their own independent stroke — that is exactly how you use multi-touch data.
//   Per finger:
//     * DOWN: first sample of that id      -> a point, so draw a DOT
//     * move: later samples of that id      -> a path, so draw a LINE
//             (interpolate, so a fast finger isn't a dotted line)
//     * UP:   id gone -> classify the stroke as TAP (stayed put) or DRAG (moved)
//
//   Two gotchas the code handles, both worth understanding:
//     * "no new data" is NOT "finger up" — gt911_read returns -1 when the
//       controller has nothing fresh (we poll far faster than it reports).
//     * track ids can be REASSIGNED when a finger lands/lifts, so a finger can
//       appear to teleport; we reject impossible one-sample jumps per id.
//
//   K1 clears the canvas; K2 inverts the colours.
// ===========================================================================
#include <stdio.h>
#include <stdlib.h>          // abs()
#include "pico/stdlib.h"
#include "st7796.h"
#include "gt911.h"

#define DOT        1    // brush radius in pixels
#define BTN_CLEAR  14   // K1 (active-low)
#define BTN_INVERT 15   // K2 (active-low)
#define MAX_ID     16
#define TAP_MOVE   8    // stroke moved <= this many px -> TAP, else DRAG
#define MAX_JUMP   80   // one-sample move bigger than this = a track-id shuffle, not motion

static void button_init(uint pin) {
    gpio_init(pin); gpio_set_dir(pin, GPIO_IN); gpio_pull_up(pin);
}
static int clampi(int v, int hi) { return v < 0 ? 0 : (v >= hi ? hi - 1 : v); }

static void draw_dot(int x, int y, uint16_t color) {
    int x0 = clampi(x - DOT, ST7796_WIDTH),  y0 = clampi(y - DOT, ST7796_HEIGHT);
    int x1 = clampi(x + DOT, ST7796_WIDTH),  y1 = clampi(y + DOT, ST7796_HEIGHT);
    st7796_fill_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, color);
}

// Stamp the brush at every pixel step along the line — this is the interpolation.
static void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int steps = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (steps == 0) { draw_dot(x0, y0, color); return; }
    for (int s = 0; s <= steps; s++)
        draw_dot(x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps, color);
}

int main(void) {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(2000);

    printf("\n=== L4 Multi-Finger Touch Paint (narrated) ===\n");
    printf("Up to 5 fingers draw at once; each is tracked by its id.\n");
    printf("DOWN=dot, move=interpolated line, UP=TAP or DRAG. K1 clears, K2 inverts.\n\n");

    st7796_init();
    button_init(BTN_CLEAR);
    button_init(BTN_INVERT);

    uint16_t fg = ST_BLUE, bg = ST_WHITE;
    st7796_fill_screen(bg);
    if (!gt911_init()) {
        printf("!! GT911 not found on I2C0\n");
        st7796_fill_screen(ST_RED);
    } else {
        // Log the hardware facts up front — knowing what you're talking to is
        // half of understanding the data that follows.
        uint8_t pid[4] = {0};
        gt911_product_id(pid);
        printf("GT911 ready: product '%c%c%c%c', I2C0 @ 0x5D, panel %dx%d\n",
               pid[0] ? pid[0] : '?', pid[1] ? pid[1] : '?',
               pid[2] ? pid[2] : '?', pid[3] ? pid[3] : '?',
               ST7796_WIDTH, ST7796_HEIGHT);
        printf("Watch the [rate] line: 'report' is how often the controller has\n");
        printf("new data; 'poll' is how often we ask. poll >> report is the whole point.\n\n");
    }

    // One slot per track id: last point, stroke start + farthest move (tap vs
    // drag), and sample count. This is what makes each finger independent.
    struct { bool active; int x, y, sx, sy, maxmove, count; } last[MAX_ID] = {0};
    gt911_point_t pts[5];
    bool clr_last = false, inv_last = false;

    // Live rate counters: how many times we polled vs how many fresh samples the
    // controller actually produced, summed over a 1-second window.
    uint64_t rate_t0 = time_us_64();
    uint32_t polls = 0, fresh = 0;
    int      fingers_now = 0;

    while (true) {
        // --- buttons ---
        bool clr = !gpio_get(BTN_CLEAR);
        bool inv = !gpio_get(BTN_INVERT);
        if (clr && !clr_last) {
            st7796_fill_screen(bg);
            for (int j = 0; j < MAX_ID; j++) last[j].active = false;
            printf("[K1] clear\n");
        }
        if (inv && !inv_last) {
            uint16_t t = fg; fg = bg; bg = t;
            st7796_fill_screen(bg);
            for (int j = 0; j < MAX_ID; j++) last[j].active = false;
            printf("[K2] invert (draw=0x%04X bg=0x%04X)\n", fg, bg);
        }
        clr_last = clr; inv_last = inv;

        // --- touch: a fresh snapshot of up to 5 fingers (or -1 = nothing new) ---
        int n = gt911_read(pts, 5);

        // Live telemetry: count every poll, and every poll that returned a fresh
        // sample, then report both once per second. 'report' Hz is the GT911's
        // real update rate; 'poll' Hz is ours — seeing poll >> report on screen is
        // exactly why "no new data" must not be read as "finger up".
        polls++;
        if (n >= 0) { fresh++; fingers_now = n; }
        uint64_t now = time_us_64();
        if (now - rate_t0 >= 1000000) {
            printf("[rate] GT911 report=%lu Hz   we poll=%lu Hz   fingers now=%d\n",
                   (unsigned long)fresh, (unsigned long)polls, fingers_now);
            rate_t0 = now; polls = 0; fresh = 0;
        }

        if (n < 0) { sleep_ms(1); continue; }

        uint16_t seen = 1;                          // which ids appear this snapshot
        for (int i = 0; i < n; i++) {
            int id = pts[i].id & (MAX_ID - 1);       // track id -> this finger's slot
            int x = clampi(pts[i].x, ST7796_WIDTH), y = clampi(pts[i].y, ST7796_HEIGHT);

            if (!last[id].active) {
                printf("finger %d DOWN at (%3d,%3d) -> drawing a DOT\n", id, x, y);
                draw_dot(x, y, fg);
                last[id].sx = x; last[id].sy = y; last[id].maxmove = 0; last[id].count = 1;
            } else {
                int gap = abs(x - last[id].x) > abs(y - last[id].y)
                        ? abs(x - last[id].x) : abs(y - last[id].y);
                if (gap > MAX_JUMP) {
                    // A finger can't teleport between two samples: this is the
                    // controller handing this id to a different finger. Don't draw
                    // a line across the screen — start a fresh segment here.
                    printf("finger %d JUMP %3dpx (track id reassigned?) -> not connecting\n", id, gap);
                    draw_dot(x, y, fg);
                    last[id].sx = x; last[id].sy = y; last[id].maxmove = 0;
                } else if (gap > 0) {
                    printf("finger %d move (%3d,%3d)->(%3d,%3d)  gap=%3dpx -> LINE (%d steps)\n",
                           id, last[id].x, last[id].y, x, y, gap, gap);
                    draw_line(last[id].x, last[id].y, x, y, fg);
                } else {
                    draw_dot(x, y, fg);              // finger held still
                }
                last[id].count++;
            }
            last[id].active = true; last[id].x = x; last[id].y = y;
            int mdx = abs(x - last[id].sx), mdy = abs(y - last[id].sy);
            int dist = mdx > mdy ? mdx : mdy;
            if (dist > last[id].maxmove) last[id].maxmove = dist;
            seen |= (uint16_t)(1u << id);
        }
        // Any id we were tracking but didn't see this snapshot has lifted.
        for (int id = 0; id < MAX_ID; id++) {
            if (last[id].active && !(seen & (1u << id))) {
                const char *kind = last[id].maxmove <= TAP_MOVE ? "TAP " : "DRAG";
                printf("finger %d UP -> %s (moved %dpx over %d samples)\n",
                       id, kind, last[id].maxmove, last[id].count);
                last[id].active = false;
            }
        }
        sleep_ms(1);
    }
}
