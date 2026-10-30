#ifndef TRACEGPU_TOUCH_INPUT_H
#define TRACEGPU_TOUCH_INPUT_H
#include <stdint.h>
#include "ps_touch_i2c.h"
#include "tracegpu_time.h"
#define TRACEGPU_TOUCH_RELEASE_MS 60U
typedef enum { TRACEGPU_TOUCH_NONE=0, TRACEGPU_TOUCH_DOWN, TRACEGPU_TOUCH_MOVE, TRACEGPU_TOUCH_UP } tracegpu_touch_event_type;
typedef struct { uint16_t x, y; uint8_t valid, point_count, online; uint32_t i2c_errors; } tracegpu_touch_sample;
typedef struct {
    tracegpu_ps_touch ps;
    tracegpu_touch_sample previous;
    tracegpu_touch_frame accepted_frame;
    XTime release_since;
    uint8_t initialized, release_pending, button_armed;
} tracegpu_touch_context;
int tracegpu_touch_init(tracegpu_touch_context *context);
tracegpu_touch_event_type tracegpu_touch_poll(tracegpu_touch_context *context, tracegpu_touch_sample *sample);
const tracegpu_touch_frame *tracegpu_touch_frame_get(const tracegpu_touch_context *context);
/* One UI action per contact; multi-touch contacts consume but cannot click. */
int tracegpu_touch_button_press(tracegpu_touch_context *context,
                               tracegpu_touch_event_type event,
                               const tracegpu_touch_sample *sample);
#endif
