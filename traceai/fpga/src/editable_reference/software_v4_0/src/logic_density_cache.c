#include "logic_density_cache.h"
#include <string.h>
int tracegpu_logic_density_copy(tracegpu_logic_density_cache *c,const tracegpu_logic_index *x,uint8_t out[64]){
 tracegpu_logic_density_key key;
 memset(&key,0,sizeof key);
 if(!out)return -1;
 if(!c||!x||!x->valid||x->busy||!x->view||!tracegpu_logic_view_valid(x->view,x->id,x->generation)||x->position!=x->view->snapshot.count){
  memset(out,0,64);if(c)c->valid=0;return -1;
 }
 const tracegpu_logic_trace_view *v=x->view;
 key.owner=v->owner;key.records=v->snapshot.records;key.total=v->snapshot.total;key.boundary_tick=v->snapshot.boundary_tick;
 key.begin=v->begin;key.end=v->end;key.changes=x->changes;key.id=x->id;key.generation=x->generation;
 key.head=v->snapshot.head;key.count=v->snapshot.count;key.capacity=v->snapshot.capacity;
 key.first=v->first_sequence;key.last=v->last_sequence;key.position=x->position;key.gaps=x->gaps;
 if(c->valid&&!memcmp(&key,&c->key,sizeof key)){memcpy(out,c->pixels,64);c->hits++;return 1;}
 memset(c->pixels,0,64);
 for(unsigned j=0;j<TG_LOGIC_INDEX_BINS;j++){
  unsigned b=j/8,n=x->bins[j].changes;
  if(x->bins[j].gap)c->pixels[b]=255;
  else if(c->pixels[b]!=255&&n){unsigned total=c->pixels[b]+n;c->pixels[b]=(uint8_t)(total>7?7:total);}
 }
 c->key=key;c->valid=1;c->rebuilds++;memcpy(out,c->pixels,64);return 0;
}
