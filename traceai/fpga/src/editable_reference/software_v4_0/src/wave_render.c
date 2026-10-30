#include "wave_render.h"
#include <string.h>
#include "ui_channel_panel.h"
#include "wave_lod.h"
#include "ui_ai_notice.h"
#include "tracegpu_time.h"

/* Not in the low application arena, capture ring, DMA or video buffers. The
 * linker reserves a dedicated NOLOAD DDR region and service clears it in
 * bounded slices before any tree node is read. */
tracegpu_lod_summary wave_lod_storage[TRACEGPU_LOD_NODE_COUNT]
    __attribute__((section(".wave_lod"),aligned(64)));
_Static_assert(sizeof(wave_lod_storage)==0xE0000U,"LOD DDR reservation");
static tracegpu_lod_index wave_lod_index;
static tracegpu_wave_lod_metrics wave_lod_metrics;
static const tracegpu_vector_ring *lod_serviced_ring;
static uint32_t lod_serviced_capture,lod_serviced_generation;
static int lod_initialized,lod_enabled=1;

/* Single foreground producer. These scratch arrays are not put on the BSP stack. */
static int32_t difference[2][TRACEGPU_WAVE_COLUMNS+1U];
static uint8_t vertical[TRACEGPU_WAVE_COLUMNS];
static uint8_t mask[TRACEGPU_WAVE_COLUMNS];
static int32_t vector_difference[TRACEGPU_LOGIC_CHANNELS][2][TRACEGPU_WAVE_COLUMNS+1U];
static uint8_t vector_vertical[TRACEGPU_LOGIC_CHANNELS][TRACEGPU_WAVE_COLUMNS];
typedef struct {
 const tracegpu_vector_ring *owner;const tracegpu_vector_record *records;
 uint64_t end,boundary,total,evicted;uint32_t head,count,capacity,window;
 uint32_t abi;
 uint8_t visible,mask,quad,dual,mode,spi,known,levels,layout,selected,channels,ui_channels,capture_mask;
} wave_cache_key;
static tracegpu_wave_plan wave_cache;
static wave_cache_key wave_key;
static uint32_t cache_capture,cache_generation,cache_hits,cache_misses;
static int cache_enabled,cache_valid;
static uint32_t slice_generation;
void tracegpu_wave_cache_enable(int enabled,uint32_t id,uint32_t generation){
 if(!enabled||id!=cache_capture||generation!=cache_generation)cache_valid=0;
 cache_enabled=enabled;cache_capture=id;cache_generation=generation;
}
uint32_t tracegpu_wave_cache_hits(void){return cache_hits;}
uint32_t tracegpu_wave_cache_misses(void){return cache_misses;}
void tracegpu_wave_lod_enable(int enabled){lod_enabled=enabled!=0;}
int tracegpu_wave_lod_enabled(void){return lod_enabled;}
void tracegpu_wave_lod_reset(void){
 slice_generation++;
 if(lod_initialized)tracegpu_lod_reset(&wave_lod_index);
 cache_valid=0;wave_lod_metrics.last_gate=TG_WAVE_LOD_COLD;
}
int tracegpu_wave_lod_service(const tracegpu_vector_ring *r,uint32_t capture,uint32_t generation,unsigned budget){
 if(!lod_initialized){tracegpu_lod_init(&wave_lod_index,wave_lod_storage,TRACEGPU_LOD_NODE_COUNT);lod_initialized=1;}
 lod_serviced_ring=r;lod_serviced_capture=capture;lod_serviced_generation=generation;
 if(!lod_enabled)return 0;
 return tracegpu_lod_service(&wave_lod_index,r,capture,generation,budget);
}
tracegpu_wave_lod_metrics tracegpu_wave_lod_get_metrics(void){
 tracegpu_wave_lod_metrics m=wave_lod_metrics;
 m.enabled=(uint32_t)lod_enabled;
 m.ready=lod_initialized&&tracegpu_lod_ready(&wave_lod_index,lod_serviced_ring,lod_serviced_capture,lod_serviced_generation);
 m.service_leaves=wave_lod_index.stats.leaf_updates;m.service_records=wave_lod_index.stats.records_read;
 m.init_nodes=wave_lod_index.stats.nodes_cleared;m.rebuilds=wave_lod_index.stats.rebuilds;
 return m;
}

