#include "ui_error_history.h"
#include "ui_paint.h"
#include "time_format.h"
#include <stdio.h>
const char *tg_ui_error_label(int id,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,char *b){
 switch(id){case UI_ERROR_HISTORY:return "异常历史";case UI_HISTORY_PREV:return "上一页";case UI_HISTORY_NEXT:return "下一页";
 case UI_HISTORY_FILTER:return tg_error_protocol_name(s->history_filter);case UI_HISTORY_SOURCE:return tg_error_source_name(s->history_source);
 case UI_HISTORY_CLEAR:return "清空历史";case UI_HISTORY_OPEN:return "定位问题波形";case UI_HISTORY_RETURN:return "返回采集";default:break;}
 if(id>=UI_HISTORY_ROW0&&id<=UI_HISTORY_ROW7){unsigned k=(unsigned)(id-UI_HISTORY_ROW0);if(k>=u->errors.count)return "";
  const tg_error_brief *r=&u->errors.rows[k];snprintf(b,96,"%s#%llu %s CH%u %s x%lu %s",r->read?"":"*",(unsigned long long)r->id,tg_error_protocol_name(r->protocol),r->channel+1,tg_error_source_name(r->source),(unsigned long)r->hits,tg_error_snapshot_name(r->snapshot_state));return b;}
 return 0;
}
void tg_ui_error_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,int x){
 if(s->menu_page!=TG_MENU_ERROR_HISTORY&&s->menu_page!=TG_MENU_ERROR_DETAIL)return;
 char b[128],t[48];
 if(s->menu_page==TG_MENU_ERROR_HISTORY){
  snprintf(b,sizeof b,"%lu 条 / 第 %lu 页；历史淘汰 %lu；归档跳过 %lu",(unsigned long)u->errors.total,(unsigned long)s->history_page+1,(unsigned long)u->errors.evicted,(unsigned long)u->errors.snapshot_skipped);tg_text(fb,x+18,141,b,0xb4c3d5,0);
  if(!u->errors.count){tg_text(fb,x+24,220,"暂无异常记录",0xb4c3d5,1);}return;
 }
 if(!u->errors.selected_valid){tg_text(fb,x+18,170,"记录已淘汰，请返回列表",0xffc68c,0);return;}
 const tg_error_record *r=&u->errors.selected;
 snprintf(b,sizeof b,"#%llu %s CH%u / %s / %s",(unsigned long long)r->id,tg_error_protocol_name(r->protocol),r->channel+1,tg_error_source_name(r->source),tg_error_kind_name(r));tg_text(fb,x+18,152,b,0xffc68c,0);
 tg_time_format(t,sizeof t,tracegpu_calibrated_ticks(r->tick>=r->context.origin?r->tick-r->context.origin:0,&r->context.calibration),u->time_unit);
 snprintf(b,sizeof b,"采集 %lu / 位置 %s / 次数 %lu",(unsigned long)r->context.capture_id,t,(unsigned long)r->hits);tg_text(fb,x+18,184,b,0xeaf1fa,0);
 if(r->source==TG_ERR_RULE)snprintf(b,sizeof b,"实测 %lu ticks / 预算 %lu ticks",(unsigned long)(uint32_t)r->ai.score,(unsigned long)(uint32_t)r->ai.cut);
 else if(r->source==TG_ERR_CNN)snprintf(b,sizeof b,"模型 UART R11 C / 得分 %ld / 阈值 %ld (Q%u)",(long)r->ai.score,(long)r->ai.cut,r->ai.fraction);
 else snprintf(b,sizeof b,"状态 0x%lx / 来源配置 %lu",(unsigned long)r->flags,(unsigned long)r->context.revision);
 tg_text(fb,x+18,216,b,0xb4c3d5,0);
 snprintf(b,sizeof b,"%s / 波形质量 0x%lx",tg_error_snapshot_name(r->snapshot_state),(unsigned long)r->snapshot_quality);tg_text(fb,x+18,248,b,0x70d7ff,0);
 snprintf(b,sizeof b,"扩展证据：%s",r->evidence.available?"已保留":"无扩展记录");tg_text(fb,x+18,280,b,0xb4c3d5,0);
 tg_text(fb,x+18,320,"定位查看问题前后波形；返回采集恢复实时画面",0xb4c3d5,0);
}
