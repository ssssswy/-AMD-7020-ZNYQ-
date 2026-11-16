#include "can_vector_decoder.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The oracle is a transmitter, not a copy of the production parser. CRC uses
 * polynomial long division on an augmented bit array (not its LFSR routine). */
static unsigned checks,cases;
static uint32_t seed=0x37ca2026u;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d case %u: %s\n",__LINE__,cases,#x);exit(1);}}while(0)
typedef struct {unsigned n;uint8_t b[512];unsigned crc_delim,ack,ack_delim,eof;unsigned stuff[128],nstuff;uint16_t crc;} wire;
typedef struct {tracegpu_can_decoded f[16];unsigned n;} results;
typedef struct {tracegpu_can_vector_decoder d;results out;uint32_t seq;unsigned level,max_used,steps;} fixture;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void collect(const tracegpu_can_decoded *f,void *ctx){results *r=ctx;CHECK(r->n<16);r->f[r->n++]=*f;}
static void append(uint8_t *b,unsigned *n,uint32_t value,unsigned bits){
 while(bits)b[(*n)++]=(uint8_t)((value>>--bits)&1u);
}
static uint16_t reference_crc(const uint8_t *b,unsigned n){
 uint8_t division[160]={0};unsigned i,j;uint16_t out=0;
 CHECK(n+15u<=sizeof division);memcpy(division,b,n);
 for(i=0;i<n;i++)if(division[i])for(j=0;j<16;j++)division[i+j]^=(uint8_t)((0xc599u>>(15-j))&1u);
 for(i=n;i<n+15;i++)out=(uint16_t)((out<<1)|division[i]);
 return out;
}
static wire encode(uint32_t id,unsigned ext,unsigned remote,unsigned dlc,const uint8_t *data,unsigned ack,unsigned bad){
 uint8_t b[160];unsigned n=0,i,j,run=0,last=2;wire w;memset(&w,0,sizeof w);
 append(b,&n,0,1);append(b,&n,ext?id>>18:id,11);
 append(b,&n,ext?((bad&4u)?0:1):remote,1);append(b,&n,ext,1);
 if(ext){append(b,&n,id&0x3ffffu,18);append(b,&n,remote,1);append(b,&n,(bad&2u)?1:0,1);}
 append(b,&n,((ext?(bad&8u):(bad&2u)))?1:0,1);append(b,&n,dlc,4);
 if(!remote)for(i=0;i<(dlc>8?8:dlc);i++)append(b,&n,data[i],8);
 w.crc=reference_crc(b,n);append(b,&n,w.crc^((bad&1u)?1u:0u),15);
 for(i=0;i<n;i++){
  unsigned v=b[i];w.b[w.n++]=(uint8_t)v;
  if(v==last)run++;else {last=v;run=1;}
  if(run==5){w.stuff[w.nstuff++]=w.n;w.b[w.n++]=(uint8_t)!v;last=!v;run=1;}
 }
 w.crc_delim=w.n;w.b[w.n++]=1;w.ack=w.n;w.b[w.n++]=(uint8_t)!ack;
 w.ack_delim=w.n;w.b[w.n++]=1;w.eof=w.n;
 for(j=0;j<7;j++)w.b[w.n++]=1;
 return w;
}
static void init(fixture *f,unsigned baud,unsigned sp,unsigned sjw,unsigned channel,unsigned invert){
 tracegpu_can_decode_config c={100000000u,baud,(uint16_t)sp,(uint16_t)sjw,(uint8_t)channel,(uint8_t)invert};
 memset(f,0,sizeof *f);CHECK(tracegpu_can_vector_begin(&f->d,&c,collect,&f->out)==0);f->level=1;
}
static void event_raw(fixture *f,unsigned type,uint64_t t,unsigned level,unsigned flags,unsigned budget){
 tracegpu_vector_event e={t,f->seq++,(uint8_t)type,(uint8_t)flags,(uint8_t)level};uint32_t used;int r;
 CHECK(tracegpu_can_vector_feed(&f->d,&e)==0);
 r=tracegpu_can_vector_step(&f->d,0,&used);CHECK(r==TG_CAN_BUSY&&used==0);
 do{used=999;r=tracegpu_can_vector_step(&f->d,budget,&used);CHECK(used<=budget);
  if(used>f->max_used)f->max_used=used;
  f->steps++;CHECK(f->steps<1000000u);
 }while(r==TG_CAN_BUSY);
 CHECK(r==0);
}
static void event(fixture *f,unsigned type,uint64_t t,unsigned level,unsigned budget){
 unsigned mask=1u<<f->d.config.channel;
 event_raw(f,type,t,((level^f->d.config.invert)&1u)<<f->d.config.channel,mask,budget);
 f->level=level;
}
static void start(fixture *f,uint64_t origin){event(f,TG_VEC_INIT,origin,1,1);}
static uint64_t transmit(fixture *f,const wire *w,uint64_t at,unsigned tx_period,unsigned budget){
 unsigned i;
 for(i=0;i<w->n;i++)if(w->b[i]!=f->level)event(f,TG_VEC_CHANGE,at+(uint64_t)i*tx_period,w->b[i],budget);
 return at+(uint64_t)w->n*tx_period;
}
static void end(fixture *f,uint64_t t){event(f,TG_VEC_END,t,f->level,1);}
static void check_good(const fixture *f,uint32_t id,unsigned ext,unsigned remote,unsigned dlc,const uint8_t *data){
 const tracegpu_can_decoded *r;unsigned i;
 CHECK(f->out.n==1);r=&f->out.f[0];
 CHECK(tracegpu_can_frame_data_valid(r));CHECK(r->complete);CHECK(r->flags==0);
 CHECK(r->id==id&&r->extended==ext&&r->remote==remote&&r->dlc==dlc);
 CHECK(r->length==(remote?0:(dlc>8?8:dlc)));CHECK(r->ack==1);
 CHECK(r->crc_calculated==r->crc_received);
 for(i=0;i<r->length;i++)CHECK(r->data[i]==data[i]);
}
static void valid_vectors(void){
 unsigned rates[]={125000,250000,500000,1000000},r,x,m,dlc,i;uint8_t data[8];
 for(r=0;r<4;r++)for(x=0;x<2;x++)for(m=0;m<2;m++)for(dlc=0;dlc<16;dlc++){
  fixture f;wire w;uint32_t id=rnd()&(x?0x1fffffffu:0x7ffu);uint64_t stop;
  cases++;for(i=0;i<8;i++)data[i]=(uint8_t)rnd();
  init(&f,rates[r],800,100,(r+x)%8,m);start(&f,0xfffff000ULL);
  w=encode(id,x,m,dlc,data,1,0);
  stop=transmit(&f,&w,0xfffff000ULL+11u*f.d.bit_ticks,f.d.bit_ticks,1);
  end(&f,stop);check_good(&f,id,x,m,dlc,data);CHECK(f.out.f[0].crc_received==w.crc);
  CHECK(f.out.f[0].end==stop);CHECK(f.max_used<=1);
 }
 /* Repeatable randomized payload/ID/reference CRC coverage. */
 for(i=0;i<256;i++){
  fixture f;wire w;unsigned ext=rnd()&1u,remote=(rnd()%7u)==0,dl=rnd()%16u,k;
  uint32_t id=rnd()&(ext?0x1fffffffu:0x7ffu);uint64_t stop;
  for(k=0;k<8;k++)data[k]=(uint8_t)rnd();
  cases++;init(&f,rates[i%4],750,100,i%8,0);start(&f,0);w=encode(id,ext,remote,dl,data,1,0);
  stop=transmit(&f,&w,11u*f.d.bit_ticks,f.d.bit_ticks,3);end(&f,stop);
  check_good(&f,id,ext,remote,dl,data);
 }
}
static void errors_and_boundaries(void){
 fixture f;wire w;uint8_t data[8]={0,255,0x55,0xaa,3,4,5,6};uint64_t stop,at;unsigned i;
 cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0x123,0,0,8,data,0,0);
 stop=transmit(&f,&w,2200,200,1);end(&f,stop);CHECK(f.out.n==1&&f.out.f[0].ack==0);
 CHECK(f.out.f[0].flags==TG_CAN_ACK_RECESSIVE&&tracegpu_can_frame_data_valid(&f.out.f[0]));
 cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0x123,0,0,8,data,1,1);
 stop=transmit(&f,&w,2200,200,1);end(&f,stop);CHECK(f.out.n==1&&f.out.f[0].complete);
 CHECK(f.out.f[0].flags==TG_CAN_CRC_ERROR&&!tracegpu_can_frame_data_valid(&f.out.f[0]));
 for(i=0;i<3;i++){
  cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0x123,0,0,8,data,1,0);
  w.b[i==0?w.crc_delim:i==1?w.ack_delim:w.eof+3]=0;
  stop=transmit(&f,&w,2200,200,1);end(&f,stop);
  CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_FORM_ERROR)&&!f.out.f[0].complete);
 }
 cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0,0,0,8,data,1,0);
 CHECK(w.nstuff);w.b[w.stuff[0]]=w.b[w.stuff[0]-1];stop=transmit(&f,&w,2200,200,1);end(&f,stop);
 CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_STUFF_ERROR));
 CHECK(f.out.f[0].flags&TG_CAN_ERROR_FLAG_CANDIDATE);
 /* Every truncation tick at whole-bit boundary before EOF end is partial. */
 w=encode(0x321,1,0,2,data,1,0);
 for(i=1;i<w.n;i++){
  wire cut=w;cases++;cut.n=i;init(&f,500000,800,100,0,0);start(&f,0);
  stop=transmit(&f,&cut,2200,200,1);end(&f,stop);
  CHECK(f.out.n==1&&!f.out.f[0].complete&&(f.out.f[0].flags&TG_CAN_PARTIAL));
 }
 /* Last EOF sample alone does not establish the remainder of its bit. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0x123,0,0,1,data,1,0);
 stop=transmit(&f,&w,2200,200,1);end(&f,stop-1);CHECK(f.out.n==1&&!f.out.f[0].complete);
 /* GAP far in the future must not synthesize remaining bits or EOF. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);w=encode(0x123,0,0,1,data,1,0);
 {wire cut=w;cut.n=w.eof+2;stop=transmit(&f,&cut,2200,200,1);}
 at=f.d.last;event_raw(&f,TG_VEC_GAP,UINT64_C(100000000000),0,1,1);
 CHECK(f.out.n==1&&!f.out.f[0].complete&&(f.out.f[0].flags&TG_CAN_GAP));CHECK(f.out.f[0].end==at);
 /* CHANGE after gap does not establish an initial state. */
 event(&f,TG_VEC_CHANGE,UINT64_C(100000003000),1,1);
 stop=transmit(&f,&w,UINT64_C(100000006000),200,1);end(&f,stop);CHECK(f.out.n==1);
 /* Explicit RESYNC + idle permits recovery. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);event_raw(&f,TG_VEC_GAP,1000,0,1,1);
 event(&f,TG_VEC_RESYNC,2000,1,1);stop=transmit(&f,&w,4200,200,1);end(&f,stop);check_good(&f,0x123,0,0,1,data);
 /* Sequence loss, retrograde time and invalid watermark do not bridge. */
 for(i=0;i<3;i++){
  wire cut=w;cut.n=10;cases++;init(&f,500000,800,100,0,0);start(&f,0);
  stop=transmit(&f,&cut,2200,200,1);at=f.d.last;
  if(i==0){f.seq++;event(&f,TG_VEC_WATERMARK,stop+100000, f.level,1);}
  else if(i==1)event(&f,TG_VEC_CHANGE,at-1,!f.level,1);
  else event(&f,TG_VEC_WATERMARK,stop+100000,!f.level,1);
  CHECK(f.out.n==1&&!f.out.f[0].complete&&(f.out.f[0].flags&TG_CAN_GAP));
 }
 /* INIT with dominant bus, or no INIT, must not guess the partial SOF. */
 for(i=0;i<2;i++){
  cases++;init(&f,500000,800,100,0,0);if(i)event(&f,TG_VEC_INIT,0,0,1);
  stop=transmit(&f,&w,2200,200,1);end(&f,stop);CHECK(f.out.n==0);
 }
 /* Back-to-back frames after exactly three intermission bits. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);
 stop=transmit(&f,&w,2200,200,1);stop=transmit(&f,&w,stop+600,200,1);end(&f,stop);
 CHECK(f.out.n==2&&tracegpu_can_frame_data_valid(&f.out.f[0])&&tracegpu_can_frame_data_valid(&f.out.f[1]));
 /* Frame recovery after a stuff fault and eleven proven idle bits. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);
 {wire bad=w;CHECK(bad.nstuff);bad.b[bad.stuff[0]]=bad.b[bad.stuff[0]-1];stop=transmit(&f,&bad,2200,200,1);}
 stop=transmit(&f,&w,stop+2200,200,1);end(&f,stop);
 CHECK(f.out.n==2&&!tracegpu_can_frame_data_valid(&f.out.f[0])&&tracegpu_can_frame_data_valid(&f.out.f[1]));
 /* A dominant interruption during intermission revokes fast SOF recovery. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);
 stop=transmit(&f,&w,2200,200,1);
 event(&f,TG_VEC_CHANGE,stop+200,0,1);event(&f,TG_VEC_CHANGE,stop+400,1,1);
 stop=transmit(&f,&w,stop+600,200,1);end(&f,stop);CHECK(f.out.n==1);
 /* FD-format/reserved recessive bits and invalid extended SRR never valid. */
 for(i=0;i<4;i++){
  cases++;init(&f,500000,800,100,0,0);start(&f,0);
  w=encode(0x123,i?1:0,0,2,data,1,i==2?4u:i==3?8u:2u);
  stop=transmit(&f,&w,2200,200,1);end(&f,stop);
  CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_FORM_ERROR));
  CHECK(!tracegpu_can_frame_data_valid(&f.out.f[0]));
  if(i!=2)CHECK(f.out.f[0].flags&TG_CAN_UNSUPPORTED);
 }
 /* Every single physical-bit mutation, except the ACK fact, must not produce
  * a clean data frame. Error type can legitimately depend on desynchronizing. */
 w=encode(0x137,0,0,8,data,1,0);
 for(i=0;i<w.n;i++)if(i!=w.ack){
  wire bad=w;unsigned j;cases++;init(&f,500000,800,100,0,0);start(&f,0);
  bad.b[i]^=1;stop=transmit(&f,&bad,2200,200,1);end(&f,stop);
  for(j=0;j<f.out.n;j++)CHECK(!tracegpu_can_frame_data_valid(&f.out.f[j]));
 }
}
static void timing_budget_input(void){
 fixture f;wire w;uint8_t data[8]={0,0,0xff,0xff,0x55,0xaa,0x12,0x34};unsigned i;
 uint64_t stop;tracegpu_vector_event e;uint32_t used;int r;
 /* 0.5 percent transmitter drift, both signs, must require actual resync. */
 for(i=0;i<2;i++){
  cases++;init(&f,125000,800,100,0,0);start(&f,0);w=encode(0x1234567,1,0,8,data,1,0);
  stop=transmit(&f,&w,8800,i?804:796,1);end(&f,stop+800);
  check_good(&f,0x1234567,1,0,8,data);CHECK(f.out.f[0].resync_count>0);
 }
 /* Configured sample point is observable with a deliberately narrow pulse. */
 for(i=0;i<2;i++){
  cases++;init(&f,500000,i?800:600,50,0,0);start(&f,0);
  event(&f,TG_VEC_CHANGE,2200,0,1);event(&f,TG_VEC_CHANGE,2330,1,1);end(&f,2600);
  if(i)CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_FORM_ERROR));
  else CHECK(f.out.n==1&&!(f.out.f[0].flags&TG_CAN_FORM_ERROR));
 }
 /* Positive/negative phase corrections saturate at SJW; a second edge
  * before the next sample cannot resynchronize that bit a second time. */
 for(i=0;i<2;i++){
  uint64_t old_next;
  cases++;init(&f,500000,800,100,0,0);start(&f,0);
  event(&f,TG_VEC_CHANGE,2200,0,1);event(&f,TG_VEC_CHANGE,2400,1,1);
  event(&f,TG_VEC_WATERMARK,2561,1,1);old_next=f.d.next;
  event(&f,TG_VEC_CHANGE,i?2630:2570,0,1);
  CHECK(f.d.next==(i?old_next+20:old_next-20));CHECK(f.d.frame.resync_count==1);
  old_next=f.d.next;event(&f,TG_VEC_CHANGE,i?2640:2580,1,1);
  event(&f,TG_VEC_CHANGE,i?2650:2590,0,1);
  CHECK(f.d.next==old_next&&f.d.frame.resync_count==1);end(&f,3000);
 }
 /* Idle for near UINT64_MAX: constant time, no empty phantom frames. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);
 event(&f,TG_VEC_WATERMARK,UINT64_MAX-1000,1,1);CHECK(f.steps==2&&f.out.n==0);
 event(&f,TG_VEC_CHANGE,UINT64_MAX-100,0,1);end(&f,UINT64_MAX-1);
 CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_TIME_OVERFLOW));
 CHECK(f.out.f[0].end>=f.out.f[0].start);
 /* A stuck dominant busy interval cannot consume unbounded steps either. */
 cases++;init(&f,500000,800,100,0,0);start(&f,0);event(&f,TG_VEC_CHANGE,2200,0,1);
 event(&f,TG_VEC_WATERMARK,UINT64_C(1)<<60,0,1);CHECK(f.out.n==1&&f.steps<12);
 /* Queued single-event contract + budget and sequence wrap. */
 cases++;init(&f,500000,800,100,0,0);f.seq=UINT32_MAX;start(&f,0);
 event(&f,TG_VEC_CHANGE,2200,0,1);e.tick=100000;e.sequence=f.seq++;e.type=TG_VEC_WATERMARK;e.flags=1;e.levels=0;
 CHECK(tracegpu_can_vector_feed(&f.d,&e)==0);CHECK(tracegpu_can_vector_feed(&f.d,&e)==TG_CAN_BUSY);
 do{r=tracegpu_can_vector_step(&f.d,1,&used);CHECK(used<=1);}while(r==TG_CAN_BUSY);
 CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_STUFF_ERROR)&&!(f.out.f[0].flags&TG_CAN_SEQUENCE_ERROR));
 /* Invalid masks/types and END are terminal, not trusted level transitions. */
 cases++;init(&f,500000,800,100,7,0);start(&f,0);event(&f,TG_VEC_CHANGE,2200,0,1);
 event_raw(&f,TG_VEC_CHANGE,3000,0,1,1);CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_INPUT_ERROR));
 end(&f,4000);CHECK(tracegpu_can_vector_feed(&f.d,&e)==TG_CAN_ENDED);
 cases++;init(&f,500000,800,100,0,0);start(&f,0);event(&f,TG_VEC_CHANGE,2200,0,1);
 event_raw(&f,TG_VEC_WATERMARK,100000,0,255,1);
 CHECK(f.out.n==1&&(f.out.f[0].flags&TG_CAN_INPUT_ERROR)&&!f.d.known);
 cases++;{tracegpu_can_decode_config c={100000000,500000,800,100,0,0};
  CHECK(tracegpu_can_decode_config_valid(&c));c.bitrate=0;CHECK(!tracegpu_can_decode_config_valid(&c));
  c.bitrate=333333;CHECK(!tracegpu_can_decode_config_valid(&c));c.bitrate=500000;c.sjw_permille=250;
  CHECK(!tracegpu_can_decode_config_valid(&c));c.sjw_permille=100;c.channel=8;CHECK(!tracegpu_can_decode_config_valid(&c));
 }
}
int main(void){
 valid_vectors();errors_and_boundaries();timing_budget_input();
 printf("CAN_V37 PASS cases=%u checks=%u initial_seed=0x37ca2026 final_seed=0x%08" PRIx32 " decoder_bytes=%zu\n",cases,checks,seed,sizeof(tracegpu_can_vector_decoder));
 return 0;
}
