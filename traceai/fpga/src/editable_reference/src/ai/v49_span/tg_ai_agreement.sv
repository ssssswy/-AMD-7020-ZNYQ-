`timescale 1ns/1ps
// Match observed time intervals, never CNN completion/arrival timestamps.
// 32 recent rule records in synchronous BRAM; one outstanding CNN window.
// Overflow can lose matches (counted); it cannot manufacture an agreement.
module tg_ai_agreement(
 input logic clk,resetn,clear,enable,window_valid,cnn_valid,rule_valid,
 input logic [63:0]window_begin,window_end,
 input logic [31:0]epoch,generation,
 input logic [511:0]cnn_packet,rule_packet,input logic[15:0]cnn_evidence_id,
 output logic valid,output logic[511:0]cause,output logic[255:0]rule_cause,
 output logic[15:0]evidence_id,output logic[31:0]overwritten);
 (* ram_style="block" *) logic[255:0]rules[0:31];
 logic[4:0]wr,scan;logic[31:0]slots;logic[255:0]read_rule;
 logic live,scanning,waiting_read;logic[511:0]cnn;logic[15:0]eid;
 logic[63:0]lo,hi;logic read_valid;
 // Rule record: identity, measured width, limit, begin, end, reserved.
 wire good_rule=rule_valid&&rule_packet[95:64]==epoch&&rule_packet[127:96]==generation&&
  rule_packet[135:128]==1&&rule_packet[143:136]==4&&rule_packet[383:320]<=rule_packet[447:384];
 wire good_cnn=cnn_valid&&cnn_packet[95:64]==epoch&&cnn_packet[127:96]==generation&&
  cnn_packet[135:128]==1&&(cnn_packet[143:136]==1||cnn_packet[143:136]==2)&&
  cnn_packet[383:320]<=cnn_packet[447:384];
 wire[255:0]incoming={32'b0,rule_packet[447:384],rule_packet[383:320],rule_packet[223:160],rule_packet[159:128]};
 function automatic logic interval_agrees(input logic[255:0]r,input logic[511:0]c);
  interval_agrees=r[31:24]==c[159:152]&&r[7:0]==1&&r[15:8]==4&&
   r[159:96]<=r[223:160]&&r[159:96]<=c[447:384]&&c[383:320]<=r[223:160];
 endfunction
 always_ff @(posedge clk)begin
  valid<=0;
  if(!resetn||clear)begin
   wr<=0;scan<=0;slots<=0;live<=0;scanning<=0;waiting_read<=0;read_valid<=0;overwritten<=0;
  end else if(!enable)begin
   wr<=0;scan<=0;slots<=0;live<=0;scanning<=0;waiting_read<=0;read_valid<=0;
  end else begin
   if(good_rule)begin
    rules[wr]<=incoming;slots[wr]<=1;wr<=wr+1'b1;
    if(slots[wr]&&overwritten!=32'hffffffff)overwritten<=overwritten+1'b1;
   end
   // Each completed normal/abnormal window supersedes the pending window.
   if(window_valid)begin live<=0;scanning<=0;waiting_read<=0;end
   if(good_cnn)begin
    cnn<=cnn_packet;eid<=cnn_evidence_id;lo<=window_begin;hi<=window_end;
    live<=1;scanning<=1;scan<=0;waiting_read<=0;
   end else if(scanning&&!window_valid)begin
    if(!waiting_read)begin read_rule<=rules[scan];read_valid<=slots[scan];waiting_read<=1;end
    else begin
     waiting_read<=0;
     if(read_valid&&live&&interval_agrees(read_rule,cnn))begin
      valid<=1;cause<=cnn;rule_cause<=read_rule;evidence_id<=eid;live<=0;scanning<=0;
     end else if(scan==31)scanning<=0;
     else scan<=scan+1'b1;
    end
   end
   // Bypass handles a delayed rule and same-clock CNN/rule arrival. A new
   // normal window invalidates old CNN before a late rule can reuse it.
   if(good_rule&&((good_cnn&&interval_agrees(incoming,cnn_packet))||(!window_valid&&live&&interval_agrees(incoming,cnn))))begin
    valid<=1;cause<=good_cnn?cnn_packet:cnn;rule_cause<=incoming;
    evidence_id<=good_cnn?cnn_evidence_id:eid;live<=0;scanning<=0;waiting_read<=0;
   end
  end
 end
endmodule
