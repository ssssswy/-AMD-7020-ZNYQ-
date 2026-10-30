`timescale 1ns/1ps
// R5 arithmetic unchanged. V2 ingress: strictly increasing addresses 0..1279.
module tg_ai_slot_core_sync #(parameter ROM_DIR="model/r5/sync_rom_v1") (
 input logic clk,resetn,input_wr_en,input_bit,start,clear_fault,
 input logic [10:0] input_addr,
 output logic busy,done,fault,
 output logic [159:0] logits
);
 typedef enum logic[3:0] {IDLE,NODE,INIT,RD,MUL,ACC,EMIT,WRITE} st_t;
 st_t state;
 (* ram_style="block" *) logic[7:0] a0[0:2047];
 (* ram_style="block" *) logic[7:0] a1[0:511];
 (* rom_style="block" *) logic[39:0] params[0:63];
 logic[7:0] a0q,a1q;
 logic signed[7:0] wq;
 logic[39:0] pq;
 logic[10:0] loaded,ar0,aw0,out_addr;
 logic[8:0] ar1,aw1;
 logic we0,we1,pad,pad_q;
 logic[7:0] dw0,dw1;
 logic[1:0] layer,ki;
 logic[3:0] oc,last_oc;
 logic[4:0] ic;
 logic[5:0] pos,last_pos;
 logic[7:0] step,last_step;
 logic[11:0] wbase,waddr,stride;
 logic[5:0] paddr;
 logic signed[8:0] operand_a;
 (* use_dsp="yes" *) logic signed[16:0] product;
 logic signed[31:0] acc;
 logic[7:0] rq,pool_even,result_byte;
 initial $readmemh({ROM_DIR,"/params.mem"},params);
 // Preserve the memory boundary against ROM/multiplier constant-fold expansion.
 (* keep_hierarchy="yes" *) tg_ai_weight_rom_sync #(.FILE({ROM_DIR,"/weights.mem"}))
  weight_rom(.clk(clk),.en(state==RD),.addr(waddr),.q(wq));
 // No reset on storage arrays/output registers: supported synchronous RAM/ROM templates.
 always_ff @(posedge clk) begin
  if(we0)a0[aw0]<=dw0;
  if(we1)a1[aw1]<=dw1;
  if(state==RD)begin a0q<=a0[ar0];a1q<=a1[ar1];end
  if(state==NODE)pq<=params[paddr];
 end
 function automatic[7:0] quantize(input logic signed[31:0] v,input logic[4:0] shift);
  logic[32:0] rounded;
  begin
   if(v<=0)quantize=0;
   else begin
    rounded={1'b0,v};
    if(shift!=0)rounded=(rounded+(33'd1<<(shift-1)))>>shift;
    quantize=(rounded>127)?8'd127:rounded[7:0];
   end
  end
 endfunction
 always_comb begin
  last_oc=7;last_pos=63;last_step=59;stride=60;paddr={2'b0,oc};
  ar0={ic,pos}+{9'b0,ki}-11'd1;ar1=0;
  pad=((pos==0)&&(ki==0))||((pos==last_pos)&&(ki==2));
  case(layer)
   1:begin
    last_pos=31;last_step=23;stride=24;paddr=6'd8+oc;
    ar1={1'b0,ic[2:0],5'b0}+pos+ki-9'd1;
    pad=((pos==0)&&(ki==0))||((pos==31)&&(ki==2));
   end
   2:begin last_oc=11;last_pos=0;last_step=127;stride=128;paddr=6'd16+oc;ar0={3'b0,step};pad=0;end
   3:begin last_oc=4;last_pos=0;last_step=11;stride=12;paddr=6'd28+oc;ar1={1'b0,step};pad=0;end
   default:begin end
  endcase
  waddr=wbase+step;
  operand_a=pad_q?9'sd0:$signed({1'b0,((layer==0)||(layer==2))?a0q:a1q});
  result_byte=quantize(acc,pq[36:32]);
  we0=0;we1=0;aw0=0;aw1=0;dw0=0;dw1=0;
  if(resetn)begin
   if(!busy && input_wr_en && input_addr<1280 && input_addr==loaded)begin
    we0=1;aw0=input_addr;dw0=input_bit?8'd64:8'd0;
   end else if(state==WRITE)begin
    if(layer==0 && pos[0])begin we1=1;aw1=out_addr[8:0];dw1=rq>pool_even?rq:pool_even;end
    else if(layer==1 && pos[0])begin we0=1;aw0=out_addr;dw0=rq>pool_even?rq:pool_even;end
    else if(layer==2)begin we1=1;aw1=out_addr[8:0];dw1=rq;end
   end
  end
 end
 always_ff @(posedge clk)begin
  if(!resetn)begin
   state<=IDLE;loaded<=0;busy<=0;done<=0;fault<=0;logits<=0;
   layer<=0;oc<=0;pos<=0;step<=0;ic<=0;ki<=0;wbase<=0;out_addr<=0;
   product<=0;acc<=0;pad_q<=0;rq<=0;pool_even<=0;
  end else begin
   done<=0;
   if(clear_fault)fault<=0;
   if(input_wr_en)begin
    if(busy || input_addr>=1280 || input_addr!=loaded)fault<=1;
    else loaded<=loaded+1'b1;
   end
   if(start && busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     if(loaded!=1280 || input_wr_en || (fault&&!clear_fault))fault<=1;
     else begin
      loaded<=0;busy<=1;layer<=0;oc<=0;pos<=0;out_addr<=0;wbase<=0;state<=NODE;
     end
    end
    NODE:state<=INIT;
    INIT:begin acc<=$signed(pq[31:0]);step<=0;ic<=0;ki<=0;state<=RD;end
    RD:begin pad_q<=pad;state<=MUL;end
    MUL:begin product<=operand_a*wq;state<=ACC;end
    ACC:begin
     acc<=acc+{{15{product[16]}},product};
     if(step==last_step)state<=EMIT;
     else begin
      step<=step+1'b1;
      if(ki==2)begin ki<=0;ic<=ic+1'b1;end else ki<=ki+1'b1;
      state<=RD;
     end
    end
    EMIT:begin
     rq<=result_byte;
     if(layer<2 && !pos[0])pool_even<=result_byte;
     if(layer==3)logits[{oc,5'b0}+:32]<=acc;
     state<=WRITE;
    end
    WRITE:begin
     if(layer<2 && pos[0] || layer==2)out_addr<=out_addr+1'b1;
     if(pos==last_pos)begin
      pos<=0;
      if(oc==last_oc)begin
       oc<=0;out_addr<=0;
       case(layer)
        0:begin layer<=1;wbase<=512;state<=NODE;end
        1:begin layer<=2;wbase<=768;state<=NODE;end
        2:begin layer<=3;wbase<=2304;state<=NODE;end
        3:begin busy<=0;done<=1;state<=IDLE;end
       endcase
      end else begin oc<=oc+1'b1;wbase<=wbase+stride;state<=NODE;end
     end else begin pos<=pos+1'b1;state<=NODE;end
    end
    default:begin busy<=0;fault<=1;state<=IDLE;loaded<=0;end
   endcase
  end
 end
endmodule

module tg_ai_weight_rom_sync #(parameter FILE="model/r5/sync_rom_v1/weights.mem") (
 input logic clk,en,input logic[11:0] addr,output logic signed[7:0] q
);
 (* ram_style="block",rom_style="block" *) logic[7:0] mem[0:4095];
 initial $readmemh(FILE,mem);
 always_ff @(posedge clk)if(en)q<=mem[addr];
endmodule
