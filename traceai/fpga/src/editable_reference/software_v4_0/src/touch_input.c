#include "touch_input.h"
#include <string.h>
int tracegpu_touch_init(tracegpu_touch_context *c)
{
    int status;
    if (!c) return -1;
    memset(c, 0, sizeof(*c));
    c->button_armed = 1U;
    status = tracegpu_ps_touch_init(&c->ps);
    c->accepted_frame = c->ps.frame;
    return status;
}

tracegpu_touch_event_type tracegpu_touch_poll(tracegpu_touch_context *c,
                                             tracegpu_touch_sample *s)
{
    tracegpu_touch_event_type e = TRACEGPU_TOUCH_NONE;
    XTime now;
    int n;
    if (!c || !s) return e;
    n = tracegpu_ps_touch_poll(&c->ps);
    XTime_GetTime(&now);
    *s = c->previous;
    s->online = c->ps.frame.online;
    s->i2c_errors = c->ps.frame.i2c_errors;
    c->accepted_frame.online = s->online;
    c->accepted_frame.i2c_errors = s->i2c_errors;
    if (n > 0) {
        c->release_pending = 0U;
        c->accepted_frame = c->ps.frame;
        s->valid = 1U;
        s->point_count = (uint8_t)n;
        s->x = c->ps.frame.point[0].x;
        s->y = c->ps.frame.point[0].y;
        if (!c->previous.valid) e = TRACEGPU_TOUCH_DOWN;
        else if (s->x != c->previous.x || s->y != c->previous.y ||
                 s->point_count != c->previous.point_count) e = TRACEGPU_TOUCH_MOVE;
    } else {
        /* Only an explicit, complete zero-point report starts release.
         * A following no-data poll may finish this timer, but never starts it.
         * GT may publish just one zero-point report when the finger lifts. */
        if (n == 0 && c->previous.valid && !c->release_pending) {
            c->release_pending = 1U;
            c->release_since = now;
        }
        if (c->release_pending &&
            (n == 0 || n == TRACEGPU_TOUCH_POLL_NO_DATA) &&
            now - c->release_since >=
                ((XTime)COUNTS_PER_SECOND * TRACEGPU_TOUCH_RELEASE_MS) / 1000U) {
            s->valid = 0U;
            s->point_count = 0U;
            c->accepted_frame = c->ps.frame;
            c->accepted_frame.count = 0U;
            memset(c->accepted_frame.point, 0, sizeof(c->accepted_frame.point));
            c->release_pending = 0U;
            e = TRACEGPU_TOUCH_UP;
        }
    }
    c->initialized = 1U;
    c->previous = *s;
    return e;
}

const tracegpu_touch_frame *tracegpu_touch_frame_get(const tracegpu_touch_context *c)
{
    return c ? &c->accepted_frame : 0;
}

int tracegpu_touch_button_press(tracegpu_touch_context *c,
                               tracegpu_touch_event_type e,
                               const tracegpu_touch_sample *s)
{
    if (!c || !s) return 0;
    if (e == TRACEGPU_TOUCH_UP) c->button_armed = 1U;
    if (e != TRACEGPU_TOUCH_DOWN || !c->button_armed) return 0;
    c->button_armed = 0U;
    return s->valid && s->point_count == 1U;
}