static uint32_t lower_bound(const tracegpu_uart_event_ring *ring,uint64_t timestamp)
{
    uint32_t lo=0U,hi=ring->count;
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2U;
        if(tracegpu_uart_event_timestamp(tracegpu_uart_ring_oldest(ring,mid))<timestamp)
            lo=mid+1U;else hi=mid;
    }
    return lo;
}
static uint32_t upper_bound(const tracegpu_uart_event_ring *ring,uint64_t timestamp)
{
    uint32_t lo=0U,hi=ring->count;
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2U;
        if(tracegpu_uart_event_timestamp(tracegpu_uart_ring_oldest(ring,mid))<=timestamp)
            lo=mid+1U;else hi=mid;
    }
    return lo;
}
static void interval(unsigned level,unsigned left,unsigned right)
{
    difference[level][left]++;difference[level][right+1U]--;
}
static void add(tracegpu_wave_plan *plan,unsigned x,unsigned y,unsigned w,unsigned h)
{
    if(plan->count>=TRACEGPU_WAVE_MAX_RECTS){plan->overflow++;return;}
    tracegpu_render_rect *r=&plan->rects[plan->count++];
    r->x=(uint16_t)x;r->y=(uint16_t)y;r->w=(uint16_t)w;r->h=(uint16_t)h;
    plan->pixels+=w*h;
}
static void wave_build_single(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,
                         const tracegpu_uart_event_ring *ring)
{
    uint64_t newest=0U,start,end,base;
    int64_t shifted;
    unsigned previous=0U,level=1U,x,first=0U,i,run;
    int known=!ui->spi_mode;
    int32_t high=0,low=0;
    memset(plan,0,sizeof(*plan));memset(difference,0,sizeof(difference));
    memset(vertical,0,sizeof(vertical));
    if(ring&&ring->count) newest=tracegpu_uart_event_timestamp(tracegpu_uart_ring_newest(ring,0U));
    base=ui->follow?newest:ui->view_anchor_ticks;
    shifted=(int64_t)base+ui->view_offset_ticks;end=shifted>0?(uint64_t)shifted:0U;
    start=end>ui->time_window_ticks?end-ui->time_window_ticks:0U;
    if(ring&&ring->count){
        if(ring->boundary_known)level=ring->boundary_level;
        else if(ring->init_seen)level=(ring->init_vector>>(ui->selected_channel&3U))&1U;
        if(ui->spi_mode&&(ring->boundary_known||ring->init_seen)){
            uint64_t valid_from=ring->boundary_known?ring->boundary_tick:ring->init_tick;
            if(valid_from>end)return;
            known=1;
            if(valid_from>start)previous=(unsigned)((valid_from-start)*(TRACEGPU_WAVE_COLUMNS-1U)/(ui->time_window_ticks?ui->time_window_ticks:1U));
        }
        first=lower_bound(ring,start);
        for(i=first;i>0U;i--){const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(ring,i-1U);
            if(ring->vector&&tracegpu_uart_event_type(e)==TG_VEC_GAP){known=0;break;}
            if(tracegpu_uart_ring_event_state(ring,e,&level)){known=1;break;}}
        for(i=first;i<ring->count;i++){
            const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(ring,i);
            uint64_t t=tracegpu_uart_event_timestamp(e);
            if(t>end)break;
            if(ring->vector){
                unsigned next;
                x=(unsigned)(((t-start)*(TRACEGPU_WAVE_COLUMNS-1U))/(ui->time_window_ticks?ui->time_window_ticks:1U));
                if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1U;
                if(tracegpu_uart_event_type(e)==TG_VEC_GAP){
                    if(known&&x>previous)interval(level,previous,x-1U);
                    known=0;previous=x;continue;
                }
                if(!tracegpu_uart_ring_event_state(ring,e,&next))continue;
                if(tracegpu_uart_ring_is_edge(ring,i)){
                    if(known){interval(level,previous,x);vertical[x]=1U;}
                    plan->edges++;
                }else if(known&&x>previous)interval(level,previous,x-1U);
                level=next;previous=x;known=1;continue;
            }
            if(tracegpu_uart_event_type(e)==TRACEGPU_RECORD_SESSION_INIT){
                unsigned initial;
                if(tracegpu_uart_event_state(e,&initial)){
                    x=(unsigned)(((t-start)*(TRACEGPU_WAVE_COLUMNS-1U))/(ui->time_window_ticks?ui->time_window_ticks:1U));
                    if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1U;
                    if(known&&x>previous)interval(level,previous,x-1U);
                    previous=x;level=initial;known=1;
                }continue;
            }
            if(tracegpu_uart_event_type(e)!=TRACEGPU_RECORD_RAW_EDGE)continue;
            x=(unsigned)(((t-start)*(TRACEGPU_WAVE_COLUMNS-1U))/(ui->time_window_ticks?ui->time_window_ticks:1U));
            if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1U;
            if(known){interval(level,previous,x);vertical[x]=1U;}plan->edges++;
            previous=x;level=tracegpu_uart_event_level(e);known=1;
        }
    }
    if(known){
        unsigned right=TRACEGPU_WAVE_COLUMNS-1U;
        if(ring&&ring->vector&&newest<end){
            if(newest<start)known=0;
            else right=(unsigned)((newest-start)*right/(ui->time_window_ticks?ui->time_window_ticks:1U));
        }
        if(known&&right>=previous)interval(level,previous,right);
    }
    for(x=0U;x<TRACEGPU_WAVE_COLUMNS;x++){
        low+=difference[0][x];high+=difference[1][x];
        mask[x]=vertical[x]?4U:(uint8_t)((high?1U:0U)|(low?2U:0U));
    }
    for(x=0U;x<TRACEGPU_WAVE_COLUMNS;x=run){
        for(run=x+1U;run<TRACEGPU_WAVE_COLUMNS&&mask[run]==mask[x];run++){}
        if(mask[x]&4U)add(plan,TRACEGPU_WAVE_LEFT+x,TRACEGPU_WAVE_HIGH,run-x,TRACEGPU_WAVE_LOW-TRACEGPU_WAVE_HIGH+1U);
        else{
            if(mask[x]&1U)add(plan,TRACEGPU_WAVE_LEFT+x,TRACEGPU_WAVE_HIGH,run-x,1U);
            if(mask[x]&2U)add(plan,TRACEGPU_WAVE_LEFT+x,TRACEGPU_WAVE_LOW,run-x,1U);
        }
    }
}
static void vector_interval(unsigned ch,unsigned level,unsigned left,unsigned right){
    vector_difference[ch][level][left]++;vector_difference[ch][level][right+1]--;
}
static void wave_build_vector(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,const tracegpu_uart_event_ring *ring){
    uint64_t end,start,last;unsigned first,upper,i,ch,x,run,levels=0,known=0,previous[TRACEGPU_LOGIC_CHANNELS]={0};
    int use_lod=0;
    unsigned captured=ring->vector->capture_mask;
    memset(plan,0,sizeof *plan);plan->dual=1;
    memset(vector_difference,0,sizeof vector_difference);memset(vector_vertical,0,sizeof vector_vertical);
    wave_lod_metrics.visible_records=0;wave_lod_metrics.last_gate=TG_WAVE_LOD_UNSUPPORTED;
    if(!ring->count)return;
    last=tracegpu_uart_event_timestamp(tracegpu_uart_ring_newest(ring,0));
    end=tracegpu_cursor_view_end(ui->follow?last:ui->view_anchor_ticks,ui->view_offset_ticks);
    start=end>ui->time_window_ticks?end-ui->time_window_ticks:0;
    if(last<start)return;
    first=lower_bound(ring,start);
    upper=upper_bound(ring,end);
    wave_lod_metrics.visible_records=upper-first;
    if(!lod_enabled)wave_lod_metrics.last_gate=TG_WAVE_LOD_OFF;
    else if(ring->count!=ring->vector->count||!ring->vector->records||!ring->vector->capacity||
            ring->vector->capacity>TRACEGPU_LOD_MAX_LEAVES*TRACEGPU_LOD_LEAF_RECORDS||
            ring->vector->head>=ring->vector->capacity||ring->vector->count>ring->vector->capacity||
            ring->vector->abi!=TRACEGPU_VECTOR_VERSION||
            ring->vector->channel_count!=TRACEGPU_VECTOR_CHANNELS)wave_lod_metrics.last_gate=TG_WAVE_LOD_UNSUPPORTED;
    else if(!lod_initialized||!tracegpu_lod_ready(&wave_lod_index,ring->vector,cache_capture,cache_generation))wave_lod_metrics.last_gate=TG_WAVE_LOD_COLD;
    /* Host experiments showed a 16k-record full view was slower. Gate by the
     * records actually visible, never by the total retained history size. */
    else if(upper-first<48U*TRACEGPU_WAVE_COLUMNS)wave_lod_metrics.last_gate=TG_WAVE_LOD_SPARSE;
    else{use_lod=1;wave_lod_metrics.last_gate=TG_WAVE_LOD_ACTIVE;}
    if(use_lod)wave_lod_metrics.lod_plans++;else wave_lod_metrics.raw_plans++;
    if(first){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_uart_ring_oldest(ring,first-1));known=e.type!=TG_VEC_GAP?(e.flags&captured):0;levels=e.levels;}
    else if(ring->vector->boundary_known&&ring->vector->boundary_tick<=start){known=captured;levels=ring->vector->boundary_levels;}
    /* Reconstruct all eight lanes in one chronological pass. No duplicated
     * raw history and no eight-way rescanning of the visible window. */
    for(i=first;i<upper;){
        tracegpu_vector_event e;
        unsigned changed;
        if(use_lod){
            tracegpu_lod_span span;
            unsigned consumed=tracegpu_lod_next(&wave_lod_index,ring->vector,cache_capture,cache_generation,
                i,upper,start,ui->time_window_ticks,TRACEGPU_WAVE_COLUMNS,known==captured,&span);
            if(span.summarized&&consumed){
                const tracegpu_lod_summary *s=&span.summary;
                x=(unsigned)((s->first_tick-start)*(TRACEGPU_WAVE_COLUMNS-1)/(ui->time_window_ticks?ui->time_window_ticks:1));
                if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1;
                for(ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){
                    unsigned edges=s->edges[ch]+(((levels^s->first_levels)>>ch)&1U);
                    if((captured&(1U<<ch))&&edges){
                        vector_interval(ch,(levels>>ch)&1U,previous[ch],x);
                        vector_vertical[ch][x]=1;previous[ch]=x;plan->edges+=edges;
                    }
                }
                levels=s->last_levels;i+=consumed;wave_lod_metrics.summary_spans++;
                wave_lod_metrics.summary_records+=consumed;continue;
            }
            /* Invalid query/state is always recoverable by the original raw
             * iterator. A summary is never trusted without exact readiness. */
            e=consumed?span.event:tracegpu_vector_unpack(tracegpu_uart_ring_oldest(ring,i));
        }else e=tracegpu_vector_unpack(tracegpu_uart_ring_oldest(ring,i));
        i++;wave_lod_metrics.raw_records++;
        x=(unsigned)((e.tick-start)*(TRACEGPU_WAVE_COLUMNS-1)/(ui->time_window_ticks?ui->time_window_ticks:1));
        if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1;
        if(e.type==TG_VEC_GAP){
            for(ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){if((known&(1U<<ch))&&x>previous[ch])vector_interval(ch,(levels>>ch)&1U,previous[ch],x-1);previous[ch]=x;}
            known=0;continue;
        }
        changed=(levels^e.levels)&captured;
        for(ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){
            unsigned bit=1U<<ch;
            if(!(captured&bit))continue;
            if(e.type==TG_VEC_CHANGE&&(known&bit)&&(changed&bit)){
                vector_interval(ch,(levels>>ch)&1U,previous[ch],x);vector_vertical[ch][x]=1;previous[ch]=x;plan->edges++;
            }else if(!(known&bit)||e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){
                if((known&bit)&&x>previous[ch])vector_interval(ch,(levels>>ch)&1U,previous[ch],x-1);
                previous[ch]=x;
            }
        }
        levels=e.levels;known=e.flags&captured;
    }
    unsigned right=last<end?(unsigned)((last-start)*(TRACEGPU_WAVE_COLUMNS-1)/(ui->time_window_ticks?ui->time_window_ticks:1)):TRACEGPU_WAVE_COLUMNS-1;
    for(ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++)if((known&(1U<<ch))&&right>=previous[ch])vector_interval(ch,(levels>>ch)&1U,previous[ch],right);
    for(unsigned lane=0;lane<tracegpu_ui_lanes(ui);lane++){
        unsigned actual=tracegpu_ui_lane_channel(ui,lane),high=tracegpu_ui_lane_high(ui,lane),height=tracegpu_ui_lane_height(ui);
        int32_t low=0,hi=0;
        if(actual>=TRACEGPU_LOGIC_CHANNELS||!(captured&(1U<<actual))||(ui->logic_mode&&ui->logic_visible&&!(ui->logic_visible&(1U<<actual))))continue;
        for(x=0;x<TRACEGPU_WAVE_COLUMNS;x++){
            low+=vector_difference[actual][0][x];hi+=vector_difference[actual][1][x];
            mask[x]=vector_vertical[actual][x]?4U:(uint8_t)((hi?1:0)|(low?2:0));
        }
        for(x=0;x<TRACEGPU_WAVE_COLUMNS;x=run){
            for(run=x+1;run<TRACEGPU_WAVE_COLUMNS&&mask[run]==mask[x];run++){}
            unsigned before=plan->count;
            if(mask[x]&4U)add(plan,TRACEGPU_WAVE_LEFT+x,high,run-x,height+1);
            else{
                if(mask[x]&1U)add(plan,TRACEGPU_WAVE_LEFT+x,high,run-x,1);
                if(mask[x]&2U)add(plan,TRACEGPU_WAVE_LEFT+x,high+height,run-x,1);
            }
            while(before<plan->count)plan->colors[before++]=tracegpu_ui_channel_color(actual);
        }
    }
}
static void wave_build_uncached(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,
                         const tracegpu_uart_event_ring *ring)
{
    static tracegpu_wave_plan lane;
    tracegpu_ui_state view=*ui;
    unsigned channel,i;
    if((ui->spi_mode||ui->logic_mode)&&ring&&ring->vector){wave_build_vector(plan,ui,ring);return;}
    if(!ui->dual_channel){wave_build_single(plan,ui,ring);return;}
    memset(plan,0,sizeof(*plan));plan->dual=1;
    /* Both lanes share the same anchor, even if one channel is silent. */
    view.follow=0;
    for(channel=0;channel<tracegpu_ui_lanes(ui);channel++){
        unsigned actual=tracegpu_ui_lane_channel(ui,channel);
        unsigned height=tracegpu_ui_lane_height(ui);
        if(actual>=TRACEGPU_UART_CHANNELS||(ui->quad_channel&&!(ui->channel_enabled_mask&(1U<<actual))))continue;
        view.selected_channel=(uint8_t)actual;
        wave_build_single(&lane,&view,ui->channel_ring[actual]);
        plan->edges+=lane.edges;
        for(i=0;i<lane.count&&plan->count<TRACEGPU_WAVE_MAX_RECTS;i++){
            tracegpu_render_rect r=lane.rects[i];
            r.y=(uint16_t)(tracegpu_ui_lane_high(ui,channel)+(r.y-TRACEGPU_WAVE_HIGH)*height/85U);
            r.h=(uint16_t)(r.h>1U?height+1U:1U);
            plan->colors[plan->count]=tracegpu_ui_channel_color(actual);
            plan->rects[plan->count++]=r;plan->pixels+=(uint32_t)r.w*r.h;
        }
        plan->overflow+=lane.overflow;
        if(i<lane.count)plan->overflow+=lane.count-i;
    }
}
static void wave_build_base(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,const tracegpu_uart_event_ring *ring){
 wave_cache_key key;
 wave_lod_metrics.last_gate=TG_WAVE_LOD_UNSUPPORTED;wave_lod_metrics.visible_records=0U;
 if(ui->logic_mode&&ui->bus_mode){memset(plan,0,sizeof *plan);plan->dual=1;return;}
 if(!cache_enabled||!ring||!ring->vector||!(ui->logic_mode||ui->spi_mode)){wave_build_uncached(plan,ui,ring);return;}
 memset(&key,0,sizeof key);key.owner=ring->vector;key.records=ring->vector->records;
 key.end=tracegpu_cursor_view_end(ui->follow&&ring->count?tracegpu_uart_event_timestamp(tracegpu_uart_ring_newest(ring,0)):ui->view_anchor_ticks,ui->view_offset_ticks);
 key.boundary=ring->vector->boundary_tick;key.total=ring->vector->total;key.evicted=ring->vector->evicted;key.head=ring->vector->head;
 key.count=ring->count;key.capacity=ring->vector->capacity;key.window=ui->time_window_ticks;
 key.visible=ui->logic_visible;key.mask=ui->channel_enabled_mask;key.quad=ui->quad_channel;key.dual=ui->dual_channel;key.mode=ui->logic_mode;key.spi=ui->spi_mode;
 key.known=ring->vector->boundary_known;key.levels=ring->vector->boundary_levels;
 key.layout=ui->view_mode;key.selected=ui->selected_channel;key.channels=ring->vector->channel_count;key.ui_channels=ui->logic_channel_count;
 key.capture_mask=ring->vector->capture_mask;key.abi=ring->vector->abi;
 if(cache_valid&&!memcmp(&key,&wave_key,sizeof key)){*plan=wave_cache;cache_hits++;wave_lod_metrics.last_gate=TG_WAVE_LOD_CACHE;return;}
 wave_build_uncached(plan,ui,ring);wave_cache=*plan;wave_key=key;cache_valid=1;cache_misses++;
}
void tracegpu_wave_build(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,const tracegpu_uart_event_ring *ring){
 wave_build_base(plan,ui,ring);tg_ai_highlight_plan(plan,ui);
}

