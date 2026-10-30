`timescale 1ns/1ps
// R5 local CNN only. Snapshot/token frontend and frozen global CNN are NOT here.
// Integer contract: binary -> 0/64, INT8 weights, INT32 bias/MAC/logits,
// ReLU, ties-away right shift, saturate 127, maxpool2, channel-major flatten.
module tg_ai_slot_core #(parameter ROM_DIR="model/r5/exports/qat_int8") (
 input logic clk,resetn,input_wr_en,input_bit,start,clear_fault,
 input logic [10:0] input_addr,
 output logic busy,done,fault,
 output logic [159:0] logits
);
 logic [7:0] a0[0:1279],a1[0:255];
 logic signed [7:0] w0[0:479],w1[0:191],wh[0:1535],wf[0:59];
 logic signed [31:0] b0[0:7],b1[0:7],bh[0:11],bf[0:4];
 logic signed [7:0] s0[0:7],s1[0:7],sh[0:11];
 logic [1279:0] valid_map;
 integer loaded,layer,oc,pos,step,ni,no,npos,nstep,ci,kk,t;
 integer av,wv,shift_value;
 logic signed [31:0] acc,total,bias_value;
 logic [7:0] rq,pool_even;
 typedef enum logic [1:0] {IDLE,INIT,MAC} state_t;
 state_t state;
 initial begin
  $readmemh({ROM_DIR,"/conv0_weight.mem"},w0);$readmemh({ROM_DIR,"/conv1_weight.mem"},w1);
  $readmemh({ROM_DIR,"/hidden_weight.mem"},wh);$readmemh({ROM_DIR,"/fc_weight.mem"},wf);
  $readmemh({ROM_DIR,"/conv0_bias.mem"},b0);$readmemh({ROM_DIR,"/conv1_bias.mem"},b1);
  $readmemh({ROM_DIR,"/hidden_bias.mem"},bh);$readmemh({ROM_DIR,"/fc_bias.mem"},bf);
  $readmemh({ROM_DIR,"/conv0_shift.mem"},s0);$readmemh({ROM_DIR,"/conv1_shift.mem"},s1);
  $readmemh({ROM_DIR,"/hidden_shift.mem"},sh);
 end
 function automatic [7:0] requant(input logic signed [31:0] v,input integer shift);
  logic signed [63:0] wide;
  begin
   wide=v;
   if(wide<0)wide=0;
   if(shift>0)wide=(wide+(64'sd1<<<(shift-1)))>>>shift;
   else if(shift<0)wide=wide<<<(-shift);
   if(wide>127)requant=127;else requant=wide[7:0];
  end
 endfunction
 always_comb begin
  ni=20;no=8;npos=64;nstep=60;bias_value=0;shift_value=0;
  av=0;wv=0;ci=0;kk=0;t=0;
  case(layer)
   0: begin
    bias_value=b0[oc];shift_value=s0[oc];ci=step/3;kk=step%3;t=pos+kk-1;
    if(t>=0 && t<64)av=a0[ci*64+t];wv=w0[oc*60+step];
   end
   1: begin
    ni=8;no=8;npos=32;nstep=24;bias_value=b1[oc];shift_value=s1[oc];
    ci=step/3;kk=step%3;t=pos+kk-1;
    if(t>=0 && t<32)av=a1[ci*32+t];wv=w1[oc*24+step];
   end
   2: begin
    ni=128;no=12;npos=1;nstep=128;bias_value=bh[oc];shift_value=sh[oc];
    av=a0[step];wv=wh[oc*128+step];
   end
   3: begin
    ni=12;no=5;npos=1;nstep=12;bias_value=bf[oc];
    av=a1[step];wv=wf[oc*12+step];
   end
   default: begin end
  endcase
  total=acc+av*wv;rq=requant(total,shift_value);
 end
 always_ff @(posedge clk) begin
  if(!resetn)begin
   state<=IDLE;busy<=0;done<=0;fault<=0;loaded<=0;valid_map<=0;
   layer<=0;oc<=0;pos<=0;step<=0;acc<=0;pool_even<=0;logits<=0;
  end else begin
   done<=0;
   if(clear_fault)fault<=0;
   if(input_wr_en)begin
    if(busy || input_addr>=1280)fault<=1;
    else begin
     a0[input_addr]<=input_bit ? 8'd64 : 8'd0;
     valid_map[input_addr]<=1;
     if(!valid_map[input_addr])loaded<=loaded+1;
    end
   end
   if(start && busy)fault<=1;
   case(state)
    IDLE: if(start)begin
     if(loaded!=1280 || input_wr_en)fault<=1;
     else begin busy<=1;valid_map<=0;loaded<=0;layer<=0;oc<=0;pos<=0;state<=INIT;end
    end
    INIT: begin acc<=bias_value;step<=0;state<=MAC;end
    MAC: begin
     acc<=total;
     if(step==nstep-1)begin
      if(layer<2)begin
       if((pos%2)==0)pool_even<=rq;
       else if(layer==0)a1[oc*32+pos/2]<=pool_even>rq ? pool_even : rq;
       else a0[oc*16+pos/2]<=pool_even>rq ? pool_even : rq;
      end else if(layer==2)a1[oc]<=rq;
      else logits[oc*32+:32]<=total;
      state<=INIT;
      if(pos==npos-1)begin
       pos<=0;
       if(oc==no-1)begin
        oc<=0;
        if(layer==3)begin busy<=0;done<=1;state<=IDLE;end
        else layer<=layer+1;
       end else oc<=oc+1;
      end else pos<=pos+1;
     end else step<=step+1;
    end
    default: begin state<=IDLE;busy<=0;fault<=1;end
   endcase
  end
 end
endmodule
