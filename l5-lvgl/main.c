// L5 — LVGL on bare-metal RP2350: a real GUI (title + live sine chart + slider +
// button) rendered by a *library*, flushed through OUR st7796 driver. This is the
// payoff after l4_gfx / l4_fb: you stop hand-drawing pixels and let LVGL render
// into a buffer, then hand the finished area to st7796_blit — the render-to-
// buffer-then-flush model from the framebuffer lab, now done for you, with
// widgets and an event system on top.
//
// Four things wire a GUI library to bare metal — and they map 1:1 onto the
// graphics-reference architecture:
//   * DISPLAY: an LVGL flush callback -> st7796_blit(area)   (a "partial buffer")
//   * TIME:    a 1 ms repeating timer -> lv_tick_inc()       (LVGL's clock)
//   * INPUT:   GT911 touch -> an lv_indev pointer            (so widgets respond)
//   * EVENTS:  the slider/button use LVGL callbacks          (the event loop §11)
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "lvgl.h"
#include "st7796.h"
#include "gt911.h"

#define DRAW_LINES 40                                   // partial draw-buffer height
static lv_color_t draw_buf_px[ST7796_WIDTH * DRAW_LINES];

// DISPLAY — hand LVGL's rendered rectangle straight to our DMA blit, then tell
// LVGL the buffer is free again. (st7796_blit sets the window and streams 16-bit
// pixels MSB-first, which is why lv_conf.h has LV_COLOR_16_SWAP 0.)
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    st7796_blit(area->x1, area->y1, w, h, (uint16_t *)px);
    lv_disp_flush_ready(drv);
}

// TIME — LVGL measures time in milliseconds via lv_tick_inc; a repeating timer
// feeds it. (-1 ms period = fire exactly 1 ms after the last start, not after the
// callback returns.)
static bool tick_cb(repeating_timer_t *t) { (void)t; lv_tick_inc(1); return true; }

// INPUT — GT911 touch as an LVGL pointer. gt911_read returns -1 ("no fresh data")
// far more often than a real change (the polling lesson from the touch lab), so we
// hold the last point/state and only update it on a fresh sample.
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    static int16_t last_x = 0, last_y = 0;
    static lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    gt911_point_t p[1];
    int n = gt911_read(p, 1);
    if      (n > 0)  { last_x = p[0].x; last_y = p[0].y; state = LV_INDEV_STATE_PRESSED; }
    else if (n == 0) { state = LV_INDEV_STATE_RELEASED; }
    // n < 0: keep the previous point and state
    data->point.x = last_x; data->point.y = last_y; data->state = state;
}

// Demo state the EVENT callbacks mutate — this is what "handling an event" means.
static lv_obj_t          *chart;
static lv_chart_series_t *series;
static int  amplitude = 100;        // the slider sets this
static bool paused     = false;     // the button toggles this

static void slider_cb(lv_event_t *e) {                  // fires on LV_EVENT_VALUE_CHANGED
    amplitude = lv_slider_get_value(lv_event_get_target(e));
}
static void button_cb(lv_event_t *e) {                  // fires on LV_EVENT_CLICKED
    paused = !paused;
    lv_obj_t *lbl = lv_obj_get_child(lv_event_get_target(e), 0);
    lv_label_set_text(lbl, paused ? "Run" : "Pause");
}

static void build_ui(void) {
    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL on RP2350 (Pico 2)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 280, 200);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 36);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 60);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -110, 110);
    series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN),
                                 LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 240);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -70);
    lv_slider_set_range(slider, 10, 110);
    lv_slider_set_value(slider, amplitude, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(btn, button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *blabel = lv_label_create(btn);
    lv_label_set_text(blabel, "Pause");
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("\n=== L5 LVGL — a library GUI flushed through our st7796 driver ===\n");

    st7796_init();
    gt911_init();
    lv_init();

    // Register the display (partial buffer + our flush).
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, draw_buf_px, NULL, ST7796_WIDTH * DRAW_LINES);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ST7796_WIDTH;
    disp_drv.ver_res  = ST7796_HEIGHT;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch as a pointer input device.
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    // Start LVGL's millisecond clock.
    static repeating_timer_t tick_timer;
    add_repeating_timer_ms(-1, tick_cb, NULL, &tick_timer);

    build_ui();
    printf("UI: title + live sine chart + slider (amplitude) + button (pause).\n");
    printf("Touch the slider/button — the GT911 drives LVGL's event loop.\n");

    // The application loop is just: pump LVGL, and feed the chart. lv_timer_handler
    // IS the event loop (§11): it polls input, runs timers, and flushes dirty areas.
    float phase = 0.0f;
    uint32_t last_push = 0;
    while (true) {
        lv_timer_handler();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (!paused && now - last_push >= 30) {
            lv_chart_set_next_value(chart, series, (lv_coord_t)(sinf(phase) * amplitude));
            phase += 0.25f;
            last_push = now;
        }
        sleep_ms(5);
    }
}
