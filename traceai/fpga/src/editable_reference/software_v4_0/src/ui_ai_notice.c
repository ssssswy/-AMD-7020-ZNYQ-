#include "ui_ai_notice.h"
#include "ui_paint.h"
#include "ui_channel_panel.h"
#include "ui_workflow.h"
#include <stdio.h>
#include <string.h>
void tg_ai_notice_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u){
 if(s->drawer||s->confirm){return;}
 const tg_ai_alert_view *v=&u->ai_alert;char text[128];
 if(!v->configured&&!v->status)return; /* Legacy/offline render snapshots have no AI service. */
 if(v->visible&&s->ai_prompts){
  tg_rect(fb,208,50,480,44,0x593023);
  snprintf(text,sizeof text,"%s CH%u %s 点击定位",v->event.source==5?"规则且CNN同窗":v->event.source==4?"规则预算违例":"PL CNN疑似异常",v->event.channel+1,tg_ai_event_type(&v->event));
  tg_text(fb,216,78,text,0xffce8c,0);tg_text(fb,658,79,"X",0xffffff,1);
  uint32_t age=s->now-v->appeared;unsigned width=!s->motion||age>=180?480:age*480/180;
  tg_rect(fb,208,91,(int)width,3,0xff9a57);return;
 }
 uint32_t color=0x8796a8;const char *state="AI等待快照";
 if(!v->configured){state="AI提示待配置";color=0xffc68c;}
 else if(v->status==TG_AI_UI_BUSY){state="PL AI判断中";color=0x67e096;}
 else if(v->status==TG_AI_UI_DONE){state=v->count?"AI疑似异常":"本段未检出异常";color=v->count?0xffc68c:0x67e096;}
 else if(v->status==TG_AI_UI_ERROR){state="AI分析失败";color=0xff847b;}
 else if(v->status==TG_AI_UI_UNSUPPORTED)state=s->simple_ui?"AI需设置信号：点击下方AI查看下一步":"AI当前模式不支持";
 else if(v->status==TG_AI_UI_OFFLINE)state="AI硬件不可用";
 else if(v->status==TG_AI_UI_REJECTED)state="AI数据不足或无效";
 else if(v->status==TG_AI_UI_CANCELLED)state="AI已取消 未检查范围未知";
 else if(v->status==TG_AI_UI_PARTIAL)state="仅规则检查 CNN上下文不足";
 if(s->simple_ui&&v->status==TG_AI_UI_WAIT){state=s->workflow_pending==TG_FLOW_AI?"正在保存数据，随后自动分析":s->running?"正在采集，点击AI可停止并分析":"点击AI检查本次数据";}
 if(v->autonomous){
  if(v->rules_only&&v->status==TG_AI_UI_ERROR){state="PL时序规则监测失败";color=0xff847b;}
  else
  if(v->rules_only&&(v->status==TG_AI_UI_WAIT||v->status==TG_AI_UI_BUSY||v->status==TG_AI_UI_DONE)){state=v->rules_enabled?(v->live?"PL时序规则监测中（无CNN）":"PL时序规则结果（无CNN）"):"PL规则未开启（无CNN）";color=v->rules_enabled?0x67e096:0xffc68c;}
  else
  if(v->status==TG_AI_UI_WAIT)state="AI待命：应用参数后开始新采集";
  else if(v->status==TG_AI_UI_BUSY){state=v->coverage_partial?"PL AI监测中（覆盖有缺口）":"PL AI判断中";color=v->coverage_partial?0xffc68c:0x67e096;}
  else if(v->status==TG_AI_UI_DONE)state=v->count?"PL监测发现疑似异常":"已监测窗口未提示异常；预热区未知";
  else if(v->status==TG_AI_UI_PARTIAL)state="PL AI覆盖不完整；未检查范围未知";
  else if(v->status==TG_AI_UI_STALE)state="AI参数已改变：请开始新采集";
  else if(v->status==TG_AI_UI_UNSUPPORTED)state="AI支持统一八路中的UART/SPI/I2C；反相暂不支持";
 }
 if(s->notice&&(int32_t)(s->notice_until-s->now)>0)return;
 snprintf(text,sizeof text,"%s%s",state,s->ai_prompts?"":" 提示已关闭");tg_text(fb,12,114,text,color,0);
 if(v->status==TG_AI_UI_BUSY){
  snprintf(text,sizeof text,"%lu/%lu",(unsigned long)v->completed,(unsigned long)v->total);tg_text(fb,672,114,text,color,0);
  if(v->total){uint32_t completed=v->completed>v->total?v->total:v->completed;tg_rect(fb,12,119,(int)((uint64_t)760*completed/v->total),2,color);}
 }
}
int tg_ai_notice_locate(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tg_ai_event *e){
 if(!u||!s||!e||e->channel>=8||e->begin>e->end||e->tick<e->begin||e->tick>e->end)return -1;
 uint64_t span=e->end-e->begin;if(span>UINT32_MAX/2U)span=UINT32_MAX/2U;
 if(u->time_window_ticks<span*2)u->time_window_ticks=(uint32_t)(span*2);
 if(!u->time_window_ticks)u->time_window_ticks=1;
 uint64_t half=u->time_window_ticks/2U;u->follow=0;u->view_offset_ticks=0;
 u->view_anchor_ticks=e->tick>UINT64_MAX-half?UINT64_MAX:e->tick+half;
 u->selected_channel=(uint8_t)e->channel;s->selected=e->channel;
 s->logic_visible|=1U<<e->channel;u->logic_visible=(uint8_t)s->logic_visible;
 /* Ensure a channel outside the current four-channel bank is actually visible. */
 u->view_mode=0;u->bus_mode=0;s->bus_mode=0;
 u->data_page=s->data_page=0;s->drawer=s->position=s->target=s->animating=0;return 0;
}
static int event_bounds(const tracegpu_ui_state *u,const tg_ai_event *e,int *left,int *right,int *top,int *bottom){
 if(!u->ai_alert.selected||!u->logic_mode||u->bus_mode||!u->time_window_ticks)return 0;
 uint64_t end=tracegpu_cursor_view_end(u->follow?u->latest_event_ticks:u->view_anchor_ticks,u->view_offset_ticks);
 uint64_t begin=end>u->time_window_ticks?end-u->time_window_ticks:0;
 if(e->end<begin||e->begin>end)return 0;
 uint64_t a=e->begin<begin?begin:e->begin,b=e->end>end?end:e->end;
 *left=TG_PLOT_LEFT+(int)((a-begin)*(TG_PLOT_RIGHT-TG_PLOT_LEFT)/u->time_window_ticks);
 *right=TG_PLOT_LEFT+(int)((b-begin)*(TG_PLOT_RIGHT-TG_PLOT_LEFT)/u->time_window_ticks);
 for(unsigned i=0;i<tracegpu_ui_lanes(u);i++)if(tracegpu_ui_lane_channel(u,i)==e->channel){*top=(int)tracegpu_ui_lane_high(u,i);*bottom=*top+(int)tracegpu_ui_lane_height(u)+2;return 1;}
 return 0;
}
static int bounds(const tracegpu_ui_state *u,int *l,int *r,int *t,int *b){return event_bounds(u,&u->ai_alert.highlight,l,r,t,b);}
void tg_ai_highlight_draw(uintptr_t fb,const tracegpu_ui_state *u){
 if(u->ai_alert.candidate_frame_visible){
  tg_ai_event frame=u->ai_alert.highlight;frame.begin=u->ai_alert.candidate_frame_begin;frame.end=u->ai_alert.candidate_frame_end;
  int fl,fr,ft,fbottom;if(event_bounds(u,&frame,&fl,&fr,&ft,&fbottom)){
   tg_rect(fb,fl,ft-5,fr-fl+1,1,0x9f795c);tg_rect(fb,fl,ft-5,1,4,0x9f795c);tg_rect(fb,fr,ft-5,1,4,0x9f795c);
  }
 }
 int l,r,t,b;if(!bounds(u,&l,&r,&t,&b))return;
 uint32_t color=u->ai_alert.highlight.source==4?0x70d7ff:0xff9672;
 tg_rect(fb,l,t-2,r-l+1,1,color);tg_rect(fb,l,b+1,r-l+1,1,color);
 tg_rect(fb,l,t,1,b-t,color);tg_rect(fb,r,t,1,b-t,color);
 uint64_t end=tracegpu_cursor_view_end(u->follow?u->latest_event_ticks:u->view_anchor_ticks,u->view_offset_ticks);
 int x=tracegpu_cursor_pixel(u->ai_alert.highlight.tick,end,u->time_window_ticks);
 if(u->ai_alert.highlight.point&&x>=0){uint32_t color=u->ai_alert.highlight.source==4?0x70d7ff:0xff9672;tg_rect(fb,x,TG_PLOT_TOP,1,TG_PLOT_BOTTOM-TG_PLOT_TOP,color);tg_text(fb,x>680?680:x,133,u->ai_alert.highlight.source==4?"规则边沿":"AI候选",color,0);}
}
void tg_ai_highlight_plan(tracegpu_wave_plan *p,const tracegpu_ui_state *u){
 static tracegpu_wave_plan scratch;int l,r,t,b;if(!bounds(u,&l,&r,&t,&b))return;
 scratch=*p;scratch.count=0;
 for(unsigned i=0;i<p->count;i++){
  tracegpu_render_rect a=p->rects[i];int end=a.x+a.w;
  if(a.y<t||a.y>=b||end<=l||a.x>r){if(scratch.count>=TRACEGPU_WAVE_MAX_RECTS)return;scratch.rects[scratch.count]=a;scratch.colors[scratch.count++]=p->colors[i];continue;}
  int cuts[4]={a.x,l>a.x?l:a.x,r+1<end?r+1:end,end};
  for(unsigned j=0;j<3;j++)if(cuts[j+1]>cuts[j]){
   if(scratch.count>=TRACEGPU_WAVE_MAX_RECTS)return;
   tracegpu_render_rect v=a;v.x=(uint16_t)cuts[j];v.w=(uint16_t)(cuts[j+1]-cuts[j]);
   scratch.rects[scratch.count]=v;scratch.colors[scratch.count++]=j==1?(u->ai_alert.highlight.source==4?0xff70d7ff:0xffff765e):p->colors[i];
  }
 }*p=scratch;
}

