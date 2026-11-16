#include "ui_search.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>
void tracegpu_ui_search_init(tracegpu_ui_shell *s){unsigned i;s->search_filter.address=-1;s->search_filter.read=-1;s->search_nibble=-1;for(i=0;i<2;i++){s->logic_search_filter[i].address=-1;s->logic_search_filter[i].read=-1;}}
static void logic_filter_load(tracegpu_ui_shell *s){
 tracegpu_search_filter *f=&s->logic_search_filter[s->search_slot];
 const tracegpu_decode_config *c=&s->decode_config[s->search_slot];
 if(f->protocol!=c->protocol){memset(f,0,sizeof *f);f->address=f->read=-1;}
 f->protocol=c->protocol;f->channel=c->map[0];s->search_filter=*f;
 s->search_nibble=-1;s->search_address_edit=c->protocol==TG_DECODE_I2C;
}
int tracegpu_ui_search_action(tracegpu_ui_shell *s,int action){
 tracegpu_search_filter *f=&s->search_filter;
 if(action==UI_SEARCH){s->logic_search=s->protocol_mode==3;if(s->logic_search){s->search_slot=(uint8_t)s->decode_slot;logic_filter_load(s);}else{if(f->protocol!=s->protocol_mode){memset(f,0,sizeof *f);f->address=f->read=-1;f->protocol=(uint8_t)s->protocol_mode;s->search_nibble=-1;s->search_address_edit=0;}f->channel=(uint8_t)s->selected;}s->menu_page=TG_MENU_SEARCH;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_SEARCH_SLOT&&s->logic_search){s->logic_search_filter[s->search_slot]=*f;if(s->search.state==TG_SEARCH_DONE||s->search.state==TG_SEARCH_MORE){s->logic_search_page[s->search_slot]=s->search.page;s->logic_search_selected[s->search_slot]=s->search.selected;}s->search_slot^=1;logic_filter_load(s);return action;}
 if(action==UI_SEARCH_RANGE&&s->logic_search){s->search_all^=1;return action;}
 if(action==UI_SEARCH_INPUT_FORMAT&&s->logic_search&&f->protocol!=TG_DECODE_I2C){s->search_ascii^=1;s->search_nibble=-1;return 0;}
 if(action==UI_SEARCH_ASCII_PAGE&&s->logic_search&&f->protocol!=TG_DECODE_I2C){if(s->search_ascii)s->search_ascii_page=(s->search_ascii_page+1)%6;return 0;}
 if(action==UI_SEARCH_RUN&&s->logic_search){s->logic_search_filter[s->search_slot]=*f;return action;}
 if(action==UI_SEARCH_EDIT){s->menu_page=TG_MENU_SEARCH;tracegpu_ui_shell_open(s,3);return 0;}
 if(action>=UI_HEX_0&&action<=UI_HEX_F){unsigned n=(unsigned)(action-UI_HEX_0);
  if(s->logic_search&&s->search_ascii&&f->protocol!=TG_DECODE_I2C&&!s->search_address_edit){unsigned b=32U+s->search_ascii_page*16U+n;if(b<=126&&f->length<8){f->bytes[f->length]=(uint8_t)b;f->mask[f->length++]=255;}return 0;}
  if(s->search_address_edit&&f->protocol==TG_DECODE_CAN){f->can_id=((f->can_id<<4)|n)&0x1fffffffU;f->can_mask=0x1fffffffU;}
  else if(s->search_address_edit){unsigned old=f->address<0?0:(unsigned)f->address;f->address=(int16_t)(((old<<4)|n)&255U);}
  else if(f->length<8){if(s->search_nibble<0)s->search_nibble=(int)n;else{f->bytes[f->length]=(uint8_t)(s->search_nibble*16+n);f->mask[f->length++]=255;s->search_nibble=-1;}}
  return 0;
 }
 if(action==UI_SEARCH_WILD){if(!s->search_address_edit&&f->length<8&&s->search_nibble<0){f->bytes[f->length]=0;f->mask[f->length++]=0;}return 0;}
 if(action==UI_SEARCH_BACKSPACE){if(s->search_address_edit&&f->protocol==TG_DECODE_CAN)f->can_id>>=4;else if(s->search_address_edit)f->address=f->address<0?-1:f->address>>4;else if(s->search_nibble>=0)s->search_nibble=-1;else if(f->length)f->length--;return 0;}
 if(action==UI_SEARCH_CLEAR){if(s->search_address_edit&&f->protocol==TG_DECODE_CAN)f->can_id=f->can_mask=0;else if(s->search_address_edit)f->address=-1;else f->length=0;s->search_nibble=-1;return 0;}
 if(action==UI_SEARCH_ADDRESS){if(s->logic_search&&f->protocol==TG_DECODE_I2C)s->search_address_edit=1;else s->search_address_edit^=1;s->search_nibble=-1;return 0;}
 if(action==UI_SEARCH_ANY_ADDRESS){f->address=-1;f->can_mask=0;s->search_address_edit=0;return 0;}
 if(action==UI_SEARCH_CHANNEL){f->channel=(f->channel+1)%4;return 0;}
 if(action==UI_SEARCH_READ){f->read=f->read==1?-1:(int8_t)(f->read+1);return 0;}
 if(action==UI_SEARCH_NACK){f->nack=(f->nack+1)%(s->logic_search?5:3);return 0;}
 if(action==UI_SEARCH_ERRORS){f->errors^=1;return 0;}
 if(action==UI_SEARCH_MISO){f->miso^=1;return 0;}
 if(action==UI_SEARCH_FIRST){f->first_only^=1;return 0;}
 if(action==UI_SEARCH_CANCEL)return action;
 if(action>=UI_SEARCH_ROW0&&action<=UI_SEARCH_ROW4){unsigned i=s->search.selected/5*5+(unsigned)(action-UI_SEARCH_ROW0);if(i<s->search.count)s->search.selected=i;if(s->logic_search)s->logic_search_selected[s->search_slot]=s->search.selected;return 0;}
 return action;
}
const char *tracegpu_ui_search_label(int id,const tracegpu_ui_shell *s,char *buf){
 const tracegpu_search_filter *f=&s->search_filter;
 if(id>=UI_HEX_0&&id<=UI_HEX_F){
  if(s->logic_search&&s->search_ascii&&f->protocol!=TG_DECODE_I2C){unsigned b=32U+s->search_ascii_page*16U+(unsigned)(id-UI_HEX_0);if(b==32)return "SPACE";if(b>126)return "--";buf[0]=(char)b;buf[1]=0;return buf;}
  buf[0]="0123456789ABCDEF"[id-UI_HEX_0];buf[1]=0;return buf;
 }
 switch(id){
 case UI_SEARCH:return "查找数据";case UI_SEARCH_RUN:return "开始查找";case UI_SEARCH_CANCEL:return "取消查找";case UI_SEARCH_EDIT:return "修改条件";
 case UI_SEARCH_SLOT:snprintf(buf,96,"解码实例 %u",s->search_slot+1);return buf;
 case UI_SEARCH_DETAIL:return "详情";
 case UI_SEARCH_INPUT_FORMAT:return s->search_ascii?"输入 ASCII":"输入 HEX";
 case UI_SEARCH_ASCII_PAGE:if(s->search_ascii)snprintf(buf,96,"字符页 %u / 6",s->search_ascii_page+1);else snprintf(buf,96,"字符页 --");return buf;
 case UI_SEARCH_RANGE:return s->search_all?"范围：全快照":"范围：当前视窗";
 case UI_SEARCH_PREV:return "上一处";case UI_SEARCH_NEXT:return "下一处";case UI_SEARCH_LOCATE:return "定位波形";
 case UI_SEARCH_LOCATE_TXN:return "事务起点";
 case UI_SEARCH_WILD:return "??";case UI_SEARCH_BACKSPACE:return "退格";case UI_SEARCH_CLEAR:return "清空输入";
 case UI_SEARCH_CHANNEL:snprintf(buf,96,"CH%u",f->channel+1);return buf;
 case UI_SEARCH_ADDRESS:if(f->protocol==TG_DECODE_CAN){if(f->can_mask)snprintf(buf,96,"ID %08lX %s",(unsigned long)f->can_id,s->search_address_edit?"EDIT":"");else snprintf(buf,96,"CAN ID ANY / EDIT");return buf;}if(f->address<0)snprintf(buf,96,"地址 任意 / %s",s->search_address_edit?"编辑":"选择");else snprintf(buf,96,"地址 %02X / %s",f->address,s->search_address_edit?"编辑":"选择");return buf;
 case UI_SEARCH_ANY_ADDRESS:return "任意地址";case UI_SEARCH_READ:return f->read<0?"读写：全部":f->read?"只读":"只写";
 case UI_SEARCH_NACK:return f->nack==1?"地址 NACK":f->nack==2?"数据 NACK":f->nack==3?"地址 ACK":f->nack==4?"数据全 ACK":"ACK：全部";
 case UI_SEARCH_ERRORS:return f->errors?"仅错误记录":"正常内容";case UI_SEARCH_MISO:return f->miso?"来源 MISO":"来源 MOSI";case UI_SEARCH_FIRST:return f->first_only?"只匹配首字节":"事务内匹配";
 default:return 0;
 }
}
