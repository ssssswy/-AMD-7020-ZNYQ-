#include "uart_burst_index.h"
#include "uart_analyzer_hw.h"

#include <string.h>

void tracegpu_uart_burst_index_clear(tracegpu_uart_burst_index *index)
{
    if (index != 0) {
        memset(index, 0, sizeof(*index));
    }
}

static void tracegpu_uart_burst_begin(tracegpu_uart_burst_index *index,
                                      const tracegpu_uart_event *event)
{
    uint64_t timestamp = tracegpu_uart_event_timestamp(event);
    uint32_t sequence = tracegpu_uart_event_sequence(event);

    index->active = 1U;
    index->active_start_ticks = timestamp;
    index->active_last_ticks = timestamp;
    index->active_first_sequence = sequence;
    index->active_last_sequence = sequence;
    index->active_byte_count = 0U;
}

static void tracegpu_uart_burst_commit(tracegpu_uart_burst_index *index,
                                       const tracegpu_uart_event *event)
{
    tracegpu_uart_burst *burst;

    if (!index->active) {
        return;
    }

    burst = &index->records[index->head];
    burst->start_ticks = index->active_start_ticks;
    burst->end_ticks = index->active_last_ticks;
    burst->flush_ticks = tracegpu_uart_event_timestamp(event);
    burst->first_sequence = index->active_first_sequence;
    burst->last_sequence = index->active_last_sequence;
    burst->byte_count = index->active_byte_count;
    burst->ordinal = ++index->total_completed;
    burst->valid = 1U;

    index->head = (index->head + 1U) % TRACEGPU_UART_BURST_CAPACITY;
    if (index->count < TRACEGPU_UART_BURST_CAPACITY) {
        index->count++;
    }
    index->active = 0U;
}

void tracegpu_uart_burst_index_feed(tracegpu_uart_burst_index *index,
                                    const tracegpu_uart_event *event)
{
    uint8_t type;

    if (index == 0 || event == 0) {
        return;
    }

    type = tracegpu_uart_event_type(event);
    if (type == TRACEGPU_RECORD_RAW_EDGE ||
        type == TRACEGPU_RECORD_DECODED_BYTE) {
        if (!index->active) {
            tracegpu_uart_burst_begin(index, event);
        }
        index->active_last_ticks = tracegpu_uart_event_timestamp(event);
        index->active_last_sequence = tracegpu_uart_event_sequence(event);
        if (type == TRACEGPU_RECORD_DECODED_BYTE) {
            index->active_byte_count++;
        }
    } else if (type == TRACEGPU_RECORD_PACKET_END) {
        tracegpu_uart_burst_commit(index, event);
    }
}

const tracegpu_uart_burst *tracegpu_uart_burst_latest(
    const tracegpu_uart_burst_index *index)
{
    uint32_t slot;

    if (index == 0 || index->count == 0U) {
        return 0;
    }
    slot = (index->head + TRACEGPU_UART_BURST_CAPACITY - 1U) %
           TRACEGPU_UART_BURST_CAPACITY;
    return &index->records[slot];
}
