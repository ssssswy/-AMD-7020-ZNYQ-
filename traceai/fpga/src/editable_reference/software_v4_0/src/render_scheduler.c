#include "render_scheduler.h"
#include "video_pipeline.h"
#include "xil_cache.h"
#include <string.h>
#include "ui_freeze_snapshot.h"
#include "performance_monitor.h"
#include "render_diagnostics.h"
#include "render_cpu.h"
#include "capture_session.h"
static void cache_range(uintptr_t address,uint32_t bytes,int invalidate){
 XTime t;XTime_GetTime(&t);tracegpu_perf_enter(TG_PERF_CACHE,t);
 if(invalidate)Xil_DCacheInvalidateRange(address,bytes);else Xil_DCacheFlushRange(address,bytes);
 XTime_GetTime(&t);tracegpu_perf_leave(t);
}
static uint32_t cpu_flush_ticks;
static void cpu_flush(uintptr_t address,uint32_t bytes){
 XTime a,b;XTime_GetTime(&a);cache_range(address,bytes,0);XTime_GetTime(&b);cpu_flush_ticks+=(uint32_t)(b-a);
}
static int partial_begin(void *context,uintptr_t canvas,uint32_t color,uint32_t id,uint32_t count){
 return tracegpu_gpu_batch_begin_color((tracegpu_gpu_batch_job*)context,canvas,id,count,color);
}
static int partial_submit(void *context,const tracegpu_render_rect *r,uint32_t count,uint32_t budget){
 return tracegpu_gpu_batch_submit((tracegpu_gpu_batch_job*)context,r,count,budget);
}
static int partial_begin_colored(void *context,uintptr_t canvas,uint32_t id,uint32_t count){
 return tracegpu_gpu_batch_begin_colored((tracegpu_gpu_batch_job*)context,canvas,id,count);
}
static int partial_submit_colored(void *context,const tracegpu_render_rect *r,const uint32_t *colors,uint32_t count,uint32_t budget){
 return tracegpu_gpu_batch_submit_colored((tracegpu_gpu_batch_job*)context,r,colors,count,budget);
}
static int partial_poll(void *context,uint32_t *cycles){
 tracegpu_gpu_batch_job *b=(tracegpu_gpu_batch_job*)context;int rc=tracegpu_gpu_batch_poll(b);
 if(rc>0)*cycles=b->cycles;
 return rc;
}
static void partial_invalidate(void *context,uintptr_t address,uint32_t bytes){
 (void)context;cache_range(address,bytes,1);
}
static const tracegpu_partial_gpu_ops partial_ops={partial_begin,partial_submit,partial_poll,partial_invalidate,0,0};
static const tracegpu_partial_gpu_ops partial_colored_ops={partial_begin,partial_submit,partial_poll,partial_invalidate,partial_begin_colored,partial_submit_colored};

