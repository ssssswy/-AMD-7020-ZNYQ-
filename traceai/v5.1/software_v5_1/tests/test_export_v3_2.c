#include "capture_export.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[1024];
static tracegpu_vector_stream stream;
static tracegpu_logic_trace_view view;
static uint8_t bytes[32768];
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t crc(const uint8_t *p,unsigned size){return ~tracegpu_crc32_update(~0U,p,size);}
static void event(uint64_t tick,uint32_t seq,unsigned type,unsigned levels){
 tracegpu_vector_record r=tracegpu_vector_pack((tracegpu_vector_event){tick,seq,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:stream.history.capture_mask),(uint8_t)levels});
 assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);
}
static unsigned emit(uint8_t mask,int evidence,const char *path){
 tracegpu_capture_export x;tracegpu_trigger_config c={0};tracegpu_trigger_ui_status s={0};
 tracegpu_vector_stream_init(&stream,records,1024,0,0);assert(!tracegpu_vector_stream_configure(&stream,mask));
 uint64_t t=0x100000010ULL;uint32_t seq=0;
 event(t++,seq++,TG_VEC_INIT,0);
 for(unsigned i=0;i<8;i++)if(mask&(1U<<i))event(t++,seq++,TG_VEC_CHANGE,1U<<i);
 if(stream.levels!=mask)event(t++,seq++,TG_VEC_CHANGE,mask);
 event(t++,seq++,TG_VEC_GAP,0);
 event(t++,seq++,TG_VEC_RESYNC,mask);event(t++,seq++,TG_VEC_WATERMARK,mask);
 event(t++,seq++,TG_VEC_END,mask);
 assert(!tracegpu_logic_view_bind(&view,&stream.history,0x12345678,0x89abcdef,1));
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL|TG_EXPORT_GAPS,0,4,0));
 if(evidence){c.pre_percent=50;c.level_mask=0x80;c.level_value=0x80;c.slots[0].type=TG_TRIG_PULSE;c.slots[0].channel=7;c.slots[0].min_ticks=10;c.slots[1].type=TG_TRIG_SEQUENCE;c.slots[1].channel=2;c.slots[1].length=8;for(unsigned i=0;i<8;i++){c.slots[1].bytes[i]=(uint8_t)i;c.slots[1].mask[i]=(uint8_t)(0xa0+i);}s.valid=1;s.state=TG_TRIG_INCOMPLETE;s.source_mask=0x80;s.records[0]=14;s.stop_reason=TG_TRIG_STOP_USER;assert(!tracegpu_export_trigger_evidence(&x,&c,&s,view.begin,view.end));}
 unsigned length=0;int n;while((n=tracegpu_export_read(&x,bytes+length,7))>0)length+=(unsigned)n;
 assert(!n&&length==512+seq*16+16&&length==tracegpu_export_size(&x));
 assert(!memcmp(bytes,"TGC3",4)&&bytes[4]==3&&bytes[5]==0&&bytes[6]==0&&bytes[7]==2);
 assert(u32(bytes+12)==0x30200&&u32(bytes+16)==100000000);
 assert(u32(bytes+20)==0x12345678&&u32(bytes+24)==0x89abcdef&&u32(bytes+28)==seq);
 assert(bytes[32]==0x10&&bytes[36]==1&&!memcmp(bytes+124,"TGE3",4)&&!memcmp(bytes+412,"TGC8",4));
 assert(bytes[416]==8&&bytes[417]==mask&&bytes[418]==4&&bytes[419]==evidence);
 for(unsigned i=0;i<4;i++)assert(bytes[420+i]==i);
 assert(bytes[424]==16&&!bytes[425]&&bytes[426]==2&&bytes[427]==4&&u32(bytes+428)==7);
 for(unsigned i=432;i<444;i++)assert(!bytes[i]);
 assert(u32(bytes+508)==crc(bytes,508)&&!memcmp(bytes+length-16,"END3",4));
 assert(u32(bytes+length-12)==seq*16&&u32(bytes+length-8)==seq&&u32(bytes+length-4)==crc(bytes+512,seq*16));
 if(evidence){assert(bytes[257]==7&&u32(bytes+168)==0x80&&!u32(bytes+408));for(unsigned i=0;i<8;i++)assert(bytes[460+i]==i&&bytes[468+i]==0xa0+i);}
 if(path){FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,length,f)==length&&!fclose(f));}
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,1,0,0,0));stream.history.capture_mask^=1;assert(tracegpu_export_read(&x,bytes,7)<0);stream.history.capture_mask^=1;
 return length;
}
int main(int argc,char **argv){
 emit(255,1,argc>1?argv[1]:0);emit(128,0,argc>2?argv[2]:0);
 puts("EXPORT_V32_PASS fixed512 LE header/CRC sequencebytes/masks 8 walking bits high-mask GAP RESYNC stale metadata");return 0;
}
