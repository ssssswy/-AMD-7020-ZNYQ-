#include "export_uart_transport.h"
#include <string.h>
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t crc(const uint8_t *p,unsigned len){uint32_t n=tracegpu_crc32_update(~0U,p,20);return ~tracegpu_crc32_update(n,p+24,len);}
int tracegpu_export_transport_busy(const tracegpu_export_transport *t){return t&&t->state>=TG_X_WAIT_HOST&&t->state<=TG_X_WAIT_ACK;}
static void prepare(tracegpu_export_transport *t,uint32_t now){
 uint32_t remaining=tracegpu_export_size(t->capture)-t->capture->offset;
 unsigned n=remaining>TG_EXPORT_PAYLOAD?TG_EXPORT_PAYLOAD:(unsigned)remaining;
 memset(t->tx,0,24);memcpy(t->tx,"TGX3",4);t->tx[4]=1;t->tx[5]=n?TG_X_DATA:TG_X_END;
 put32(t->tx+8,t->session);put32(t->tx+12,t->sequence);put32(t->tx+16,(uint32_t)n);
 t->tx_size=24+n;t->tx_pos=0;t->last=!n;t->state=TG_X_SEND;t->retries=0;
 t->prepare_pos=0;t->prepare_crc=tracegpu_crc32_update(~0U,t->tx,20);t->preparing=n!=0;
 if(!n)put32(t->tx+20,~t->prepare_crc);
 t->deadline=now+5000;
}
void tracegpu_export_transport_begin(tracegpu_export_transport *t,tracegpu_capture_export *c,tracegpu_export_get get,tracegpu_export_put put,void *ctx,uint32_t now){
 memset(t,0,sizeof *t);t->capture=c;t->get=get;t->put=put;t->context=ctx;t->session=c->id^c->generation^now;if(!t->session)t->session=1;t->state=TG_X_WAIT_HOST;t->deadline=now+30000;t->last_step_ms=now;
}
void tracegpu_export_transport_cancel(tracegpu_export_transport *t){if(t)t->state=TG_X_CANCELLED;}
void tracegpu_export_transport_progress(const tracegpu_export_transport *t,uint32_t now,tracegpu_export_progress *p){
 if(!p)return;
 memset(p,0,sizeof *p);if(!t||!t->capture)return;
 p->confirmed_bytes=t->acked_bytes;p->total_bytes=tracegpu_export_size(t->capture);p->retries=t->total_retries;
 if(!t->started)return;
 p->elapsed_ms=(tracegpu_export_transport_busy(t)?now:t->last_step_ms)-t->started_ms;
 if(p->elapsed_ms>=500&&p->confirmed_bytes>=1024){
  p->bytes_per_second=(uint32_t)(((uint64_t)p->confirmed_bytes*1000U)/p->elapsed_ms);p->rate_valid=p->bytes_per_second!=0;
  if(p->rate_valid){uint32_t remaining=p->total_bytes-p->confirmed_bytes;p->eta_seconds=(remaining+p->bytes_per_second-1)/p->bytes_per_second;p->eta_valid=1;}
 }
}
static void receive(tracegpu_export_transport *t,uint32_t now){
 uint8_t *p=t->rx;uint32_t len=get32(p+16);unsigned type=p[5];
 if(p[4]!=1||p[6]||p[7]||len||get32(p+20)!=crc(p,0))return;
 if(type==TG_X_CANCEL&&get32(p+8)==t->session){t->state=TG_X_CANCELLED;return;}
 if(type==TG_X_START&&get32(p+8)==0&&get32(p+12)==0){
  /* Repeated START never rewinds or changes an active session/deadline. */
  if(t->state==TG_X_WAIT_HOST){t->started=1;t->started_ms=now;prepare(t,now);}return;
 }
 if(t->state==TG_X_WAIT_ACK&&type==TG_X_ACK&&get32(p+8)==t->session&&get32(p+12)==t->sequence){
  t->acked_bytes+=t->tx_size-24;if(t->last){t->state=TG_X_DONE;return;}t->sequence++;prepare(t,now);
 }
}
void tracegpu_export_transport_step(tracegpu_export_transport *t,uint32_t now,unsigned budget){
 unsigned rx_budget=budget;if(!tracegpu_export_transport_busy(t))return;
 t->last_step_ms=now;
 if(!t->get||!t->put||!t->capture||!tracegpu_logic_view_valid(t->capture->view,t->capture->id,t->capture->generation)){t->state=TG_X_FAILED;return;}
 while(rx_budget--){int b=t->get(t->context);if(b<0)break;
  if(t->rx_pos<4){static const uint8_t magic[4]={'T','G','X','3'};if((uint8_t)b!=magic[t->rx_pos]){t->rx_pos=(uint8_t)b=='T'?1:0;continue;}}
  t->rx[t->rx_pos++]=(uint8_t)b;
  /* Host control frames have no body: reject bogus lengths immediately. */
  if(t->rx_pos==20){if(t->rx[4]!=1||t->rx[6]||t->rx[7]||get32(t->rx+16)!=0||!(t->rx[5]==TG_X_START||t->rx[5]==TG_X_ACK||t->rx[5]==TG_X_CANCEL)){t->rx_pos=0;continue;}t->rx_size=24;}
  if(t->rx_pos>=24&&t->rx_pos==t->rx_size){receive(t,now);t->rx_pos=0;}
 }
 if(t->state==TG_X_SEND){if((int32_t)(now-t->deadline)>=0){t->state=TG_X_FAILED;return;}
  if(t->preparing){
   unsigned remaining=t->tx_size-24-t->prepare_pos,n=budget<remaining?budget:remaining;if(n>64)n=64;
   if(n){int got=tracegpu_export_read(t->capture,t->tx+24+t->prepare_pos,n);if(got!=(int)n){t->state=TG_X_FAILED;return;}
    t->prepare_crc=tracegpu_crc32_update(t->prepare_crc,t->tx+24+t->prepare_pos,n);t->prepare_pos+=n;
    if(t->prepare_pos==t->tx_size-24){put32(t->tx+20,~t->prepare_crc);t->preparing=0;}
   }return;
  }
  while(budget--&&t->tx_pos<t->tx_size){if(!t->put(t->context,t->tx[t->tx_pos]))break;t->tx_pos++;}
  if(t->tx_pos==t->tx_size){t->state=TG_X_WAIT_ACK;t->deadline=now+2000;}}
 else if((t->state==TG_X_WAIT_ACK||t->state==TG_X_WAIT_HOST)&&(int32_t)(now-t->deadline)>=0){
  if(t->state==TG_X_WAIT_HOST||++t->retries>3)t->state=TG_X_FAILED;
  else{t->total_retries++;t->tx_pos=0;t->state=TG_X_SEND;t->deadline=now+5000;}
 }
}
