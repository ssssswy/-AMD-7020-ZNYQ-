#include "capture_quality.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static tracegpu_quality_input clean(void){
 tracegpu_quality_input in;memset(&in,0,sizeof in);
 in.capture_id=42;in.generation=9;in.identity_valid=1;in.capture_started=1;
 in.scope=TG_QUALITY_SCOPE_SESSION;in.required_mask=TG_QUALITY_ACQUISITION_MASK;
 for(unsigned n=0;n<TG_QUALITY_METRICS;n++)in.counter[n]=(tracegpu_quality_counter){0,1,1,TG_QUALITY_SCOPE_SESSION,0};
 return in;
}
int main(void){
 tracegpu_quality_input in=clean(),copy;tracegpu_quality_status out,first;
 CHECK(sizeof(tracegpu_quality_status)<256U);
 tracegpu_quality_project(&in,&out);
 CHECK(out.level==TG_QUALITY_NO_FAULT_OBSERVED);
 CHECK(out.known_mask==TG_QUALITY_ALL_MASK&&out.uncertain_mask==0);
 CHECK(!out.interval_localized);
 for(unsigned n=0;n<TG_QUALITY_METRICS;n++){
  in=clean();in.counter[n].value=13;
  copy=in;tracegpu_quality_project(&in,&out);
  CHECK(!memcmp(&in,&copy,sizeof in));
  if(n<TG_QUALITY_HISTORY_EVICTED){CHECK(out.level==TG_QUALITY_AFFECTED);CHECK(out.acquisition_issue_mask==TG_QUALITY_BIT(n));}
  else {CHECK(out.level==TG_QUALITY_NO_FAULT_OBSERVED);CHECK(out.presentation_mask==TG_QUALITY_BIT(n));}
  CHECK(out.counter[n].value==13&&!out.interval_localized);
 }
 /* Correlated hardware loss and GAP remain separate source units. */
 in=clean();in.counter[TG_QUALITY_PL_DROP].value=100;in.counter[TG_QUALITY_RAW_GAP].value=2;
 tracegpu_quality_project(&in,&out);
 CHECK(out.counter[TG_QUALITY_PL_DROP].value==100&&out.counter[TG_QUALITY_RAW_GAP].value==2);
 CHECK(out.acquisition_issue_mask==(TG_QUALITY_BIT(TG_QUALITY_PL_DROP)|TG_QUALITY_BIT(TG_QUALITY_RAW_GAP)));
 /* Unsupported zero is not a measured zero; optional display unknowns do
  * not turn an otherwise checked acquisition into a receive-loss report. */
 for(unsigned n=0;n<TG_QUALITY_METRICS;n++){
  in=clean();in.counter[n].supported=0;tracegpu_quality_project(&in,&out);
  CHECK(out.unsupported_mask&TG_QUALITY_BIT(n));CHECK(!(out.known_mask&TG_QUALITY_BIT(n)));
  CHECK(out.level==(n<TG_QUALITY_HISTORY_EVICTED?TG_QUALITY_UNKNOWN:TG_QUALITY_NO_FAULT_OBSERVED));
  in=clean();in.counter[n].known=0;in.counter[n].value=99;tracegpu_quality_project(&in,&out);
  CHECK(!out.acquisition_issue_mask&&!out.presentation_mask);
 }
 /* There is no inferred unwrap or history subtraction. Producers explicitly
  * report reset/wrap uncertainty; modulo zero never fabricates exact zero. */
 in=clean();in.counter[TG_QUALITY_SEQUENCE_MISSING].value=UINT32_MAX;
 tracegpu_quality_project(&in,&first);CHECK(first.level==TG_QUALITY_AFFECTED);
 in.counter[TG_QUALITY_SEQUENCE_MISSING].value=0;in.counter[TG_QUALITY_SEQUENCE_MISSING].flags=TG_QUALITY_COUNT_WRAPPED;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_AFFECTED);CHECK(!(out.known_mask&TG_QUALITY_BIT(TG_QUALITY_SEQUENCE_MISSING)));
 in.counter[TG_QUALITY_SEQUENCE_MISSING].flags=TG_QUALITY_COUNT_RESET_UNKNOWN;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN);
 in.counter[TG_QUALITY_SEQUENCE_MISSING].value=7;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN&&!out.acquisition_issue_mask);
 CHECK(!out.counter[TG_QUALITY_SEQUENCE_MISSING].known);
 in=clean();in.capture_id=43;in.generation=10;tracegpu_quality_project(&in,&out);
 CHECK(out.level==TG_QUALITY_NO_FAULT_OBSERVED&&out.capture_id==43&&out.generation==10);
 in.counter[TG_QUALITY_PL_DROP].value=UINT64_MAX;in.counter[TG_QUALITY_PL_DROP].flags=TG_QUALITY_COUNT_SATURATED;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_AFFECTED);CHECK(!(out.known_mask&TG_QUALITY_BIT(TG_QUALITY_PL_DROP)));
 in=clean();in.counter[TG_QUALITY_RAW_GAP].value=1;in.counter[TG_QUALITY_RAW_GAP].flags=TG_QUALITY_COUNT_APPROXIMATE;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_AFFECTED);CHECK(!(out.known_mask&TG_QUALITY_BIT(TG_QUALITY_RAW_GAP)));
 /* A stopped snapshot or current viewport cannot prove the complete session
  * had no issue just because its retained subset has zero faults. */
 in=clean();in.stopped=1;for(unsigned n=0;n<TG_QUALITY_HISTORY_EVICTED;n++)in.counter[n].scope=TG_QUALITY_SCOPE_SNAPSHOT;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN);
 in.scope=TG_QUALITY_SCOPE_SNAPSHOT;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_NO_FAULT_OBSERVED);
 in.scope=TG_QUALITY_SCOPE_SESSION;in.counter[TG_QUALITY_RAW_GAP].value=1;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_AFFECTED&&!out.interval_localized);
 in=clean();in.required_mask=0;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN);
 in=clean();in.source_issue_flags=TG_QUALITY_SOURCE_SCOPE_UNCERTAIN;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN);
 for(unsigned f=TG_QUALITY_SOURCE_PARSER_FAILED;f<=TG_QUALITY_SOURCE_CAPTURE_ISSUE;f<<=1){
  in=clean();in.source_issue_flags=f;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_AFFECTED);
 }
 in=clean();in.identity_valid=0;in.counter[TG_QUALITY_PL_DROP].value=7;
 tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_UNKNOWN);
 in=clean();in.capture_started=0;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_WAITING);
 in=clean();in.capture_id=0;tracegpu_quality_project(&in,&out);CHECK(out.level==TG_QUALITY_NO_FAULT_OBSERVED);
 tracegpu_quality_project(0,&out);CHECK(out.level==TG_QUALITY_UNKNOWN&&out.uncertain_mask==TG_QUALITY_ALL_MASK);
 tracegpu_quality_project(&in,0);
 CHECK(!strcmp(tracegpu_quality_level_label(TG_QUALITY_UNKNOWN,0),"UNKNOWN"));
 CHECK(!strcmp(tracegpu_quality_metric_label(TG_QUALITY_PL_DROP,0),"PL DROP"));
 CHECK(!strcmp(tracegpu_quality_scope_label(TG_QUALITY_SCOPE_SESSION,0),"SESSION"));
 CHECK(strlen(tracegpu_quality_level_label(99,1))>0&&strlen(tracegpu_quality_metric_label(99,1))>0&&strlen(tracegpu_quality_scope_label(99,1))>0);
 printf("CAPTURE_QUALITY_PASS checks=%u status_bytes=%u no_total_loss_counter no_history_scan not_board_accepted\n",checks,(unsigned)sizeof out);
 return 0;
}
