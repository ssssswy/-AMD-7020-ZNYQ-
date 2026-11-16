#define main can_core_golden_main
#include "test_can_decoder_v3_7.c"
#undef main
#include "logic_review.h"
#include "logic_annotation_query.h"
#include "capture_export.h"

static tracegpu_vector_record capture[65536];
static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;
static tracegpu_decode_manager manager;
static tracegpu_logic_review review;
static unsigned rows,all_levels;
static uint64_t now;
static void record(unsigned type,uint64_t t,unsigned levels){
 CHECK(rows<65536);capture[rows]=tracegpu_vector_pack((tracegpu_vector_event){t,0xfffffff0U+rows,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:255),(uint8_t)levels});rows++;
}
static void change(unsigned ch,unsigned value,uint64_t t){
 unsigned next=(all_levels&~(1U<<ch))|(value<<ch);
 if(next!=all_levels){all_levels=next;record(TG_VEC_CHANGE,t,next);}
}
static void serial_byte(unsigned value){
 uint64_t start=now;unsigned bit=0;
 change(0,0,start);bit++;
 for(unsigned i=0;i<8;i++)change(0,(value>>i)&1,start+((uint64_t)bit++*100000000+4800)/9600);
 change(0,1,start+((uint64_t)bit++*100000000+4800)/9600);
 now=start+((uint64_t)bit*100000000+4800)/9600;
}
static tracegpu_decode_config configuration(unsigned protocol,unsigned channel){
 tracegpu_decode_config c={0};c.protocol=(uint8_t)protocol;
 c.map[0]=(uint8_t)channel;c.uart=(tracegpu_uart_decode_config){9600,(uint8_t)channel,8,0,1};
 c.can=(tracegpu_can_decode_config){100000000,500000,800,100,(uint8_t)channel,0};return c;
}
static void make_capture(void){
 rows=0;all_levels=255;now=0x100000000ULL;record(TG_VEC_INIT,now,255);now+=3000;
 for(unsigned frame=0;frame<100;frame++){
  uint8_t data[8];for(unsigned i=0;i<8;i++)data[i]=(uint8_t)(frame+i);
  wire w=encode(0x100+frame,0,0,8,data,1,0);
  for(unsigned i=0;i<w.n;i++)change(7,w.b[i],now+(uint64_t)i*200);
  now+=(uint64_t)w.n*200+600;
 }
 now+=500000;
 for(unsigned frame=0;frame<70;frame++){
  static const uint8_t adu[]={1,3,0,0,0,10,0xc5,0xcd};
  for(unsigned i=0;i<8;i++)serial_byte(adu[i]);
  now+=364584;
 }
 record(TG_VEC_END,now,all_levels);
 ring=(tracegpu_vector_ring){capture,65536,rows,rows,rows,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,255};
 CHECK(!tracegpu_logic_view_bind(&view,&ring,77,88,1));
}
static void run_review(unsigned budget){
 unsigned calls=0;
 while(review.state==TG_REVIEW_BUSY){uint32_t prior=review.index;CHECK(tracegpu_logic_review_step(&review,budget)>=0);CHECK(review.index-prior<=budget);CHECK(++calls<2000000);}
 CHECK(review.state==TG_REVIEW_DONE&&tracegpu_logic_review_progress(&review)==100);
}
static void managers(void){
 tracegpu_decode_config can=configuration(TG_DECODE_CAN,7),mb=configuration(TG_DECODE_MODBUS,0);
 CHECK(!tracegpu_decode_begin(&manager,0,&view,&can,9));CHECK(!tracegpu_decode_begin(&manager,1,&view,&mb,10));
 CHECK(!tracegpu_decode_step(&manager,0,0));CHECK(manager.task[0].index==0);
 for(unsigned count=0;manager.task[0].busy||manager.task[1].busy;count++){
  CHECK(count<2000000);for(unsigned s=0;s<2;s++){uint32_t prior=manager.task[s].index;CHECK(tracegpu_decode_step(&manager,s,1)>=0);CHECK(manager.task[s].index-prior<=1);}
 }
 CHECK(manager.task[0].total==100&&manager.task[0].count==64&&manager.task[0].omitted==36);
 CHECK(manager.task[1].total==70&&manager.task[1].count==64&&manager.task[1].omitted==6);
 tracegpu_decode_row row;CHECK(!tracegpu_decode_row_get(&manager,0,63,&row)&&row.value==0x13f&&row.channel==7);
 cases++;
}
static void pages_and_search(void){
 tracegpu_decode_config c=configuration(TG_DECODE_CAN,7);tracegpu_review_result r;
 for(unsigned budget=1;budget<=127;budget=budget==1?7:budget==7?127:128){
  CHECK(!tracegpu_logic_review_begin(&review,&view,&c,9,11,view.begin,view.end,0,12,8));run_review(budget);
  CHECK(review.decoded_total==100&&review.count==4&&review.results[0].ordinal==96);
  CHECK(!tracegpu_logic_review_result_get(&review,3,&r)&&r.bus.can.id==0x163&&r.bus.can.data[0]==99&&r.row.start>UINT32_MAX);
  CHECK(tracegpu_can_frame_data_valid(&r.bus.can));cases++;
 }
 tracegpu_search_filter f={0};f.protocol=TG_DECODE_CAN;f.address=f.read=-1;f.can_id=0x149;f.can_mask=0x1fffffff;
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,9,12,view.begin,view.end,&f,0,8));run_review(1);
 CHECK(review.matches==1&&review.results[0].bus.can.data[0]==73);cases++;
 c=configuration(TG_DECODE_MODBUS,0);
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,10,13,view.begin,view.end,0,8,8));run_review(7);
 CHECK(review.decoded_total==70&&review.count==6&&review.results[0].ordinal==64);
 CHECK(review.results[5].bus.modbus.valid&&review.results[5].bus.modbus.length==8&&review.results[5].bus.modbus.crc_received==0xcdc5);cases++;
 f=(tracegpu_search_filter){0};f.protocol=TG_DECODE_MODBUS;f.address=1;f.read=-1;f.length=2;f.bytes[0]=0;f.bytes[1]=10;f.mask[0]=f.mask[1]=255;
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,10,14,view.begin,view.end,&f,0,8));run_review(7);
 CHECK(review.matches==70&&review.has_more&&review.results[0].offset==4);cases++;
 c=configuration(TG_DECODE_RS485,0);
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,11,15,view.begin,view.end,0,69,8));run_review(16);
 CHECK(review.decoded_total==560&&review.count==8&&review.results[0].ordinal==552&&review.results[7].row.value==0xcd);cases++;
 tracegpu_logic_review_cancel(&review);CHECK(tracegpu_logic_review_result_get(&review,0,&r)<0);
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,11,16,view.begin,view.end,0,0,8));
 ring.total++;CHECK(tracegpu_logic_review_step(&review,1)<0&&review.state==TG_REVIEW_STALE);ring.total--;
 tracegpu_decode_cancel(&manager,0);CHECK(tracegpu_decode_row_get(&manager,0,0,&r.row)<0);cases++;
}
static void export_file(const char *path){
 tracegpu_capture_export x;tracegpu_decode_config c=configuration(TG_DECODE_CAN,7),m=configuration(TG_DECODE_MODBUS,0);uint8_t slots[32],bytes[512];
 tracegpu_decode_config_pack(&c,slots);tracegpu_decode_config_pack(&m,slots+16);
 CHECK(slots[0]==3&&slots[1]==7&&slots[12]==0x20&&slots[13]==3&&slots[14]==100&&slots[16]==5);
 CHECK(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,0,0,slots));
 char file[1024];snprintf(file,sizeof file,"%s/bus_capture.tgc",path);FILE *f=fopen(file,"wb");CHECK(f);int n;
 while((n=tracegpu_export_read(&x,bytes,sizeof bytes))>0)CHECK(fwrite(bytes,1,(unsigned)n,f)==(unsigned)n);
 CHECK(!n&&!fclose(f));cases++;
}
static void boundaries_and_gaps(void){
 tracegpu_decode_config c=configuration(TG_DECODE_CAN,7);
 /* Selection starts inside known idle; seed sequence must precede first SOF. */
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,9,20,view.begin+100,view.end,0,0,8));run_review(1);
 CHECK(review.decoded_total==100);cases++;
 /* No explicit END: a final watermark provides measured idle, not fake data. */
 capture[rows-1]=tracegpu_vector_pack((tracegpu_vector_event){now,0xfffffff0U+rows-1,TG_VEC_WATERMARK,255,(uint8_t)all_levels});
 CHECK(!tracegpu_logic_view_bind(&view,&ring,77,89,1));c=configuration(TG_DECODE_MODBUS,0);
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,10,21,view.begin,view.end,0,0,8));run_review(1);
 CHECK(review.decoded_total==70);cases++;
 rows=0;all_levels=255;now=10000;record(TG_VEC_INIT,now,255);now+=400000;
 serial_byte(0x55);now+=400000;record(TG_VEC_WATERMARK,now,255);now+=100;record(TG_VEC_GAP,now,0);now+=100;
 record(TG_VEC_RESYNC,now,255);now+=400000;serial_byte(0xaa);now+=400000;
 serial_byte(0x55);serial_byte(0xaa);now+=400000;record(TG_VEC_END,now,255);
 ring=(tracegpu_vector_ring){capture,65536,rows,rows,rows,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,255};
 CHECK(!tracegpu_logic_view_bind(&view,&ring,78,90,1));c=configuration(TG_DECODE_RS485,0);
 tracegpu_search_filter f={0};f.protocol=TG_DECODE_RS485;f.address=f.read=-1;f.length=2;
 f.bytes[0]=0x55;f.bytes[1]=0xaa;f.mask[0]=f.mask[1]=255;
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,11,22,view.begin,view.end,&f,0,8));run_review(1);
 CHECK(review.matches==1&&review.results[0].ordinal==2);cases++;
 /* CRC failure remains a visible error row, never a successful data match. */
 rows=0;all_levels=255;now=10000;record(TG_VEC_INIT,now,255);now+=400000;
 {const uint8_t bad[]={1,3,0,0,0,10,0xc5,0xcc};for(unsigned i=0;i<sizeof bad;i++)serial_byte(bad[i]);}
 now+=400000;record(TG_VEC_END,now,255);ring.count=ring.head=rows;ring.total=rows;
 CHECK(!tracegpu_logic_view_bind(&view,&ring,79,91,1));c=configuration(TG_DECODE_MODBUS,0);
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,12,23,view.begin,view.end,0,0,8));run_review(1);
 CHECK(review.count==1&&!review.results[0].bus.modbus.valid&&review.results[0].row.flags);cases++;
 f=(tracegpu_search_filter){0};f.protocol=TG_DECODE_MODBUS;f.address=1;f.read=-1;f.length=1;f.bytes[0]=3;f.mask[0]=255;
 CHECK(!tracegpu_logic_review_begin(&review,&view,&c,12,24,view.begin,view.end,&f,0,8));run_review(1);CHECK(!review.matches);cases++;
}
int main(int argc,char **argv){
 make_capture();managers();pages_and_search();
 if(argc==2)export_file(argv[1]);
 boundaries_and_gaps();
 printf("BUS_REVIEW_V37_PASS cases=%u checks=%u capture_records=%u CAN=100 MODBUS=70 RS485=560 budget1=1 late_pages=1 cancel_stale=1\n",cases,checks,rows);
 return 0;
}