/* Keep input untouched until the complete highlighted result fits, matching
 * the synchronous transform's overflow fallback. Only the used prefix moves. */
static struct {tracegpu_wave_plan scratch;tracegpu_wave_plan *owner;unsigned at,commit;int l,r,t,b;uint32_t color;unsigned phase;} highlight_slice;
int tg_ai_highlight_slice_begin(tracegpu_wave_plan *p,const tracegpu_ui_state *u){
 if(!bounds(u,&highlight_slice.l,&highlight_slice.r,&highlight_slice.t,&highlight_slice.b))return 0;
 highlight_slice.owner=p;highlight_slice.at=highlight_slice.commit=0;highlight_slice.phase=1;
 highlight_slice.scratch.count=0;highlight_slice.color=u->ai_alert.highlight.source==4?0xff70d7ff:0xffff765e;return 1;
}
int tg_ai_highlight_slice_step(tracegpu_wave_plan *p,unsigned budget){
 if(highlight_slice.owner!=p||!highlight_slice.phase)return 1;
 while(budget--){
  if(highlight_slice.phase==2){
   if(highlight_slice.commit==highlight_slice.scratch.count){p->count=highlight_slice.commit;highlight_slice.phase=0;return 1;}
   unsigned k=highlight_slice.commit++;p->rects[k]=highlight_slice.scratch.rects[k];p->colors[k]=highlight_slice.scratch.colors[k];continue;
  }
  if(highlight_slice.at==p->count){highlight_slice.phase=2;continue;}
  unsigned i=highlight_slice.at++;tracegpu_render_rect a=p->rects[i];int end=a.x+a.w;
  int l=highlight_slice.l,r=highlight_slice.r;
  int cut[4]={a.x,l>a.x?l:a.x,r+1<end?r+1:end,end};
  int outside=a.y<highlight_slice.t||a.y>=highlight_slice.b||end<=l||a.x>r;
  for(unsigned j=0;j<(outside?1U:3U);j++){
   if(!outside&&cut[j+1]<=cut[j])continue;
   unsigned k=highlight_slice.scratch.count;
   if(k>=TRACEGPU_WAVE_MAX_RECTS){highlight_slice.phase=0;return 1;}
   tracegpu_render_rect v=a;
   if(!outside){v.x=(uint16_t)cut[j];v.w=(uint16_t)(cut[j+1]-cut[j]);}
   highlight_slice.scratch.rects[k]=v;highlight_slice.scratch.colors[k]=!outside&&j==1?highlight_slice.color:p->colors[i];highlight_slice.scratch.count++;
  }
 }
 return 0;
}
