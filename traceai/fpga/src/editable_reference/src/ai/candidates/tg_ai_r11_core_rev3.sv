`timescale 1ns/1ps
// Serial shared DSP, synchronous activation/weight/parameter memories.
module tg_ai_r11_core #(parameter ROM_DIR="model/r11")(
 input logic clk,resetn,wr,start,input logic[1:0] model_id,
 input logic[12:0] addr,input logic signed[7:0] data,
 output logic busy,done,fault,output logic[159:0] logits);
 typedef enum logic[4:0]{IDLE,NODE,INIT,RD,MUL,ACC,EMIT,WRITE,RINIT,RRD,RACC,RMAX,RMEAN,ADDR} state_t;
 state_t state;
 (* ram_style="block" *) logic[7:0] a[0:8191],b[0:8191];

 (* rom_style="block" *) logic[39:0] params[0:255];
 initial begin $readmemh({ROM_DIR,"/params.mem"},params);end
 logic[1:0] model;logic[2:0] layer;logic bank;
 logic[1:0] op;logic[5:0]n_out;logic[9:0]n_in;logic[8:0]ln;logic[3:0]kernel;logic[6:0]padding;logic[4:0]dilation;logic[14:0]wb;logic[7:0]pb;logic last_layer;
 logic[5:0]oc,ic;logic[8:0]pos;logic[3:0]ki;logic[10:0]step;logic[12:0]loaded;integer expected,read_addr,write_addr,waddr;
 logic[14:0]registered_waddr;logic[12:0]registered_raddr;logic registered_pad;
 logic[7:0] aq,bq,rq,pool_even,maximum;
 logic signed[7:0] wq;logic[39:0] pq;
 logic pad_q;logic signed[8:0] operand;
 (* use_dsp="yes" *) logic signed[16:0] product;
 logic signed[31:0] acc;
 logic write_a,write_b;logic[7:0] write_data;
 always_comb begin
  op=3;n_out=1;n_in=1;ln=1;kernel=1;padding=0;dilation=1;wb=0;pb=0;last_layer=1;
  case({model,layer})
   `include "r11_layers.vh"
  endcase
  case(model_id) 0:expected=6144;1,2:expected=1280;default:expected=384;endcase
  case(ln)
 256:read_addr=ic<<8;128:read_addr=ic<<7;64:read_addr=ic<<6;32:read_addr=ic<<5;default:read_addr=ic<<4;endcase
 if(op==0)read_addr=read_addr+pos+ki*dilation-padding;else read_addr=step;
  if(op==2)read_addr=(oc<<4)+pos;
  waddr=wb+oc*(op==0?n_in*kernel:n_in)+step;
  operand=pad_q?9'sd0:((layer==0)?$signed({(bank?bq[7]:aq[7]),(bank?bq:aq)}):$signed({1'b0,(bank?bq:aq)}));
  write_a=0;write_b=0;write_addr=0;write_data=rq;
  if(resetn)begin
   if(wr&&!busy&&addr==loaded&&addr<expected)begin write_a=1;write_addr=addr;write_data=data;end
   else if(state==WRITE && op!=3 && (op!=0||pos[0]))begin
    write_addr=op==0?oc*(ln/2)+pos/2:oc;
    write_data=op==0?(rq>pool_even?rq:pool_even):rq;
    write_a=bank;write_b=!bank;
   end else if(state==RMAX||state==RMEAN)begin
    write_addr=state==RMAX?oc:oc+n_out;
    case(ln)
     16:write_data=(acc+8)>>4;
     32:write_data=(acc+16)>>5;
     64:write_data=(acc+32)>>6;
     default:write_data=0;
    endcase
    if(state==RMAX)write_data=maximum;
    write_a=bank;write_b=!bank;
   end
  end
 end
 (* keep_hierarchy="yes" *) tg_ai_r11_weight_rom #(.FILE({ROM_DIR,"/weights.mem"})) rom(.clk(clk),.en(state==RD),.addr(registered_waddr),.q(wq));
 always_ff @(posedge clk)begin
  if(write_a)a[write_addr]<=write_data;
  if(write_b)b[write_addr]<=write_data;
  if(state==RD||state==RRD)begin aq<=a[state==RD?registered_raddr:(read_addr&8191)];bq<=b[state==RD?registered_raddr:(read_addr&8191)];end

  if(state==NODE)pq<=params[pb+oc];
 end
 function automatic[7:0] quant(input logic signed[31:0] v,input logic signed[7:0] shift);
  logic[32:0]t;
  begin
   t=v>0?{1'b0,v}:0;
   if(shift>0)t=(t+(33'd1<<(shift[4:0]-1)))>>shift[4:0];
   quant=t>127?127:t[7:0];
  end
 endfunction
 always_ff @(posedge clk)begin
  if(!resetn)begin state<=IDLE;busy<=0;done<=0;fault<=0;loaded<=0;logits<=0;model<=0;layer<=0;bank<=0;oc<=0;pos<=0;ic<=0;ki<=0;step<=0;acc<=0;product<=0;rq<=0;pool_even<=0;maximum<=0;pad_q<=0;end
  else begin
   done<=0;
   if(wr)begin if(busy||addr!=loaded||addr>=expected)fault<=1;else loaded<=loaded+1;end
   if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     if(wr||fault||loaded!=expected)fault<=1;
     else begin busy<=1;loaded<=0;logits<=0;model<=model_id;layer<=0;bank<=0;oc<=0;pos<=0;state<=NODE;end
    end
    NODE:state<=op==2?RINIT:INIT;
    INIT:begin acc<=$signed(pq[31:0]);step<=0;ic<=0;ki<=0;state<=ADDR;end
    ADDR:begin registered_waddr<=waddr;registered_raddr<=read_addr;registered_pad<=op==0&&((pos+ki*dilation<padding)||(pos+ki*dilation>=ln+padding));state<=RD;end
    RD:begin pad_q<=registered_pad;state<=MUL;end
    MUL:begin product<=operand*wq;state<=ACC;end
    ACC:begin
     acc<=acc+{{15{product[16]}},product};
     if(step==(op==0?n_in*kernel:n_in)-1)state<=EMIT;
     else begin step<=step+1;if(ki==kernel-1)begin ki<=0;ic<=ic+1;end else ki<=ki+1;state<=ADDR;end
    end
    EMIT:begin rq<=quant(acc,$signed(pq[39:32]));if(op==0&&!pos[0])pool_even<=quant(acc,$signed(pq[39:32]));if(op==3)logits[oc*32+:32]<=acc;state<=WRITE;end
    WRITE:begin
     if(op==0&&pos<ln-1)begin pos<=pos+1;state<=NODE;end
     else if(oc<n_out-1)begin oc<=oc+1;pos<=0;state<=NODE;end
     else if(last_layer)begin busy<=0;done<=1;state<=IDLE;end
     else begin layer<=layer+1;oc<=0;pos<=0;bank<=!bank;state<=NODE;end
    end
    RINIT:begin acc<=0;maximum<=0;pos<=0;state<=RRD;end
    RRD:state<=RACC;
    RACC:begin
     acc<=acc+(bank?bq:aq);if((bank?bq:aq)>maximum)maximum<=bank?bq:aq;
     if(pos==ln-1)state<=RMAX;else begin pos<=pos+1;state<=RRD;end
    end
    RMAX:state<=RMEAN;
    RMEAN:if(oc<n_out-1)begin oc<=oc+1;state<=RINIT;end else begin oc<=0;pos<=0;layer<=layer+1;bank<=!bank;state<=NODE;end
    default:begin busy<=0;fault<=1;state<=IDLE;end
   endcase
  end
 end
endmodule

(* keep_hierarchy="yes" *) module tg_ai_r11_weight_rom #(parameter FILE="model/r11/weights.mem")(
input logic clk,en,input logic[14:0]addr,output logic signed[7:0]q);
(* ram_style="block",rom_style="block" *) logic[7:0]mem[0:32767];
initial $readmemh(FILE,mem);
always_ff @(posedge clk)if(en)q<=mem[addr];
endmodule
