#include "quality_epoch.h"
#include <string.h>

void tracegpu_quality_epoch_init(tracegpu_quality_epoch *state){
 if(state)memset(state,0,sizeof *state);
}
int tracegpu_quality_epoch_observe(tracegpu_quality_epoch *state,uint32_t id,
 unsigned mode,int identity_valid,unsigned slot,uint32_t raw,uint32_t *mask){
 uint32_t metric;
 uint16_t bit;
 if(!state||!mask||!identity_valid||mode>TG_QUALITY_EPOCH_VECTOR||slot>=TG_QUALITY_EPOCH_SLOTS)return -1;
 if((mode==TG_QUALITY_EPOCH_UART&&slot>=TG_QUALITY_EPOCH_VECTOR_BAD)||
    (mode==TG_QUALITY_EPOCH_VECTOR&&slot<TG_QUALITY_EPOCH_VECTOR_BAD))return -1;
 if(mode==TG_QUALITY_EPOCH_VECTOR)metric=TG_QUALITY_PARSE_REJECT;
 else metric=slot<TG_QUALITY_EPOCH_UART_LOST_FIRST?TG_QUALITY_PL_DROP:TG_QUALITY_SEQUENCE_MISSING;
 if(!state->initialized||state->capture_id!=id||state->mode!=mode){
  tracegpu_quality_epoch_init(state);state->capture_id=id;
  state->mode=(uint8_t)mode;state->initialized=1;
 }
 bit=(uint16_t)(1U<<slot);
 if((state->seen_slots&bit)&&raw<state->last[slot])state->uncertain_metrics|=TG_QUALITY_BIT(metric);
 state->last[slot]=raw;state->seen_slots|=bit;
 if(raw)state->observed_nonzero_metrics|=TG_QUALITY_BIT(metric);
 *mask=state->uncertain_metrics;
 return 0;
}
