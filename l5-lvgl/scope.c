// L5 (variant) — LVGL multi-channel "scope". Same library-GUI plumbing as main.c
// (flush -> st7796_blit, a 1 ms tick, GT911 touch as an lv_indev), but now the
// chart shows FOUR real traces instead of one fake sine:
//
//   * CH_SINE  — a software sine we generate ourselves: a known-good reference
//                signal to compare the live inputs against.
//   * CH_A     — ADC input 0 on GP26 (the joystick Y axis on the carrier — move
//                the stick and watch this trace move).
//   * CH_B     — ADC input 1 on GP27 (the joystick X axis).
//   * CH_NOISE — ADC input 2 on GP28, left FLOATING. Nothing drives it, so it
//                reads whatever electrical noise is around — the "why analog
//                inputs need grounding/filtering" lesson from L2, drawn live.
//
// All four live in the SAME 12-bit space (0..4095, 3.3 V ref) that L2 used, so
// the y-axis is honest ADC counts. Two controls, both routed through LVGL's
// event system (see graphics reference §11):
//   * a SLIDER sets the sine amplitude — a signal YOU control, for comparison.
//   * a SWITCH toggles AUTOSCALE. Fixed range shows absolute levels (a small
//     signal looks flat near the middle); autoscale zooms the y-axis to the data
//     (you see the detail, but lose the absolute reference). Choosing between
//     those two is a real oscilloscope decision, not a toy one.
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "lvgl.h"
#include "st7796.h"
#include "gt911.h"

#define DRAW_LINES 40                                   // partial draw-buffer height
static lv_color_t draw_buf_px[ST7796_WIDTH * DRAW_LINES];

#define POINTS   60          // samples shown across the chart (~1.8 s at 30 ms)
#define ADC_MAX  4095        // 12-bit full scale, matches L2

// The four traces. CH_SINE is generated; the other three are real ADC inputs.
enum { CH_SINE, CH_A, CH_B, CH_NOISE, N_CH };

// Per-channel description: which ADC input to read (-1 = software-generated),
// a short legend name, and a trace colour.
typedef struct { int adc_input; const char *name; lv_palette_t colour; } channel_t;
static const channel_t CH[N_CH] = {
    { -1, "sine",  LV_PALETTE_GREEN  },   // software reference
    {  0, "A26",   LV_PALETTE_AMBER  },   // GP26 = joystick Y
    {  1, "B27",   LV_PALETTE_BLUE   },   // GP27 = joystick X
    {  2, "noise", LV_PALETTE_GREY   },   // GP28 floating
};

// ---- DISPLAY / TIME / INPUT: identical wiring to main.c -------------------
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    st7796_blit(area->x1, area->y1, w, h, (uint16_t *)px);
    lv_disp_flush_ready(drv);
}
static bool tick_cb(repeating_timer_t *t) { (void)t; lv_tick_inc(1); return true; }
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    static int16_t last_x = 0, last_y = 0;
    static lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    gt911_point_t p[1];
    int n = gt911_read(p, 1);
    if      (n > 0)  { last_x = p[0].x; last_y = p[0].y; state = LV_INDEV_STATE_PRESSED; }
    else if (n == 0) { state = LV_INDEV_STATE_RELEASED; }
    data->point.x = last_x; data->point.y = last_y; data->state = state;
}

// ---- Demo state the widgets mutate ----------------------------------------
static lv_obj_t          *chart;
static lv_chart_series_t *series[N_CH];
static lv_obj_t          *readout;                 // live numeric line
static int   amplitude = 1500;                     // sine amplitude in ADC counts
static bool  autoscale = false;                    // the switch toggles this

// Our own copy of the on-screen samples, so autoscale can find the min/max of
// exactly what's visible. (LVGL stores these internally too, but keeping our own
// ring keeps the autoscale maths readable.)
static lv_coord_t hist[N_CH][POINTS];
static int        hpos = 0, hcount = 0;