static int render_prepared(tracegpu_render_scheduler *,const tracegpu_uart_event_ring *,int);
int tracegpu_render_begin(tracegpu_render_scheduler *s,const tracegpu_ui_state *ui,
                          const tracegpu_uart_event_ring *ring,
                          const tracegpu_touch_frame *touch,int settings,unsigned index)
{
    XTime now;
    if(s->phase)return -1;
    if(tracegpu_video_current_buffer()==index)return -2;
    s->index=index;s->canvas=index?TRACEGPU_UI_CANVAS_1:TRACEGPU_UI_CANVAS_0;
    s->frame=index?TRACEGPU_FRAMEBUFFER_1:TRACEGPU_FRAMEBUFFER_0;
    s->snapshot=*ui;s->live=*ui;if(touch)s->touch=*touch;else s->touch.count=0U;
    /* Scanout reads ARGB canvases directly; legacy GPU remains optional for wave drawing. */
    s->use_wave=0U;s->freeze_ready=0U;
    s->policy_requested_gpu=0U;
    /* The header displays the last CONFIRMED path from live, never a request. */
    s->snapshot.gpu_wave_defer=0U;s->cursor=0U;s->wave_ticks=0U;
    XTime_GetTime(&s->frame_begin);
    memset(&s->pending_metrics,0,sizeof s->pending_metrics);
    if(!settings&&(ui->running||ui->drain_state==CAPTURE_DRAINING)&&!s->shell.frozen&&
       tracegpu_wave_slice_begin(&s->plan,&s->snapshot,ring)>0){
        s->planning_ring=ring;s->phase=9U;
        XTime_GetTime(&now);s->pending_metrics.plan_ticks=(uint32_t)(now-s->frame_begin);
        return 0;
    }
    if(!settings)tracegpu_freeze_resolve(&s->snapshot,&s->plan,ring,s->shell.frozen,s->shell.freeze_pending);
    else s->plan.count=s->plan.pixels=0U;
    XTime_GetTime(&now);s->pending_metrics.plan_ticks=(uint32_t)(now-s->frame_begin);
    if(s->shell.freeze_pending)s->freeze_ready=1U;
    return render_prepared(s,ring,settings);
}
static int render_prepared(tracegpu_render_scheduler *s,const tracegpu_uart_event_ring *ring,int settings){
    const tracegpu_ui_state *ui=&s->snapshot;
    XTime now,prepared_begin;XTime_GetTime(&prepared_begin);
    uint32_t prefix=s->pending_metrics.plan_ticks;
    /* Legacy single-rectangle hardware falls back BEFORE GPU ownership. */
    if(s->batch_available&&s->wave_available&&ui->gpu_active&&!settings&&s->plan.count&&!s->plan.overflow&&!s->snapshot.data_page&&!s->cpu_full_reference){
        s->use_wave=s->requested==1U;
    }
    s->snapshot.gpu_wave_defer=0U;
    XTime_GetTime(&now);
    s->pending_metrics.plan_ticks=prefix+(uint32_t)(now-prepared_begin);
    s->pending_metrics.rect_count=s->plan.count;
    s->pending_metrics.pixels=s->plan.pixels;
    {const tracegpu_render_bucket *b=&s->policy.bucket[tracegpu_render_policy_bucket(s->plan.count)];
     s->pending_metrics.path_reason=tracegpu_render_explain(s->requested,s->wave_available&&s->batch_available,ui->gpu_active,
         s->plan.count,s->use_wave,b->cpu_samples<2U||b->gpu_samples<2U,0);}
    if(s->cpu_full_reference)s->pending_metrics.path_reason=TG_RENDER_REASON_FULL_CPU;
    else if(s->requested&&s->batch_available&&s->wave_available&&ui->gpu_active&&(s->plan.overflow||s->snapshot.data_page))
        s->pending_metrics.path_reason=TG_RENDER_REASON_CPU_FALLBACK;
    s->cpu_partial=(uint8_t)!settings;
    if(s->cpu_partial){
        cpu_flush_ticks=0;
        tracegpu_dashboard_cpu_latch(&s->snapshot,ring);
        if(tracegpu_cpu_begin(s->index,tracegpu_video_current_buffer(),s->canvas,&s->snapshot,&s->live,
                            &s->shell,&s->touch,&s->plan,s->cpu_full_reference)){
            s->phase=6;s->failures++;return -8;
        }
        memset(&s->batch.frame_perf,0,sizeof s->batch.frame_perf);
        s->policy_bucket=tracegpu_render_policy_partial_bucket(s->plan.count,s->plan.pixels,tracegpu_cpu_stats()->tiles);
        if(s->requested==2U&&s->batch_available&&s->wave_available&&ui->gpu_active&&
           s->plan.count&&!s->plan.overflow&&!s->snapshot.data_page&&!s->cpu_full_reference){
            int probing=tracegpu_render_policy_partial_probing(&s->policy,s->policy_bucket);
            s->use_wave=(uint8_t)tracegpu_render_policy_partial_choose(&s->policy,s->policy_bucket);
            s->policy_requested_gpu=s->use_wave;
            s->pending_metrics.path_reason=tracegpu_render_explain(s->requested,1,1,s->plan.count,s->use_wave,probing,0);
        }
        /* Unknown hardware is never assumed to understand per-command color.
           V1 keeps the original single-color batch path; unsupported hardware
           was excluded by the startup selftest before canvas ownership. */
        const tracegpu_partial_gpu_ops *ops=s->use_wave&&tracegpu_gpu_batch_colored_available()?&partial_colored_ops:&partial_ops;
        if(s->use_wave&&tracegpu_cpu_set_gpu(ops,&s->batch)){
            tracegpu_cpu_fault();s->phase=6;s->failures++;return -8;
        }
        if(s->use_wave&&s->requested==2U&&tracegpu_cpu_gpu_min_area(4096U)){
            tracegpu_cpu_fault();s->phase=6;s->failures++;return -8;
        }
        XTime_GetTime(&now);s->pending_metrics.plan_ticks=prefix+(uint32_t)(now-prepared_begin);
        s->phase=8;return 0;
    }
    /* A full/GPU frame bypasses tile epochs: never reuse stale CPU validity. */
    tracegpu_cpu_reset();tracegpu_dashboard_cpu_release();
    tracegpu_dashboard_set_wave_plan(settings?0:&s->plan);
    if(settings)tracegpu_dashboard_draw_settings(s->canvas,&s->snapshot);
    else tracegpu_dashboard_draw_uart(s->canvas,&s->snapshot,ring);
    tracegpu_dashboard_set_wave_plan(0);
    if(!s->use_wave&&!settings)s->wave_ticks=tracegpu_dashboard_cpu_wave_ticks();
    XTime_GetTime(&now);s->prepare_ticks=prefix+(uint32_t)(now-prepared_begin);
    s->wave_begin=now;
    if(s->use_wave){
        unsigned r,min_y=TRACEGPU_LCD_HEIGHT,max_y=0U;
        for(r=0U;r<s->plan.count;r++){
            unsigned y=s->plan.rects[r].y,end=y+s->plan.rects[r].h;
            if(y<min_y)min_y=y;
            if(end>max_y)max_y=end;
        }
        /* Whole aligned rows form one cache-line-safe merged dirty region.
           Other UI rows stay CPU-owned until the later conversion flush. */
        s->dirty_address=s->canvas+(uintptr_t)min_y*TRACEGPU_LCD_WIDTH*4U;
        s->dirty_bytes=(max_y-min_y)*TRACEGPU_LCD_WIDTH*4U;
        cache_range(s->dirty_address,s->dirty_bytes,0);
        cache_range(s->dirty_address,s->dirty_bytes,1);
        XTime_GetTime(&now);
        s->pending_metrics.cache_ticks=(uint32_t)(now-s->wave_begin);
        if(s->batch_available){
            if(tracegpu_gpu_batch_begin(&s->batch,s->canvas,s->frames+1U,s->plan.count)){
                s->phase=6U;s->failures++;return -3;
            }
            s->phase=7U;
        }else s->phase=1U;
    }else s->phase=2U;
    return 0;
}

