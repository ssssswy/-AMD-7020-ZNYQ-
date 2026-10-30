`timescale 1ns/1ps
// UART R2 input contract: 768 raw bits -> channel-major Q6 [24,256].
// unit = 100000000 / unit_den. No floating point, no PS per-window features.
// Twice-phase is measured in unit_den units: anchor at i-0.5 exactly.
module tg_ai_uart_global(
 input logic clk,resetn,wr,start,input logic[9:0]wr_addr,input logic wr_data,
 input logic[31:0]unit_den,input logic data8,parity_enable,parity_odd,stop2,
 output logic busy,done,fault,output logic out_valid,input logic out_ready,
 output logic[12:0]out_addr,output logic signed[7:0]out_data);
 typedef enum logic[3:0]{IDLE,DSTART,DWAIT,READ,CONTEXT,PHASE,STORE,OREAD,OSEND}state_t;
 state_t state;
 (* ram_style="block" *)logic raw[0:767];
 (* ram_style="block" *)logic[23:0]features[0:255];
 logic raw_q;logic[23:0]feature_q;
 logic[9:0]loaded,index;logic[7:0]out_pos;logic[4:0]channel;
 logic[31:0]den,phase,cphase,high_phase,phase_end,phase_reopen,feature_phase;logic[11:0]rate_code;
 logic feature_active,feature_central;logic[3:0]feature_slot;
 logic cfg_data8,cfg_parity,cfg_odd,cfg_stop2;
 logic active,candidate,previous;logic[3:0]sampled;logic parity_sum;
 wire ds=state==DSTART;logic db,dd,dz;logic[63:0]dq;logic[31:0]dr;
 tg_ai_udiv64 div(.clk,.resetn,.start(ds),.numerator(64'd12800000000+{32'b0,den[31:1]}),
  .denominator(den),.busy(db),.done(dd),.divide_zero(dz),.quotient(dq),.remainder(dr));
 logic na,nc,nprev,npx;logic[31:0]np,ncp,nh;logic[3:0]ns;
 logic[23:0]encoded;integer bits,dbits,slot;logic[31:0]fraction_phase,phase_base;
 function automatic[31:0]sample_threshold(input logic[3:0]s);
  case(s)
   0:sample_threshold=300000000;1:sample_threshold=500000000;2:sample_threshold=700000000;3:sample_threshold=900000000;
   4:sample_threshold=1100000000;5:sample_threshold=1300000000;6:sample_threshold=1500000000;default:sample_threshold=1700000000;
  endcase
 endfunction
 function automatic[31:0]end_phase(input logic d,p,s);
  case({d,p,s})
   0:end_phase=1800000000;1,2,4:end_phase=2000000000;3,5,6:end_phase=2200000000;7:end_phase=2400000000;
  endcase
 endfunction
 always_comb begin
  dbits=cfg_data8?8:7;bits=1+dbits+cfg_parity+1+cfg_stop2;
  na=active;nc=candidate;np=phase;ncp=cphase;ns=sampled;npx=parity_sum;nprev=raw_q;
  nh=raw_q ? (high_phase>=32'd100000000 ? 32'd100000000 : high_phase+den+den) : 0;
  if(na&&np>=phase_end)na=0;
  if(na&&previous&&!raw_q&&high_phase>=32'd100000000&&np>=phase_reopen)na=0;
  if(!na)begin
   if(!nc&&previous&&!raw_q&&high_phase>=32'd100000000)begin nc=1;ncp=den;end
   if(nc)begin
    if(raw_q)nc=0;
    else if(ncp>=32'd100000000)begin na=1;np=ncp;nc=0;ns=0;npx=0;end
   end
  end
  encoded=0;encoded[0]=raw_q;encoded[8]=cfg_parity;encoded[9]=cfg_stop2;
  encoded[21:10]=rate_code;encoded[22]=cfg_odd;encoded[23]=cfg_data8;
  // u>=4.5: adjacent data centers are >=4.5 samples apart. Activation is
  // near 0.5 bit, so at most ONE center can be crossed in any sample step.
  // This is equivalent to C's while loop without an eight-DSP combinational chain.
  if(na)begin
   if(ns<dbits&&np>=sample_threshold(ns))begin npx=npx^raw_q;ns=ns+1;end
  end
  slot=0;phase_base=0;
  for(integer k=0;k<13;k=k+1)if(feature_phase>=k*32'd200000000)begin slot=k;phase_base=k*32'd200000000;end
  fraction_phase=feature_phase-phase_base;
  if(feature_active&&feature_slot<bits)begin
   encoded[1]=1;encoded[2]=feature_slot==0;encoded[3]=feature_slot>=1&&feature_slot<1+dbits;
   encoded[4]=cfg_parity&&feature_slot==1+dbits;encoded[5]=feature_slot>=1+dbits+cfg_parity;
   encoded[6]=feature_central;encoded[7]=cfg_parity&&(parity_sum^cfg_odd);
  end
  out_valid=state==OSEND;out_addr={channel,out_pos};out_data=feature_q[channel]?8'sd64:8'sd0;
 end
 always_ff @(posedge clk)begin
  if(wr&&!busy&&wr_addr==loaded&&wr_addr<768)raw[wr_addr]<=wr_data;
  if(state==READ)raw_q<=raw[index];
  if(state==STORE&&index>=512)features[index-512]<=encoded;
  if(state==OREAD)feature_q<=features[out_pos];
  if(!resetn)begin
   state<=IDLE;loaded<=0;busy<=0;done<=0;fault<=0;index<=0;out_pos<=0;channel<=0;
   den<=0;rate_code<=0;cfg_data8<=0;cfg_parity<=0;cfg_odd<=0;cfg_stop2<=0;
   phase_end<=0;phase_reopen<=0;feature_phase<=0;feature_active<=0;feature_slot<=0;feature_central<=0;
   active<=0;candidate<=0;previous<=1;phase<=0;cphase<=0;high_phase<=0;sampled<=0;parity_sum<=0;
  end else begin
   done<=0;
   if(wr)begin if(busy||wr_addr!=loaded||wr_addr>=768)fault<=1;else loaded<=loaded+1;end
   if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     // Unit in [4.5,28], tested without a rounded reciprocal.
     if(fault||wr||loaded!=768||unit_den<3571429||unit_den>22222222||(!parity_enable&&parity_odd))fault<=1;
     else begin
      busy<=1;loaded<=0;den<=unit_den;cfg_data8<=data8;cfg_parity<=parity_enable;cfg_odd<=parity_odd;cfg_stop2<=stop2;
      phase_end<=end_phase(data8,parity_enable,stop2);phase_reopen<=end_phase(data8,parity_enable,stop2)-80000000;
      active<=0;candidate<=0;previous<=1;phase<=0;cphase<=0;high_phase<=0;sampled<=0;parity_sum<=0;index<=0;state<=DSTART;
     end
    end
    DSTART:state<=DWAIT;
    DWAIT:if(dd)begin if(dz||dq>4095)begin fault<=1;busy<=0;state<=IDLE;end
     else begin rate_code<=dq[11:0];state<=READ;end end
    READ:state<=CONTEXT;
    CONTEXT:begin
     active<=na;candidate<=nc;previous<=nprev;phase<=np+den+den;cphase<=ncp+den+den;
     sampled<=ns;parity_sum<=npx;high_phase<=nh;feature_phase<=np;feature_active<=na;state<=PHASE;
    end
    PHASE:begin feature_slot<=slot;feature_central<=fraction_phase>=50000000&&fraction_phase<150000000;state<=STORE;end
    STORE:begin
     if(index==767)begin out_pos<=0;channel<=0;state<=OREAD;end else begin index<=index+1;state<=READ;end
    end
    OREAD:state<=OSEND;
    OSEND:if(out_ready)begin
     if(out_pos==255)begin out_pos<=0;if(channel==23)begin done<=1;busy<=0;state<=IDLE;end else begin channel<=channel+1;state<=OREAD;end end
     else begin out_pos<=out_pos+1;state<=OREAD;end
    end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
  end
 end
endmodule
