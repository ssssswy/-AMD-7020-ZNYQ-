`timescale 1ns/1ps
// Autonomous live UART path: capture tap -> locked window -> PL engine -> FIFO.
// There is no per-window ARM data/configuration/decision handshake.
module tg_ai_uart_monitor #(parameter ROM_DIR="model/v46_p16",parameter FIFO_ADDR_BITS=6,parameter MAX_WINDOW_CYCLES=8000000)(
 input logic clk,resetn,clear,armed,capture_start,capture_run,input_ready,
 input logic[63:0]timestamp,input logic[7:0]levels,capture_mask,
 input logic[31:0]period_ticks,baud,epoch,config_generation,input logic[2:0]channel,
 input logic data8,parity_enable,parity_odd,stop2,
 output logic event_valid,output logic[511:0]event_data,input logic event_ready,
 output logic[FIFO_ADDR_BITS:0]event_level,output logic event_overflow,output logic[31:0]events_dropped,
 output logic[63:0]samples_seen,samples_dropped,windows_skipped,
 output logic[31:0]windows_checked,windows_failed,last_window_cycles,last_error,
 output logic active,window_locked,partial,config_fault);
 typedef enum logic[4:0]{IDLE,TAKE,READ_REQ,READ_WAIT,START_ENGINE,WAIT_RESULT,POSITION_A,POSITION_B,POSITION_C,
  DIV_START,DIV_WAIT,PACKET,RELEASE_WINDOW,RECOVER}state_t;
 state_t state;wire rn=resetn&&!clear;
 logic wv,wa,wo,wcf,rdv;logic[7:0]rddata,wm;logic[9:0]ri;
 logic[63:0]origin,window_end;logic[31:0]we,wg,wp,wid;
 logic[2:0]ch;logic cfg_d8,cfg_parity,cfg_odd,cfg_stop2,cfg_checked,cfg_valid;
 logic[31:0]cfg_baud,cfg_period,unit_den;logic[35:0]den_product;logic[1:0]cfg_stage;
 tg_ai_window_capture capture(.clk,.resetn,.clear,.armed,.capture_start,.capture_run,.input_ready,
  .timestamp,.levels,.capture_mask,.period_ticks,.epoch,.config_generation,
  .window_valid(wv),.window_take(state==TAKE),.window_release(state==RELEASE_WINDOW),
  .window_origin(origin),.window_end,.window_epoch(we),.window_generation(wg),.window_period(wp),.window_id(wid),.window_mask(wm),
  .rd_en(state==READ_REQ),.rd_offset(ri),.rd_valid(rdv),.rd_data(rddata),.samples_seen,.samples_dropped,.windows_skipped,
  .active(wa),.locked(window_locked),.overflow(wo),.config_fault(wcf));
 logic eb,ed,ef,ev,ep,eprimary;logic[7:0]ee;logic[2:0]eclass;logic signed[31:0]escore,ecut;
 logic[4:0]efrac;logic[47:0]ecenter;logic[31:0]ecenter_den,ejobs,ecycles;logic[9:0]eanchor;logic[3:0]eslot;logic[8:0]etokens;
 tg_ai_uart_engine #(.ROM_DIR(ROM_DIR))engine(.clk,.resetn,.clear(clear||state==RECOVER),
  .wr(state==READ_WAIT&&rdv),.start(state==START_ENGINE),.wr_addr(ri),.wr_data(rddata[ch]),
  .unit_den,.data8(cfg_d8),.parity_enable(cfg_parity),.parity_odd(cfg_odd),.stop2(cfg_stop2),
  .protocol_mode(2'd0),.bus_wr_data(4'd0),.bus_cpol(1'b0),.bus_cpha(1'b0),.bus_word16(1'b0),.bus_period_code(7'd0),
  .bus_setup(3'd0),.bus_hold(3'd0),.bus_glitch(8'd0),.bus_clock(8'd0),.bus_timeout(8'd0),
  .busy(eb),.done(ed),.fault(ef),.error_code(ee),.result_valid(ev),.result_ready(state==RELEASE_WINDOW),
  .result_class(eclass),.result_score(escore),.result_cutoff(ecut),.result_fraction(efrac),.result_point(ep),
  .result_center_numerator(ecenter),.result_center_denominator(ecenter_den),.result_anchor(eanchor),.result_slot(eslot),.result_primary(eprimary),
  .jobs_completed(ejobs),.window_cycles(ecycles),.tokens_total(etokens));
 logic[31:0]event_sequence,engine_watchdog;logic[511:0]packet;logic push,can_push;logic[7:0]event_source;
 tg_ai_event_fifo #(.ADDR_BITS(FIFO_ADDR_BITS))queue(.clk,.resetn,.clear,.s_valid(push),.s_data(packet),.s_ready(can_push),
  .m_valid(event_valid),.m_data(event_data),.m_ready(event_ready),.level(event_level),.overflow(event_overflow),.dropped(events_dropped));
 logic[63:0]target_begin,event_tick;logic point_flag;
 (* use_dsp="yes" *)logic[31:0]position_low,position_mid,position_high;
 logic[63:0]position_sum,position_numerator;
 logic db,dd,dz;logic[63:0]dq;logic[31:0]dr;
 tg_ai_udiv64 divider(.clk,.resetn(rn),.start(state==DIV_START),.numerator(position_numerator),.denominator(ecenter_den),
  .busy(db),.done(dd),.divide_zero(dz),.quotient(dq),.remainder(dr));
 always_comb begin
  active=wa;partial=wo||windows_skipped!=0||windows_failed!=0||events_dropped!=0;
  target_begin=origin+64'd512*wp;
  // Producer is a one-cycle event attempt. Full FIFO drops are counted; they
  // never stall the acquisition path or wait for the PS reader.
  push=state==PACKET&&eclass!=0&&armed&&!capture_start&&!config_fault;
  event_source=ep?8'd1:8'd2;
  packet=0;packet[31:0]=event_sequence+1;packet[63:32]=wid;packet[95:64]=we;packet[127:96]=wg;
  packet[159:128]={5'b0,ch,5'b0,eclass,event_source,8'd1};
  packet[191:160]=escore;packet[223:192]=ecut;
  packet[255:224]={20'b0,partial,(eprimary&&ep),1'b1,point_flag,3'b0,efrac};
  packet[319:256]=event_tick;packet[383:320]=target_begin;packet[447:384]=window_end;
  packet[479:448]=32'h4a77aa8b;packet[511:480]=wp;
 end
 always_ff @(posedge clk)begin
  if(!rn)begin
   state<=IDLE;ri<=0;ch<=0;cfg_d8<=0;cfg_parity<=0;cfg_odd<=0;cfg_stop2<=0;cfg_baud<=0;cfg_period<=0;
   unit_den<=0;den_product<=0;cfg_stage<=0;cfg_checked<=0;cfg_valid<=0;config_fault<=0;
   windows_checked<=0;windows_failed<=0;last_window_cycles<=0;last_error<=0;event_sequence<=0;engine_watchdog<=0;
   event_tick<=0;point_flag<=0;position_low<=0;position_mid<=0;position_high<=0;position_sum<=0;position_numerator<=0;
  end else begin
   if(capture_start&&armed)begin
    ch<=channel;cfg_d8<=data8;cfg_parity<=parity_enable;cfg_odd<=parity_odd;cfg_stop2<=stop2;
    cfg_baud<=baud;cfg_period<=period_ticks;cfg_stage<=1;cfg_checked<=0;
    cfg_valid<=period_ticks!=0&&period_ticks<=65535&&baud>=4800&&baud<=921600&&capture_mask[channel]&&!(parity_odd&&!parity_enable);
    if(state!=IDLE||window_locked)begin config_fault<=1;last_error<=16;end
   end else if(cfg_stage==1)begin den_product<=cfg_period[15:0]*cfg_baud[19:0];cfg_stage<=2;end
   else if(cfg_stage==2)begin
    cfg_checked<=1;cfg_stage<=0;unit_den<=den_product[31:0];
    if(den_product<3571429||den_product>22222222||!cfg_valid)begin cfg_valid<=0;config_fault<=1;last_error<=17;end
   end
   if(state==WAIT_RESULT)engine_watchdog<=engine_watchdog+1;else engine_watchdog<=0;
   if(wcf)begin config_fault<=1;if(last_error==0)last_error<=18;end
   case(state)
    IDLE:if(wv)begin
     if(!cfg_checked||!cfg_valid||config_fault||wp!=cfg_period)begin windows_failed<=windows_failed+1;state<=RELEASE_WINDOW;end
     else state<=TAKE;
    end
    TAKE:begin ri<=0;state<=READ_REQ;end
    READ_REQ:state<=READ_WAIT;
    READ_WAIT:if(rdv)begin if(ri==767)state<=START_ENGINE;else begin ri<=ri+1;state<=READ_REQ;end end
    START_ENGINE:state<=WAIT_RESULT;
    WAIT_RESULT:if(ef)begin windows_failed<=windows_failed+1;last_error<={24'b0,ee};state<=RECOVER;end
     else if(ev)begin
      last_window_cycles<=ecycles;windows_checked<=windows_checked+1;
      if(!ep||ecenter<64'd512*ecenter_den)begin point_flag<=0;event_tick<=target_begin+(window_end-target_begin)/2;state<=PACKET;end
      else if(ecenter>64'd767*ecenter_den)begin point_flag<=1;event_tick<=window_end;state<=PACKET;end
      else begin point_flag<=1;state<=POSITION_A;end
     end
    POSITION_A:begin
     position_low<=ecenter[15:0]*wp[15:0];position_mid<=ecenter[31:16]*wp[15:0];position_high<=ecenter[47:32]*wp[15:0];state<=POSITION_B;
    end
    POSITION_B:begin position_sum<={32'b0,position_low}+({32'b0,position_mid}<<16)+({32'b0,position_high}<<32);state<=POSITION_C;end
    POSITION_C:begin position_numerator<=position_sum+(ecenter_den>>1);state<=DIV_START;end
    DIV_START:state<=DIV_WAIT;
    DIV_WAIT:if(dd)begin
     if(dz||dq>64'd767*wp)begin windows_failed<=windows_failed+1;last_error<=19;state<=RECOVER;end
     else begin event_tick<=origin+dq;state<=PACKET;end
    end
    PACKET:begin if(eclass!=0)event_sequence<=event_sequence+1;state<=RELEASE_WINDOW;end
    RECOVER:state<=RELEASE_WINDOW;
    RELEASE_WINDOW:state<=IDLE;
    default:begin config_fault<=1;last_error<=20;state<=RECOVER;end
   endcase
   if(state==WAIT_RESULT&&engine_watchdog>=MAX_WINDOW_CYCLES)begin windows_failed<=windows_failed+1;last_error<=23;state<=RECOVER;end
   if(capture_start&&armed&&(state!=IDLE||window_locked))begin cfg_valid<=0;windows_failed<=windows_failed+1;last_error<=16;state<=RECOVER;end
   if(!armed&&state!=IDLE&&state!=RECOVER&&state!=RELEASE_WINDOW)begin windows_failed<=windows_failed+1;last_error<=21;state<=RECOVER;end
  end
 end
endmodule
