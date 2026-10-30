#include "ui_shell.h"
#include "ui_layout.h"
#include "ui_search.h"
#include "ui_trigger.h"
#include "ui_workflow.h"
#include <string.h>
enum {OWN_NONE,OWN_TAP,OWN_BOTTOM,OWN_RIGHT,OWN_PANEL,OWN_WAVE,OWN_BLOCK,OWN_TABLE};
static int abs_i(int x){return x<0?-x:x;}
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
int tracegpu_ui_decode_locate(tracegpu_ui_state *u,const tracegpu_decode_detail *d){
 if(!u||!d||d->row.channel>=TRACEGPU_LOGIC_CHANNELS||d->row.start>d->row.end)return -1;
 uint64_t half=u->time_window_ticks/2U;
 u->follow=0;u->view_offset_ticks=0;u->view_anchor_ticks=d->row.start>UINT64_MAX-half?UINT64_MAX:d->row.start+half;
 u->marker_ticks=d->row.start;u->marker_channel=d->row.channel;u->marker_valid=1;u->data_page=0;return 0;
}
int tracegpu_config_valid(const tracegpu_uart_config *c){
 static const uint32_t rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};
 unsigned i;for(i=0;i<9&&rates[i]!=c->baud;i++);
 return i<9&&(c->bits==7||c->bits==8)&&c->parity<=2&&(c->stops==1||c->stops==2);
}
int tracegpu_config_equal(const tracegpu_uart_config *a,const tracegpu_uart_config *b){return a->baud==b->baud&&a->bits==b->bits&&a->parity==b->parity&&a->stops==b->stops;}
int tracegpu_config_prepare(const tracegpu_ui_shell *s,int all,tracegpu_uart_config out[4],unsigned *mask){
 unsigned i;int changed=0;if(s->selected>=4||!tracegpu_config_valid(&s->draft[s->selected]))return -1;
 *mask=all?s->applied_mask:((s->applied_mask&~(1U<<s->selected))|(s->enable_mask&(1U<<s->selected)));
 if(!*mask||(*mask&~15U))return -1;
 for(i=0;i<4;i++){out[i]=(all||i==s->selected)?s->draft[s->selected]:s->applied[i];if(!tracegpu_config_valid(&out[i]))return -1;changed|=!tracegpu_config_equal(&out[i],&s->applied[i]);}
 return changed||*mask!=s->applied_mask;
}
void tracegpu_ui_shell_init(tracegpu_ui_shell *s,const tracegpu_ui_state *ui){
 unsigned i;memset(s,0,sizeof *s);s->ai_prompts=1;s->ai_rules=1;s->ai_trigger_mode=3;s->enable_mask=s->applied_mask=ui->channel_enabled_mask;s->render_request=2;s->motion=1;s->running=ui->running;
 for(unsigned slot=0;slot<2;slot++)s->ai_profile[slot]=(tg_ai_bus_profile){100000,8,2,1,2,4,0,0};
 for(i=0;i<4;i++){s->draft[i].baud=ui->baud;s->draft[i].bits=ui->data_bits;s->draft[i].parity=ui->parity;s->draft[i].stops=ui->stop_bits;s->applied[i]=s->draft[i];}
 tracegpu_ui_search_init(s);tracegpu_ui_trigger_init(s);s->motion_mode=1;s->show_fps=1;s->i2c_show_ack=1;
 {tracegpu_spi_config cfg={0,0,0,2};tracegpu_spi_draft_reset(&s->spi_draft,&cfg);}
 s->logic_visible=(1U<<TRACEGPU_LOGIC_CHANNELS)-1U;
 for(i=0;i<TG_BUS_GROUPS;i++)tracegpu_bus_config_default(&s->bus_config[i],i);
 s->bus_revision=1;
 for(i=0;i<2;i++){s->decode_config[i].uart=(tracegpu_uart_decode_config){115200,(uint8_t)i,8,0,1};s->decode_config[i].spi=(tracegpu_spi_config){0,0,0,2};s->decode_config[i].can=(tracegpu_can_decode_config){100000000,500000,800,100,(uint8_t)i,0};for(unsigned j=0;j<4;j++)s->decode_config[i].map[j]=(uint8_t)j;s->decode_config[i].map[0]=(uint8_t)i;}
}
static int extent(const tracegpu_ui_shell *s){return s->drawer==1?256:(s->drawer==2||s->drawer==4)?266:800;}
static void animate(tracegpu_ui_shell *s,int target){s->target=target;s->anim_from=s->position;s->anim_start=s->now;s->animating=s->motion!=0;s->pressed=0;if(!s->motion){s->position=target;if(!target)s->drawer=0;}}
void tracegpu_ui_shell_open(tracegpu_ui_shell *s,int drawer){
 int opening_home=!s->drawer&&drawer==3&&s->menu_page==TG_MENU_HOME;
 s->drawer=drawer;s->scroll=0;s->page_motion=0;
 if(opening_home){s->position=0;animate(s,extent(s));}
 else{s->position=s->target=extent(s);s->animating=0;s->pressed=0;}
}
void tracegpu_ui_shell_tick(tracegpu_ui_shell *s,uint32_t now){
 s->now=now;if(s->animating){uint32_t t=now-s->anim_start;if(t>=180||!s->motion){s->position=s->target;s->animating=0;if(!s->target)s->drawer=0;}
 else {int q=(int)(t*t*(540U-2U*t)/32400U);s->position=s->anim_from+(s->target-s->anim_from)*q/180;}}
 if(s->page_motion&&now-s->page_motion_start>=180)s->page_motion=0;
}
static int hit(tracegpu_ui_shell *s,int x,int y){
 tracegpu_ui_control a[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,a),i;
 for(i=0;i<n;i++){
 if(x>=a[i].x&&x<a[i].x+a[i].w&&y>=a[i].y&&y<a[i].y+a[i].h)return a[i].id;}return 0;
}
int tracegpu_ui_shell_wave_allowed(const tracegpu_ui_shell *s){return s->owner==OWN_WAVE&&!s->data_page&&!s->frozen&&!s->freeze_pending&&!s->drawer&&!s->confirm;}
int tracegpu_ui_shell_input(tracegpu_ui_shell *s,const tracegpu_touch_frame *f){
 int dx,dy,result=0;unsigned i;const tracegpu_touch_point *p=0;
 if(!f)return 0;
 if(!f->count){
  if(s->contact){
   if(s->owner==OWN_BOTTOM||s->owner==OWN_RIGHT)animate(s,s->position>=extent(s)/3?extent(s):0);
   else if((s->owner==OWN_TAP||s->owner==OWN_PANEL)&&!s->moved&&s->pressed&&hit(s,s->x,s->y)==s->pressed)result=s->pressed;
   else if(s->owner==OWN_TABLE&&!s->table_multi&&!s->frozen&&!s->freeze_pending){
    if(s->table_swipe)result=s->table_swipe>0?UI_NEXT_PAGE:UI_PREV_PAGE;
    else if(!s->moved){
     if(s->protocol_mode==3){if(s->logic_results&&s->y>=TG_TABLE_TOP&&s->y<TG_TABLE_TOP+216)result=UI_ROW0+(s->y-TG_TABLE_TOP)/27;}
     else if(s->y>=TG_TABLE_TOP&&s->y<TG_TABLE_TOP+(int)TG_TABLE_VISIBLE_ROWS*TG_TABLE_ROW_HEIGHT)result=UI_ROW0+(s->y-TG_TABLE_TOP)/TG_TABLE_ROW_HEIGHT;
    }
   }
  }
  if((result==UI_AI_NOTICE_LOCATE||result==UI_AI_NOTICE_CLOSE)&&s->ai_pressed_event!=s->ai_banner_id)result=0;
  if(result){s->feedback_id=result;s->feedback_until=s->now+140;}
  s->contact=0;s->owner=OWN_NONE;s->pressed=0;return result;
 }
 if(!s->contact){
  s->contact=1;s->id=f->point[0].id;s->x0=s->x=f->point[0].x;s->y0=s->y=f->point[0].y;s->moved=0;s->pressed=0;s->press_start=s->position;s->scroll_start=s->scroll;s->table_swipe=0;s->table_multi=0;
  int control=hit(s,s->x,s->y);
  s->ai_pressed_event=s->ai_banner_id;
  if(s->animating&&control!=UI_MENU){s->owner=OWN_BLOCK;return 0;}
  if(s->confirm){s->owner=OWN_TAP;s->pressed=hit(s,s->x,s->y);}
  else if(control){s->owner=s->drawer?OWN_PANEL:OWN_TAP;s->pressed=control;}
  else if(s->drawer){
   if((s->drawer==1&&s->y<480-s->position)||((s->drawer==2||s->drawer==4)&&s->x<800-s->position)){animate(s,0);s->owner=OWN_BLOCK;}
   else{s->owner=OWN_PANEL;s->pressed=hit(s,s->x,s->y);}
  }else if(s->y>=472){if(s->simple_ui){s->menu_page=TG_MENU_HOME;tracegpu_ui_shell_open(s,3);s->owner=OWN_BLOCK;}else{s->owner=OWN_BOTTOM;s->drawer=1;s->position=0;}}
  else if(s->x>=788&&s->y>=TG_CONTENT_TOP&&s->y<TG_CONTENT_BOTTOM){if(s->simple_ui){s->menu_page=TG_MENU_HOME;tracegpu_ui_shell_open(s,3);s->owner=OWN_BLOCK;}else{s->owner=OWN_RIGHT;s->drawer=2;s->position=0;}}
  else if(s->y>=TG_CONTENT_TOP&&s->y<TG_CONTENT_BOTTOM&&s->x>=12&&s->x<788){s->owner=s->data_page?OWN_TABLE:OWN_WAVE;}
  else{s->owner=OWN_TAP;s->pressed=hit(s,s->x,s->y);}
 }
 if(f->count>1&&s->owner!=OWN_WAVE){s->pressed=0;s->owner=OWN_BLOCK;if(s->drawer&&!s->animating&&s->position!=extent(s))animate(s,0);return 0;}
 for(i=0;i<f->count;i++)if(f->point[i].id==s->id)p=&f->point[i];
 if(!p){s->owner=OWN_BLOCK;s->pressed=0;return 0;}
 s->x=p->x;s->y=p->y;dx=s->x-s->x0;dy=s->y-s->y0;
 if(abs_i(dx)>12||abs_i(dy)>12){s->moved=1;s->pressed=0;}
 if(s->owner==OWN_BOTTOM){if(abs_i(dy)>=32&&abs_i(dy)*2>abs_i(dx)*3)s->position=clamp(s->press_start-dy,0,256);}
 else if(s->owner==OWN_RIGHT){if(abs_i(dx)>=32&&abs_i(dx)*2>abs_i(dy)*3)s->position=clamp(s->press_start-dx,0,266);}
 else if(s->owner==OWN_PANEL&&s->moved){
  if(s->drawer==1&&dy>=32&&dy*2>abs_i(dx)*3){s->owner=OWN_BOTTOM;s->position=clamp(256-dy,0,256);/* release chooses nearest */}
  else if((s->drawer==2||s->drawer==4)&&dx>=32&&dx*2>abs_i(dy)*3){s->owner=OWN_RIGHT;s->position=clamp(266-dx,0,266);}
 }
 if(s->owner==OWN_TABLE&&abs_i(dy)>=40&&abs_i(dy)>abs_i(dx))s->table_swipe=dy<0?1:-1;
 return 0;
}