static int fail(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats,int error)
{
    if(s->cpu_partial)tracegpu_cpu_fault();
    s->phase=6U;s->failures++;stats->frames_failed++;stats->active=0U;
    stats->last_state=s->job.last_state;return error;
}
int tracegpu_render_poll(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats)
{
    unsigned budget;int result;XTime now;
    if(!s->phase||s->phase==6U)return 0;
    if(s->phase==9U){
        XTime begin;XTime_GetTime(&begin);
        result=tracegpu_wave_slice_step(&s->plan,128U);
        XTime_GetTime(&now);s->pending_metrics.plan_ticks+=(uint32_t)(now-begin);
        if(!result)return 0;
        if(result<0){s->phase=0U;return 0;}
        if(s->shell.freeze_pending){tracegpu_freeze_store_prepared(&s->snapshot,&s->plan,s->planning_ring);s->freeze_ready=1U;}
        return render_prepared(s,s->planning_ring,0);
    }
    if(s->phase==8U){
        /* Feed consecutive ready stages without paying an entire application
         * loop between each cache slice, scan, submission and GPU completion.
         * Always yield after eight steps or the 125us cooperative target. */
        const XTime slice_ticks=COUNTS_PER_SECOND/8000U?COUNTS_PER_SECOND/8000U:1U;
        XTime slice_begin,slice_now;XTime_GetTime(&slice_begin);
        for(unsigned steps=0;steps<8U;steps++){
            result=tracegpu_cpu_step(tracegpu_video_current_buffer(),cpu_flush);
            if(result)break;
            XTime_GetTime(&slice_now);
            if(slice_now-slice_begin>=slice_ticks)break;
        }
        /* CPU step flushes through the bounded callback below. */
        if(result<0)return fail(s,stats,-8);
        if(!result)return 0;
        XTime_GetTime(&now);
        s->prepare_ticks=s->pending_metrics.plan_ticks+tracegpu_cpu_stats()->work_ticks;
        s->wave_ticks=tracegpu_cpu_stats()->wave_ticks;
        if(s->use_wave&&!tracegpu_cpu_stats()->gpu_batches)s->pending_metrics.path_reason=TG_RENDER_REASON_NO_DIRTY_WAVE;
        s->use_wave=(uint8_t)(tracegpu_cpu_stats()->gpu_batches!=0U);
        s->rectangles+=tracegpu_cpu_stats()->gpu_rects;
        s->pending_metrics.pl_cycles=tracegpu_cpu_stats()->pl_cycles;
        s->pending_metrics.gpu_jobs=s->batch.frame_perf;
        s->pending_metrics.submit_ticks=s->batch.frame_perf.submit_ticks;
        s->pending_metrics.cache_ticks=tracegpu_cpu_stats()->cache_ticks;
        s->pending_metrics.present_flush_ticks=cpu_flush_ticks;
        s->pending_metrics.ready_ticks=(uint32_t)(now-s->frame_begin);s->ready_frames++;
        if((uint64_t)s->pending_metrics.ready_ticks*1000000ULL>(uint64_t)COUNTS_PER_SECOND*22176ULL)s->late_frames++;
        tracegpu_video_select(s->index);s->phase=5U;
        s->switch_begin=now;s->switch_deadline=now+COUNTS_PER_SECOND/5U;
        return 0;
    }
    for(budget=0U;budget<8U;budget++){
        if(s->phase==7U){
            XTime submit_begin;XTime_GetTime(&submit_begin);
            result=tracegpu_gpu_batch_submit(&s->batch,s->plan.rects,s->plan.count,16U);
            XTime_GetTime(&now);s->pending_metrics.submit_ticks+=(uint32_t)(now-submit_begin);
            if(result<0)return fail(s,stats,result);
            result=tracegpu_gpu_batch_poll(&s->batch);
            if(result<0)return fail(s,stats,result);
            if(!result)return 0;
            s->rectangles+=s->plan.count;s->pending_metrics.pl_cycles=s->batch.cycles;
            XTime_GetTime(&now);
            s->wave_ticks=(uint32_t)(now-s->wave_begin);
            XTime_GetTime(&s->switch_begin);
            cache_range(s->dirty_address,s->dirty_bytes,1);
            XTime_GetTime(&now);s->pending_metrics.cache_ticks+=(uint32_t)(now-s->switch_begin);
            s->wave_ticks=(uint32_t)(now-s->wave_begin);
            XTime_GetTime(&s->switch_begin);tracegpu_dashboard_draw_wave_overlay(s->canvas,&s->snapshot);
            XTime_GetTime(&now);s->pending_metrics.overlay_ticks=(uint32_t)(now-s->switch_begin);s->phase=2U;
        }
        if(s->phase==1U){
            if(s->job.phase){
                result=tracegpu_gpu_job_poll(&s->job);
                if(result<0)return fail(s,stats,result);
                if(!result)return 0;
                s->cursor++;s->rectangles++;
            }
            if(s->cursor<s->plan.count){
                if(tracegpu_gpu_rect_begin(&s->job,s->canvas,&s->plan.rects[s->cursor]))
                    return fail(s,stats,-5);
                continue;
            }
            XTime_GetTime(&now);s->wave_ticks=(uint32_t)(now-s->wave_begin);
            XTime_GetTime(&s->switch_begin);
            cache_range(s->dirty_address,s->dirty_bytes,1);
            XTime_GetTime(&now);s->pending_metrics.cache_ticks+=(uint32_t)(now-s->switch_begin);
            s->wave_ticks=(uint32_t)(now-s->wave_begin);
            XTime_GetTime(&s->switch_begin);
            tracegpu_dashboard_draw_wave_overlay(s->canvas,&s->snapshot);
            XTime_GetTime(&now);
            s->pending_metrics.overlay_ticks=(uint32_t)(now-s->switch_begin);
            s->phase=2U;
        }
        if(s->phase==2U){
            XTime flush_begin;
            tracegpu_dashboard_draw_analysis(s->canvas,&s->snapshot);
            tracegpu_ui_shell_draw(s->canvas,&s->shell,&s->live);
            tracegpu_dashboard_draw_touch_points(s->canvas,&s->touch);
            XTime_GetTime(&flush_begin);
            cache_range(s->canvas,TRACEGPU_UI_CANVAS_BYTES,0);
            XTime_GetTime(&now);
            s->pending_metrics.present_flush_ticks=(uint32_t)(now-flush_begin);
            s->pending_metrics.ready_ticks=(uint32_t)(now-s->frame_begin);
            s->ready_frames++;
            /* 1056 * 525 pixels at 25 MHz; budget is one physical frame. */
            if((uint64_t)s->pending_metrics.ready_ticks*1000000ULL>
               (uint64_t)COUNTS_PER_SECOND*22176ULL)s->late_frames++;
            tracegpu_video_select(s->index);s->phase=5U;
            XTime_GetTime(&now);s->switch_begin=now;s->switch_deadline=now+COUNTS_PER_SECOND/5U;
        }
        if(s->phase==5U){
            XTime_GetTime(&now);
            /* Do not reuse the previous scanout buffer until the reader switched. */
            if(tracegpu_video_current_buffer()!=s->index){
                if(now>=s->switch_deadline)return fail(s,stats,-7);
                return 0;
            }
            if(s->cpu_partial){
                if(tracegpu_cpu_confirm(tracegpu_video_current_buffer()))return fail(s,stats,-9);
                tracegpu_dashboard_cpu_release();
            }
            s->frames++;if(s->use_wave)s->wave_frames++;
            stats->frames_ok++;stats->last_ticks=0U;
            s->frame_ticks=(uint32_t)(now-s->frame_begin);
            s->pending_metrics.frame_id=s->frames;
            s->pending_metrics.prepare_ticks=s->prepare_ticks;
            s->pending_metrics.wave_ticks=s->wave_ticks;
            s->pending_metrics.switch_ticks=(uint32_t)(now-s->switch_begin);
            s->pending_metrics.frame_ticks=s->frame_ticks;
            s->pending_metrics.gpu_wave=s->use_wave;s->pending_metrics.valid=1U;
            s->completed_metrics=s->pending_metrics; /* publish only at scanout barrier */
            /* Train only matching partial-work feature bins, after the actual
               scanout barrier, with preparation/cache/asynchronous waits. */
            if(s->requested==2U&&s->cpu_partial&&!s->cpu_full_reference&&
               s->batch_available&&s->wave_available&&s->snapshot.gpu_active&&
               !s->plan.overflow&&!s->snapshot.data_page&&tracegpu_cpu_stats()->wave_dirty_bands&&
               (!s->policy_requested_gpu||s->use_wave)&&
               (!s->use_wave||!s->batch.frame_perf.invalid_jobs))
                tracegpu_render_policy_partial_observe(&s->policy,s->policy_bucket,s->use_wave,s->pending_metrics.ready_ticks);
            s->phase=0U;return 1;
        }
    }
    return 0;
}
