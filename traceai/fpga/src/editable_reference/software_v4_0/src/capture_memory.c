#include "capture_memory.h"
#ifdef TRACEGPU_HISTORY_EXTERNAL
#if defined(__arm__)
static tracegpu_uart_event storage[TRACEGPU_UART_CHANNELS][TRACEGPU_UART_EVENT_RING_RECORDS]
 __attribute__((section(".capture_history"),aligned(64)));
#else
static tracegpu_uart_event storage[TRACEGPU_UART_CHANNELS][TRACEGPU_UART_EVENT_RING_RECORDS];
#endif
#endif
int tracegpu_capture_memory_bind(tracegpu_uart_dual_stream *stream)
{
 if(!stream)return -1;
#ifdef TRACEGPU_HISTORY_EXTERNAL
 unsigned ch;
#if defined(__arm__)
 uintptr_t begin=(uintptr_t)storage,end=begin+sizeof(storage);
 if(begin<TRACEGPU_HISTORY_BASE||end>TRACEGPU_HISTORY_LIMIT||(begin&63U))return -2;
#endif
 for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++)stream->channel[ch].history.records=storage[ch];
#endif
 return 0;
}
