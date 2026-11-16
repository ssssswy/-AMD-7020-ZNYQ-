#include "trigger_capture.h"
#include "uart_analyzer_hw.h"
#include "board_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This model follows the published MMIO contract. It does not reuse the
 * production packing helpers, so it can detect register-image mistakes. */
static uint32_t regs[1024],common_status,stop_reason,committed_generation;
static uint64_t stopped_tick;
static unsigned commit_failure,arm_failure,readback_failure,reads,writes;
static unsigned corrupt_offset=0x10;
static unsigned command_log[64],commands,start_order;
static unsigned checks;
static tracegpu_trigger_capture capture;
static tracegpu_uart_event memory[TG_TRIGGER_VECTOR_CAPACITY];
#define CHECK(v) do { ++checks; if(!(v)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#v);exit(1);} } while(0)
static uint64_t get64(unsigned o){return regs[o/4]|((uint64_t)regs[o/4+1]<<32);}
static void set64(unsigned o,uint64_t v){regs[o/4]=(uint32_t)v;regs[o/4+1]=(uint32_t)(v>>32);}
static void model_reset(void){
 memset(regs,0,sizeof regs);regs[0]=TG_TRIGGER_VERSION;regs[1]=0x3f;
 regs[0xb0/4]=8;regs[0xb4/4]=4;regs[0xb8/4]=4;regs[0xbc/4]=TG_TRIGGER_VERSION;
 common_status=stop_reason=committed_generation=0;stopped_tick=0;
 commit_failure=arm_failure=readback_failure=reads=writes=commands=start_order=0;
 corrupt_offset=0x10;
}
uint32_t Xil_In32(uintptr_t address){
 reads++;
 if(address>=TRACEGPU_TRIGGER_BASE&&address<TRACEGPU_TRIGGER_BASE+4096U){
  unsigned o=(unsigned)(address-TRACEGPU_TRIGGER_BASE);
  if(readback_failure&&o==corrupt_offset)return regs[o/4]^1U;
  return regs[o/4];
 }
 if(address==TRACEGPU_CAPTURE_CONTROL_BASE+0x64U)return stop_reason;
 if(address==TRACEGPU_CAPTURE_CONTROL_BASE+0x68U)return (uint32_t)stopped_tick;
 if(address==TRACEGPU_CAPTURE_CONTROL_BASE+0x6cU)return (uint32_t)(stopped_tick>>32);
 fprintf(stderr,"Unexpected read %lx\n",(unsigned long)address);exit(1);
}
void Xil_Out32(uintptr_t address,uint32_t value){
 writes++;
 CHECK(address>=TRACEGPU_TRIGGER_BASE&&address<TRACEGPU_TRIGGER_BASE+4096U);
 unsigned o=(unsigned)(address-TRACEGPU_TRIGGER_BASE);
 if(o!=8){regs[o/4]=value;return;}
 CHECK(commands<64);command_log[commands++]=value;
 if(value==8){
  unsigned committed=regs[3]&1U;memset(&regs[0x40/4],0,0x70);regs[3]=committed;regs[0x18/4]=0;
 }else if(value==4){regs[3]&=~(128U|4U|2U);}
 else if(value==1){
  if(commit_failure){regs[3]|=16U;regs[0x18/4]=7;}
  else{committed_generation=regs[0x10/4];regs[3]=1;regs[0x18/4]=0;}
 }else if(value==2){
  if(arm_failure||common_status&1U){regs[3]|=32U;regs[0x18/4]=3;}
  else{regs[3]=131;regs[0x58/4]=committed_generation;regs[0x5c/4]=regs[0x14/4];}
 }else{fprintf(stderr,"Unexpected command %u\n",value);exit(1);}
}
uint32_t tracegpu_uart_common_status(void){return common_status;}
static void model_start(uint64_t t){
 CHECK((regs[3]&131U)==131U);CHECK(commands&&command_log[commands-1]==2);
 start_order=commands;common_status=1;regs[3]=(regs[3]&~2U)|4U;set64(0x88,t);
}
static void model_fire(uint64_t t,uint64_t first,unsigned reason,unsigned sources,unsigned slots){
 regs[3]|=8U;regs[0x40/4]=reason|(sources<<8)|(slots<<16)|(15U<<20);
 set64(0x48,t);set64(0x50,first);
 for(unsigned n=0;n<4;n++){
  set64(0x60+n*8,(slots&(1U<<n))?t-n:0);
  set64(0x90+n*8,(slots&(1U<<n))?first-n:0);
 }
 regs[0x80/4]=0x12345678U;regs[0x84/4]=0x01020304U;
}
static void model_stop(uint64_t t,unsigned reason){common_status=0;stopped_tick=t;stop_reason=reason;regs[3]&=~4U;}
static tracegpu_uart_event uart(unsigned ch,unsigned type,uint64_t t,uint32_t seq,unsigned data,unsigned flags){
 tracegpu_uart_event e={{(uint32_t)t,(uint32_t)(t>>32),(seq<<8)|ch,(seq>>24)|(data<<8)|(flags<<16)|(type<<24)}};return e;
}
static void record(unsigned ch,unsigned type,uint64_t t,uint32_t seq,unsigned flags){tracegpu_uart_event e=uart(ch,type,t,seq,0x55,flags);if(type==5)e.word[3]&=~0x0000ff00U;tracegpu_trigger_capture_record(&capture,ch,&e);}
static tracegpu_trigger_config config_default(void){tracegpu_trigger_config c;tracegpu_trigger_config_default(&c);return c;}

