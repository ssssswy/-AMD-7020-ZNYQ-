#include "uart_dual_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* MMIO is never exercised by these host-side stream tests. */
uint32_t Xil_In32(uintptr_t address) { (void)address; assert(0); return 0; }
void Xil_Out32(uintptr_t a, uint32_t v) { (void)a; (void)v; assert(0); }
void Xil_DCacheFlushRange(uintptr_t a, unsigned n) { (void)a; (void)n; assert(0); }
void Xil_DCacheInvalidateRange(uintptr_t a, unsigned n) { (void)a; (void)n; assert(0); }
static tracegpu_uart_dual_stream stream;
static tracegpu_uart_event event(unsigned ch, unsigned type, uint32_t seq,
                                uint64_t ticks, unsigned data, unsigned flags)
{
    tracegpu_uart_event e = {{(uint32_t)ticks, (uint32_t)(ticks >> 32),
        (seq << 8) | ch, (type << 24) | (flags << 16) | (data << 8) | (seq >> 24)}};
    return e;
}
static void feed(tracegpu_uart_event e) {
    assert(tracegpu_uart_dual_feed(&stream, &e, sizeof(e)) == 1);
}
int main(void)
{
    unsigned i;
    tracegpu_uart_event e;
    const tracegpu_uart_burst *burst;
    unsigned char unaligned[17];
    tracegpu_uart_dual_clear(&stream);
    for (i = 0; i < 256; ++i) {
        feed(event(0, 2, i, 1000U + i * 100U, i, 0));
        feed(event(1, 2, i, 500U + i * 10U, 255U-i, i == 3 ? 3 : 0));
    }
    feed(event(0, 4, 256, 30000, 0, 0));
    assert(stream.channel[0].bursts.count == 1);
    assert(stream.channel[1].bursts.count == 0);
    assert(stream.channel[1].bursts.active);
    feed(event(1, 4, 256, 5000, 0, 0));
    for (i = 0; i < 2; ++i) {
        assert(stream.channel[i].bytes == 256);
        assert(stream.channel[i].history.lost_events == 0);
        assert(stream.channel[i].timestamp_regressions == 0);
        burst = tracegpu_uart_burst_latest(&stream.channel[i].bursts);
        assert(burst && burst->byte_count == 256);
    }
    assert(stream.channel[1].frame_errors == 1 && stream.channel[1].parity_errors == 1);
    assert(tracegpu_uart_event_data(tracegpu_uart_ring_oldest(&stream.channel[0].history,255)) == 255);
    assert(tracegpu_uart_event_data(tracegpu_uart_ring_oldest(&stream.channel[1].history,255)) == 0);
    e = event(0, 2, 256, 30001, 1, 0);
    assert(tracegpu_uart_dual_feed(&stream, &e, sizeof(e)) == 0);
    assert(stream.channel[0].rejected_sequence == 1);
    feed(event(0, 2, 259, 30002, 1, 0));
    assert(stream.channel[0].history.lost_events == 2);
    assert(stream.channel[1].history.lost_events == 0);
    e = event(TRACEGPU_UART_CHANNELS, 2, 0, 0, 0, 0);
    assert(tracegpu_uart_dual_feed(&stream, &e, sizeof(e)) == 0);
    e = event(0, 0, 0, 0, 0, 0);
    assert(tracegpu_uart_dual_feed(&stream, &e, sizeof(e)) == 0);
    assert(tracegpu_uart_dual_feed(&stream, &e, 15) == -1);
    assert(stream.bad_channel == 1 && stream.bad_type == 1 && stream.bad_length == 1);
    assert(tracegpu_uart_dual_feed(&stream, NULL, 0) == 0);
    tracegpu_uart_dual_clear(&stream);
    feed(event(1, 2, 0xffffffffU, 100, 1, 0));
    e = event(1, 2, 0, 101, 2, 0);
    memcpy(unaligned + 1, &e, sizeof(e));
    assert(tracegpu_uart_dual_feed(&stream, unaligned + 1, 16) == 1);
    assert(stream.channel[1].history.lost_events == 0);
    e=event(1,2,1,99,3,0);assert(tracegpu_uart_dual_feed(&stream,&e,sizeof e)==0);
    assert(stream.channel[1].timestamp_regressions == 1);
    for (i = 2; i < 65540; ++i) feed(event(1, 2, i, 200U+i, i&255U, 0));
    assert(stream.channel[1].history.count == TRACEGPU_UART_EVENT_RING_RECORDS);
    assert(stream.channel[0].history.count == 0);
    assert(stream.channel[1].history.lost_events == 1);
    puts("PASS dual stream: interleave, payload, burst isolation, error isolation, sequence wrap, stale/time-back rejection, malformed input, unaligned input, history wrap");
    return 0;
}
