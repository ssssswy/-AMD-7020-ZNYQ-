#include "wave_lod.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static tracegpu_vector_record records[2048];
static tracegpu_vector_stream stream;
static struct {
    uint64_t before;
    tracegpu_lod_summary nodes[256];
    uint64_t after;
} workspace;
static tracegpu_lod_index index_state;
static uint64_t checks,summary_checks,raw_checks,budget_calls;
static uint32_t capture=7U,generation=11U;

static void put(unsigned type,uint64_t tick,unsigned levels)
{
    tracegpu_vector_event e={tick,stream.expected_sequence,(uint8_t)type,
        (uint8_t)(type==TG_VEC_GAP?1U:stream.history.capture_mask),(uint8_t)levels};
    tracegpu_vector_record r=tracegpu_vector_pack(e);
    assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);
}
static void begin(unsigned capacity,unsigned mask)
{
    tracegpu_vector_stream_init(&stream,records,capacity,0,0);
    assert(!tracegpu_vector_stream_configure(&stream,mask));
    put(TG_VEC_INIT,0U,0U);
}
static void service(unsigned budget)
{
    uint64_t leaves=index_state.stats.leaf_updates,read=index_state.stats.records_read;
    uint64_t cleared=index_state.stats.nodes_cleared;
    unsigned capped=budget>TRACEGPU_LOD_MAX_SERVICE_LEAVES?TRACEGPU_LOD_MAX_SERVICE_LEAVES:budget;
    (void)tracegpu_lod_service(&index_state,&stream.history,capture,generation,budget);
    assert(index_state.stats.leaf_updates-leaves<=capped);
    assert(index_state.stats.records_read-read<=TRACEGPU_LOD_LEAF_RECORDS*capped);
    assert(index_state.stats.nodes_cleared-cleared<=2U*capped);
    assert(!(index_state.stats.leaf_updates-leaves)||!(index_state.stats.nodes_cleared-cleared));
    assert(workspace.before==UINT64_C(0x123456789ABCDEF0));
    assert(workspace.after==UINT64_C(0x0FEDCBA987654321));
    budget_calls++;
}
static void finish(void)
{
    unsigned attempts=0U;
    while(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation)){
        service(1U);assert(++attempts<10000U);
    }
}
static unsigned column(uint64_t tick,uint64_t start,uint32_t window,unsigned columns)
{
    uint64_t x=(tick-start)*(columns-1U)/(window?window:1U);
    return x>=columns?columns-1U:(unsigned)x;
}
static void check_range(unsigned first,unsigned upper,uint64_t start,uint32_t window,
                        unsigned columns,int allow)
{
    for(unsigned i=first;i<upper;){
        tracegpu_lod_span span;
        unsigned n=tracegpu_lod_next(&index_state,&stream.history,capture,generation,
            i,upper,start,window,columns,allow,&span);
        assert(n&&n<=upper-i&&span.records==n);
        if(span.summarized){
            tracegpu_lod_summary expected={0};
            assert(allow&&columns&&n>=TRACEGPU_LOD_LEAF_RECORDS&&n%TRACEGPU_LOD_LEAF_RECORDS==0U);
            assert(tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
            for(unsigned j=0;j<n;j++){
                const tracegpu_vector_record *r=tracegpu_vector_oldest(&stream.history,i+j);
                tracegpu_vector_event e=tracegpu_vector_unpack(r);
                assert(e.type==TG_VEC_CHANGE&&e.flags==stream.history.capture_mask);
                assert(tracegpu_vector_record_valid_mask(r,stream.history.capture_mask));
                if(!j){expected.first_tick=e.tick;expected.first_levels=e.levels;}
                else{
                    assert(e.tick>expected.last_tick);
                    for(unsigned ch=0;ch<8U;ch++)expected.edges[ch]+=((expected.last_levels^e.levels)>>ch)&1U;
                }
                expected.last_tick=e.tick;expected.last_levels=e.levels;
            }
            assert(expected.first_tick>=start);
            assert(column(expected.first_tick,start,window,columns)==column(expected.last_tick,start,window,columns));
            assert(expected.first_tick==span.summary.first_tick&&expected.last_tick==span.summary.last_tick);
            assert(expected.first_levels==span.summary.first_levels&&expected.last_levels==span.summary.last_levels);
            assert(!memcmp(expected.edges,span.summary.edges,sizeof expected.edges));
            summary_checks++;
        }else{
            tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,i));
            assert(n==1U);
            assert(e.tick==span.event.tick&&e.sequence==span.event.sequence&&e.type==span.event.type&&
                   e.flags==span.event.flags&&e.levels==span.event.levels);
            raw_checks++;
        }
        i+=n;checks++;
    }
}
static void check_views(void)
{
    uint64_t first=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,0)).tick;
    uint64_t last=stream.last_tick;
    unsigned count=stream.history.count;
    /* Broad same-column summaries and exact-column descent, including both
     * oldest/head partial leaves and wrapped physical storage. */
    check_range(0U,count,0U,(uint32_t)(last+1U),2U,1);
    check_range(0U,count,0U,(uint32_t)(last+1U),758U,1);
    check_range(0U,count,0U,(uint32_t)(last+1U),758U,0);
    if(count>20U){
        unsigned lower=count/5U,upper=count-count/7U;
        uint64_t start=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,lower)).tick;
        check_range(lower,upper,start,(uint32_t)(last-start+1U),2U,1);
        check_range(lower,upper,start,(uint32_t)(last-start+1U),758U,1);
    }
    check_range(0U,count,first,(uint32_t)(last-first+1U),0U,1);
}
int main(void)
{
    assert(sizeof(tracegpu_lod_summary)==56U);
    memset(&workspace,0xA5,sizeof workspace);
    workspace.before=UINT64_C(0x123456789ABCDEF0);workspace.after=UINT64_C(0x0FEDCBA987654321);
    tracegpu_lod_init(&index_state,workspace.nodes,256U);
    begin(1024U,255U);
    for(unsigned i=1U;i<900U;i++)put(TG_VEC_CHANGE,7U*i,stream.levels^(1U+(i%255U)));
    assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
    check_views(); /* Uninitialized NOLOAD workspace cannot produce summaries. */
    service(0U);assert(index_state.stats.nodes_cleared==0U);
    for(unsigned i=0U;i<10U;i++){service(1U);assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));}
    finish();check_views();assert(summary_checks);
    /* A quiet capture does no repeated tree scan or leaf re-read. */
    {uint64_t leaves=index_state.stats.leaf_updates,read=index_state.stats.records_read;
     for(unsigned i=0;i<100U;i++)service(1U);
     assert(index_state.stats.leaf_updates==leaves&&index_state.stats.records_read==read);}
    /* Live append, head wrap and sustained one-record eviction. Every changed
     * snapshot withdraws READY before bounded maintenance catches up. */
    for(unsigned i=0;i<4200U;i++){
        put(TG_VEC_CHANGE,stream.last_tick+7U,stream.levels^(1U+((i*19U)%255U)));
        assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
        if(i%41U==0U)check_views();
        service(1U);assert(tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
        if(i%37U==0U)check_views();
    }
    /* More than a complete retained span between services requires a new,
     * budgeted epoch rebuild, never an alias of an old physical node. */
    for(unsigned i=0;i<1200U;i++)put(TG_VEC_CHANGE,stream.last_tick+7U,stream.levels^(1U+(i%255U)));
    service(1U);assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
    check_views();finish();check_views();
    /* Non-power-of-two capacity keeps the physical final partial leaf raw. */
    capture++;generation++;begin(997U,128U);
    for(unsigned i=1;i<3600U;i++){
        if(i%211U==0U)put(TG_VEC_GAP,stream.last_tick+7U,0U);
        else if(stream.needs_resync)put(TG_VEC_RESYNC,stream.last_tick+7U,stream.levels);
        else if(i%83U==0U)put(TG_VEC_WATERMARK,stream.last_tick+7U,stream.levels);
        else put(TG_VEC_CHANGE,stream.last_tick+7U,stream.levels^128U);
        if(i%32U==0U){service(1U);if(tracegpu_lod_ready(&index_state,&stream.history,capture,generation))check_views();}
    }
    if(stream.needs_resync)put(TG_VEC_RESYNC,stream.last_tick+7U,stream.levels);
    put(TG_VEC_END,stream.last_tick+7U,stream.levels);finish();check_views();
    /* Explicit session/generation, pointer and metadata changes fail closed,
     * including a reset whose storage address is reused. */
    assert(!tracegpu_lod_ready(&index_state,&stream.history,capture+1U,generation));
    assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation+1U));
    generation++;service(1U);assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));finish();
    stream.history.boundary_levels^=128U;
    assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
    service(1U);finish();check_views();
    tracegpu_lod_reset(&index_state);assert(!tracegpu_lod_ready(&index_state,&stream.history,capture,generation));
    finish();check_views();
    capture++;generation++;begin(64U,3U);
    for(unsigned i=1;i<64U;i++)put(TG_VEC_CHANGE,i*7U,stream.levels^(1U+(i%3U)));
    finish();check_views();
    /* Corrupt count/head, unsupported ABI/channel count, insufficient nodes
     * and epoch wrap must not expose stale summaries. */
    {tracegpu_vector_ring saved=stream.history;
     stream.history.head=stream.history.capacity;assert(!tracegpu_lod_service(&index_state,&stream.history,capture,generation,1U));
     stream.history=saved;finish();
     stream.history.abi++;assert(!tracegpu_lod_service(&index_state,&stream.history,capture,generation,1U));
     stream.history=saved;finish();
     stream.history.channel_count=4U;assert(!tracegpu_lod_service(&index_state,&stream.history,capture,generation,1U));
     stream.history=saved;finish();}
    index_state.epoch=UINT32_MAX;tracegpu_lod_reset(&index_state);assert(!index_state.cleared_nodes);
    finish();check_views();
    {tracegpu_lod_index small;tracegpu_lod_init(&small,workspace.nodes,1U);
     assert(!tracegpu_lod_service(&small,&stream.history,capture,generation,10000U));}
    printf("wave_lod_core PASS checks=%" PRIu64 " summary_spans=%" PRIu64 " raw_spans=%" PRIu64
        " budget_calls=%" PRIu64 " workspace_bytes=%zu leaf_records=%u\n",
        checks,summary_checks,raw_checks,budget_calls,(size_t)TRACEGPU_LOD_NODE_COUNT*sizeof(tracegpu_lod_summary),TRACEGPU_LOD_LEAF_RECORDS);
    return 0;
}
