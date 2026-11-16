#include "logic_review.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CAPACITY 250000U
static tracegpu_vector_record records[CAPACITY];static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;static tracegpu_logic_review review;static tracegpu_decode_manager manager,saved;
static uint32_t used,levels;static uint64_t tick;
static void add(unsigned type,uint64_t t,unsigned l){assert(used<CAPACITY);records[used]=tracegpu_vector_pack((tracegpu_vector_event){t,used,(uint8_t)type,15,(uint8_t)l});used++;}
static void wire(unsigned ch,unsigned level,uint64_t t){unsigned next=(levels&~(1U<<ch))|(level<<ch);if(next!=levels){levels=next;add(TG_VEC_CHANGE,t,levels);}}
static void reset(unsigned l){used=0;levels=l;tick=1000;add(TG_VEC_INIT,0,l);}
static void bind(void){add(TG_VEC_END,tick,levels);ring=(tracegpu_vector_ring){records,CAPACITY,used,used,used,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};assert(!tracegpu_logic_view_bind(&view,&ring,21,31,1));}
static uint64_t bit_at(uint64_t start,unsigned b,unsigned baud){return start+((uint64_t)b*100000000U+baud/2)/baud;}
static void uart(unsigned value,unsigned bad){unsigned b=0;uint64_t start=tick;wire(0,0,bit_at(start,b++,9600));for(unsigned i=0;i<8;i++)wire(0,(value>>i)&1,bit_at(start,b++,9600));wire(0,bad?0:1,bit_at(start,b++,9600));tick=bit_at(start,b,9600);wire(0,1,tick);tick+=10;}
static tracegpu_decode_config config(unsigned protocol){tracegpu_decode_config c={0};c.protocol=(uint8_t)protocol;for(unsigned i=0;i<4;i++)c.map[i]=(uint8_t)i;c.uart=(tracegpu_uart_decode_config){9600,0,8,0,1};c.spi=(tracegpu_spi_config){0,0,0,2};return c;}
static tracegpu_search_filter filter(unsigned protocol){tracegpu_search_filter f={0};f.protocol=(uint8_t)protocol;f.address=-1;f.read=-1;return f;}
static void run(unsigned budget){unsigned calls=0;while(review.state==TG_REVIEW_BUSY){uint32_t index=review.index;assert(tracegpu_logic_review_step(&review,budget)>=0);assert(review.index-index<=budget);assert(++calls<CAPACITY+10);}assert(review.state==TG_REVIEW_DONE);assert(tracegpu_logic_review_progress(&review)==100);}
static void begin(const tracegpu_decode_config *c,const tracegpu_search_filter *f,unsigned page,unsigned size){assert(!tracegpu_logic_review_begin(&review,&view,c,4,5,view.begin,view.end,f,page,size));assert(!tracegpu_logic_review_progress(&review));}
static void uart_capacity(void){
 tracegpu_decode_config c=config(TG_DECODE_UART);reset(15);tick=0x100000100ULL;for(unsigned i=0;i<2048;i++)uart(i&255,0);bind();
 assert(!tracegpu_decode_begin(&manager,0,&view,&c,44));assert(!tracegpu_decode_begin(&manager,1,&view,&c,45));while(manager.task[0].busy||manager.task[1].busy){tracegpu_decode_step(&manager,0,127);tracegpu_decode_step(&manager,1,7);}saved=manager;
 for(unsigned step=1;step<=127;step=step==1?7:step==7?127:128){begin(&c,0,255,8);run(step);assert(review.decoded_total==2048&&review.matches==2048&&review.count==8);for(unsigned i=0;i<8;i++){tracegpu_review_result r;assert(!tracegpu_logic_review_result_get(&review,i,&r));assert(r.ordinal==2040+i&&r.row.value==248+i&&!r.row.flags&&r.row.start>UINT32_MAX);assert(r.capture_id==21&&r.source_generation==31&&r.slot_revision==4&&r.query_revision==5);}assert(!memcmp(&manager,&saved,sizeof manager));}
 tracegpu_search_filter f=filter(0);f.length=1;f.bytes[0]=255;f.mask[0]=255;begin(&c,&f,0,32);run(7);assert(review.matches==8&&review.count==8);for(unsigned i=0;i<8;i++)assert(review.results[i].ordinal==i*256U+255U);
 begin(&c,&f,1,4);run(127);assert(review.count==4&&review.results[3].ordinal==2047&&!review.has_more);
 f.length=2;f.bytes[0]=0xf0;f.bytes[1]=0xf0;f.mask[0]=f.mask[1]=0xf0;begin(&c,&f,0,32);run(7);assert(review.matches==120&&review.has_more);
 uint64_t lo=manager.task[0].state.uart.rows[256].start;assert(!tracegpu_logic_review_begin(&review,&view,&c,4,5,lo,view.end,0,0,8));assert(review.progress_begin_index>0&&tracegpu_logic_review_progress(&review)==0);run(1);assert(review.decoded_total==1792&&review.results[0].row.value==0);
 assert(!tracegpu_decode_begin_range(&manager,0,&view,&c,44,lo,view.end));assert(manager.task[0].progress_begin_index>0&&manager.task[0].index==manager.task[0].progress_begin_index);while(manager.task[0].busy)tracegpu_decode_step(&manager,0,7);assert(manager.task[0].total==1792&&manager.task[0].state.uart.rows[0].value==0);
 puts("REVIEW_UART_CAPACITY_PASS 2048 frames first/middle/last stable ordinals splits=1,7,127 masks/64bit/independent slots");
}
static void uart_boundaries(void){
 tracegpu_decode_config c=config(0);tracegpu_search_filter f=filter(0);f.length=2;f.bytes[0]=0xaa;f.bytes[1]=0xbb;f.mask[0]=f.mask[1]=255;
 reset(15);uart(0xaa,0);add(TG_VEC_GAP,tick,levels);tick+=100000;add(TG_VEC_RESYNC,tick,15);levels=15;tick+=100000;uart(0xbb,0);uart(0xaa,0);uart(0xbb,0);bind();begin(&c,&f,0,32);run(1);assert(review.matches==1&&review.results[0].ordinal==2);
 begin(&c,0,0,8);tracegpu_logic_review_step(&review,7);tracegpu_logic_review_cancel(&review);assert(review.state==TG_REVIEW_CANCELLED&&!review.count&&tracegpu_logic_review_step(&review,7)<0);
 begin(&c,0,0,8);run(7);assert(!tracegpu_logic_review_valid(&review,99,5));ring.total++;assert(tracegpu_logic_review_result_get(&review,0,&(tracegpu_review_result){0})<0);assert(tracegpu_logic_review_step(&review,7)<0&&review.state==TG_REVIEW_STALE&&!review.count);ring.total--;
 begin(&c,0,0,8);ring.total++;assert(tracegpu_logic_review_step(&review,7)<0&&review.state==TG_REVIEW_STALE&&!review.count);ring.total--;
 reset(15);uart(0xaa,1);wire(0,0,tick);tick+=100;bind();f.errors=1;begin(&c,&f,0,32);run(1);assert(review.matches==2&&(review.results[0].row.flags&TG_UART_FRAME_ERROR)&&(review.results[1].row.flags&TG_UART_PARTIAL));
 puts("REVIEW_UART_BOUNDARIES_PASS GAP/RESYNC no sequence bridge error semantics cancel/stale/revision");
}
static void i2c_byte(unsigned value,unsigned nack){
 for(unsigned i=0;i<8;i++){wire(0,0,tick);tick+=20;wire(1,(value>>(7-i))&1,tick);tick+=20;wire(0,1,tick);tick+=20;wire(0,0,tick);tick+=20;}
 wire(1,nack,tick);tick+=20;wire(0,1,tick);tick+=20;wire(0,0,tick);tick+=20;
}
static void i2c_transaction(unsigned read,unsigned bytes,unsigned addr_nack,unsigned last_nack){
 wire(1,0,tick);tick+=20;i2c_byte(0xa0|read,addr_nack);for(unsigned i=0;i<bytes;i++)i2c_byte(i,last_nack&&i+1==bytes);
 wire(1,0,tick);tick+=20;wire(0,1,tick);tick+=20;wire(1,1,tick);tick+=20;
}
static void i2c_capacity(void){
 tracegpu_decode_config c=config(1);reset(3);for(unsigned i=0;i<299;i++)i2c_transaction(i&1,2,i==297,i&1);i2c_transaction(1,40,0,1);bind();
 for(unsigned chunk=1;chunk<=127;chunk=chunk==1?7:chunk==7?127:128){begin(&c,0,37,8);run(chunk);assert(review.decoded_total==300&&review.count==4);tracegpu_i2c_transaction *t=&review.results[3].i2c;assert(review.results[3].ordinal==299&&t->count==32&&t->total==40&&t->nack_total==1&&!t->nack&&(t->flags&I2C_TRUNCATED));for(unsigned i=0;i<32;i++)assert(t->data[i]==i);}
 tracegpu_search_filter f=filter(1);f.address=0x50;f.read=1;f.nack=2;begin(&c,&f,4,32);run(7);assert(review.matches==150&&review.count==22&&review.results[21].ordinal==299);
 f.read=-1;f.nack=1;begin(&c,&f,0,32);run(127);assert(review.matches==1&&review.results[0].ordinal==297);
 f.nack=3;begin(&c,&f,0,32);run(7);assert(review.matches==299);f.nack=4;begin(&c,&f,0,32);run(7);assert(review.matches==150);
 f.nack=0;f.errors=1;begin(&c,&f,0,32);run(7);assert(review.matches==1&&review.results[0].ordinal==299); /* normal STOP, read NACK not errors */
 f.length=1;assert(tracegpu_logic_review_begin(&review,&view,&c,4,5,view.begin,view.end,&f,0,32)<0); /* no misleading truncated-data full-text search */
 reset(3);wire(1,0,tick);tick+=20;bind();f=filter(1);f.nack=3;begin(&c,&f,0,32);run(1);assert(!review.matches);begin(&c,0,0,8);run(1);assert(review.count==1&&!review.results[0].i2c.address_valid&&(review.results[0].row.flags&I2C_PARTIAL));
 puts("REVIEW_I2C_CAPACITY_PASS 300 transactions tail/32-byte detail total40 ACK/NACK beyond cache normal STOP semantics");
}
static void spi_begin(void){wire(3,0,tick);tick+=20;}
static void spi_byte(unsigned tx,unsigned rx){for(unsigned i=0;i<8;i++){wire(1,(tx>>(7-i))&1,tick);wire(2,(rx>>(7-i))&1,tick);tick+=20;wire(0,1,tick);tick+=20;wire(0,0,tick);tick+=20;}}
static void spi_end(void){wire(3,1,tick);tick+=20;}
static void spi_capacity(void){
 tracegpu_decode_config c=config(2);reset(8);spi_begin();for(unsigned i=0;i<5000;i++)spi_byte(i&255,(i^0x55)&255);spi_end();bind();
 for(unsigned chunk=1;chunk<=127;chunk=chunk==1?7:chunk==7?127:128){begin(&c,0,624,8);run(chunk);assert(review.decoded_total==5000&&review.count==8&&review.results[7].ordinal==4999&&review.results[7].row.value==(((4999U^0x55U)&255U)<<16|(4999U&255U)));assert(review.results[7].spi.total==5000&&!review.results[7].row.flags);}
 tracegpu_search_filter f=filter(2);f.length=2;f.bytes[0]=0xfe;f.bytes[1]=0xff;f.mask[0]=f.mask[1]=255;begin(&c,&f,2,8);run(127);assert(review.matches==19&&review.count==3&&review.results[2].ordinal==4862);
 f.miso=1;f.bytes[0]=0xab;f.bytes[1]=0xaa;begin(&c,&f,0,32);run(7);assert(review.matches==19);
 reset(8);spi_begin();spi_byte(0xaa,0);spi_end();spi_begin();spi_byte(0xbb,0);spi_end();spi_begin();spi_byte(0xaa,0);spi_byte(0xbb,0);spi_end();bind();f.miso=0;f.bytes[0]=0xaa;f.bytes[1]=0xbb;begin(&c,&f,0,32);run(1);assert(review.matches==1&&review.results[0].ordinal==2&&review.results[0].transaction==2);
 reset(8);spi_begin();spi_byte(0xaa,0); /* missing CS release at END */ bind();f.errors=1;begin(&c,&f,0,32);run(7);assert(review.matches==1&&(review.results[0].spi.flags&SPI_PARTIAL_END));
 puts("REVIEW_SPI_CAPACITY_PASS 5000 words single-CS tail/MOSI/MISO masks no cross-CS partial transaction");
}
int main(void){uart_capacity();uart_boundaries();i2c_capacity();spi_capacity();printf("LOGIC_REVIEW_PASS workspace=%u detail=%u manager=%u\n",(unsigned)sizeof review,(unsigned)sizeof(tracegpu_decode_detail),(unsigned)sizeof manager);return 0;}