static void config_validation(void){
 tracegpu_trigger_config base=config_default(),c=base;
 CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==0);
 CHECK(tracegpu_trigger_config_valid(NULL,15,0,0)==-1);
 CHECK(tracegpu_trigger_config_valid(&c,0,0,0)==-1);
 CHECK(tracegpu_trigger_config_valid(&c,16,0,0)==-1);
 CHECK(tracegpu_trigger_config_valid(&c,15,3,0)==-1);
 CHECK(tracegpu_trigger_pre_ticks(&c)==5000000ULL&&tracegpu_trigger_post_ticks(&c)==5000000ULL);
 CHECK(tracegpu_trigger_pre_limit(&c,0)==65536U&&tracegpu_trigger_post_limit(&c,0)==65280U);
 CHECK(tracegpu_trigger_pre_limit(&c,1)==262144U&&tracegpu_trigger_post_limit(&c,1)==261888U);
 c.combine=3;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-2);
 c=base;c.level_mask=2;CHECK(tracegpu_trigger_config_valid(&c,1,0,0)==-2);
 c=base;c.pre_percent=100;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-3);
 c=base;c.pre_percent=1;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-3);
 c=base;c.total_ticks=TG_TRIGGER_HZ/1000-1;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-4);
 c.total_ticks=TG_TRIGGER_HZ/1000;CHECK(!tracegpu_trigger_config_valid(&c,15,0,0));
 c.total_ticks=10*TG_TRIGGER_HZ;CHECK(!tracegpu_trigger_config_valid(&c,15,0,0));
 c.total_ticks++;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-4);
 c=base;c.slots[0].type=5;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-5);
 c=base;c.slots[0].channel=4;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-6);
 c=base;c.slots[0].channel=1;CHECK(tracegpu_trigger_config_valid(&c,1,0,0)==-6);
 c=base;c.slots[0].mode=1;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-7);
 c=base;c.slots[0].polarity=2;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-7);
 c=base;CHECK(tracegpu_trigger_config_valid(&c,15,1,0)==-8);
 c.slots[0].type=TG_TRIG_SEQUENCE;c.slots[0].length=0;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-9);
 c.slots[0].length=9;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-9);
 c.slots[0].length=8;memset(c.slots[0].mask,0,8);CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-9);
 c.slots[0].mask[0]=15;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-9);
 c.slots[0].mask[0]=255;CHECK(!tracegpu_trigger_config_valid(&c,15,0,0));
 c=base;c.slots[0].error_mask=0;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-10);
 c.slots[0].error_mask=8;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-10);
 c.slots[0].error_mask=2;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-11);CHECK(!tracegpu_trigger_config_valid(&c,15,0,1));
 c=base;c.slots[0].type=TG_TRIG_PULSE;c.slots[0].min_ticks=99;CHECK(tracegpu_trigger_config_valid(&c,15,2,0)==-12);
 c.slots[0].min_ticks=100;CHECK(!tracegpu_trigger_config_valid(&c,15,2,0));
 c.slots[0].mode=TG_TRIG_INSIDE;c.slots[0].max_ticks=99;CHECK(tracegpu_trigger_config_valid(&c,15,2,0)==-12);
 c=base;c.slots[0].type=TG_TRIG_TIMEOUT;c.slots[0].mode=TG_TRIG_GOOD_BYTE;CHECK(tracegpu_trigger_config_valid(&c,15,2,0)==-8);
 c=base;c.combine=TG_TRIG_LEVEL;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-13);
 c.level_mask=3;memset(c.slots,0,sizeof c.slots);CHECK(!tracegpu_trigger_config_valid(&c,15,2,0));
 c.combine=TG_TRIG_ANY;CHECK(tracegpu_trigger_config_valid(&c,15,0,0)==-14);
 for(unsigned p=0;p<=75;p+=25){c=base;c.pre_percent=(uint8_t)p;CHECK(tracegpu_trigger_pre_limit(&c,0)+tracegpu_trigger_post_limit(&c,0)+256==131072);CHECK(tracegpu_trigger_pre_ticks(&c)+tracegpu_trigger_post_ticks(&c)==c.total_ticks);}
 puts("PASS trigger driver configuration boundaries");
}
static void register_image_and_failures(void){
 tracegpu_trigger_config c=config_default();model_reset();c.combine=TG_TRIG_ALL;c.level_mask=10;c.level_value=2;
 c.total_ticks=1000000000ULL;c.pre_percent=25;c.window_ticks=123456;
 c.slots[0].type=TG_TRIG_SEQUENCE;c.slots[0].length=8;c.slots[0].channel=3;
 for(unsigned n=0;n<8;n++){c.slots[0].bytes[n]=(uint8_t)(0x10+n);c.slots[0].mask[n]=(n==3)?0:255;}
 c.slots[1].type=TG_TRIG_TIMEOUT;c.slots[1].mode=TG_TRIG_EDGE_FALL;
 c.slots[2].type=TG_TRIG_PULSE;c.slots[2].mode=TG_TRIG_OUTSIDE;c.slots[2].polarity=1;
 c.slots[3].type=TG_TRIG_ERROR;c.slots[3].error_mask=7;
 CHECK(!tracegpu_trigger_config_valid(&c,15,0,15));
 CHECK(!tracegpu_trigger_hw_configure(&c,0,0x11223344,0x55667788));
 CHECK(commands==2&&command_log[0]==8&&command_log[1]==1);
 CHECK(regs[0x20/4]==0x20a01&&get64(0x28)==123456&&get64(0x30)==750000000);
 CHECK(regs[0x38/4]==98048&&regs[0x10/4]==0x11223344&&regs[0x14/4]==0x55667788);
 CHECK(regs[0x100/4]==0x18031U); /* default error_mask remains in an unused field */
 CHECK(get64(0x104)==0x1716151413121110ULL&&get64(0x10c)==0xffffffff00ffffffULL);
 CHECK(regs[0x140/4]==(4U|(1U<<4)|(2U<<8)|(2U<<12)));
 CHECK(regs[0x180/4]==(3U|(2U<<4)|(3U<<8)|(1U<<10)|(2U<<12)));
 CHECK(regs[0x1c0/4]==(2U|(3U<<4)|(2U<<12)|(7U<<16)));
 for(unsigned n=0;n<4;n++){unsigned o=0x100+n*64;CHECK(get64(o+20)==10000&&get64(o+28)==20000&&get64(o+36)==1000000);}
 CHECK(!tracegpu_trigger_hw_arm());CHECK(command_log[commands-1]==2&&regs[3]==131);
 model_start(0x1fffffffeULL);CHECK(start_order==3);CHECK(tracegpu_trigger_hw_arm_tick()==0x1fffffffeULL);
 model_fire(0x200000123ULL,0x1fffffff0ULL,3,9,5);regs[0x44/4]=2;
 tracegpu_trigger_hw_snapshot s=tracegpu_trigger_hw_read();
 CHECK(s.tick==0x200000123ULL&&s.start_tick==0x1fffffff0ULL&&s.reason==3&&s.source_mask==9&&s.predicate_mask==5&&s.levels==15);
 CHECK(s.generation==0x11223344&&s.capture_id==0x55667788&&s.quality==2&&s.candidate[2]==s.tick-2);
 CHECK(s.candidate_start[2]==0x1ffffffeeULL&&s.candidate_start[1]==0&&s.data==0x12345678U&&s.details==0x01020304U);
 tracegpu_trigger_hw_disarm();s=tracegpu_trigger_hw_read();CHECK(s.status&8U);CHECK(s.tick==0x200000123ULL);
 model_stop(0x200100000ULL,TG_TRIG_STOP_CAPACITY);CHECK(tracegpu_trigger_hw_stop_tick()==stopped_tick&&tracegpu_trigger_hw_stop_reason()==2);
 tracegpu_trigger_hw_clear();unsigned before=reads;s=tracegpu_trigger_hw_read();CHECK(reads==before+7&&s.tick==0&&s.status==1);
 model_reset();regs[0]=0x20600;CHECK(!tracegpu_trigger_hw_available());CHECK(tracegpu_trigger_hw_configure(&c,0,1,1)==-1&&writes==0);
 model_reset();regs[1]=31;CHECK(!tracegpu_trigger_hw_available());
 model_reset();common_status=1;CHECK(tracegpu_trigger_hw_configure(&c,0,1,1)==-1&&writes==0);
 model_reset();readback_failure=1;CHECK(tracegpu_trigger_hw_configure(&c,0,1,1)==-2&&commands==1);
 for(unsigned slot=0;slot<4;slot++)for(unsigned off=0;off<=40;off+=4){
  model_reset();readback_failure=1;corrupt_offset=0x100+slot*64+off;
  CHECK(tracegpu_trigger_hw_configure(&c,0,1,1)==-2&&commands==1);
 }
 model_reset();commit_failure=1;CHECK(tracegpu_trigger_hw_configure(&c,0,1,1)==-3);
 model_reset();CHECK(!tracegpu_trigger_hw_configure(&c,0,1,1));arm_failure=1;CHECK(tracegpu_trigger_hw_arm()==-1);
 puts("PASS trigger driver MMIO image, failures and sticky 64-bit metadata");
}
static void setup_capture(unsigned vector){
 tracegpu_trigger_config c=config_default();c.total_ticks=100000;c.pre_percent=50;
 if(vector){c.slots[0].type=TG_TRIG_TIMEOUT;c.slots[0].mode=TG_TRIG_EDGE_ANY;}
 model_reset();CHECK(!tracegpu_trigger_capture_arm(&capture,memory,&c,vector,vector?15:1,91,29,500));
 CHECK(capture.active&&capture.ui.state==TG_TRIG_ARMED&&!capture.arm_observed&&capture.store.arm_tick==500);
 tracegpu_trigger_capture_poll(&capture);CHECK(!capture.arm_observed&&!common_status);
 model_start(1000);tracegpu_trigger_capture_poll(&capture);
 CHECK(capture.arm_observed&&capture.store.arm_tick==1000&&capture.store.lane[0].boundary_tick==1000);
}
static void capture_deliveries(void){
 setup_capture(0);model_fire(101000,99000,1,1,1);tracegpu_trigger_capture_poll(&capture);
 CHECK(capture.ui.has_trigger&&capture.ui.state==TG_TRIG_POST&&capture.store.state==TG_STORE_POST);
 CHECK(capture.evidence.tick==101000&&capture.evidence.candidate_start[0]==99000&&capture.evidence.data==0x12345678U);
 record(0,5,1000,0,255);record(0,2,99000,1,0);record(0,2,101000,2,0);record(0,4,151000,3,0);
 CHECK(capture.store.lane[0].pre_count==1&&capture.store.lane[0].post_count==2);
 model_stop(151000,1);tracegpu_trigger_capture_poll(&capture);CHECK(capture.ui.state==TG_TRIG_DRAINING&&capture.ui.stop_reason==1&&!capture.ui.valid);
 tracegpu_trigger_capture_finish(&capture,1,0);CHECK(capture.ui.state==TG_TRIG_READY&&capture.ui.valid&&!capture.active&&!capture.ui.quality);
 CHECK(capture.ui.actual_pre_ticks[0]==50000&&capture.ui.actual_post_ticks[0]==50000&&capture.ui.records[0]==3);
 unsigned saved_count=capture.ui.records[0];record(0,2,160000,4,0);CHECK(capture.ui.records[0]==saved_count&&capture.store.lane[0].count==saved_count);
 setup_capture(0);record(0,5,1000,0,255);record(0,1,75000,1,0);record(0,2,111000,2,0);record(0,4,151000,3,0);
 model_fire(101000,99000,2,1,1);model_stop(151000,1);tracegpu_trigger_capture_poll(&capture);
 CHECK(capture.store.lane[0].pre_count==1&&capture.store.lane[0].post_count==2&&capture.ui.state==TG_TRIG_DRAINING);
 tracegpu_trigger_capture_finish(&capture,1,0);CHECK(capture.ui.state==TG_TRIG_READY&&!capture.ui.quality);
 setup_capture(0);record(0,5,1000,0,255);model_fire(101000,99000,2,1,1);tracegpu_trigger_capture_poll(&capture);
 record(0,4,151100,1,0);model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_READY&&capture.ui.actual_post_ticks[0]==50000); /* flush is transport, not extra sampling */
 puts("PASS trigger capture ARM-before-START, late PRE DMA and late trigger observation");
}
static void capture_failures(void){
 tracegpu_trigger_config c=config_default();
 model_reset();commit_failure=1;CHECK(tracegpu_trigger_capture_arm(&capture,memory,&c,0,1,1,1,0)==-2);
 CHECK(!capture.active&&capture.ui.state==TG_TRIG_FAULT&&capture.store.state==TG_STORE_IDLE&&command_log[commands-1]==4);
 model_reset();arm_failure=1;CHECK(tracegpu_trigger_capture_arm(&capture,memory,&c,0,1,1,1,0)==-2&&capture.ui.state==TG_TRIG_FAULT&&!capture.active);
 model_reset();CHECK(tracegpu_trigger_capture_arm(&capture,NULL,&c,0,1,1,1,0)==-1&&!writes);
 setup_capture(0);record(0,5,1000,0,255);model_fire(101000,99000,2,1,1);tracegpu_trigger_capture_poll(&capture);
 record(0,2,120000,1,0);model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&(capture.ui.quality&TG_TRIG_Q_POST_SHORT));
 setup_capture(0);record(0,5,1000,0,255);model_fire(101000,99000,2,1,1);tracegpu_trigger_capture_poll(&capture);
 record(0,4,151000,3,0);model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&(capture.ui.quality&TG_TRIG_Q_LOSS));
 setup_capture(0);record(0,5,1000,0,255);model_fire(101000,99000,2,1,1);regs[0x44/4]=1;tracegpu_trigger_capture_poll(&capture);
 record(0,4,151000,1,0);model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&(capture.ui.quality&TG_TRIG_Q_LOSS));
 setup_capture(0);record(0,5,1000,0,255);model_fire(1500,1400,2,1,1);tracegpu_trigger_capture_poll(&capture);
 record(0,4,51500,1,0);model_stop(51500,1);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&(capture.ui.quality&TG_TRIG_Q_PRE_SHORT));
 setup_capture(0);record(0,5,1000,0,255);model_fire(101000,99000,2,1,1);tracegpu_trigger_capture_poll(&capture);
 record(0,4,151000,1,0);model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,0,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&(capture.ui.quality&TG_TRIG_Q_ABORTED));
 setup_capture(0);record(0,5,1000,0,255);tracegpu_trigger_capture_abort(&capture);
 CHECK(capture.aborted&&capture.ui.state==TG_TRIG_DRAINING&&(capture.ui.quality&TG_TRIG_Q_ABORTED));
 record(0,4,151000,1,0);model_stop(151000,3);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&!capture.ui.has_trigger&&(capture.store.quality&TG_STORE_TRIGGER_MISSING));
 setup_capture(0);record(0,5,1000,0,255);model_stop(151000,4);record(0,4,151000,1,0);tracegpu_trigger_capture_finish(&capture,1,0);
 CHECK(capture.ui.state==TG_TRIG_INCOMPLETE&&!capture.ui.has_trigger);
 setup_capture(0);model_fire(101000,99000,2,1,1);regs[0x5c/4]=999;tracegpu_trigger_capture_poll(&capture);
 CHECK(capture.ui.state==TG_TRIG_FAULT&&!capture.ui.has_trigger&&(capture.ui.quality&TG_TRIG_Q_LOSS));
 model_stop(151000,4);tracegpu_trigger_capture_finish(&capture,0,0);CHECK(capture.ui.state!=TG_TRIG_READY);
 tracegpu_trigger_capture_reset(&capture);CHECK(!capture.active&&!capture.ui.valid&&capture.ui.state==TG_TRIG_IDLE);
 puts("PASS trigger capture abort, no-hit, bad identity, missing tails and quality");
}
static void vector_delivery(void){
 setup_capture(1);
 tracegpu_vector_event v={1000,0,TG_VEC_INIT,15,15};tracegpu_uart_event e=tracegpu_vector_pack(v);tracegpu_trigger_capture_record(&capture,0,&e);
 model_fire(101000,101000,9,1,1);tracegpu_trigger_capture_poll(&capture);
 v.tick=151000;v.sequence=1;v.type=TG_VEC_END;e=tracegpu_vector_pack(v);tracegpu_trigger_capture_record(&capture,0,&e);
 model_stop(151000,1);tracegpu_trigger_capture_finish(&capture,1,0);CHECK(capture.ui.state==TG_TRIG_READY);
 CHECK(capture.ui.actual_pre_ticks[0]==50000&&capture.ui.actual_post_ticks[0]==50000&&capture.ui.records[0]==1);
 for(unsigned n=1;n<4;n++)CHECK(!capture.ui.actual_pre_ticks[n]&&!capture.ui.actual_post_ticks[n]&&!capture.ui.records[n]);
 puts("PASS trigger capture VECTOR shared-history metrics");
}
int main(void){
 config_validation();register_image_and_failures();capture_deliveries();capture_failures();vector_delivery();
 printf("TRIGGER_DRIVER_V2_9_PASS checks=%u\n",checks);return 0;
}
