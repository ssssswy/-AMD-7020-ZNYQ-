`timescale 1ns/1ps
// PL-only four-model UART window scheduler. Bus CNNs are not instantiated.
// UART uses ONE physical core. Raw writes are internal PL;
// this is an INTERNAL interface, not a PS per-window submission port.
module tg_ai_uart_engine #(parameter ROM_DIR="model/v46_uart")(
 input logic clk,resetn,clear,wr,start,input logic[9:0]wr_addr,input logic wr_data,
 input logic[31:0]unit_den,input logic data8,parity_enable,parity_odd,stop2,
 input logic[1:0]protocol_mode,input logic[3:0]bus_wr_data,
 input logic bus_cpol,bus_cpha,bus_word16,input logic[6:0]bus_period_code,
 input logic[2:0]bus_setup,bus_hold,input logic[7:0]bus_glitch,bus_clock,bus_timeout,
 output logic busy,done,fault,output logic[7:0]error_code,
 output logic result_valid,input logic result_ready,output logic[2:0]result_class,
 output logic signed[31:0]result_score,result_cutoff,output logic[4:0]result_fraction,
 output logic result_point,output logic[47:0]result_center_numerator,output logic[31:0]result_center_denominator,
 output logic[9:0]result_anchor,output logic[3:0]result_slot,output logic result_primary,output logic signed[31:0]result_anchor_margin,
 output logic[1:0]result_protocol,output logic[7:0]result_source,output logic[2:0]result_bin_samples,
 output logic[31:0]jobs_completed,window_cycles,output logic[8:0]tokens_total);
 typedef enum logic[4:0]{IDLE,BEGIN_WINDOW,GLOBAL_STREAM,CORE_START,CORE_WAIT,DEC_START,DEC_WAIT,
  TOKEN_WAIT,LOCAL_EMIT,LOCAL_STREAM,ADVANCE_TOKEN,RESULT,ERROR}state_t;
 state_t state;logic local_complete;logic[31:0]elapsed,watchdog;logic[2:0]core_model,decision_model,decision_kind;
 wire rn=resetn&&!clear;
 logic[1:0]protocol;logic bb,bd,bf,bv,br;logic[11:0]ba;logic signed[7:0]bdata;
 logic[6:0]core_result_addr;logic[5:0]position_index,best_position;logic signed[31:0]best_position_score;
 logic[2:0]bus_class;logic signed[31:0]bus_score,bus_cut;logic[8:0]local_tokens;
 logic gb,gd,gf,gv,gr;logic[12:0]ga;logic signed[7:0]gdata;
 tg_ai_uart_global global_encoder(.clk,.resetn(rn),.wr(wr&&state==IDLE&&protocol_mode==0),.start(state==BEGIN_WINDOW&&protocol==0),.wr_addr,.wr_data,
  .unit_den,.data8,.parity_enable,.parity_odd,.stop2,.busy(gb),.done(gd),.fault(gf),.out_valid(gv),.out_ready(gr),.out_addr(ga),.out_data(gdata));
 logic lb,ld,lf,lt,lv,lr,lsdone,lp,lip,lri;logic[10:0]la;logic signed[7:0]ldata;logic[9:0]lanchor;logic[3:0]lslot;
 logic[47:0]lcenter;logic[31:0]lcenter_den;logic signed[31:0]am;
 tg_ai_uart_local local_encoder(.clk,.resetn(rn),.wr(wr&&state==IDLE&&protocol_mode==0),.start(state==BEGIN_WINDOW&&protocol==0),.wr_addr,.wr_data,
  .unit_den,.data8,.parity_enable,.parity_odd,.stop2,.busy(lb),.done(ld),.fault(lf),.token_valid(lt),
  .emit_local(state==LOCAL_EMIT&&core_model!=3),.emit_stop(state==LOCAL_EMIT&&core_model==3),.advance(state==ADVANCE_TOKEN),.anchor_margin(am),
  .anchor_index(lanchor),.slot_index(lslot),.is_parity(lip),.primary(lp),.role_index(lri),.token_count(local_tokens),
  .center_numerator(lcenter),.center_denominator(lcenter_den),.out_valid(lv),.out_ready(lr),.out_addr(la),.out_data(ldata),.stream_done(lsdone));
 logic cb,cd,cf,cwr;logic[12:0]ca;logic signed[7:0]cdata;logic[159:0]cz;logic signed[31:0]crdata;
 tg_ai_p16_core #(.ROM_DIR(ROM_DIR))core(.clk,.resetn(rn),.wr(cwr),.start(state==CORE_START),.model_id(core_model),.addr(ca),.data(cdata),
  .busy(cb),.done(cd),.fault(cf),.logits(cz),.result_addr(core_result_addr),.result_data(crdata));
 logic db,dd,df,da;logic[2:0]dclass;logic signed[31:0]dmargin,dcut;logic[4:0]dfraction;logic[159:0]decision_logits;
 tg_ai_decision #(.ROM_FILE({ROM_DIR,"/global_decision.mem"}))decision(.clk,.resetn(rn),.start(state==DEC_START),.model_id(decision_model),
  .logits(decision_logits),.anchor_margin(am),.busy(db),.done(dd),.fault(df),.abnormal(da),.class_id(dclass),.margin(dmargin),.cutoff(dcut),.fraction_bits(dfraction));
 logic global_abnormal,local_abnormal;logic[2:0]global_class,local_class;
 logic signed[31:0]global_score,local_score,local_cut;
 logic[47:0]best_center;logic[31:0]best_center_den;logic[9:0]best_anchor;logic[3:0]best_slot;logic best_primary;logic signed[31:0]best_anchor_margin;
 always_comb begin
  busy=state!=IDLE&&state!=ERROR;result_valid=state==RESULT;
  cwr=0;ca=0;cdata=0;gr=0;lr=0;br=0;
  if(state==GLOBAL_STREAM)begin
   if(protocol==0)begin gr=!cb;cwr=gv&&gr;ca=ga;cdata=gdata;end
  end
  if(state==LOCAL_STREAM)begin lr=!cb;cwr=lv&&lr;ca={2'b0,la};cdata=ldata;end
  result_class=global_abnormal ? global_class : local_abnormal ? local_class : 0;
  result_score=global_abnormal ? global_score : local_abnormal ? local_score : global_score;
  result_cutoff=global_abnormal||!local_abnormal ? 607 : local_cut;
  result_fraction=global_abnormal||!local_abnormal ? 8 : 11;
  result_point=!global_abnormal&&local_abnormal;
  result_center_numerator=best_center;result_center_denominator=best_center_den;
  result_anchor=best_anchor;result_slot=best_slot;result_primary=best_primary;result_anchor_margin=best_anchor_margin;
  result_protocol=protocol;result_source=global_abnormal||!local_abnormal?2:1;result_bin_samples=1;tokens_total=local_tokens;
 end
 always_ff @(posedge clk)begin
  if(!rn)begin
   state<=IDLE;done<=0;fault<=0;error_code<=0;local_complete<=0;elapsed<=0;watchdog<=0;window_cycles<=0;jobs_completed<=0;
   core_model<=0;decision_model<=0;decision_kind<=0;am<=0;decision_logits<=0;
   global_abnormal<=0;local_abnormal<=0;global_class<=0;local_class<=0;global_score<=0;local_score<=0;local_cut<=0;
   best_center<=0;best_center_den<=1;best_anchor<=0;best_slot<=0;best_primary<=0;best_anchor_margin<=0;
   protocol<=0;core_result_addr<=0;position_index<=0;best_position<=0;best_position_score<=32'sh80000000;bus_class<=0;bus_score<=0;bus_cut<=0;
  end else begin
   done<=0;if(ld)local_complete<=1;
   if(busy)elapsed<=elapsed+1;
   if(state==CORE_WAIT)watchdog<=watchdog+1;else watchdog<=0;
   case(state)
    IDLE:if(start)begin
     if(fault||wr||protocol_mode!=0)begin fault<=1;error_code<=1;state<=ERROR;end
     else begin
      protocol<=0;core_model<=0;global_abnormal<=0;local_abnormal<=0;global_class<=0;local_class<=0;
      global_score<=0;local_score<=0;local_cut<=0;am<=0;local_complete<=0;elapsed<=0;
      best_center<=0;best_center_den<=1;best_anchor<=0;best_slot<=0;best_primary<=0;best_anchor_margin<=0;state<=BEGIN_WINDOW;
      core_result_addr<=0;position_index<=0;best_position<=0;best_position_score<=32'sh80000000;bus_class<=0;bus_score<=0;bus_cut<=0;
     end
    end
    BEGIN_WINDOW:state<=GLOBAL_STREAM;
    GLOBAL_STREAM:if(gd)state<=CORE_START;
    CORE_START:state<=CORE_WAIT;
    CORE_WAIT:if(cd)begin
     jobs_completed<=jobs_completed+1;decision_logits<=cz;decision_model<=core_model;
     decision_kind<=core_model==0 ? 0 : core_model==2 ? 2 : 1;
     state<=DEC_START;
    end
    DEC_START:state<=DEC_WAIT;
    DEC_WAIT:if(dd)begin
     if(decision_kind==0)begin global_abnormal<=da;global_class<=dclass;global_score<=dmargin;state<=TOKEN_WAIT;end
     else if(decision_kind==2)begin am<=dmargin;core_model<=lip?1:3;state<=LOCAL_EMIT;end
     else begin
      if(da&&(!local_abnormal||dmargin>local_score))begin
       local_abnormal<=1;local_class<=dclass;local_score<=dmargin;local_cut<=dcut;
       best_center<=lcenter;best_center_den<=lcenter_den;best_anchor<=lanchor;best_slot<=lslot;best_primary<=lp;best_anchor_margin<=am;
      end
      state<=ADVANCE_TOKEN;
     end
    end
    TOKEN_WAIT:if(local_complete||ld)begin window_cycles<=elapsed;state<=RESULT;end
     else if(lt)begin core_model<=2;state<=LOCAL_EMIT;end
    LOCAL_EMIT:state<=LOCAL_STREAM;
    LOCAL_STREAM:if(lsdone)state<=CORE_START;
    ADVANCE_TOKEN:state<=TOKEN_WAIT;
    RESULT:if(result_ready)begin done<=1;state<=IDLE;end
    ERROR:state<=ERROR;
    default:begin fault<=1;error_code<=2;state<=ERROR;end
   endcase
   // Reject a partial window: no result is presented on any submodule fault.
   if(gf||lf||cf||df)begin fault<=1;error_code<=gf?3:lf?4:cf?5:6;state<=ERROR;end
   // Includes the 8-lane resource configuration (global R2 needs >100k clocks).
   if(watchdog>=250000)begin fault<=1;error_code<=7;state<=ERROR;end
   if((start&&state!=IDLE)||(wr&&state!=IDLE))begin fault<=1;error_code<=8;state<=ERROR;end
  end
 end
endmodule
