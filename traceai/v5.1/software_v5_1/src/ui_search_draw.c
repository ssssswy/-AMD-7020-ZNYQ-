#include "ui_search.h"
#include "ui_paint.h"
#include "decode_result_status.h"
#include <stdio.h>
void tracegpu_ui_search_draw(uintptr_t fb,const tracegpu_ui_shell *s,int x){
 char line[128];const tracegpu_search *q=&s->search;unsigned i,at=0;
 if(s->menu_page==TG_MENU_SEARCH){
  at=(unsigned)snprintf(line,sizeof line,"HEX: ");
  for(i=0;i<s->search_filter.length;i++)at+=(unsigned)snprintf(line+at,sizeof line-at,s->search_filter.mask[i]?"%02X ":"?? ",s->search_filter.bytes[i]);
  if(s->search_nibble>=0)snprintf(line+at,sizeof line-at,"%X_",s->search_nibble);
  tg_text(fb,x+16,126,line,0xeaf1fa,0);
  at=(unsigned)snprintf(line,sizeof line,"ASCII: ");
  for(i=0;i<s->search_filter.length;i++){unsigned b=s->search_filter.bytes[i];line[at++]=!s->search_filter.mask[i]?'?':b>=32&&b<127?(char)b:'.';}line[at]=0;
  tg_text(fb,x+16,146,line,0xb4c3d5,0);
  if(s->logic_search){
   if(s->search_filter.protocol==TG_DECODE_UART)tg_text(fb,x+432,346,"最多8字节；??匹配一字节",0xb4c3d5,0);
   tg_text(fb,x+432,402,s->search_filter.protocol==TG_DECODE_I2C?"7位地址 / ACK 是协议事实":"搜索只读快照，源变化自动失效",0xf7be62,0);
  }else{
   tg_text(fb,x+432,346,s->search_address_edit?"键盘编辑7位地址":"最多8字节；??匹配一字节",0xb4c3d5,0);
   tg_text(fb,x+432,392,"查找前确认暂停并完成收尾",0xf7be62,0);
  }return;
 }
 if(s->menu_page!=TG_MENU_SEARCH_RESULTS)return;
 const char *status=q->state==TG_SEARCH_BUSY?"查找中":q->state==TG_SEARCH_DONE?"保留范围扫描完成":q->state==TG_SEARCH_MORE?"结果窗口已满，可继续查找":q->state==TG_SEARCH_STALE?"历史已变化，请重新查找":q->state==TG_SEARCH_CANCELLED?"已取消":q->state==TG_SEARCH_BAD_QUERY?"条件无效":"尚未查找";
 if(s->logic_search)snprintf(line,sizeof line,"实例%u %s %lu%% / 已解码 %llu / 命中 %llu%s",s->search_slot+1,status,(unsigned long)s->logic_search_progress,(unsigned long long)s->logic_search_decoded,(unsigned long long)s->logic_search_matches,q->incomplete?" / 有缺失":"");
 else snprintf(line,sizeof line,"%s / %lu 条记录%s",status,(unsigned long)q->scanned,q->incomplete?" / 有缺失":"");
 tg_text(fb,x+16,120,line,0xeaf1fa,0);
 for(i=0;i<5;i++){unsigned n=q->selected/5*5+i;if(n>=q->count)break;const tracegpu_search_result *r=&q->results[n];
  int y=136+(int)i*44;tg_rect(fb,x+16,y,768,40,n==q->selected?0x285582:0x22334b);
  char result_status[64];
  const char *summary=s->logic_search?tracegpu_decode_status_text(r->protocol,r->flags,result_status,sizeof result_status):r->flags?"不完整":"正常";
  snprintf(line,sizeof line,"#%lu  %s CH%u  raw=%llu us  %s",(unsigned long)(q->page*32+n+1),r->protocol==2?(q->filter.miso?"MISO":"MOSI"):tracegpu_decode_protocol_name(r->protocol),r->channel+1,(unsigned long long)(r->start/100),summary);tg_text(fb,x+24,y+17,line,s->logic_search?tracegpu_decode_status_color(r->protocol,r->flags):0xeaf1fa,0);
  at=0;for(unsigned k=0;k<r->length;k++)at+=(unsigned)snprintf(line+at,sizeof line-at,"%02X ",r->bytes[k]);line[at]=0;
  tg_text(fb,x+24,y+35,line,0xb4c3d5,0);
 }
 if(!q->count)tg_text(fb,x+32,220,q->state!=TG_SEARCH_DONE?status:q->incomplete?"无结果；缺失或截断内容无法确认":"当前保留范围没有结果",0xb4c3d5,0);
 /* No footer over the 362..406 action row. Quality remains in the header. */
}
