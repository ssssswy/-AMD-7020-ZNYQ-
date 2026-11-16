#include "logic_review.h"
#include "logic_measure_task.h"
#include "trigger_store.h"
#include "trigger_config.h"
#include "uart_dual_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CAP 50000U
static tracegpu_vector_record raw[CAP];
static tracegpu_vector_stream stream;
static tracegpu_logic_trace_view view;
static tracegpu_decode_manager manager;
static tracegpu_logic_review review;
static uint64_t at;
static unsigned levels;
static void put(unsigned type,uint64_t tick,unsigned value){
 tracegpu_vector_event e={tick,stream.expected_sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:stream.history.capture_mask),(uint8_t)value};
 tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);
}
static void reset(unsigned mask,unsigned initial){
 tracegpu_vector_stream_init(&stream,raw,CAP,0,0);assert(!tracegpu_vector_stream_configure(&stream,mask));
 levels=initial;at=1000;put(TG_VEC_INIT,0,levels);
}
static void wire(unsigned ch,unsigned value,uint64_t tick){unsigned next=(levels&~(1U<<ch))|(value<<ch);if(next!=levels){levels=next;put(TG_VEC_CHANGE,tick,levels);}}
static void bind(void){put(TG_VEC_END,at,levels);assert(!tracegpu_logic_view_bind(&view,&stream.history,77,88,1));}
static tracegpu_decode_config config(unsigned protocol){tracegpu_decode_config c={0};c.protocol=(uint8_t)protocol;for(unsigned i=0;i<4;i++)c.map[i]=(uint8_t)(4+i);c.uart=(tracegpu_uart_decode_config){9600,7,8,0,1};c.spi=(tracegpu_spi_config){0,0,0,2};return c;}
static void raw_mask_and_lanes(void){
 reset(255,255);for(unsigned ch=0;ch<8;ch++){wire(ch,0,at++);wire(ch,1,at++);}bind();assert(stream.line_edges==16&&stream.changes==16);
 tracegpu_uart_event_ring lanes[8];memset(lanes,0,sizeof lanes);for(unsigned ch=0;ch<8;ch++){tracegpu_uart_ring_vector_view(&lanes[ch],&stream,ch);assert(lanes[ch].vector==&stream.history);unsigned value;assert(tracegpu_uart_ring_event_state(&lanes[ch],tracegpu_uart_ring_newest(&lanes[ch],0),&value)&&value==1);}
 assert(tracegpu_vector_stream_configure(&stream,128)<0);
 reset(128,128);wire(7,0,at++);wire(7,1,at++);bind();tracegpu_uart_ring_vector_view(&lanes[0],&stream,0);unsigned value;assert(!tracegpu_uart_ring_event_state(&lanes[0],tracegpu_uart_ring_newest(&lanes[0],0),&value));
 tracegpu_decode_config c=config(0);c.map[0]=0;assert(tracegpu_decode_begin(&manager,0,&view,&c,1)<0);c.map[0]=7;assert(!tracegpu_decode_begin(&manager,0,&view,&c,1));
 stream.history.capture_mask=255;assert(!tracegpu_logic_view_valid(&view,77,88));
 reset(128,128);tracegpu_vector_record bad=tracegpu_vector_pack((tracegpu_vector_event){at,1,TG_VEC_CHANGE,128,1});assert(tracegpu_vector_feed(&stream,&bad,sizeof bad)<0&&stream.failed);
 puts("EIGHT_VECTOR_MASK_LANES_PASS raw16bytes shared8 no_uncollected_zero stale_identity");
}
static uint64_t bit_at(uint64_t start,unsigned b){return start+((uint64_t)b*100000000U+4800U)/9600U;}
static void uart(unsigned byte){uint64_t start=at;unsigned b=0;wire(7,0,bit_at(start,b++));for(unsigned i=0;i<8;i++)wire(7,(byte>>i)&1U,bit_at(start,b++));wire(7,1,bit_at(start,b++));at=bit_at(start,b)+20;}
static void uart_tail(void){
 reset(255,255);at=0x100000100ULL;for(unsigned i=0;i<2048;i++)uart(i&255U);bind();tracegpu_decode_config c=config(0);c.map[0]=7;
 assert(!tracegpu_decode_begin(&manager,0,&view,&c,5));c.map[0]=0;c.uart.channel=0;assert(!tracegpu_decode_begin(&manager,1,&view,&c,6));
 while(manager.task[0].busy||manager.task[1].busy){tracegpu_decode_step(&manager,0,7);tracegpu_decode_step(&manager,1,1);}assert(manager.task[0].total==2048&&manager.task[0].count==512&&manager.task[1].total==0);
 c.map[0]=7;c.uart.channel=7;tracegpu_search_filter f={0};f.protocol=0;f.channel=7;f.address=f.read=-1;f.length=1;f.bytes[0]=f.mask[0]=255;
 assert(!tracegpu_logic_review_begin(&review,&view,&c,5,9,view.begin,view.end,&f,0,32));while(review.state==TG_REVIEW_BUSY)assert(tracegpu_logic_review_step(&review,7)>=0);
 assert(review.matches==8&&review.count==8&&review.results[7].ordinal==2047&&review.results[7].row.channel==7&&review.results[7].row.value==255&&!review.results[7].row.flags);
 tracegpu_logic_review_cancel(&review);assert(!review.count&&!tracegpu_logic_review_valid(&review,5,9));
 puts("EIGHT_CH8_UART_REVIEW_PASS 2048frames tailFF independent2 64bit cancel");
}
static void step_wire(unsigned ch,unsigned v){at+=20;wire(ch,v,at);}
static void i2c_byte(unsigned byte,unsigned nack){for(unsigned i=0;i<8;i++){step_wire(7,(byte>>(7-i))&1U);step_wire(4,1);step_wire(4,0);}step_wire(7,nack);step_wire(4,1);step_wire(4,0);}
static void high_protocols(void){
 reset(255,255);step_wire(7,0);step_wire(4,0);i2c_byte(0xA0,0);i2c_byte(0x55,0);step_wire(7,0);step_wire(4,1);step_wire(7,1);at+=100;bind();
 tracegpu_decode_config c=config(1);c.map[0]=4;c.map[1]=7;assert(!tracegpu_decode_begin(&manager,0,&view,&c,1));while(manager.task[0].busy)tracegpu_decode_step(&manager,0,1);tracegpu_decode_row r;assert(!tracegpu_decode_row_get(&manager,0,0,&r));assert(r.channel==4&&(r.value>>1)==0x50&&(r.flags&I2C_STOP));
 reset(255,0xEF);step_wire(7,0);for(unsigned i=0;i<8;i++){step_wire(5,(0xA5>>(7-i))&1U);step_wire(6,(0x3C>>(7-i))&1U);step_wire(4,1);step_wire(4,0);}step_wire(7,1);at+=100;bind();c=config(2);
 assert(!tracegpu_decode_begin(&manager,0,&view,&c,2));while(manager.task[0].busy)tracegpu_decode_step(&manager,0,7);assert(!tracegpu_decode_row_get(&manager,0,0,&r));assert(r.channel==4&&(r.value&65535)==0xA5&&(r.value>>16)==0x3C&&!r.flags);
 c.map[3]=c.map[0];assert(!tracegpu_decode_config_valid(&c));
 puts("EIGHT_HIGH_I2C_SPI_PASS CH5_CH8_i2c CH5to8_spi repeated_map_rejected");
}
static void measure_and_gap(void){
 reset(255,127);for(unsigned i=0;i<1025;i++){wire(7,1,100+100*i);wire(7,0,150+100*i);}at=102600;bind();tracegpu_logic_measure_task m;
 assert(!tracegpu_measure_begin(&m,&view,7,0,view.end,3));while(m.state==TG_MEASURE_RUNNING)tracegpu_measure_step(&m,7);assert(m.result.valid&&m.result.period==100&&m.result.high==50&&m.result.cycles==1024);
 reset(128,128);wire(7,0,at++);put(TG_VEC_GAP,at++,0);put(TG_VEC_RESYNC,at++,128);levels=128;at+=100;bind();assert(!tracegpu_measure_begin(&m,&view,7,0,view.end,4));while(m.state==TG_MEASURE_RUNNING)tracegpu_measure_step(&m,1);assert(!m.result.valid&&m.result.gap);
 puts("EIGHT_CH8_MEASURE_GAP_PASS period100 high50 1024cycles no_false_GAP_result");
}
static void trigger_mask(void){
 tracegpu_trigger_config c;tracegpu_trigger_config_default(&c);memset(c.slots,0,sizeof c.slots);c.slots[0].type=TG_TRIG_PULSE;c.slots[0].channel=7;c.slots[0].min_ticks=100;
 assert(!tracegpu_trigger_config_valid(&c,255,2,0));assert(tracegpu_trigger_config_valid(&c,15,0,0)<0);c.slots[0].type=TG_TRIG_ERROR;c.slots[0].error_mask=1;assert(tracegpu_trigger_config_valid(&c,255,2,0)<0);
 memset(c.slots,0,sizeof c.slots);c.combine=TG_TRIG_LEVEL;c.level_mask=0x81;c.level_value=0x80;assert(!tracegpu_trigger_config_valid(&c,255,2,0));
 tracegpu_trigger_store s;tracegpu_vector_record memory[32];assert(!tracegpu_trigger_store_init(&s,memory,32,1,128));assert(!tracegpu_trigger_store_arm(&s,1,2,0,128,128,100,100,8,8));
 tracegpu_vector_record init=tracegpu_vector_pack((tracegpu_vector_event){0,0,TG_VEC_INIT,128,128});assert(tracegpu_trigger_store_push(&s,0,&init)==1);assert(!tracegpu_trigger_store_lock(&s,100,90));tracegpu_vector_record end=tracegpu_vector_pack((tracegpu_vector_event){200,1,TG_VEC_END,128,128});assert(tracegpu_trigger_store_push(&s,0,&end)==1);assert(!tracegpu_trigger_store_finish(&s,200,1));
 tracegpu_vector_stream v;assert(!tracegpu_trigger_store_vector_view(&s,&v));assert(v.history.capture_mask==128&&v.history.channel_count==8&&v.levels==128);
 puts("EIGHT_TRIGGER_STORE_PASS source80 independent_levels_known fixed4predicates shared_vector");
}
int main(void){assert(sizeof(tracegpu_vector_record)==16);assert(TRACEGPU_UART_CHANNELS==4);raw_mask_and_lanes();uart_tail();high_protocols();measure_and_gap();trigger_mask();puts("V3_2_EIGHT_LOGIC_CORE_PASS_NOT_BOARD_ACCEPTED");return 0;}