static void slider_cb(lv_event_t *e) {             // LV_EVENT_VALUE_CHANGED
    amplitude = lv_slider_get_value(lv_event_get_target(e));
}
static void autoscale_cb(lv_event_t *e) {          // LV_EVENT_VALUE_CHANGED
    autoscale = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

// Zoom the y-axis to the data (autoscale) or pin it to the full 12-bit span.
static void apply_range(void) {
    if (!autoscale || hcount == 0) {
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, ADC_MAX);
        return;
    }
    lv_coord_t lo = ADC_MAX, hi = 0;
    for (int c = 0; c < N_CH; c++)
        for (int i = 0; i < hcount; i++) {
            lv_coord_t v = hist[c][i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    lv_coord_t margin = (hi - lo) / 8;
    if (margin < 64) margin = 64;                  // never collapse to a flat line
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, lo - margin, hi + margin);
}

static void build_ui(void) {
    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL scope — 4 channels on RP2350");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    // A compact colour legend so the traces aren't anonymous.
    static const lv_coord_t lx[N_CH] = { 10, 95, 165, 235 };
    for (int c = 0; c < N_CH; c++) {
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, CH[c].name);
        lv_obj_set_style_text_color(lbl, lv_palette_main(CH[c].colour), 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, lx[c], 30);
    }

    chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 300, 250);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 48);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, ADC_MAX);
    for (int c = 0; c < N_CH; c++)
        series[c] = lv_chart_add_series(chart, lv_palette_main(CH[c].colour),
                                        LV_CHART_AXIS_PRIMARY_Y);

    readout = lv_label_create(scr);
    lv_label_set_text(readout, "sine=---- A=---- B=---- noise=----");
    lv_obj_align(readout, LV_ALIGN_TOP_MID, 0, 308);

    lv_obj_t *amp_lbl = lv_label_create(scr);
    lv_label_set_text(amp_lbl, "Sine amplitude");
    lv_obj_align(amp_lbl, LV_ALIGN_TOP_MID, 0, 338);
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 240);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 362);
    lv_slider_set_range(slider, 0, 2000);
    lv_slider_set_value(slider, amplitude, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *sw_lbl = lv_label_create(scr);
    lv_label_set_text(sw_lbl, "Autoscale");
    lv_obj_align(sw_lbl, LV_ALIGN_TOP_MID, -30, 420);
    lv_obj_t *sw = lv_switch_create(scr);
    lv_obj_align(sw, LV_ALIGN_TOP_MID, 55, 414);
    lv_obj_add_event_cb(sw, autoscale_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// Read one channel: real ADC input, or generate the software sine.
static lv_coord_t sample(int c, float phase) {
    if (CH[c].adc_input < 0) {                     // CH_SINE: centred, slider-scaled
        return (lv_coord_t)(2048.0f + sinf(phase) * amplitude);
    }
    adc_select_input(CH[c].adc_input);             // round-robin, exactly like L2
    return (lv_coord_t)adc_read();
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("\n=== L5 LVGL scope — sine + 2 ADC inputs + a floating noise channel ===\n");

    st7796_init();
    gt911_init();

    // ADC: one converter, three inputs. adc_gpio_init hands each pin to the ADC
    // (kills its digital input/pulls). GP28 stays floating on purpose = noise.
    adc_init();
    adc_gpio_init(26);   // ADC0 — joystick Y
    adc_gpio_init(27);   // ADC1 — joystick X
    adc_gpio_init(28);   // ADC2 — floating noise pickup

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, draw_buf_px, NULL, ST7796_WIDTH * DRAW_LINES);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ST7796_WIDTH;
    disp_drv.ver_res  = ST7796_HEIGHT;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    static repeating_timer_t tick_timer;
    add_repeating_timer_ms(-1, tick_cb, NULL, &tick_timer);

    build_ui();
    printf("Traces: sine (SW ref), A=GP26/ADC0, B=GP27/ADC1, noise=GP28/ADC2 (floating).\n");
    printf("Slider = sine amplitude. Switch = autoscale (zoom y-axis to the data).\n");

    // The application loop: pump LVGL (lv_timer_handler IS the event loop, §11),
    // then every ~30 ms read all four channels, push them, and rescale.
    float phase = 0.0f;
    uint32_t last_push = 0;
    while (true) {
        lv_timer_handler();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_push >= 30) {
            lv_coord_t v[N_CH];
            for (int c = 0; c < N_CH; c++) {
                v[c] = sample(c, phase);
                lv_chart_set_next_value(chart, series[c], v[c]);
                hist[c][hpos] = v[c];
            }
            hpos = (hpos + 1) % POINTS;
            if (hcount < POINTS) hcount++;
            apply_range();
            lv_label_set_text_fmt(readout, "sine=%4d A=%4d B=%4d noise=%4d",
                                  v[CH_SINE], v[CH_A], v[CH_B], v[CH_NOISE]);
            phase += 0.25f;
            last_push = now;
        }
        sleep_ms(5);
    }
}
