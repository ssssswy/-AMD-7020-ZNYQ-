#include "protocol_search.h"
#include <string.h>
int tracegpu_search_filter_valid(const tracegpu_search_filter *f){
 unsigned i,fixed=0;
 if(!f||f->protocol>2||f->channel>3||f->length>8||f->address < -1||f->address>127||f->read < -1||f->read>1||f->nack>2)return 0;
 for(i=0;i<f->length;i++){if(f->mask[i]!=0&&f->mask[i]!=255)return 0;fixed|=f->mask[i];}
 if(f->length&&!fixed)return 0;
 return f->protocol||f->errors||f->length;
}
void tracegpu_search_begin(tracegpu_search *q,const tracegpu_search_filter *f,const tracegpu_search_source *s,unsigned page){
 tracegpu_search_filter copy=*f;
 tracegpu_search_source source_copy=*s;
 memset(q,0,sizeof *q);q->filter=copy;q->source=source_copy;f=&q->filter;s=&q->source;q->page=page;q->skip=page*TG_SEARCH_RESULTS;q->incomplete=s->incomplete!=0;
 if(!tracegpu_search_filter_valid(f)||page>UINT32_MAX/TG_SEARCH_RESULTS){q->state=TG_SEARCH_BAD_QUERY;return;}
 if(f->protocol==0){if(!s->uart){q->state=TG_SEARCH_BAD_QUERY;return;}q->head=s->uart->head;q->size=s->uart->count;}
 else if(f->protocol==1){if(!s->i2c){q->state=TG_SEARCH_BAD_QUERY;return;}q->size=s->i2c->count;q->incomplete|=s->i2c->discarded!=0;}
 else {if(!s->spi){q->state=TG_SEARCH_BAD_QUERY;return;}q->size=s->spi->count;q->words=s->spi->word_count;q->incomplete|=s->spi->omitted!=0||s->spi->active;}
 if((f->protocol==0&&q->size>tracegpu_uart_ring_capacity(s->uart))||(f->protocol==1&&q->size>I2C_MAX_TRANSACTIONS)||(f->protocol==2&&(q->size>TRACEGPU_SPI_TRANSACTIONS||q->words>TRACEGPU_SPI_WORDS))){q->state=TG_SEARCH_BAD_QUERY;return;}
 q->state=TG_SEARCH_BUSY;
}
int tracegpu_search_source_valid(const tracegpu_search *q,const tracegpu_search_source *s){
 if(q->source.session!=s->session||q->source.generation!=s->generation||q->source.records!=s->records||q->source.first_id!=s->first_id||q->source.incomplete!=s->incomplete)return 0;
 if(q->filter.protocol==0)return s->uart==q->source.uart&&s->uart&&q->head==s->uart->head&&q->size==s->uart->count;
 if(q->filter.protocol==1)return s->i2c==q->source.i2c&&s->i2c&&q->size==s->i2c->count&&!s->i2c->busy;
 return s->spi==q->source.spi&&s->spi&&q->size==s->spi->count&&q->words==s->spi->word_count&&!s->spi->busy;
}
static void append(tracegpu_search *q,const tracegpu_search_result *r){
 if(q->seen++<q->skip)return;
 q->results[q->count++]=*r;
 if(q->count==TG_SEARCH_RESULTS)q->state=TG_SEARCH_MORE;
}
int tracegpu_search_step(tracegpu_search *q,const tracegpu_search_source *s,unsigned budget){
 if(q->state!=TG_SEARCH_BUSY)return 0;
 if(!tracegpu_search_source_valid(q,s)){q->state=TG_SEARCH_STALE;q->count=0;return -1;}
 while(budget--&&q->state==TG_SEARCH_BUSY){
  tracegpu_search_result r;int rc=tracegpu_search_record(q,&r);q->scanned++;
  if(rc<0){q->state=TG_SEARCH_DONE;break;}
  if(!rc)continue;
  if(q->filter.errors||!q->filter.length){append(q,&r);continue;}
  if(q->recent_count==8){memmove(q->recent,q->recent+1,7*sizeof r);q->recent_count--;}
  q->recent[q->recent_count++]=r;
  if(q->recent_count>=q->filter.length){
   unsigned begin=q->recent_count-q->filter.length,i;int match=1;
   if(q->filter.first_only&&q->recent[begin].offset!=0)match=0;
   for(i=0;i<q->filter.length;i++)if((q->recent[begin+i].bytes[0]&q->filter.mask[i])!=(q->filter.bytes[i]&q->filter.mask[i]))match=0;
   if(match){tracegpu_search_result out=q->recent[begin];out.end=r.end;out.length=q->filter.length;
    for(i=0;i<out.length;i++){out.bytes[i]=q->recent[begin+i].bytes[0];out.flags|=q->recent[begin+i].flags;}
    append(q,&out);
   }
  }
 }
 if(q->state!=TG_SEARCH_BUSY&&q->select_last){q->selected=q->count?q->count-1:0;q->select_last=0;}
 return q->state==TG_SEARCH_BUSY?0:1;
}
void tracegpu_search_cancel(tracegpu_search *q){q->state=TG_SEARCH_CANCELLED;q->count=0;q->recent_count=0;}
