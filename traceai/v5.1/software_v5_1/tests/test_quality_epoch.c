#include "quality_epoch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)

static unsigned metric_for(unsigned slot){return slot>=8?TG_QUALITY_PARSE_REJECT:slot>=4?TG_QUALITY_SEQUENCE_MISSING:TG_QUALITY_PL_DROP;}
int main(void){
 tracegpu_quality_epoch state,copy;uint32_t mask;
 CHECK(sizeof state<=64U);
 tracegpu_quality_epoch_init(&state);CHECK(!state.initialized&&!state.seen_slots&&!state.uncertain_metrics);
 for(unsigned slot=0;slot<TG_QUALITY_EPOCH_SLOTS;slot++){
  unsigned mode=slot<8?TG_QUALITY_EPOCH_UART:TG_QUALITY_EPOCH_VECTOR;
  unsigned metric=metric_for(slot);tracegpu_quality_epoch_init(&state);mask=UINT32_MAX;
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,10,&mask));
  CHECK(!mask&&state.last[slot]==10&&state.seen_slots==(1U<<slot));
  CHECK(state.observed_nonzero_metrics==TG_QUALITY_BIT(metric));
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,10,&mask)&&!mask);
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,20,&mask)&&!mask);
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,19,&mask));
  CHECK(mask==TG_QUALITY_BIT(metric));
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,21,&mask));
  CHECK(mask==TG_QUALITY_BIT(metric));
  /* Hardware counter wrap is deliberately not guessed or unwrapped. */
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,UINT32_MAX,&mask));
  CHECK(!tracegpu_quality_epoch_observe(&state,42,mode,1,slot,0,&mask));
  CHECK(mask==TG_QUALITY_BIT(metric));
  CHECK(state.observed_nonzero_metrics==TG_QUALITY_BIT(metric));
  CHECK(!tracegpu_quality_epoch_observe(&state,43,mode,1,slot,0,&mask));
  CHECK(!mask&&!state.observed_nonzero_metrics&&state.seen_slots==(1U<<slot));
 }
 /* Each slot is independent: a first lower sample in a different engine
  * does not imply a decrease. A metric remains uncertain after other polls. */
 tracegpu_quality_epoch_init(&state);
 for(unsigned n=0;n<8;n++)CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_UART,1,n,100U-n,&mask)&&!mask);
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_UART,1,3,0,&mask));
 CHECK(mask==TG_QUALITY_BIT(TG_QUALITY_PL_DROP));
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_UART,1,7,0,&mask));
 CHECK(mask==(TG_QUALITY_BIT(TG_QUALITY_PL_DROP)|TG_QUALITY_BIT(TG_QUALITY_SEQUENCE_MISSING)));
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_VECTOR,1,8,0,&mask));
 CHECK(!mask&&state.seen_slots==(1U<<8)&&!state.observed_nonzero_metrics);
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_VECTOR,1,8,7,&mask));
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_VECTOR,1,9,0,&mask)&&!mask);
 CHECK(!tracegpu_quality_epoch_observe(&state,1,TG_QUALITY_EPOCH_VECTOR,1,8,0,&mask));
 CHECK(mask==TG_QUALITY_BIT(TG_QUALITY_PARSE_REJECT));
 /* Explicit validity, invalid mode and invalid slots reject without a state
  * change, including an attempted change to a different capture identity. */
 copy=state;mask=UINT32_C(0x12345678);
 CHECK(tracegpu_quality_epoch_observe(&state,2,TG_QUALITY_EPOCH_VECTOR,0,8,0,&mask)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state)&&mask==UINT32_C(0x12345678));
 CHECK(tracegpu_quality_epoch_observe(&state,2,2,1,8,0,&mask)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state));
 CHECK(tracegpu_quality_epoch_observe(&state,2,TG_QUALITY_EPOCH_VECTOR,1,11,0,&mask)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state));
 CHECK(tracegpu_quality_epoch_observe(&state,2,TG_QUALITY_EPOCH_VECTOR,1,0,0,&mask)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state));
 CHECK(tracegpu_quality_epoch_observe(&state,2,TG_QUALITY_EPOCH_UART,1,8,0,&mask)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state));
 CHECK(tracegpu_quality_epoch_observe(0,2,TG_QUALITY_EPOCH_VECTOR,1,8,0,&mask)==-1);
 CHECK(tracegpu_quality_epoch_observe(&state,2,TG_QUALITY_EPOCH_VECTOR,1,8,0,0)==-1);
 CHECK(!memcmp(&copy,&state,sizeof state));
 tracegpu_quality_epoch_init(0);
 CHECK(!tracegpu_quality_epoch_observe(&state,0,TG_QUALITY_EPOCH_UART,1,0,0,&mask)&&!mask);
 CHECK(state.initialized&&state.capture_id==0);
 /* Integration: reset-ambiguous numeric totals stay unknown, with prior
  * positive cause evidence retained separately rather than added twice. */
 {
  tracegpu_quality_input in={0};tracegpu_quality_status out;
  CHECK(!tracegpu_quality_epoch_observe(&state,0,TG_QUALITY_EPOCH_UART,1,0,1,&mask));
  CHECK(!tracegpu_quality_epoch_observe(&state,0,TG_QUALITY_EPOCH_UART,1,0,0,&mask));
  in.identity_valid=in.capture_started=1;in.scope=TG_QUALITY_SCOPE_SESSION;
  in.required_mask=TG_QUALITY_BIT(TG_QUALITY_PL_DROP);
  in.counter[TG_QUALITY_PL_DROP]=(tracegpu_quality_counter){0,1,1,TG_QUALITY_SCOPE_SESSION,TG_QUALITY_COUNT_RESET_UNKNOWN};
  if(mask&state.observed_nonzero_metrics)in.source_issue_flags|=TG_QUALITY_SOURCE_CAPTURE_ISSUE;
  tracegpu_quality_project(&in,&out);
  CHECK(out.level==TG_QUALITY_AFFECTED);
  CHECK(!out.counter[TG_QUALITY_PL_DROP].known);
  CHECK(!out.acquisition_issue_mask);
 }
 printf("QUALITY_EPOCH_PASS checks=%u state_bytes=%u slots=11 no_inferred_wrap no_pseudo_total not_board_accepted\n",checks,(unsigned)sizeof state);
 return 0;
}
