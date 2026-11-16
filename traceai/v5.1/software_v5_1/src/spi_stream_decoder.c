#include "spi_stream_decoder.h"
#include <string.h>
static int selected(const tracegpu_spi_analysis *a,unsigned levels){return ((levels>>3)&1U)==a->config.cs_high;}
static void evict_row(tracegpu_spi_stream *s){
 tracegpu_spi_analysis *a=s->result;unsigned n,words=a->rows[0].count;
 memmove(a->words,a->words+words,(a->word_count-words)*sizeof a->words[0]);a->word_count-=words;
 memmove(a->rows,a->rows+1,(a->count-1)*sizeof a->rows[0]);a->count--;
 for(n=0;n<a->count;n++)a->rows[n].first-=words;
 if(a->active)a->current.first-=words;
 s->first_id++;s->evicted_rows++;s->evicted_words+=words;
}
static void begin_row(tracegpu_spi_analysis *a,uint64_t tick,unsigned flags){
 memset(&a->current,0,sizeof a->current);a->current.start=tick;a->current.end=tick;
 a->current.first=a->word_count;a->current.flags=(uint8_t)flags;
 a->active=1;a->bits=a->tx=a->rx=a->word_flags=0;
}
static void finish_row(tracegpu_spi_stream *s,uint64_t tick,unsigned flags){
 tracegpu_spi_analysis *a=s->result;
 if(!a->active)return;
 a->current.end=tick;a->current.flags|=(uint8_t)flags;a->current.remaining_bits=a->bits;
 if(a->bits)a->current.flags|=SPI_PARTIAL_END;
 if(!a->current.total&&!a->bits&&!a->current.flags)a->current.flags|=SPI_EMPTY;
 if(s->emit_transaction)s->emit_transaction(&a->current,s->closed_total,s->emit_context);
 if(!s->stream_only){if(a->count==TRACEGPU_SPI_TRANSACTIONS)evict_row(s);a->rows[a->count++]=a->current;}
 s->closed_total++;a->active=0;
}
static void sample(tracegpu_spi_stream *s,uint64_t tick,int ambiguous){
 tracegpu_spi_analysis *a=s->result;unsigned tx=(a->levels>>1)&1U,rx=(a->levels>>2)&1U;
 if(!a->bits)a->word_start=tick;
 if(ambiguous){a->word_flags|=SPI_AMBIG;a->current.flags|=SPI_AMBIG;}
 if(a->config.lsb_first){a->tx|=(uint8_t)(tx<<a->bits);a->rx|=(uint8_t)(rx<<a->bits);}
 else{a->tx=(uint8_t)((a->tx<<1)|tx);a->rx=(uint8_t)((a->rx<<1)|rx);}
 if(++a->bits==8){
  tracegpu_spi_word emitted;memset(&emitted,0,sizeof emitted);
  emitted.start=a->word_start;emitted.end=tick;emitted.mosi=a->tx;emitted.miso=a->rx;emitted.bits=8;emitted.flags=a->word_flags;
  if(s->emit_word)s->emit_word(&emitted,s->words_total,s->closed_total,a->current.total,s->emit_context);
  a->current.total++;s->words_total++;
  if(s->stream_only)a->current.count++;
  else {if(a->word_count==TRACEGPU_SPI_WORDS&&a->count)evict_row(s);
  if(a->word_count<TRACEGPU_SPI_WORDS){
   tracegpu_spi_word *w=&a->words[a->word_count++];memset(w,0,sizeof *w);
   w->start=a->word_start;w->end=tick;w->mosi=a->tx;w->miso=a->rx;w->bits=8;w->flags=a->word_flags;
   a->current.count++;
  }else a->current.flags|=SPI_TRUNCATED;}
  a->bits=a->tx=a->rx=a->word_flags=0;
 }
 a->current.end=tick;
}
static void process_change(tracegpu_spi_stream *s,const tracegpu_vector_event *e,int bounded_end){
 tracegpu_spi_analysis *a=s->result;unsigned old=a->levels,changed=old^e->levels,ch,i;
 int ambiguous=(changed&14U)!=0;
 a->levels=e->levels;a->last_tick=e->tick;s->processed_tick=e->tick;
 for(ch=0;ch<4;ch++)if(changed&(1U<<ch)){a->edges[ch]++;a->last_change[ch]=e->tick;}
 if((changed&1U)&&(e->levels&1U)){
  if(a->rise_seen){uint64_t dt=e->tick-a->rise_tick;uint32_t d=dt>UINT32_MAX?UINT32_MAX:(uint32_t)dt;
   if(d<a->period_min)a->period_min=d;
   if(d>a->period_max)a->period_max=d;
   a->periods++;
  }
  a->rise_seen=1;a->rise_tick=e->tick;
 }
 if(s->recover){if(!selected(a,e->levels))s->recover=0;return;}
 if((changed&8U)&&selected(a,e->levels)&&!selected(a,old))begin_row(a,e->tick,0);
 if(a->active&&(changed&1U)&&((e->levels&1U)==!((a->config.mode>>1)^(a->config.mode&1U)))){
  unsigned future=e->levels;
  for(ch=1;ch<4;ch++)if(e->tick-a->last_change[ch]<=a->config.guard_ticks)ambiguous=1;
  for(i=1;i<s->pending_count;i++){
   const tracegpu_vector_event *p=&s->pending[i];
   if(p->tick-e->tick>a->config.guard_ticks)break;
   if(p->type==TG_VEC_CHANGE){if((p->levels^future)&14U)ambiguous=1;future=p->levels;}
   if(p->type==TG_VEC_GAP)ambiguous=1;
  }
  if(bounded_end&&s->last_wire_tick-e->tick<=a->config.guard_ticks)ambiguous=1;
  if(!(a->current.flags&SPI_PARTIAL_START))sample(s,e->tick,ambiguous);
 }
 if((changed&8U)&&!selected(a,e->levels)&&selected(a,old))finish_row(s,e->tick,0);
}
static void drain(tracegpu_spi_stream *s,uint64_t watermark,int final){
 while(s->pending_count){
  tracegpu_vector_event e=s->pending[0];
  if(!final&&(watermark<e.tick||watermark-e.tick<=s->result->config.guard_ticks))break;
  if(e.type==TG_VEC_CHANGE)process_change(s,&e,final);
  s->pending_count--;memmove(s->pending,s->pending+1,s->pending_count*sizeof s->pending[0]);
 }
}
void tracegpu_spi_stream_init(tracegpu_spi_stream *s,tracegpu_spi_analysis *a,const tracegpu_spi_config *cfg,uint32_t session){
 memset(s,0,sizeof *s);memset(a,0,sizeof *a);s->result=a;a->session=session;a->period_min=UINT32_MAX;
 if(!tracegpu_spi_config_valid(cfg)){s->failed=1;a->errors=1;return;}a->config=*cfg;
}
void tracegpu_spi_stream_set_emit(tracegpu_spi_stream *s,tracegpu_spi_word_emit_fn w,tracegpu_spi_transaction_emit_fn t,void *ctx,int only){
 s->emit_word=w;s->emit_transaction=t;s->emit_context=ctx;s->stream_only=(uint8_t)(only!=0);
}
void tracegpu_spi_stream_fail(tracegpu_spi_stream *s){
 if(!s||!s->result||s->failed)return;
 s->failed=1;s->pending_count=0;s->result->errors++;
 finish_row(s,s->last_wire_tick,SPI_GAP|SPI_PARTIAL_END);s->result->valid=s->result->busy=0;
}
int tracegpu_spi_stream_index(const tracegpu_spi_stream *s,uint64_t id){
 if(id<s->first_id||id-s->first_id>=s->result->count)return -1;
 return (int)(id-s->first_id);
}
void tracegpu_spi_stream_event(const tracegpu_vector_event *e,void *context){
 tracegpu_spi_stream *s=context;tracegpu_spi_analysis *a=s->result;unsigned ch;
 if(s->failed)return;
 if(s->ended||(!s->have_init&&e->type!=TG_VEC_INIT)||(s->have_init&&e->tick<s->last_wire_tick)){tracegpu_spi_stream_fail(s);return;}
 s->last_wire_tick=e->tick;
 if(e->type==TG_VEC_INIT){
  if(s->have_init){tracegpu_spi_stream_fail(s);return;}
  s->have_init=1;a->init_tick=e->tick;a->init_vector=a->levels=e->levels;
  for(ch=0;ch<4;ch++)a->last_change[ch]=e->tick;
  a->busy=a->valid=1;
  if(selected(a,e->levels))begin_row(a,e->tick,SPI_PARTIAL_START);
 }else if(e->type==TG_VEC_GAP){
  // Changes inside the unknown lookahead window cannot be certified.
  drain(s,e->tick,0);s->pending_count=0;
  finish_row(s,e->tick,SPI_GAP|SPI_PARTIAL_END);a->errors++;a->valid=0;s->recover=1;s->wait_resync=1;
 }else if(e->type==TG_VEC_RESYNC){
  if(!s->wait_resync){tracegpu_spi_stream_fail(s);return;}
  s->wait_resync=0;a->levels=e->levels;s->recover=(uint8_t)selected(a,e->levels);
  for(ch=0;ch<4;ch++)a->last_change[ch]=e->tick;
  a->rise_seen=0;s->processed_tick=e->tick;
 }else if(s->wait_resync){tracegpu_spi_stream_fail(s);return;}
 else if(e->type==TG_VEC_CHANGE){
  // With <=4 guard ticks and at most one CHANGE/tick, at most six records
  // are pending. Treat malformed density as a fault, never overwrite.
  if(s->pending_count==16){tracegpu_spi_stream_fail(s);return;}
  s->pending[s->pending_count++]=*e;drain(s,e->tick,0);
 }else if(e->type==TG_VEC_WATERMARK){drain(s,e->tick,0);}
 else if(e->type==TG_VEC_END){
  drain(s,e->tick,1);finish_row(s,e->tick,SPI_PARTIAL_END);s->ended=1;a->busy=0;a->valid=a->errors==0;
 }else tracegpu_spi_stream_fail(s);
 s->wire_levels=e->levels;
}
