#ifndef TG_AI_GLOBAL_H
#define TG_AI_GLOBAL_H
#include "ai_slots.h"
/* R2 encoding and decision only on PS; all neural inference executes in PL.
 * Bounded cooperative work: one cell is <=336 integer MACs. No heap. */
typedef struct {
 uint8_t buffer[2][24*256];int32_t logits[3];
 unsigned layer,channel,time,length,input_channels,input_bank,done,failed,cells;
} tg_ai_global;
int tg_ai_global_begin(tg_ai_global *,const uint8_t raw[768],tg_ai_config);

unsigned tg_ai_global_decision(const int32_t logits[3],int32_t *margin_q8);
#endif
