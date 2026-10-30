`timescale 1ns/1ps
// Four unchanged UART networks; 8/16 output MAC lanes, one issue per clock.
// The legacy module/ROM-bank names remain stable; LANES is the actual hardware
// parallelism. Eight lanes reduce full-SoC congestion without changing values.
module tg_ai_p16_core #(parameter ROM_DIR="model/v46_uart",parameter WEIGHT_DEPTH=2048,parameter LANES=8)(
 input logic clk,resetn,wr,start,input logic[2:0] model_id,
 input logic[12:0] addr,input logic signed[7:0] data,
 output logic busy,done,fault,output logic[159:0] logits,
 input logic[6:0] result_addr,output logic signed[31:0] result_data);
 typedef enum logic[4:0]{IDLE,BIAS_REQ,BIAS_WAIT,BIAS_LOAD,PREP,DOT,EMIT,STORE,ADVANCE,
 RINIT,RRD,RACC,RMAX,RMEAN,QREAD,QROUND,QSHIFT} state_t;
 state_t state;
 (* ram_style="block" *) logic[7:0] a[0:8191],b[0:8191];
 (* rom_style="block" *) logic[39:0] params[0:255];
 initial $readmemh({ROM_DIR,"/params.mem"},params);
 logic[2:0] model,layer;logic bank;localparam fcn=1'b0;logic[1:0]op;
 logic[5:0] n_out;logic[9:0] n_in;logic[8:0] ln;logic[3:0]kernel;logic[6:0]padding;logic[4:0]dilation;
 logic[11:0]weight_base;logic[7:0]pb;logic last_layer;
 logic[3:0] lane,ki;logic[5:0]ic,reduce_oc;logic[8:0]pos;logic[10:0]step;
 logic upper_group;wire[3:0]group_base=upper_group?4'd8:4'd0;
 wire[3:0]logical_lane=group_base+lane;
 logic[12:0]loaded;integer expected,read_addr,write_addr,weight_addr,total_steps;
 logic[7:0] aq,bq,pool_even[0:15],maximum,quant_value;
 logic signed[31:0] bias[0:15];logic signed[7:0] shift[0:15];logic[39:0]pq;
 logic signed[7:0]wq[0:15];logic pad_q,valid0,valid1,last0,last1,issue_active;
 wire issue=state==DOT&&issue_active;
 wire signed[8:0] operand=pad_q?9'sd0:(layer==0?$signed({(bank?bq[7]:aq[7]),(bank?bq:aq)}):$signed({1'b0,(bank?bq:aq)}));
 (* use_dsp="yes" *) logic signed[16:0] product[0:LANES-1];
 (* use_dsp="yes" *) logic signed[31:0] acc[0:LANES-1];
 logic signed[31:0] sum_reduce;
 logic signed[31:0]quant_acc;logic signed[7:0]quant_shift;logic[32:0]quant_rounded,quant_shifted;
 logic write_a,write_b;logic[7:0]write_data;
 always_comb begin
  op=3;n_out=1;n_in=1;ln=1;kernel=1;padding=0;dilation=1;pb=0;last_layer=1;weight_base=0;
  case({model,layer})
   `include "v46_uart_layers.vh"
  endcase
  case(model_id)0:expected=6144;1,2:expected=1280;3:expected=384;default:expected=0;endcase
  total_steps=op==0?n_in*kernel:n_in;
  case(ln)256:read_addr=ic<<8;128:read_addr=ic<<7;64:read_addr=ic<<6;32:read_addr=ic<<5;default:read_addr=ic<<4;endcase
  if(op==0||(fcn&&op==3))read_addr=read_addr+pos+ki*dilation-padding;else read_addr=step;
  if(op==2)read_addr=(reduce_oc<<4)+pos;
  weight_addr=weight_base+step;
  write_a=0;write_b=0;write_addr=0;write_data=0;
  if(resetn)begin
   if(wr&&!busy&&addr==loaded&&addr<expected)begin write_a=1;write_addr=addr;write_data=data;end
   else if(state==STORE&&op!=3&&!(op==0&&!fcn&&!pos[0]))begin
    write_addr=fcn?(logical_lane<<6)+pos:op==0?logical_lane*(ln/2)+pos/2:logical_lane;
    write_data=op==0&&!fcn?(quant_value>pool_even[logical_lane]?quant_value:pool_even[logical_lane]):quant_value;
    write_a=bank;write_b=!bank;
   end else if(state==RMAX||state==RMEAN)begin
    write_addr=state==RMAX?reduce_oc:reduce_oc+n_out;
    case(ln)16:write_data=(sum_reduce+8)>>4;32:write_data=(sum_reduce+16)>>5;64:write_data=(sum_reduce+32)>>6;default:write_data=0;endcase
    if(state==RMAX)write_data=maximum;write_a=bank;write_b=!bank;
   end
  end
 end
 genvar g;
 generate for(g=0;g<16;g=g+1)begin:weight_lane
  localparam [7:0] HEX=g<10?8'h30+g:8'h61+g-10;
  (* keep_hierarchy="yes" *) tg_ai_p16_weight_rom #(.FILE({ROM_DIR,"/lane_",HEX,".mem"}),.DEPTH(WEIGHT_DEPTH)) rom(
   .clk(clk),.en(issue),.addr(weight_addr[$clog2(WEIGHT_DEPTH)-1:0]),.q(wq[g]));
 end endgenerate
 always_ff @(posedge clk)begin
  if(write_a)a[write_addr]<=write_data;if(write_b)b[write_addr]<=write_data;
  if(issue||state==RRD)begin aq<=a[read_addr&8191];bq<=b[read_addr&8191];end
  if(state==BIAS_REQ)pq<=params[pb+lane];
  if(!resetn)result_data<=0;
  else result_data<=result_addr<5?logits[result_addr*32+:32]:32'sd0;
 end
 function automatic[7:0]quant(input logic signed[31:0]v,input logic signed[7:0]sh);
  logic[32:0]t;begin t=v>0?{1'b0,v}:0;if(sh>0)t=(t+(33'd1<<(sh[4:0]-1)))>>sh[4:0];quant=t>127?127:t[7:0];end
 endfunction
 // STORE already emits one output per clock. Share the round/shift/saturate
 // datapath at that boundary rather than instantiating eight barrel shifters.
 // Select, round, and shift/saturate have separate clock boundaries. A single
 // combinational acc mux -> rounding -> barrel shift -> BRAM input does not
 // meet 100 MHz even though its numerical result is correct.
 always_comb quant_shifted=quant_shift>0?quant_rounded>>quant_shift[4:0]:quant_rounded;
 always_ff @(posedge clk)begin
  if(!resetn)begin
   state<=IDLE;busy<=0;done<=0;fault<=0;loaded<=0;logits<=0;model<=0;layer<=0;bank<=0;
   lane<=0;ic<=0;ki<=0;pos<=0;step<=0;reduce_oc<=0;sum_reduce<=0;maximum<=0;
   valid0<=0;valid1<=0;last0<=0;last1<=0;pad_q<=0;issue_active<=0;upper_group<=0;
   quant_acc<=0;quant_shift<=0;quant_rounded<=0;quant_value<=0;
   for(integer i=0;i<LANES;i=i+1)begin product[i]<=0;acc[i]<=0;end
   for(integer i=0;i<16;i=i+1)begin pool_even[i]<=0;bias[i]<=0;shift[i]<=0;end
  end else begin
   done<=0;valid0<=issue;valid1<=valid0;
   if(issue)begin pad_q<=op==0&&((pos+ki*dilation<padding)||(pos+ki*dilation>=ln+padding));last0<=step==total_steps-1;end
   if(valid0)begin for(integer i=0;i<LANES;i=i+1)product[i]<=operand*wq[group_base+i];last1<=last0;end
   if(valid1)for(integer i=0;i<LANES;i=i+1)acc[i]<=acc[i]+{{15{product[i][16]}},product[i]};
   if(wr)begin if(busy||addr!=loaded||addr>=expected)fault<=1;else loaded<=loaded+1;end
   if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     if(wr||fault||loaded!=expected||expected==0)fault<=1;
     else begin busy<=1;loaded<=0;logits<=0;model<=model_id;layer<=0;bank<=0;lane<=0;pos<=0;state<=BIAS_REQ;end
    end
    BIAS_REQ:state<=BIAS_WAIT;
    BIAS_WAIT:state<=BIAS_LOAD;
    BIAS_LOAD:begin
     bias[lane]<=$signed(pq[31:0]);shift[lane]<=$signed(pq[39:32]);
     if(lane+1<n_out)begin lane<=lane+1;state<=BIAS_REQ;end
     else begin lane<=0;reduce_oc<=0;state<=op==2?RINIT:PREP;end
    end
    PREP:begin for(integer i=0;i<LANES;i=i+1)acc[i]<=bias[group_base+i];step<=0;ic<=0;ki<=0;issue_active<=1;state<=DOT;end
    DOT:begin
     if(issue)begin
      if(step==total_steps-1)issue_active<=0;
      else begin step<=step+1;if(ki==kernel-1)begin ki<=0;ic<=ic+1;end else ki<=ki+1;end
     end
     if(valid1&&last1)state<=EMIT;
    end
    EMIT:begin
     lane<=0;state<=QREAD;
    end
    QREAD:begin quant_acc<=acc[lane];quant_shift<=shift[logical_lane];state<=QROUND;end
    QROUND:begin
     quant_rounded<=(quant_acc[31]?33'd0:{1'b0,quant_acc})+(quant_shift>0?(33'd1<<(quant_shift[4:0]-1)):33'd0);state<=QSHIFT;
    end
    QSHIFT:begin quant_value<=quant_shifted>127?127:quant_shifted[7:0];state<=STORE;end
    STORE:begin
     if(op==0&&!fcn&&!pos[0])pool_even[logical_lane]<=quant_value;
     if(op==3&&!fcn)logits[logical_lane*32+:32]<=acc[lane];
     if(lane+1<LANES&&logical_lane+1<n_out)begin lane<=lane+1;state<=QREAD;end
     else if(LANES==8&&!upper_group&&n_out>8)begin upper_group<=1;lane<=0;state<=PREP;end
     else begin upper_group<=0;lane<=0;state<=ADVANCE;end
    end
    ADVANCE:begin
     if((op==0||fcn)&&pos<ln-1)begin pos<=pos+1;state<=PREP;end
     else if(last_layer)begin busy<=0;done<=1;state<=IDLE;end
     else begin layer<=layer+1;lane<=0;pos<=0;bank<=!bank;state<=BIAS_REQ;end
    end
    RINIT:begin sum_reduce<=0;maximum<=0;pos<=0;state<=RRD;end
    RRD:state<=RACC;
    RACC:begin sum_reduce<=sum_reduce+(bank?bq:aq);if((bank?bq:aq)>maximum)maximum<=bank?bq:aq;
     if(pos==ln-1)state<=RMAX;else begin pos<=pos+1;state<=RRD;end end
    RMAX:state<=RMEAN;
    RMEAN:if(reduce_oc<n_out-1)begin reduce_oc<=reduce_oc+1;state<=RINIT;end
     else begin layer<=layer+1;lane<=0;pos<=0;bank<=!bank;state<=BIAS_REQ;end
    default:begin busy<=0;fault<=1;state<=IDLE;end
   endcase
  end
 end
endmodule

// Retain the synchronous ROM boundary: otherwise synthesis can absorb q into
// the downstream DSP input register and map the remaining ROM to LUT logic.
(* keep_hierarchy="yes" *) module tg_ai_p16_weight_rom #(parameter FILE="",parameter DEPTH=2048)(
 input logic clk,en,input logic[$clog2(DEPTH)-1:0]addr,output logic signed[7:0]q);
 (* ram_style="block",rom_style="block" *)logic[7:0]mem[0:DEPTH-1];
 initial $readmemh(FILE,mem);
 always_ff @(posedge clk)if(en)q<=mem[addr];
endmodule
