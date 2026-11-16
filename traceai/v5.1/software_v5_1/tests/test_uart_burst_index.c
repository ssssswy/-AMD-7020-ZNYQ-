#include "uart_analyzer_hw.h"
#include "uart_burst_index.h"

#include <assert.h>
#include <stdio.h>

uint8_t tracegpu_uart_event_type(const tracegpu_uart_event *event)
{
    return (uint8_t)(event->word[3] >> 24);
}

uint8_t tracegpu_uart_event_flags(const tracegpu_uart_event *event)
{
    return (uint8_t)(event->word[3] >> 16);
}

uint8_t tracegpu_uart_event_data(const tracegpu_uart_event *event)
{
    return (uint8_t)(event->word[3] >> 8);
}

uint8_t tracegpu_uart_event_level(const tracegpu_uart_event *event)
{
    return (uint8_t)(tracegpu_uart_event_flags(event) & 1U);
}

uint32_t tracegpu_uart_event_sequence(const tracegpu_uart_event *event)
{
    return ((event->word[3] & 0xffU) << 24) | (event->word[2] >> 8);
}

uint64_t tracegpu_uart_event_timestamp(const tracegpu_uart_event *event)
{
    return ((uint64_t)event->word[1] << 32) | event->word[0];
}

static tracegpu_uart_event make_event(uint8_t type, uint8_t data,
                                      uint32_t sequence, uint64_t timestamp)
{
    tracegpu_uart_event event = {{0U, 0U, 0U, 0U}};
    event.word[0] = (uint32_t)timestamp;
    event.word[1] = (uint32_t)(timestamp >> 32);
    event.word[2] = (sequence & 0x00ffffffU) << 8;
    event.word[3] = ((uint32_t)type << 24) |
                    ((uint32_t)data << 8) |
                    (sequence >> 24);
    return event;
}

static void feed_burst(tracegpu_uart_burst_index *index, uint32_t ordinal,
                       uint32_t byte_count, uint64_t start)
{
    uint32_t sequence = ordinal * 1000U;
    uint32_t byte_index;
    tracegpu_uart_event event =
        make_event(TRACEGPU_RECORD_RAW_EDGE, 0U, sequence++, start);
    tracegpu_uart_burst_index_feed(index, &event);

    for (byte_index = 0U; byte_index < byte_count; byte_index++) {
        event = make_event(TRACEGPU_RECORD_DECODED_BYTE,
                           (uint8_t)byte_index, sequence++,
                           start + 100U + byte_index * 100U);
        tracegpu_uart_burst_index_feed(index, &event);
    }
    event = make_event(TRACEGPU_RECORD_PACKET_END, 0U, sequence,
                       start + 100U + byte_count * 100U + 500U);
    tracegpu_uart_burst_index_feed(index, &event);
}

int main(void)
{
    tracegpu_uart_burst_index index;
    const tracegpu_uart_burst *latest;
    uint32_t burst;

    tracegpu_uart_burst_index_clear(&index);
    assert(tracegpu_uart_burst_latest(&index) == 0);

    feed_burst(&index, 0U, 16U, 1000U);
    latest = tracegpu_uart_burst_latest(&index);
    assert(latest != 0);
    assert(latest->ordinal == 1U);
    assert(latest->byte_count == 16U);
    assert(latest->start_ticks == 1000U);
    assert(latest->end_ticks == 2600U);
    assert(latest->flush_ticks == 3200U);

    feed_burst(&index, 1U, 256U, 100000U);
    latest = tracegpu_uart_burst_latest(&index);
    assert(latest->ordinal == 2U);
    assert(latest->byte_count == 256U);
    assert(latest->start_ticks == 100000U);
    assert(latest->end_ticks == 125600U);
    assert(index.count == 2U);

    for (burst = 2U; burst < 70U; burst++) {
        feed_burst(&index, burst, 4U, (uint64_t)burst * 100000U);
    }
    latest = tracegpu_uart_burst_latest(&index);
    assert(index.count == TRACEGPU_UART_BURST_CAPACITY);
    assert(index.total_completed == 70U);
    assert(latest->ordinal == 70U);
    assert(latest->byte_count == 4U);

    puts("PASS uart_burst_index: boundaries, byte counts, 64-entry wrap");
    return 0;
}
