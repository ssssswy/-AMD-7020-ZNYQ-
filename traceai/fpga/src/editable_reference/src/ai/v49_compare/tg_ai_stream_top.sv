`timescale 1ns/1ps
// Autonomous UART CNN plus SPI/I2C/UART raw timing rules. No bus CNN.
// There is no per-window ARM data/configuration/decision handshake.
`include "v46_uart_contract.vh"
module tg_ai_stream_top #(parameter ROM_DIR="model/v46_uart",parameter FIFO_ADDR_BITS=6,parameter MAX_WINDOW_CYCLES=8000000)(
 input logic clk,resetn,clear,armed,capture_start,capture_run,input_ready,
 input logic[63:0]timestamp,input logic[7:0]levels,capture_mask,
 input logic[31:0]period_ticks,rate_hz,epoch,config_generation,input logic[2:0]channel,
 input logic[1:0]protocol_mode,input logic[11:0]bus_map,
 input logic bus_cpol,bus_cpha,bus_word16,spi_cs_high,input logic[2:0]bus_setup,bus_hold,
 input logic[7:0]bus_glitch,bus_clock,bus_timeout,
 input logic data8,parity_enable,parity_odd,stop2,
 input logic rules_enable,input logic[31:0]rule_glitch_ticks,rule_phase_ticks,rule_setup_ticks,rule_hold_ticks,
 output logic event_valid,output logic[511:0]event_data,input logic event_ready,
 output logic[FIFO_ADDR_BITS:0]event_level,output logic event_overflow,output logic[31:0]events_dropped,
 output logic[63:0]samples_seen,samples_dropped,windows_skipped,
 output logic[31:0]windows_checked,windows_failed,last_window_cycles,last_error,
 output logic active,window_locked,partial,config_fault,output logic[31:0]rules_dropped,events_merged,
 output logic trigger_window_valid,output logic[63:0]trigger_window_begin,trigger_window_end,
 output logic trigger_rule_valid,trigger_cnn_valid,output logic[511:0]trigger_rule_packet,trigger_cnn_packet,
 output logic[15:0]trigger_evidence_id,input logic protect_evidence,input logic[15:0]protect_evidence_id,
 input logic evidence_read_enable,input logic[6:0]evidence_read_address,output logic[31:0]evidence_read_data);
 typedef enum logic[4:0]{IDLE,TAKE,READ_REQ,READ_WAIT,START_ENGINE,WAIT_RESULT,POSITION_A,POSITION_B,POSITION_C,
  DIV_START,DIV_WAIT,PACKET,RELEASE_WINDOW,RECOVER,TAIL_START,TAIL_STREAM,TAIL_DISCARD,POSITION_LIMITS,POSITION_CHECK,EVIDENCE_START,EVIDENCE_WAIT,EVIDENCE_PUBLISH,EVIDENCE_STORE}state_t;
 state_t state;wire rn=resetn&&!clear;wire cnn_armed=armed&&protocol_mode==0;
 logic wv,wa,wo,wcf,rdv;logic[7:0]rddata,wm;logic[9:0]ri;
 logic[63:0]origin,window_end;logic[31:0]we,wg,wp,wid;
 logic[63:0]ring_origin,ring_end,last_checked_end;logic[31:0]ring_epoch,ring_generation,ring_period,ring_id;logic[7:0]ring_mask;
 logic ring_locked,tail_job,have_checked;
 logic tail_pending,tail_busy,tail_done,tail_rejected,tail_uncovered,tail_valid;logic[7:0]tail_reject_code,tail_mask,tail_data,tail_select_mask,sample_data;
 logic[63:0]tail_origin,tail_end;logic[31:0]tail_period,tail_epoch,tail_generation;logic[9:0]tail_index;
 logic[2:0]ch;logic cfg_d8,cfg_parity,cfg_odd,cfg_stop2,cfg_checked,cfg_valid;
 logic[31:0]cfg_rate,cfg_period,unit_den;logic[39:0]den_product;logic[2:0]cfg_stage;
 logic[1:0]cfg_protocol,result_protocol;logic[11:0]cfg_map;logic[3:0]bus_raw;
 logic cfg_cpol,cfg_cpha,cfg_word16,cfg_cs_high;logic[2:0]cfg_setup,cfg_hold,result_bin_samples;
 logic[7:0]cfg_glitch,cfg_clock,cfg_timeout,result_source;logic[6:0]cfg_period_code;
 logic map_valid;
 always_comb begin
  map_valid=capture_mask[bus_map[2:0]]&&capture_mask[bus_map[5:3]]&&bus_map[2:0]!=bus_map[5:3];
  if(protocol_mode==1)map_valid=map_valid&&capture_mask[bus_map[8:6]]&&capture_mask[bus_map[11:9]]&&
   bus_map[8:6]!=bus_map[2:0]&&bus_map[8:6]!=bus_map[5:3]&&bus_map[11:9]!=bus_map[2:0]&&bus_map[11:9]!=bus_map[5:3]&&bus_map[11:9]!=bus_map[8:6];
  sample_data=tail_job?tail_data:rddata;
  bus_raw={cfg_protocol==1 ? sample_data[cfg_map[11:9]] : 1'b0,cfg_protocol==1 ? sample_data[cfg_map[8:6]] : 1'b0,
   sample_data[cfg_map[5:3]]^(cfg_protocol==1&&cfg_cs_high),sample_data[cfg_map[2:0]]};
  tail_select_mask=protocol_mode==0 ? (8'b1<<channel) : (8'b1<<bus_map[2:0])|(8'b1<<bus_map[5:3]);
  if(protocol_mode==1)tail_select_mask=tail_select_mask|(8'b1<<bus_map[8:6])|(8'b1<<bus_map[11:9]);
  tail_select_mask=tail_select_mask&capture_mask;
 end
 tg_ai_tail_cache tail_cache(.clk,.resetn,.clear,.armed(cnn_armed),.capture_start,.capture_run,.timestamp,.levels,.selected_mask(tail_select_mask),.period_ticks,.epoch,.config_generation,
  .pending(tail_pending),.replay_start(state==TAIL_START),.discard(state==TAIL_DISCARD||state==RECOVER),.busy(tail_busy),.done(tail_done),.rejected(tail_rejected),
  .uncovered(tail_uncovered),.reject_code(tail_reject_code),.tail_origin,.tail_end,.tail_period,.tail_epoch,.tail_generation,.tail_mask,
  .out_valid(tail_valid),.out_ready(state==TAIL_STREAM),.out_index(tail_index),.out_data(tail_data));
 tg_ai_window_capture capture(.clk,.resetn,.clear,.armed(cnn_armed),.capture_start,.capture_run,.input_ready,
  .timestamp,.levels,.capture_mask,.period_ticks,.epoch,.config_generation,
  .window_valid(wv),.window_take(state==TAKE),.window_release(state==RELEASE_WINDOW),
  .window_origin(ring_origin),.window_end(ring_end),.window_epoch(ring_epoch),.window_generation(ring_generation),.window_period(ring_period),.window_id(ring_id),.window_mask(ring_mask),
  .rd_en(state==READ_REQ),.rd_offset(ri),.rd_valid(rdv),.rd_data(rddata),.samples_seen,.samples_dropped,.windows_skipped,
  .active(wa),.locked(ring_locked),.overflow(wo),.config_fault(wcf));
 logic signed[31:0]eanchor_margin;logic eb,ed,ef,ev,ep,eprimary;logic[7:0]ee;logic[2:0]eclass;logic signed[31:0]escore,ecut;
 logic[4:0]efrac;logic[47:0]ecenter;logic[31:0]ecenter_den,ejobs,ecycles;logic[9:0]eanchor;logic[3:0]eslot;logic[8:0]etokens;
 tg_ai_uart_engine #(.ROM_DIR(ROM_DIR))engine(.clk,.resetn,.clear(clear||state==RECOVER),
  .wr((state==READ_WAIT&&rdv)||(state==TAIL_STREAM&&tail_valid)),.start(state==START_ENGINE),.wr_addr(tail_job?tail_index:ri),.wr_data(sample_data[ch]),
  .unit_den,.data8(cfg_d8),.parity_enable(cfg_parity),.parity_odd(cfg_odd),.stop2(cfg_stop2),
  .protocol_mode(cfg_protocol),.bus_wr_data(bus_raw),.bus_cpol(cfg_cpol),.bus_cpha(cfg_cpha),.bus_word16(cfg_word16),.bus_period_code(cfg_period_code),
  .bus_setup(cfg_setup),.bus_hold(cfg_hold),.bus_glitch(cfg_glitch),.bus_clock(cfg_clock),.bus_timeout(cfg_timeout),
  .busy(eb),.done(ed),.fault(ef),.error_code(ee),.result_valid(ev),.result_ready(state==RELEASE_WINDOW),
  .result_class(eclass),.result_score(escore),.result_cutoff(ecut),.result_fraction(efrac),.result_point(ep),
  .result_center_numerator(ecenter),.result_center_denominator(ecenter_den),.result_anchor(eanchor),.result_slot(eslot),.result_primary(eprimary),.result_anchor_margin(eanchor_margin),
  .result_protocol,.result_source,.result_bin_samples,
  .jobs_completed(ejobs),.window_cycles(ecycles),.tokens_total(etokens));
 logic[63:0]target_begin,event_tick;logic point_flag;
 logic[31:0]engine_watchdog;logic[511:0]packet,rule_packet,merged_packet;
 logic push,can_push,rule_valid,rule_ready,rule_partial,rule_pending,merge_ready,merge_valid,merge_busy,grant_rule,last_rule,cnn_accept;
 logic evidence_busy,evidence_done,evidence_fault,store_busy,store_done;
 logic[15:0]evidence_sequence,allocation_id;logic[4:0]evidence_word_index;logic[31:0]evidence_word_data;
 tg_ai_uart_evidence evidence(.clk,.resetn,.clear,
  .wr((state==READ_WAIT&&rdv)||(state==TAIL_STREAM&&tail_valid)),.wr_addr(tail_job?tail_index:ri),.wr_data(sample_data[ch]),
  .start(state==EVIDENCE_START),.unit_den,.period_ticks(wp),.baud(cfg_rate),.epoch(we),.generation(wg),.window_id(wid),
  .origin,.evidence_tick(event_tick),.identity(packet[159:128]),.point_valid(point_flag&&ep),.primary(eprimary),.anchor(eanchor),.slot(eslot),
  .anchor_margin(eanchor_margin),.data8(cfg_d8),.parity_enable(cfg_parity),.parity_odd(cfg_odd),.stop2(cfg_stop2),.partial,
  .publish(state==EVIDENCE_PUBLISH),.sequence_id(evidence_sequence),.timestamp,
  .word_index(evidence_word_index),.word_data(evidence_word_data),.busy(evidence_busy),.done(evidence_done),.fault(evidence_fault));
 tg_ai_evidence_store evidence_store(.clk,.resetn,.clear,.new_capture(capture_start),.start(state==EVIDENCE_PUBLISH),.protect(protect_evidence),.protect_sequence_id(protect_evidence_id),
  .sequence_id(evidence_sequence),.allocation_id,.word_index(evidence_word_index),.word_data(evidence_word_data),.busy(store_busy),.done(store_done),
  .read_enable(evidence_read_enable),.read_address(evidence_read_address),.read_data(evidence_read_data));
 assign trigger_window_valid=(state==EVIDENCE_PUBLISH||(state==PACKET&&eclass==0))&&armed&&!config_fault;
 assign trigger_window_begin=target_begin;assign trigger_window_end=window_end;
 assign trigger_rule_valid=rule_valid;
 assign trigger_rule_packet=rule_packet;
 assign trigger_cnn_valid=state==EVIDENCE_PUBLISH&&eclass!=0&&armed&&!config_fault;
 assign trigger_cnn_packet={packet[511:256],allocation_id,packet[239:0]};
 assign trigger_evidence_id=allocation_id;
 logic[31:0]fifo_dropped;logic[32:0]drop_sum;logic[7:0]event_protocol;
 tg_ai_edge_rules rules(.clk,.resetn,.clear,.armed(armed&&!config_fault),.enable(rules_enable),.capture_start,.capture_run,.input_ready,.timestamp,.levels,
  .protocol_mode,.channel,.bus_map,.cpol(bus_cpol),.cpha(bus_cpha),.cs_high(spi_cs_high),.epoch,.config_generation,
  .glitch_ticks(rule_glitch_ticks),.phase_ticks(rule_phase_ticks),.setup_ticks(rule_setup_ticks),.hold_ticks(rule_hold_ticks),
  .event_valid(rule_valid),.event_data(rule_packet),.event_ready(rule_ready),.partial(rule_partial),.pending(rule_pending),.dropped(rules_dropped));
 assign grant_rule=rule_valid&&(!push||!last_rule);
 assign rule_ready=merge_ready&&grant_rule;
 assign cnn_accept=merge_ready&&push&&!grant_rule;
 tg_ai_event_merge merge_events(.clk,.resetn,.clear,.s_valid(push||rule_valid),.s_data(grant_rule?rule_packet:packet),.s_ready(merge_ready),
  .m_valid(merge_valid),.m_data(merged_packet),.m_ready(1'b1),.busy(merge_busy),.merged(events_merged));
 tg_ai_event_fifo #(.ADDR_BITS(FIFO_ADDR_BITS))queue(.clk,.resetn,.clear,.s_valid(merge_valid),.s_data(merged_packet),.s_ready(can_push),
  .m_valid(event_valid),.m_data(event_data),.m_ready(event_ready),.level(event_level),.overflow(event_overflow),.dropped(fifo_dropped));
 always_ff @(posedge clk)begin
  if(!rn)events_dropped<=0;
  else events_dropped<=drop_sum[32]?32'hffffffff:drop_sum[31:0];
 end
 logic[47:0]center_num;logic[31:0]center_den;logic center_point;logic[63:0]lower_center,upper_center;
 (* use_dsp="yes" *)logic[31:0]position_low,position_mid,position_high;
 logic[63:0]position_sum,position_numerator;
 logic db,dd,dz;logic[63:0]dq;logic[31:0]dr;
 tg_ai_udiv64 divider(.clk,.resetn(rn),.start(state==DIV_START||cfg_stage==3),
  .numerator(cfg_stage==3 ? 64'd200000000+(unit_den>>1) : position_numerator),.denominator(cfg_stage==3?unit_den:center_den),
  .busy(db),.done(dd),.divide_zero(dz),.quotient(dq),.remainder(dr));
 always_comb begin
  active=wa;window_locked=ring_locked||tail_job||tail_busy||tail_pending||rule_pending||merge_busy;
  drop_sum={1'b0,fifo_dropped}+{1'b0,rules_dropped};
  partial=wo||tail_uncovered||rule_partial||windows_skipped!=0||windows_failed!=0||fifo_dropped!=0;
  target_begin=origin+64'd512*wp;
  // Producer is a one-cycle event attempt. Full FIFO drops are counted; they
  // never stall the acquisition path or wait for the PS reader.
  push=state==PACKET&&eclass!=0&&armed&&!capture_start&&!config_fault;
  event_protocol={6'b0,result_protocol}+1;
  packet=0;packet[63:32]=wid;packet[95:64]=we;packet[127:96]=wg;
  packet[159:128]={5'b0,ch,5'b0,eclass,result_source,event_protocol};
  packet[191:160]=escore;packet[223:192]=ecut;
  packet[255:224]={evidence_sequence,4'b0,partial,(eprimary&&ep),1'b1,point_flag,3'b0,efrac};
  packet[319:256]=event_tick;packet[383:320]=target_begin;packet[447:384]=window_end;
  packet[479:448]=`TG_AI_UART_HASH0;packet[511:480]=wp*result_bin_samples;
 end
 always_ff @(posedge clk)begin
  if(!rn)begin
   state<=IDLE;ri<=0;ch<=0;cfg_d8<=0;cfg_parity<=0;cfg_odd<=0;cfg_stop2<=0;cfg_rate<=0;cfg_period<=0;
   cfg_protocol<=0;cfg_map<=0;cfg_cpol<=0;cfg_cpha<=0;cfg_word16<=0;cfg_cs_high<=0;cfg_setup<=0;cfg_hold<=0;
   cfg_glitch<=0;cfg_clock<=0;cfg_timeout<=0;cfg_period_code<=0;
   unit_den<=0;den_product<=0;cfg_stage<=0;cfg_checked<=0;cfg_valid<=0;config_fault<=0;
   windows_checked<=0;windows_failed<=0;last_window_cycles<=0;last_error<=0;engine_watchdog<=0;last_rule<=0;
   event_tick<=0;point_flag<=0;position_low<=0;position_mid<=0;position_high<=0;position_sum<=0;position_numerator<=0;
   center_num<=0;center_den<=0;center_point<=0;lower_center<=0;upper_center<=0;
   origin<=0;window_end<=0;we<=0;wg<=0;wp<=0;wid<=0;wm<=0;last_checked_end<=0;have_checked<=0;tail_job<=0;
  end else begin
   if(merge_ready&&(push||rule_valid))last_rule<=grant_rule;
   if(capture_start&&armed)begin
    have_checked<=0;
    ch<=protocol_mode==0?channel:bus_map[2:0];cfg_d8<=data8;cfg_parity<=parity_enable;cfg_odd<=parity_odd;cfg_stop2<=stop2;
    cfg_protocol<=protocol_mode;cfg_map<=bus_map;cfg_cpol<=bus_cpol;cfg_cpha<=bus_cpha;cfg_word16<=bus_word16;cfg_cs_high<=spi_cs_high;
    cfg_setup<=bus_setup;cfg_hold<=bus_hold;cfg_glitch<=bus_glitch;cfg_clock<=bus_clock;cfg_timeout<=bus_timeout;
    cfg_rate<=rate_hz;cfg_period<=period_ticks;cfg_stage<=1;cfg_checked<=0;
    cfg_valid<=period_ticks!=0&&period_ticks<=65535&&protocol_mode!=3&&
     (protocol_mode==0 ? rate_hz>=4800&&rate_hz<=921600&&capture_mask[channel]&&!(parity_odd&&!parity_enable) :
      rate_hz>0&&rate_hz<=12500000&&map_valid&&bus_setup<=4&&bus_hold<=4&&bus_glitch!=0&&bus_clock!=0);
    if(state!=IDLE||window_locked)begin config_fault<=1;last_error<=16;end
   end else if(cfg_stage==1)begin den_product<=cfg_period[15:0]*cfg_rate[23:0];cfg_stage<=2;end
   else if(cfg_stage==2)begin
    unit_den<=den_product[31:0];
    if(!cfg_valid||(cfg_protocol==0&&(den_product<3571429||den_product>22222222)))begin cfg_checked<=1;cfg_stage<=0;cfg_valid<=0;config_fault<=1;last_error<=17;end
    else begin cfg_checked<=1;cfg_stage<=0;end
   end else if(cfg_stage==3)cfg_stage<=4;
   else if(cfg_stage==4&&dd)begin
    cfg_checked<=1;cfg_stage<=0;
    if(dz||dq<16||dq>56)begin cfg_valid<=0;config_fault<=1;last_error<=24;end else cfg_period_code<=dq[6:0];
   end
   if(state==WAIT_RESULT)engine_watchdog<=engine_watchdog+1;else engine_watchdog<=0;
   if(wcf)begin config_fault<=1;if(last_error==0)last_error<=18;end
   case(state)
    IDLE:if(wv)begin
     if(!cfg_checked||!cfg_valid||config_fault||ring_period!=cfg_period)begin windows_failed<=windows_failed+1;state<=RELEASE_WINDOW;end
     else state<=TAKE;
    end else if(tail_pending&&armed)begin
     if(!cfg_valid||config_fault||(have_checked&&last_checked_end==tail_end))state<=TAIL_DISCARD;
     else state<=TAIL_START;
    end
    TAKE:begin
     origin<=ring_origin;window_end<=ring_end;we<=ring_epoch;wg<=ring_generation;wp<=ring_period;wm<=ring_mask;wid<=wid+1;
     tail_job<=0;ri<=0;state<=READ_REQ;
    end
    READ_REQ:state<=READ_WAIT;
    READ_WAIT:if(rdv)begin if(ri==767)state<=START_ENGINE;else begin ri<=ri+1;state<=READ_REQ;end end
    START_ENGINE:state<=WAIT_RESULT;
    WAIT_RESULT:if(ef)begin windows_failed<=windows_failed+1;last_error<={24'b0,ee};state<=RECOVER;end
     else if(ev)begin
      last_window_cycles<=ecycles;windows_checked<=windows_checked+1;
      center_num<=ecenter;center_den<=ecenter_den;center_point<=ep;state<=POSITION_LIMITS;
     end
    POSITION_LIMITS:begin lower_center<=64'd512*center_den;upper_center<=64'd767*center_den;state<=POSITION_CHECK;end
    POSITION_CHECK:begin
     if(!center_point||center_num<lower_center)begin point_flag<=0;event_tick<=target_begin+(window_end-target_begin)/2;state<=eclass==0?PACKET:EVIDENCE_START;end
     else if(center_num>upper_center)begin point_flag<=1;event_tick<=window_end;state<=eclass==0?PACKET:EVIDENCE_START;end
     else begin point_flag<=1;state<=POSITION_A;end
    end
    POSITION_A:begin
     position_low<=center_num[15:0]*wp[15:0];position_mid<=center_num[31:16]*wp[15:0];position_high<=center_num[47:32]*wp[15:0];state<=POSITION_B;
    end
    POSITION_B:begin position_sum<={32'b0,position_low}+({32'b0,position_mid}<<16)+({32'b0,position_high}<<32);state<=POSITION_C;end
    POSITION_C:begin position_numerator<=position_sum+(center_den>>1);state<=DIV_START;end
    DIV_START:state<=DIV_WAIT;
    DIV_WAIT:if(dd)begin
     if(dz||dq>64'd767*wp)begin windows_failed<=windows_failed+1;last_error<=19;state<=RECOVER;end
     else begin event_tick<=origin+dq;state<=eclass==0?PACKET:EVIDENCE_START;end
    end
    EVIDENCE_START:state<=EVIDENCE_WAIT;
    EVIDENCE_WAIT:if(evidence_done)begin
     if(evidence_fault)begin windows_failed<=windows_failed+1;last_error<=25;state<=RECOVER;end
     else state<=EVIDENCE_PUBLISH;
    end
    EVIDENCE_PUBLISH:state<=EVIDENCE_STORE;
    EVIDENCE_STORE:if(store_done)state<=PACKET;
    PACKET:if(eclass==0||cnn_accept||!armed||config_fault)begin last_checked_end<=window_end;have_checked<=1;state<=RELEASE_WINDOW;end
    RECOVER:state<=RELEASE_WINDOW;
    RELEASE_WINDOW:begin tail_job<=0;state<=IDLE;end
    TAIL_START:begin
     origin<=tail_origin;window_end<=tail_end;we<=tail_epoch;wg<=tail_generation;wp<=tail_period;wm<=tail_mask;wid<=wid+1;tail_job<=1;state<=TAIL_STREAM;
    end
    TAIL_STREAM:if(tail_done)state<=START_ENGINE;
    TAIL_DISCARD:state<=IDLE;
    default:begin config_fault<=1;last_error<=20;state<=RECOVER;end
   endcase
   if(tail_rejected&&armed&&state!=RECOVER&&state!=RELEASE_WINDOW)begin
    windows_failed<=windows_failed+1;last_error<=32'h80+tail_reject_code;
    if(state==TAIL_STREAM)state<=RECOVER;
   end
   if(state==WAIT_RESULT&&engine_watchdog>=MAX_WINDOW_CYCLES)begin windows_failed<=windows_failed+1;last_error<=23;state<=RECOVER;end
   if(capture_start&&armed&&(state!=IDLE||window_locked))begin cfg_valid<=0;windows_failed<=windows_failed+1;last_error<=16;state<=RECOVER;end
   if(!armed&&state!=IDLE&&state!=RECOVER&&state!=RELEASE_WINDOW)begin windows_failed<=windows_failed+1;last_error<=21;state<=RECOVER;end
  end
 end
endmodule
