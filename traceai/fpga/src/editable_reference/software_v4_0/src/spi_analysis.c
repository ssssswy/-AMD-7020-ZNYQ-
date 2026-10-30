#include "spi_analysis.h"
#include "uart_analyzer_hw.h"
#include <string.h>

int tracegpu_spi_config_valid(const tracegpu_spi_config *c){return c&&c->mode<4&&c->lsb_first<2&&c->cs_high<2&&c->guard_ticks<=4;}
int tracegpu_spi_selftest(const tracegpu_spi_analysis *a,unsigned pattern,unsigned length){
 unsigned n,expected=pattern==6?1:length;
 if(!a->valid||a->errors||a->omitted||pattern>7||!length)return -1;
 if(a->count!=(pattern==4?(length+3)/4:1)||a->word_count!=expected)return -2;
 for(n=0;n<a->count;n++){
  unsigned flags=pattern==5||pattern==6?SPI_PARTIAL_END:pattern==7?SPI_AMBIG:0;
  if(a->rows[n].flags!=flags)return -3;
 }
 if(pattern==6&&a->rows[0].remaining_bits!=3)return -4;
 if(a->edges[0]!=(pattern==6?22:length*16)||a->edges[3]!=(pattern==5?1:pattern==4?2*((length+3)/4):2))return -5;
 for(n=0;n<expected;n++){
  unsigned tx=pattern==1?0:pattern==2?255:pattern==3?0xaa:n;
  unsigned rx=pattern==1?0:pattern==2?255:pattern==3?0x55:255-n;
  if(pattern==7&&n==0){if(!(a->words[n].flags&SPI_AMBIG))return -6;continue;}
  if(a->words[n].flags||a->words[n].mosi!=(tx&255)||a->words[n].miso!=(rx&255))return -7;
 }
 return 1;
}
static int selected(const tracegpu_spi_analysis *a,unsigned levels){return ((levels>>3)&1U)==a->config.cs_high;}
static void begin_transaction(tracegpu_spi_analysis *a,uint64_t tick,unsigned flags){
 memset(&a->current,0,sizeof a->current);a->current.start=tick;a->current.first=a->word_count;a->current.flags=(uint8_t)flags;
 a->active=1;a->bits=a->tx=a->rx=a->word_flags=0;
}
static void finish(tracegpu_spi_analysis *a,uint64_t tick,unsigned flags){
 if(!a->active)return;
 a->current.end=tick;a->current.flags|=(uint8_t)flags;a->current.remaining_bits=a->bits;
 if(a->bits)a->current.flags|=SPI_PARTIAL_END;
 if(!a->current.total&&!a->bits&&!a->current.flags)a->current.flags|=SPI_EMPTY;
 if(a->count<TRACEGPU_SPI_TRANSACTIONS)a->rows[a->count++]=a->current;else a->omitted++;
 a->active=0;
}
void tracegpu_spi_invalidate(tracegpu_spi_analysis *a){
 unsigned i;a->errors++;a->valid=0;a->busy=0;
 finish(a,a->last_tick,SPI_GAP|SPI_PARTIAL_END);
 for(i=0;i<a->count;i++)a->rows[i].flags|=SPI_GAP;
 for(i=0;i<a->word_count;i++)a->words[i].flags|=SPI_GAP;
}
void tracegpu_spi_begin(tracegpu_spi_analysis *a,const tracegpu_uart_event_ring *const r[4],uint32_t session,const tracegpu_spi_config *config,int trusted){
 unsigned ch;memset(a,0,sizeof *a);a->session=session;a->period_min=UINT32_MAX;
 if(!trusted||!tracegpu_spi_config_valid(config)){a->errors=1;return;}a->config=*config;
 for(ch=0;ch<4;ch++){
  const tracegpu_uart_event_ring *p=r[ch];
  if(!p||!p->count||!p->init_seen||p->lost_events){a->errors=1;return;}
  if(!ch){a->init_tick=p->init_tick;a->init_vector=p->init_vector;a->common_tick=p->init_tick;}
  else if(p->init_tick!=a->init_tick||p->init_vector!=a->init_vector){a->errors=1;return;}
  a->ring[ch]=p;a->head[ch]=p->head;a->size[ch]=p->count;
  if(p->boundary_known&&p->boundary_tick>a->common_tick)a->common_tick=p->boundary_tick;
  if(p->boundary_known?a->ring[ch]->boundary_level:((a->init_vector>>ch)&1U))a->levels|=(uint8_t)(1U<<ch);
  a->last_change[ch]=p->boundary_known?p->boundary_tick:a->init_tick;
 }
 a->busy=1;
}
static void sample(tracegpu_spi_analysis *a,uint64_t tick,int uncertain){
 unsigned tx=(a->levels>>1)&1U,rx=(a->levels>>2)&1U;
 if(!a->bits)a->word_start=tick;
 if(uncertain){a->word_flags|=SPI_AMBIG;a->current.flags|=SPI_AMBIG;}
 if(a->config.lsb_first){a->tx|=(uint8_t)(tx<<a->bits);a->rx|=(uint8_t)(rx<<a->bits);}
 else {a->tx=(uint8_t)((a->tx<<1)|tx);a->rx=(uint8_t)((a->rx<<1)|rx);}
 if(++a->bits==8){
  a->current.total++;
  if(a->count<TRACEGPU_SPI_TRANSACTIONS&&a->word_count<TRACEGPU_SPI_WORDS){
   tracegpu_spi_word *w=&a->words[a->word_count++];w->start=a->word_start;w->end=tick;w->mosi=a->tx;w->miso=a->rx;w->bits=8;w->flags=a->word_flags;a->current.count++;
  }else a->current.flags|=SPI_TRUNCATED;
  a->bits=a->tx=a->rx=a->word_flags=0;
 }
}
int tracegpu_spi_step(tracegpu_spi_analysis *a,uint32_t session,unsigned budget){
 unsigned ch;if(!a->busy)return a->valid?1:-1;
 if(session!=a->session){tracegpu_spi_invalidate(a);return -1;}
 for(ch=0;ch<4;ch++)if(a->ring[ch]->head!=a->head[ch]||a->ring[ch]->count!=a->size[ch]||a->ring[ch]->lost_events){tracegpu_spi_invalidate(a);return -1;}
 while(budget){
  const tracegpu_uart_event *e[4]={0,0,0,0};uint64_t t[4],tick=UINT64_MAX;unsigned changed=0,old=a->levels,consumed=0;
  for(ch=0;ch<4;ch++){
   t[ch]=UINT64_MAX;
   if(a->index[ch]<a->size[ch]){e[ch]=tracegpu_uart_ring_oldest(a->ring[ch],a->index[ch]);t[ch]=tracegpu_uart_event_timestamp(e[ch]);if(t[ch]<tick)tick=t[ch];}
  }
  if(tick==UINT64_MAX){finish(a,a->last_tick,SPI_PARTIAL_END);a->busy=0;a->valid=a->errors==0;return a->valid?1:-1;}
  if(tick<a->last_tick){tracegpu_spi_invalidate(a);return -1;}a->last_tick=tick;
  if(!a->started&&tick>=a->common_tick){a->started=1;if(selected(a,a->levels))begin_transaction(a,tick,SPI_PARTIAL_START);}
  for(ch=0;ch<4;ch++)if(e[ch]&&t[ch]==tick){
   unsigned type=tracegpu_uart_event_type(e[ch]),level;
   a->index[ch]++;consumed++;
   if((e[ch]->word[2]&255U)!=ch){tracegpu_spi_invalidate(a);return -1;}
   if(type==TRACEGPU_RECORD_SESSION_INIT){
    if(tick!=a->init_tick||tracegpu_uart_event_flags(e[ch])!=(0xf0U|a->init_vector)||(a->init_consumed&(1U<<ch))){tracegpu_spi_invalidate(a);return -1;}
    a->init_consumed|=(uint8_t)(1U<<ch);
   }else if(type==TRACEGPU_RECORD_RAW_EDGE){
    level=tracegpu_uart_event_level(e[ch]);
    if(tick<=a->init_tick||level==((a->levels>>ch)&1U)){tracegpu_spi_invalidate(a);return -1;}
    changed|=1U<<ch;a->edges[ch]++;a->levels=(uint8_t)((a->levels&~(1U<<ch))|(level<<ch));a->last_change[ch]=tick;
   }else if(type!=TRACEGPU_RECORD_PACKET_END){tracegpu_spi_invalidate(a);return -1;}
  }
  budget=consumed>=budget?0:budget-consumed;
  if(tick<a->common_tick)continue;
  if((changed&1U)&&(a->levels&1U)){
   if(a->rise_seen){uint64_t dt=tick-a->rise_tick;uint32_t d=dt>UINT32_MAX?UINT32_MAX:(uint32_t)dt;if(d<a->period_min)a->period_min=d;if(d>a->period_max)a->period_max=d;a->periods++;}
   a->rise_seen=1;a->rise_tick=tick;
  }
  if((changed&8U)&&selected(a,a->levels)&&!selected(a,old))begin_transaction(a,tick,0);
  if(a->active&&(changed&1U)&&((a->levels&1U)==!( (a->config.mode>>1)^(a->config.mode&1U)))){
   int uncertain=(changed&14U)!=0;
   for(ch=1;ch<4;ch++){
    if(tick>=a->last_change[ch]&&tick-a->last_change[ch]<=a->config.guard_ticks)uncertain=1;
    /* The next chronological event on this line is enough to bound lookahead.
       Tail/metadata cannot hide an earlier edge in a valid stopped stream. */
    if(a->index[ch]<a->size[ch]){
     const tracegpu_uart_event *next=tracegpu_uart_ring_oldest(a->ring[ch],a->index[ch]);uint64_t nt=tracegpu_uart_event_timestamp(next);
     if(tracegpu_uart_event_type(next)==TRACEGPU_RECORD_RAW_EDGE&&nt>=tick&&nt-tick<=a->config.guard_ticks)uncertain=1;
    }
   }
   if(!(a->current.flags&SPI_PARTIAL_START))sample(a,tick,uncertain);
  }
  if((changed&8U)&&!selected(a,a->levels)&&selected(a,old))finish(a,tick,0);
 }
 return 0;
}
