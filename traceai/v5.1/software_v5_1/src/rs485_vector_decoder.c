#include "rs485_vector_decoder.h"
#include <string.h>

static void character(const tracegpu_uart_decoded *c, void *context) {
    tracegpu_rs485_vector_decoder *d = context;
    d->characters++;
    if (tracegpu_modbus_character(&d->modbus, c)) d->input_errors++;
    if (d->byte_emit) d->byte_emit(c,d->byte_context);
}

static void reset_uart(tracegpu_rs485_vector_decoder *d) {
    (void)tracegpu_uart_vector_begin(&d->uart, &d->config, character, d);
    d->waiting_idle = 1;
}

static void cut(tracegpu_rs485_vector_decoder *d, uint64_t t) {
    uint64_t start = d->modbus.cursor;
    if (t < start) t = start;
    (void)tracegpu_modbus_gap(&d->modbus, start, t);
    d->gaps++;
    reset_uart(d);
    d->source_known = 0;
}

static void forward_uart(tracegpu_rs485_vector_decoder *d,
    const tracegpu_vector_event *e) {
    uint32_t previous_characters = d->characters;
    uint64_t previous_start = d->uart.frame.start;
    unsigned previous_busy = d->uart.busy;
    unsigned high = (e->levels >> d->config.channel) & 1U;
    tracegpu_uart_vector_event(&d->uart, e);
    if (previous_busy && d->characters == previous_characters &&
        (!d->uart.busy || d->uart.frame.start != previous_start)) {
        /* A false UART start was discarded without a byte callback. That
         * observed pulse is NOT Modbus silence; conservatively cut the ADU.
         * Keep old UART behavior unchanged for the existing application. */
        cut(d, e->tick);
        d->source_known = 1;
        if (high) {
            tracegpu_vector_event initial = *e;
            initial.type = TG_VEC_INIT;
            tracegpu_uart_vector_event(&d->uart, &initial);
            d->waiting_idle = 0;
        }
        return;
    }
    if (e->type == TG_VEC_CHANGE && d->uart.busy &&
        d->uart.frame.start == e->tick &&
        (!previous_busy || previous_start != d->uart.frame.start)) {
        /* The known-high interval BEFORE this new falling edge is evidence,
         * even if its new UART start is later rejected as a short pulse. */
        if (tracegpu_modbus_watermark(&d->modbus, e->tick)) d->input_errors++;
    }
}

int tracegpu_rs485_vector_begin(tracegpu_rs485_vector_decoder *d,
    const tracegpu_uart_decode_config *config, unsigned invert,
    tracegpu_modbus_frame_fn emit, void *context) {
    if (!d || !config || invert > 1) return -1;
    memset(d, 0, sizeof *d);
    if (tracegpu_modbus_begin(&d->modbus, config, 0, emit, context)) return -1;
    d->config = *config;
    d->invert = (uint8_t)invert;
    reset_uart(d);
    d->initialized = 1;
    return 0;
}

static int metadata_valid(const tracegpu_rs485_vector_decoder *d,
    const tracegpu_vector_event *e) {
    if (e->type < TG_VEC_INIT || e->type > TG_VEC_RESYNC) return 0;
    if (e->type == TG_VEC_GAP) return e->flags == 1 && !e->levels;
    return (e->flags & (1U << d->config.channel)) && !(e->levels & ~e->flags);
}

int tracegpu_rs485_vector_event(tracegpu_rs485_vector_decoder *d,
    const tracegpu_vector_event *input) {
    tracegpu_vector_event e;
    int discontinuity, high;
    if (!d || !input || !d->initialized || d->ended) return -1;
    if (d->have_event && input->tick < d->last) {
        cut(d, d->last);
        d->input_errors++;
        return -1;
    }
    discontinuity = d->have_event && input->sequence != d->sequence + 1U;
    d->sequence = input->sequence;
    d->have_event = 1;
    if (!metadata_valid(d, input) || (input->type != TG_VEC_GAP &&
        d->capture_mask && d->capture_mask != input->flags)) {
        cut(d, input->tick);
        d->last = input->tick;
        d->input_errors++;
        return -1;
    }
    if (input->type != TG_VEC_GAP && !d->capture_mask)
        d->capture_mask = input->flags;
    if (discontinuity || input->type == TG_VEC_GAP) {
        cut(d, input->tick);
        if (discontinuity) d->input_errors++;
    }
    e = *input;
    if (e.type != TG_VEC_GAP && d->invert)
        e.levels ^= (uint8_t)(1U << d->config.channel);
    high = (e.levels >> d->config.channel) & 1U;
    if (e.type == TG_VEC_INIT || e.type == TG_VEC_RESYNC) {
        /* Never allow the old UART decoder to advance across a reinitialization
         * before reporting the loss of its previous interval. */
        if (d->source_known || d->uart.busy) cut(d, e.tick);
        d->source_known = 1;
    }
    d->last = e.tick;
    if (e.type == TG_VEC_GAP) return 0;
    if (d->source_known && !d->waiting_idle &&
        (e.type == TG_VEC_WATERMARK || e.type == TG_VEC_END) && high != d->uart.level) {
        cut(d, e.tick);
        d->input_errors++;
        if (e.type == TG_VEC_END) {
            (void)tracegpu_modbus_end(&d->modbus, e.tick);
            d->ended = 1;
        }
        return -1;
    }
    if (e.type == TG_VEC_END) {
        if (d->source_known && !d->waiting_idle)
            forward_uart(d, &e);
        if (!d->source_known || d->waiting_idle)
            cut(d, e.tick);
        (void)tracegpu_modbus_end(&d->modbus, e.tick);
        d->ended = 1;
        return 0;
    }
    if (!d->source_known) return 0;
    if (d->waiting_idle) {
        if (!high) return 0;
        /* An initially dominant/break interval was not silence. Start the
         * Modbus t3.5 proof only at this first known recessive/high instant. */
        (void)tracegpu_modbus_gap(&d->modbus, d->modbus.cursor, e.tick);
        e.type = TG_VEC_INIT;
        tracegpu_uart_vector_event(&d->uart, &e);
        d->waiting_idle = 0;
        return 0;
    }
    /* The legacy UART schedules absolute sample/stop times by addition. Never
     * pass a falling edge whose full character cannot fit in this session's
     * uint64_t time domain (including a back-to-back start after a pending byte).
     * A new session, not modular time arithmetic, is required after overflow. */
    if (e.type == TG_VEC_CHANGE && !high && d->uart.level &&
        e.tick > UINT64_MAX - d->modbus.char_ceil) {
        cut(d, e.tick);
        d->input_errors++;
        return -1;
    }
    forward_uart(d, &e);
    if (!d->uart.busy) {
        if (high) {
            if (tracegpu_modbus_watermark(&d->modbus, e.tick)) d->input_errors++;
        } else {
            cut(d, e.tick);
            /* Level is still known, but a clean idle interval is required. */
            d->source_known = 1;
        }
    }
    return 0;
}