/* Private scratch: synchronous reference/frozen paints may run between steps.
 * Only ring metadata is copied; raw acquisition storage is never pinned. */
static struct {
 tracegpu_vector_ring ring;const tracegpu_vector_ring *live;
 tracegpu_uart_event_ring view;const tracegpu_ui_state *ui;tracegpu_wave_plan *plan;
 int32_t diff[TRACEGPU_LOGIC_CHANNELS][2][TRACEGPU_WAVE_COLUMNS+1U];
 uint8_t vert[TRACEGPU_LOGIC_CHANNELS][TRACEGPU_WAVE_COLUMNS],mask[TRACEGPU_WAVE_COLUMNS];
 uint32_t thresholds[TRACEGPU_WAVE_COLUMNS],previous[TRACEGPU_LOGIC_CHANNELS];
 uint64_t start,end,last,base;uint32_t generation,index,upper,clear_at,column,phase,lane,x,run_start;
 uint32_t threshold_floor,threshold_rem,threshold_q,threshold_r,den;
 unsigned levels,known,captured,actual,high,height,use_lod,run_mask;
 int32_t low,hi;uint8_t active;
} slice;
static tracegpu_wave_slice_stats slice_stats;
tracegpu_wave_slice_stats tracegpu_wave_slice_get_stats(void){return slice_stats;}
int tracegpu_wave_slice_begin(tracegpu_wave_plan *p,const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r){
 if(!p||!u||!r||!r->vector||!r->vector->records||!r->count||
    !(u->logic_mode||u->spi_mode)||u->data_page||(u->logic_mode&&u->bus_mode))return 0;
 const tracegpu_vector_ring *v=r->vector;
 if(!v->capacity||v->head>=v->capacity||v->count>v->capacity||v->total<v->count||r->count!=v->count||
    v->abi!=TRACEGPU_VECTOR_VERSION||v->channel_count!=TRACEGPU_VECTOR_CHANNELS)return 0;
 slice.ring=*v;slice.live=v;slice.view=*r;slice.view.vector=&slice.ring;
 slice.ui=u;slice.plan=p;slice.base=v->total-v->count;slice.generation=slice_generation;
 slice.last=tracegpu_uart_event_timestamp(tracegpu_uart_ring_newest(&slice.view,0));
 slice.end=tracegpu_cursor_view_end(u->follow?slice.last:u->view_anchor_ticks,u->view_offset_ticks);
 slice.start=slice.end>u->time_window_ticks?slice.end-u->time_window_ticks:0;
 slice.index=lower_bound(&slice.view,slice.start);slice.upper=upper_bound(&slice.view,slice.end);
 slice.levels=slice.known=0;slice.captured=v->capture_mask;
 if(slice.index){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_uart_ring_oldest(&slice.view,slice.index-1));
  slice.levels=e.levels;slice.known=e.type!=TG_VEC_GAP?(e.flags&slice.captured):0;
 }else if(v->boundary_known&&v->boundary_tick<=slice.start){slice.known=slice.captured;slice.levels=v->boundary_levels;}
 memset(slice.previous,0,sizeof slice.previous);
 slice.column=slice.clear_at=slice.phase=slice.lane=slice.x=0;
 slice.den=u->time_window_ticks?u->time_window_ticks:1;
 slice.threshold_q=slice.den/(TRACEGPU_WAVE_COLUMNS-1U);slice.threshold_r=slice.den%(TRACEGPU_WAVE_COLUMNS-1U);
 slice.threshold_floor=slice.threshold_rem=0;slice.thresholds[0]=0;
 int lod_ready=lod_initialized&&tracegpu_lod_ready(&wave_lod_index,v,cache_capture,cache_generation);
 slice.use_lod=lod_enabled&&lod_ready&&slice.upper-slice.index>=48U*TRACEGPU_WAVE_COLUMNS;
 wave_lod_metrics.visible_records=slice.upper-slice.index;
 wave_lod_metrics.last_gate=!lod_enabled?TG_WAVE_LOD_OFF:!lod_ready?TG_WAVE_LOD_COLD:
  slice.use_lod?TG_WAVE_LOD_ACTIVE:TG_WAVE_LOD_SPARSE;
 if(slice.use_lod)wave_lod_metrics.lod_plans++;else wave_lod_metrics.raw_plans++;
 p->count=p->edges=p->pixels=p->overflow=0;p->dual=1;
 if(slice.last<slice.start)slice.index=slice.upper;
 slice.active=1;slice_stats.started++;return 1;
}
static unsigned slice_column(uint64_t tick){
 uint64_t t=tick-slice.start;
 while(slice.column+1U<TRACEGPU_WAVE_COLUMNS&&t>=slice.thresholds[slice.column+1U])slice.column++;
 return slice.column;
}
static void slice_interval(unsigned ch,unsigned level,unsigned a,unsigned b){
 slice.diff[ch][level][a]++;slice.diff[ch][level][b+1U]--;
}
static void slice_emit(tracegpu_wave_plan *p,unsigned end){
 unsigned x=slice.run_start,n=p->count;
 if(slice.run_mask&4U)add(p,TRACEGPU_WAVE_LEFT+x,slice.high,end-x,slice.height+1U);
 else{
  if(slice.run_mask&1U)add(p,TRACEGPU_WAVE_LEFT+x,slice.high,end-x,1);
  if(slice.run_mask&2U)add(p,TRACEGPU_WAVE_LEFT+x,slice.high+slice.height,end-x,1);
 }
 while(n<p->count)p->colors[n++]=tracegpu_ui_channel_color(slice.actual);
}
int tracegpu_wave_slice_step(tracegpu_wave_plan *p,unsigned budget){
 if(!slice.active||slice.plan!=p)return -1;
 const tracegpu_vector_ring *v=slice.live;
 if(slice.generation!=slice_generation||v->records!=slice.ring.records||v->capacity!=slice.ring.capacity||
    v->capture_mask!=slice.ring.capture_mask||v->abi!=slice.ring.abi||v->channel_count!=slice.ring.channel_count||
    v->total<slice.ring.total||v->count>v->total||
    (slice.index<slice.upper&&v->total-v->count>slice.base+slice.index)){
  slice.active=0;slice_stats.cancelled++;p->count=p->pixels=0;return -1;
 }
 XTime begin,now;XTime_GetTime(&begin);unsigned records=0,units=0;
 const XTime limit=COUNTS_PER_SECOND/8000U?COUNTS_PER_SECOND/8000U:1U;
 slice_stats.calls++;
 while(budget--){
  if(units && (units&7U)==0){XTime_GetTime(&now);if(now-begin>=limit)break;}
  units++;
  if(slice.phase==0){
   unsigned total=sizeof slice.diff/sizeof(int32_t),at=slice.clear_at;
   unsigned n=total-at;if(n>128U)n=128U;
   memset((int32_t*)slice.diff+at,0,n*sizeof(int32_t));slice.clear_at+=n;
   if(slice.clear_at==total){slice.clear_at=0;slice.phase=1;}
  }else if(slice.phase==1){
   unsigned n=sizeof slice.vert-slice.clear_at;if(n>128U)n=128U;
   memset((uint8_t*)slice.vert+slice.clear_at,0,n);slice.clear_at+=n;
   if(slice.clear_at==sizeof slice.vert){slice.x=1;slice.phase=2;}
  }else if(slice.phase==2){
   slice.threshold_floor+=slice.threshold_q;slice.threshold_rem+=slice.threshold_r;
   if(slice.threshold_rem>=TRACEGPU_WAVE_COLUMNS-1U){slice.threshold_floor++;slice.threshold_rem-=TRACEGPU_WAVE_COLUMNS-1U;}
   slice.thresholds[slice.x++]=slice.threshold_floor+(slice.threshold_rem!=0);
   if(slice.x==TRACEGPU_WAVE_COLUMNS)slice.phase=3;
  }else if(slice.phase==3){
   if(slice.index==slice.upper){
    unsigned right=slice.last<slice.start?0:slice.last<slice.end?slice_column(slice.last):TRACEGPU_WAVE_COLUMNS-1U;
    if(slice.last>=slice.start)for(unsigned ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++)
     if((slice.known&(1U<<ch))&&right>=slice.previous[ch])slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],right);
    slice.phase=4;slice.lane=0;continue;
   }
   tracegpu_vector_event e;
   if(slice.use_lod&&slice.live->total!=slice.ring.total)slice.use_lod=0;
   if(slice.use_lod){
    tracegpu_lod_span span;unsigned n=tracegpu_lod_next(&wave_lod_index,slice.live,cache_capture,cache_generation,
     slice.index,slice.upper,slice.start,slice.den,TRACEGPU_WAVE_COLUMNS,slice.known==slice.captured,&span);
    if(span.summarized&&n){
     const tracegpu_lod_summary *s=&span.summary;unsigned x=slice_column(s->first_tick);
     for(unsigned ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){
      unsigned edges=s->edges[ch]+(((slice.levels^s->first_levels)>>ch)&1U);
      if((slice.captured&(1U<<ch))&&edges){slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],x);
       slice.vert[ch][x]=1;slice.previous[ch]=x;p->edges+=edges;}
     }
     slice.levels=s->last_levels;slice.index+=n;wave_lod_metrics.summary_spans++;wave_lod_metrics.summary_records+=n;continue;
    }
    e=n?span.event:tracegpu_vector_unpack(tracegpu_uart_ring_oldest(&slice.view,slice.index));
   }else e=tracegpu_vector_unpack(tracegpu_uart_ring_oldest(&slice.view,slice.index));
   slice.index++;records++;wave_lod_metrics.raw_records++;
   unsigned x=slice_column(e.tick);
   if(e.type==TG_VEC_GAP){
    for(unsigned ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){
     if((slice.known&(1U<<ch))&&x>slice.previous[ch])slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],x-1U);
     slice.previous[ch]=x;
    }slice.known=0;continue;
   }
   unsigned changed=(slice.levels^e.levels)&slice.captured;
   if(e.type==TG_VEC_CHANGE&&slice.known==slice.captured){
    unsigned bits=changed;
    while(bits){unsigned ch=(unsigned)__builtin_ctz(bits);bits&=bits-1U;
     slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],x);slice.vert[ch][x]=1;slice.previous[ch]=x;p->edges++;}
   }else for(unsigned ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++){
    unsigned bit=1U<<ch;if(!(slice.captured&bit))continue;
    if(e.type==TG_VEC_CHANGE&&(slice.known&bit)&&(changed&bit)){
     slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],x);slice.vert[ch][x]=1;slice.previous[ch]=x;p->edges++;
    }else if(!(slice.known&bit)||e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){
     if((slice.known&bit)&&x>slice.previous[ch])slice_interval(ch,(slice.levels>>ch)&1U,slice.previous[ch],x-1U);
     slice.previous[ch]=x;
    }
   }
   slice.levels=e.levels;slice.known=e.flags&slice.captured;
  }else if(slice.phase==4){
   if(slice.lane==tracegpu_ui_lanes(slice.ui)){
    if(tg_ai_highlight_slice_begin(p,slice.ui)){slice.phase=7;continue;}
    slice.active=0;slice_stats.completed++;break;
   }
   slice.actual=tracegpu_ui_lane_channel(slice.ui,slice.lane);slice.high=tracegpu_ui_lane_high(slice.ui,slice.lane);
   slice.height=tracegpu_ui_lane_height(slice.ui);slice.lane++;
   if(slice.actual>=TRACEGPU_LOGIC_CHANNELS||!(slice.captured&(1U<<slice.actual))||
      (slice.ui->logic_mode&&slice.ui->logic_visible&&!(slice.ui->logic_visible&(1U<<slice.actual))))continue;
   slice.x=0;slice.low=slice.hi=0;slice.phase=5;
  }else if(slice.phase==5){
   unsigned x=slice.x++,a=slice.actual;
   slice.low+=slice.diff[a][0][x];slice.hi+=slice.diff[a][1][x];
   slice.mask[x]=slice.vert[a][x]?4U:(uint8_t)((slice.hi?1U:0U)|(slice.low?2U:0U));
   if(slice.x==TRACEGPU_WAVE_COLUMNS){slice.x=1;slice.run_start=0;slice.run_mask=slice.mask[0];slice.phase=6;}
  }else if(slice.phase==7){
   if(tg_ai_highlight_slice_step(p,8U)){slice.active=0;slice_stats.completed++;break;}
  }else{
   if(slice.x==TRACEGPU_WAVE_COLUMNS){slice_emit(p,slice.x);slice.phase=4;continue;}
   if(slice.mask[slice.x]!=slice.run_mask){slice_emit(p,slice.x);slice.run_start=slice.x;slice.run_mask=slice.mask[slice.x];}
   slice.x++;
  }
 }
 XTime_GetTime(&now);uint32_t elapsed=(uint32_t)(now-begin);
 if(elapsed>slice_stats.max_ticks)slice_stats.max_ticks=elapsed;
 if(records>slice_stats.max_records)slice_stats.max_records=records;
 return slice.active?0:1;
}
int tracegpu_render_rect_valid(const tracegpu_render_rect *r)
{
    return r&&r->w&&r->h&&r->x<TRACEGPU_LCD_WIDTH&&r->y<TRACEGPU_LCD_HEIGHT&&
        r->w<=TRACEGPU_LCD_WIDTH-r->x&&r->h<=TRACEGPU_LCD_HEIGHT-r->y;
}
#include "render_damage.h"
static const tracegpu_wave_plan *range_plan;
static unsigned range_first,range_end;
void tracegpu_wave_cpu_range(const tracegpu_wave_plan *p,unsigned first,unsigned end){range_plan=p;range_first=first;range_end=end;}
void tracegpu_wave_cpu(uintptr_t canvas,const tracegpu_wave_plan *plan)
{
    unsigned i,y,x,first=0,end=plan->count;
    if(range_plan==plan){first=range_first;end=range_end;if(end>plan->count)end=plan->count;}
    for(i=first;i<end;i++){
        const tracegpu_render_rect *r=&plan->rects[i];
        if(!tracegpu_render_rect_valid(r))continue;
        int cx=r->x,cy=r->y,cw=r->w,ch=r->h;
        if(!tracegpu_clip_rect(canvas,&cx,&cy,&cw,&ch))continue;
        for(y=0U;y<(unsigned)ch;y++){
            uint32_t *dst=(uint32_t*)canvas+((unsigned)cy+y)*TRACEGPU_LCD_WIDTH+(unsigned)cx;
            for(x=0U;x<(unsigned)cw;x++)dst[x]=plan->colors[i]?plan->colors[i]:((plan->dual&&r->y>=166U)?0xFFFFB448U:TRACEGPU_WAVE_COLOR);
        }
    }
}
