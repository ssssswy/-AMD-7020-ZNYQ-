#include "logic_trace_view.h"
#include <string.h>
int tracegpu_logic_view_valid(const tracegpu_logic_trace_view *v,uint32_t id,uint32_t gen){
    const tracegpu_vector_ring *r;
    const tracegpu_vector_record *first,*last;
    if(!v||!v->bound||v->capture_id!=id||v->generation!=gen)return 0;
    r=v->owner;
    if(!r||r->records!=v->snapshot.records||r->head!=v->snapshot.head||
       r->count!=v->snapshot.count||r->capacity!=v->snapshot.capacity||
       r->total!=v->snapshot.total||r->boundary_tick!=v->snapshot.boundary_tick||
       r->abi!=v->snapshot.abi||r->channel_count!=v->snapshot.channel_count||r->capture_mask!=v->snapshot.capture_mask||
       r->boundary_known!=v->snapshot.boundary_known||r->boundary_levels!=v->snapshot.boundary_levels)return 0;
    if(!r->count)return 1;
    first=tracegpu_vector_oldest(r,0);last=tracegpu_vector_oldest(r,r->count-1);
    return tracegpu_vector_unpack(first).sequence==v->first_sequence&&
           tracegpu_vector_unpack(last).sequence==v->last_sequence;
}
int tracegpu_logic_view_bind(tracegpu_logic_trace_view *v,const tracegpu_vector_ring *r,
                            uint32_t id,uint32_t gen,int stopped){
    if(!v)return -1;
    memset(v,0,sizeof *v);
    if(!stopped||!r||!r->records||!r->capacity||r->count>r->capacity||r->head>=r->capacity||
       r->abi!=TRACEGPU_VECTOR_VERSION||r->channel_count!=TRACEGPU_VECTOR_CHANNELS||!r->capture_mask)return -1;
    v->owner=r;v->snapshot=*r;v->capture_id=id;v->generation=gen;v->bound=1;
    if(r->count){
        tracegpu_vector_event first=tracegpu_vector_unpack(tracegpu_vector_oldest(r,0));
        tracegpu_vector_event last=tracegpu_vector_unpack(tracegpu_vector_oldest(r,r->count-1));
        v->begin=r->boundary_known?r->boundary_tick:first.tick;v->end=last.tick;
        v->first_sequence=first.sequence;v->last_sequence=last.sequence;
        if(v->begin>v->end){v->bound=0;return -1;}
    }
    return 0;
}
const tracegpu_vector_record *tracegpu_logic_view_record(const tracegpu_logic_trace_view *v,uint32_t i){
    if(!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return 0;
    return tracegpu_vector_oldest(&v->snapshot,i);
}
uint32_t tracegpu_logic_view_lower_bound(const tracegpu_logic_trace_view *v,uint64_t tick){
    if(!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return 0;
    return tracegpu_vector_lower_bound(&v->snapshot,tick);
}
int tracegpu_logic_view_state(const tracegpu_logic_trace_view *v,uint64_t tick,unsigned *levels){
    uint32_t lo=0,hi;
    if(!v||!levels||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return -1;
    if(!v->snapshot.count||tick<v->begin||tick>v->end)return 0;
    hi=v->snapshot.count;
    /* upper_bound: if metadata shares a tick, its final state wins. */
    while(lo<hi){uint32_t m=lo+(hi-lo)/2;
        if(tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,m)).tick<=tick)lo=m+1;else hi=m;}
    if(lo){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,lo-1));
        if(e.type==TG_VEC_GAP)return 0;
        *levels=e.levels;return 1;}
    if(v->snapshot.boundary_known){*levels=v->snapshot.boundary_levels;return 1;}
    return 0;
}
int tracegpu_logic_view_sample8(const tracegpu_logic_trace_view *v,uint64_t begin,
                              uint32_t period,unsigned count,uint8_t *out){
    uint32_t lo=0,hi;unsigned levels=0,known=0;
    if(!out||!period||!count||count>768||!v||
       !tracegpu_logic_view_valid(v,v->capture_id,v->generation))return -1;
    uint64_t span=(uint64_t)(count-1)*period;
    if(begin>UINT64_MAX-span||begin<v->begin||begin+span>v->end||
       !tracegpu_vector_retained(&v->snapshot,begin,begin+span))return -1;
    hi=v->snapshot.count;
    while(lo<hi){uint32_t m=lo+(hi-lo)/2;
        if(tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,m)).tick<=begin)lo=m+1;else hi=m;}
    if(lo){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,lo-1));levels=e.levels;known=e.type!=TG_VEC_GAP;}
    else{levels=v->snapshot.boundary_levels;known=v->snapshot.boundary_known;}
    /* Dense unrelated channels can otherwise make a cursor visit millions of
     * records. Retention/gap validation above remains mandatory; use the old
     * bounded random queries when the candidate interval is too dense. */
    uint32_t tail=tracegpu_vector_lower_bound(&v->snapshot,begin+span);
    if(tail>=lo&&tail-lo>count*8U){
        for(unsigned i=0;i<count;i++){
            if(tracegpu_logic_view_state(v,begin+(uint64_t)i*period,&levels)!=1)return -1;
            out[i]=(uint8_t)levels;
        }
        return tracegpu_logic_view_valid(v,v->capture_id,v->generation)?0:-1;
    }
    for(unsigned i=0;i<count;i++){
        uint64_t tick=begin+(uint64_t)i*period;
        while(lo<v->snapshot.count){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,lo));
            if(e.tick>tick)break;
            levels=e.levels;known=e.type!=TG_VEC_GAP;lo++;
        }
        if(!known)return -1;
        out[i]=(uint8_t)levels;
    }
    return tracegpu_logic_view_valid(v,v->capture_id,v->generation)?0:-1;
}
