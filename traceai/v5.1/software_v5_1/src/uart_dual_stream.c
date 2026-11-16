#include "uart_dual_stream.h"
#include <string.h>
#include "capture_memory.h"
static tracegpu_uart_record_observer record_observer;
static void *observer_context;
void tracegpu_uart_dual_set_observer(tracegpu_uart_record_observer observer,void *context){record_observer=observer;observer_context=context;}

void tracegpu_uart_dual_clear(tracegpu_uart_dual_stream *stream)
{
    if (stream) {
        memset(stream, 0, sizeof(*stream));
#ifdef TRACEGPU_HISTORY_EXTERNAL
        (void)tracegpu_capture_memory_bind(stream);
#endif
    }
}

int tracegpu_uart_dual_feed(tracegpu_uart_dual_stream *stream,
                           const void *data, size_t bytes)
{
    size_t offset;
    int accepted = 0;
    if (!stream) return -1;
    if ((!data && bytes) || bytes % sizeof(tracegpu_uart_event) ||
        bytes / sizeof(tracegpu_uart_event) > 0x7fffffffU) {
        stream->bad_length++;
        return -1;
    }
    for (offset = 0; offset < bytes; offset += sizeof(tracegpu_uart_event)) {
        tracegpu_uart_event event;
        tracegpu_uart_channel_stream *channel;
        uint32_t id, type, sequence, delta;
        uint64_t timestamp;
        /* DMA records are little-endian words on the Zynq ARM target.
         * memcpy also allows an unaligned caller buffer. */
        memcpy(&event, (const unsigned char *)data + offset, sizeof(event));
        id = event.word[2] & 0xffU;
        type = tracegpu_uart_event_type(&event);
        if (id >= TRACEGPU_UART_CHANNELS) { stream->bad_channel++; continue; }
        if (type < 1U || type > 5U || (type==5U&&(id>=4U||(tracegpu_uart_event_flags(&event)>>4)!=15U||tracegpu_uart_event_data(&event)!=0))) { stream->bad_type++; continue; }
        channel = &stream->channel[id];
        sequence = tracegpu_uart_event_sequence(&event);
        delta = sequence - channel->history.expected_sequence;
        /* Modulo comparison permits wrap, rejects duplicate/stale records.
         * Gaps >= 2^31 records require a new session, not inferred loss. */
        if (channel->history.sequence_valid && delta >= 0x80000000U) {
            channel->rejected_sequence++;
            continue;
        }
        timestamp = tracegpu_uart_event_timestamp(&event);
        if (channel->timestamp_valid && timestamp < channel->last_timestamp){
            channel->timestamp_regressions++;
            continue; /* Ordered history and exact measurements must not consume this record. */
        }
        channel->last_timestamp = timestamp;
        channel->timestamp_valid = 1U;
        tracegpu_uart_ring_push(&channel->history, &event);
        tracegpu_uart_burst_index_feed(&channel->bursts, &event);
        channel->records++;
        if(type==4U)channel->tails++;
        if (type == 2U) {
            uint8_t flags = tracegpu_uart_event_flags(&event);
            channel->bytes++;
            channel->frame_errors += (flags & 1U) != 0U;
            channel->parity_errors += (flags & 2U) != 0U;
        }
        accepted++;
        if(record_observer)record_observer(id,&event,observer_context);
    }
    return accepted;
}
