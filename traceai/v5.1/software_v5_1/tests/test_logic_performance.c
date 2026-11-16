#include "logic_measure_task.h"
#include "logic_annotation_query.h"
#include "ui_refresh_policy.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static tracegpu_vector_record records[4096];
static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;
static tracegpu_decode_manager manager;
/* Isolate query geometry from the independently tested decoder manager. */
int tracegpu_decode_row_get(const tracegpu_decode_manager *m,unsigned slot,unsigned i,tracegpu_decode_row *r){
 if(slot>=2||i>=m->task[slot].count)return -1;
 *r=m->task[slot].state.uart.rows[i];return 0;
}
static void build(void){
 unsigned n=0;
 records[n++]=tracegpu_vector_pack((tracegpu_vector_event){0,0,TG_VEC_INIT,15,0});
 for(unsigned i=0;i<1025;i++){
  records[n]=tracegpu_vector_pack((tracegpu_vector_event){100+100*i,n,TG_VEC_CHANGE,15,1});n++;
  records[n]=tracegpu_vector_pack((tracegpu_vector_event){150+100*i,n,TG_VEC_CHANGE,15,0});n++;
 }
 records[n]=tracegpu_vector_pack((tracegpu_vector_event){102600,n,TG_VEC_END,15,0});n++;
 ring=(tracegpu_vector_ring){records,4096,n,n,n,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};assert(!tracegpu_logic_view_bind(&view,&ring,10,20,1));
}
static void measure(void){
 tracegpu_logic_measure_task t;unsigned steps=0;
 build();assert(!tracegpu_measure_begin(&t,&view,0,0,view.end,1));
 assert(t.state==TG_MEASURE_RUNNING&&!t.result.valid&&tracegpu_measure_progress(&t)==0);
 while(t.state==TG_MEASURE_RUNNING){assert(!t.result.valid);tracegpu_measure_step(&t,7);steps++;assert(steps<1000);}
 assert(t.state==TG_MEASURE_DONE&&t.result.valid&&t.result.cycles==1024&&t.result.period==100&&t.result.high==50);
 assert(t.period_min==100&&t.period_max==100&&tracegpu_measure_progress(&t)==100&&steps>1);
 assert(!tracegpu_measure_begin(&t,&view,0,0,view.end,2));tracegpu_measure_step(&t,1);tracegpu_measure_cancel(&t);assert(t.state==TG_MEASURE_CANCELLED&&!t.result.valid);
 assert(!tracegpu_measure_begin(&t,&view,0,0,view.end,3));ring.total++;assert(tracegpu_measure_step(&t,7)<0&&t.state==TG_MEASURE_CANCELLED);ring.total--;
 records[3]=tracegpu_vector_pack((tracegpu_vector_event){200,3,TG_VEC_GAP,1,0});
 assert(!tracegpu_measure_begin(&t,&view,0,0,view.end,4));while(t.state==TG_MEASURE_RUNNING)tracegpu_measure_step(&t,3);assert(t.state==TG_MEASURE_INVALID&&t.result.gap&&!t.result.valid);
 assert(tracegpu_measure_begin(&t,&view,0,0,view.end+1,5)<0);
 puts("V3_1_MEASURE_PASS dense>512, splits7, complete-only, cancel, stale, GAP, range");
}
static void annotation(void){
 tracegpu_annotation_query q={0};build();memset(&manager,0,sizeof manager);
 tracegpu_decode_task *t=&manager.task[0];t->view=&view;t->capture_id=10;t->source_generation=20;t->generation=3;t->count=6;t->index=10;
 for(unsigned i=0;i<6;i++)t->state.uart.rows[i]=(tracegpu_decode_row){100*i,100*i+95,i,0,0,0};
 tracegpu_annotation_get(&q,&manager,0,40,570);assert(q.valid&&q.returned==4&&q.omitted==2&&q.rows[0].start==0);
 tracegpu_annotation_get(&q,&manager,0,96,99);assert(q.valid&&!q.returned);
 tracegpu_annotation_get(&q,&manager,0,450,570);assert(q.returned==2&&q.rows[0].start==400);
 t->cancelled=1;tracegpu_annotation_get(&q,&manager,0,40,570);assert(!q.valid&&!q.returned);
 puts("V3_1_ANNOTATION_PASS left-crossing, omitted, empty interval, cancellation");
}
static void refresh(void){
 tracegpu_refresh_policy p={0};assert(tracegpu_refresh_due(&p,0,0,0,0));tracegpu_refresh_begin(&p,0);
 for(unsigned i=1;i<=10000;i++)assert(!tracegpu_refresh_due(&p,i,0,0,0));
 assert(p.requested==1&&p.suppressed==10000);
 tracegpu_refresh_mark(&p);assert(tracegpu_refresh_due(&p,10001,0,0,0));tracegpu_refresh_begin(&p,10001);
 assert(!tracegpu_refresh_due(&p,10010,0,1,0));assert(tracegpu_refresh_due(&p,10021,0,1,0));tracegpu_refresh_begin(&p,10021);
 assert(!tracegpu_refresh_due(&p,11020,0,0,1));assert(tracegpu_refresh_due(&p,11021,0,0,1));tracegpu_refresh_begin(&p,11021);
 assert(!tracegpu_refresh_due(&p,11022,0,0,1));
 puts("V3_1_REFRESH_PASS 10000 idle loops suppressed, dirty, animation20ms, diagnostic1Hz");
}
#include "latency_stats.h"
static void latency(void){
 tracegpu_latency_stats s={0};tracegpu_latency_report r;
 tracegpu_latency_get(&s,&r);assert(!r.count&&!r.p95_us);
 for(unsigned i=1;i<=128;i++)tracegpu_latency_add(&s,i);
 tracegpu_latency_get(&s,&r);assert(r.count==128&&r.p50_us==64&&r.p95_us==122&&r.window_max_us==128&&r.max_us==128);
 tracegpu_latency_add(&s,1000);tracegpu_latency_get(&s,&r);assert(r.p50_us==65&&r.max_us==1000&&r.over_250==1);
 for(unsigned i=0;i<128;i++)tracegpu_latency_add(&s,10);
 tracegpu_latency_get(&s,&r);assert(r.p50_us==10&&r.p95_us==10&&r.window_max_us==10&&r.max_us==1000&&r.over_250==1);
 puts("V3_1_LATENCY_PASS exact nearest-rank window128, wrap, lifetime max/over250");
}
int main(void){measure();annotation();refresh();latency();return 0;}
