#include "wave_lod.h"
#include <string.h>

_Static_assert(sizeof(tracegpu_lod_summary)==56U,"LOD workspace ABI");

static int ring_valid(const tracegpu_vector_ring *r)
{
    return r&&r->records&&r->capacity&&r->capacity<=TRACEGPU_LOD_MAX_LEAVES*TRACEGPU_LOD_LEAF_RECORDS&&
        r->head<r->capacity&&r->count<=r->capacity&&r->total>=r->count&&
        r->abi==TRACEGPU_VECTOR_VERSION&&r->channel_count==TRACEGPU_VECTOR_CHANNELS&&
        r->capture_mask;
}
static tracegpu_lod_identity identity(const tracegpu_vector_ring *r,uint32_t capture,uint32_t generation)
{
    tracegpu_lod_identity s;
    memset(&s,0,sizeof s);s.owner=r;s.records=r->records;s.total=r->total;s.evicted=r->evicted;
    s.boundary=r->boundary_tick;s.capacity=r->capacity;s.head=r->head;s.count=r->count;
    s.abi=r->abi;s.capture_id=capture;s.generation=generation;s.mask=r->capture_mask;
    s.channels=r->channel_count;s.known=r->boundary_known;s.levels=r->boundary_levels;
    return s;
}
static int same_owner(const tracegpu_lod_identity *a,const tracegpu_lod_identity *b)
{
    return a->owner==b->owner&&a->records==b->records&&a->capacity==b->capacity&&
        a->abi==b->abi&&a->capture_id==b->capture_id&&a->generation==b->generation&&
        a->mask==b->mask&&a->channels==b->channels;
}
static int same_snapshot(const tracegpu_lod_identity *a,const tracegpu_lod_identity *b)
{
    return same_owner(a,b)&&a->total==b->total&&a->evicted==b->evicted&&
        a->boundary==b->boundary&&a->head==b->head&&a->count==b->count&&
        a->known==b->known&&a->levels==b->levels;
}
static int continuous(const tracegpu_lod_identity *a,const tracegpu_lod_identity *b)
{
    uint64_t delta,grown,extra;
    if(!same_owner(a,b)||b->total<a->total||b->evicted<a->evicted)return 0;
    delta=b->total-a->total;
    if(delta>UINT64_MAX-a->count)return 0;
    grown=a->count+delta;extra=grown>a->capacity?grown-a->capacity:0U;
    if(extra>UINT64_MAX-a->evicted)return 0;
    if(b->head!=(a->head+delta%a->capacity)%a->capacity||
       b->count!=(grown>a->capacity?a->capacity:(uint32_t)grown)||
       b->evicted!=a->evicted+extra)return 0;
    return delta||same_snapshot(a,b);
}
static uint32_t oldest_slot(const tracegpu_vector_ring *r)
{
    return (r->head+r->capacity-r->count)%r->capacity;
}
static void next_epoch(tracegpu_lod_index *x)
{
    if(x->epoch==UINT32_MAX){x->epoch=1U;x->cleared_nodes=0U;}
    else x->epoch++;
    x->snapshot_valid=0U;
}
void tracegpu_lod_init(tracegpu_lod_index *x,tracegpu_lod_summary *nodes,uint32_t node_count)
{
    if(!x)return;
    memset(x,0,sizeof *x);x->nodes=nodes;x->node_count=node_count;x->epoch=1U;
}
void tracegpu_lod_reset(tracegpu_lod_index *x)
{
    if(!x)return;
    next_epoch(x);x->bound=0U;x->rebuild_remaining=0U;
}
static void restart(tracegpu_lod_index *x,const tracegpu_vector_ring *r,
                    const tracegpu_lod_identity *s)
{
    uint32_t occupied;
    next_epoch(x);x->observed=*s;x->bound=1U;x->indexed_total=r->total;
    x->physical_leaves=(r->capacity+TRACEGPU_LOD_LEAF_RECORDS-1U)/TRACEGPU_LOD_LEAF_RECORDS;
    x->tree_leaves=1U;while(x->tree_leaves<x->physical_leaves)x->tree_leaves*=2U;
    x->rebuild_next=oldest_slot(r)/TRACEGPU_LOD_LEAF_RECORDS;
    occupied=(oldest_slot(r)%TRACEGPU_LOD_LEAF_RECORDS+r->count+TRACEGPU_LOD_LEAF_RECORDS-1U)/TRACEGPU_LOD_LEAF_RECORDS;
    if(occupied>x->physical_leaves)occupied=x->physical_leaves;
    x->rebuild_remaining=occupied;x->stats.rebuilds++;
}
static tracegpu_lod_summary merge(const tracegpu_lod_index *x,uint32_t left,uint32_t right)
{
    tracegpu_lod_summary s;
    const tracegpu_lod_summary *a=&x->nodes[left],*b=&x->nodes[right];
    memset(&s,0,sizeof s);s.epoch=x->epoch;
    /* Missing/incomplete children cannot be compressed as a full subtree. */
    if(a->epoch!=x->epoch||b->epoch!=x->epoch||!a->present||!b->present)return s;
    s.present=1U;s.first_tick=a->first_tick;s.last_tick=b->last_tick;
    s.first_levels=a->first_levels;s.last_levels=b->last_levels;
    s.eligible=(uint8_t)(a->eligible&&b->eligible&&a->last_tick<b->first_tick);
    for(unsigned ch=0;ch<TRACEGPU_VECTOR_CHANNELS;ch++)
        s.edges[ch]=a->edges[ch]+b->edges[ch]+(((a->last_levels^b->first_levels)>>ch)&1U);
    return s;
}
static void update_leaf(tracegpu_lod_index *x,const tracegpu_vector_ring *r,uint32_t leaf)
{
    uint32_t begin=leaf*TRACEGPU_LOD_LEAF_RECORDS,logical;
    uint32_t node=x->tree_leaves+leaf;
    tracegpu_lod_summary s;
    memset(&s,0,sizeof s);s.epoch=x->epoch;
    logical=(begin+r->capacity-oldest_slot(r))%r->capacity;
    if(begin<=r->capacity&&TRACEGPU_LOD_LEAF_RECORDS<=r->capacity-begin&&
       logical<r->count&&TRACEGPU_LOD_LEAF_RECORDS<=r->count-logical){
        unsigned prior=0U;
        s.present=1U;s.eligible=1U;
        for(unsigned i=0;i<TRACEGPU_LOD_LEAF_RECORDS;i++){
            const tracegpu_vector_record *v=&r->records[begin+i];
            tracegpu_vector_event e=tracegpu_vector_unpack(v);
            if(!i){s.first_tick=e.tick;s.first_levels=e.levels;}
            else{
                if(e.tick<=s.last_tick)s.eligible=0U;
                for(unsigned ch=0;ch<TRACEGPU_VECTOR_CHANNELS;ch++)s.edges[ch]+=((prior^e.levels)>>ch)&1U;
            }
            s.last_tick=e.tick;s.last_levels=e.levels;prior=e.levels;
            if(e.type!=TG_VEC_CHANGE||e.flags!=r->capture_mask||
               !tracegpu_vector_record_valid_mask(v,r->capture_mask))s.eligible=0U;
        }
        x->stats.records_read+=TRACEGPU_LOD_LEAF_RECORDS;
        x->stats.last_records+=TRACEGPU_LOD_LEAF_RECORDS;
    }
    x->nodes[node]=s;
    for(node/=2U;node;node/=2U)x->nodes[node]=merge(x,node*2U,node*2U+1U);
    x->stats.leaf_updates++;x->stats.last_leaves++;
}
int tracegpu_lod_ready(const tracegpu_lod_index *x,const tracegpu_vector_ring *r,
                      uint32_t capture,uint32_t generation)
{
    tracegpu_lod_identity s;
    if(!x||!x->nodes||!x->bound||!x->snapshot_valid||
       x->cleared_nodes<x->node_count||!ring_valid(r))return 0;
    s=identity(r,capture,generation);return same_snapshot(&x->snapshot,&s);
}
int tracegpu_lod_service(tracegpu_lod_index *x,const tracegpu_vector_ring *r,
                        uint32_t capture,uint32_t generation,unsigned budget)
{
    tracegpu_lod_identity s;
    uint32_t need;
    if(!x)return 0;
    x->stats.service_calls++;x->stats.last_leaves=x->stats.last_records=x->stats.last_cleared=0U;
    if(!ring_valid(r)||!x->nodes){x->snapshot_valid=0U;x->bound=0U;return 0;}
    need=1U;while(need<(r->capacity+TRACEGPU_LOD_LEAF_RECORDS-1U)/TRACEGPU_LOD_LEAF_RECORDS)need*=2U;
    if(x->node_count<2U*need){x->snapshot_valid=0U;x->bound=0U;return 0;}
    s=identity(r,capture,generation);
    if(!x->bound||!continuous(&x->observed,&s)||r->total<x->indexed_total||
       r->total-x->indexed_total>r->capacity)restart(x,r,&s);
    x->observed=s;
    if(budget>TRACEGPU_LOD_MAX_SERVICE_LEAVES)budget=TRACEGPU_LOD_MAX_SERVICE_LEAVES;
    if(!budget)return tracegpu_lod_ready(x,r,capture,generation);
    if(x->cleared_nodes<x->node_count){
        uint32_t count=2U*budget;
        if(count>x->node_count-x->cleared_nodes)count=x->node_count-x->cleared_nodes;
        memset(x->nodes+x->cleared_nodes,0,(size_t)count*sizeof *x->nodes);
        x->cleared_nodes+=count;x->stats.nodes_cleared+=count;x->stats.last_cleared=count;
        return 0;
    }
    while(budget&&x->rebuild_remaining){
        update_leaf(x,r,x->rebuild_next);x->rebuild_next=(x->rebuild_next+1U)%x->physical_leaves;
        x->rebuild_remaining--;budget--;
    }
    while(budget&&!x->rebuild_remaining&&x->indexed_total<r->total){
        uint64_t remaining=r->total-x->indexed_total;
        uint32_t p=(r->head+r->capacity-(uint32_t)(remaining%r->capacity))%r->capacity;
        uint32_t n=TRACEGPU_LOD_LEAF_RECORDS-p%TRACEGPU_LOD_LEAF_RECORDS;
        if(n>r->capacity-p)n=r->capacity-p;
        if((uint64_t)n>remaining)n=(uint32_t)remaining;
        update_leaf(x,r,p/TRACEGPU_LOD_LEAF_RECORDS);x->indexed_total+=n;budget--;
    }
    if(!x->rebuild_remaining&&x->indexed_total==r->total){x->snapshot=s;x->snapshot_valid=1U;}
    return tracegpu_lod_ready(x,r,capture,generation);
}
static unsigned column(uint64_t tick,uint64_t start,uint32_t window,unsigned columns)
{
    uint64_t t=(tick-start)*(columns-1U)/(window?window:1U);
    return t>=columns?columns-1U:(unsigned)t;
}
uint32_t tracegpu_lod_next(tracegpu_lod_index *x,const tracegpu_vector_ring *r,
                          uint32_t capture,uint32_t generation,
                          uint32_t first,uint32_t upper,uint64_t start,
                          uint32_t window,unsigned columns,int allowed,
                          tracegpu_lod_span *out)
{
    uint32_t p,available,node,span;
    if(!out)return 0U;
    out->records=0U;out->summarized=0U;
    if(!r||!r->records||!r->capacity||r->head>=r->capacity||r->count>r->capacity||first>=r->count||first>=upper)return 0U;
    if(upper>r->count)upper=r->count;
    p=(oldest_slot(r)+first)%r->capacity;available=upper-first;
    if(available>r->capacity-p)available=r->capacity-p;
    if(allowed&&columns&&p%TRACEGPU_LOD_LEAF_RECORDS==0U&&
       available>=TRACEGPU_LOD_LEAF_RECORDS&&tracegpu_lod_ready(x,r,capture,generation)){
        node=x->tree_leaves+p/TRACEGPU_LOD_LEAF_RECORDS;span=TRACEGPU_LOD_LEAF_RECORDS;
        while(node>1U&&!(node&1U)&&span<=available/2U){node/=2U;span*=2U;}
        while(span>=TRACEGPU_LOD_LEAF_RECORDS){
            const tracegpu_lod_summary *s=&x->nodes[node];
            if(s->epoch==x->epoch&&s->present&&s->eligible&&s->first_tick>=start&&
               column(s->first_tick,start,window,columns)==column(s->last_tick,start,window,columns)){
                out->summary=*s;out->records=span;out->summarized=1U;
                x->stats.summary_spans++;x->stats.summary_records+=span;return span;
            }
            if(span==TRACEGPU_LOD_LEAF_RECORDS)break;
            node*=2U;span/=2U;
        }
    }
    out->event=tracegpu_vector_unpack(&r->records[p]);out->records=1U;
    if(x)x->stats.raw_spans++;
    return 1U;
}
