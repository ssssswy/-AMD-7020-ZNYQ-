`timescale 1ns/1ps
// First cause independent of display FIFO and PS polling. Trigger time is the
// current decision time; the older observation time is separately preserved.
module tg_ai_trigger_gate(
 input logic clk,resetn,clear,capture_start,capture_run,monitor_armed,config_fault,
 input logic[63:0]timestamp,input logic[31:0]epoch,generation,store_generation,
 input logic[2:0]mode,input logic[63:0]post_ticks,input logic[31:0]post_limit,
 input logic command_valid,input logic[1:0]command,
 input logic rule_valid,input logic[511:0]rule_packet,input logic cnn_valid,input logic[511:0]cnn_packet,
 input logic window_valid,input logic[63:0]window_begin,window_end,
 input logic[15:0]cnn_evidence_id,output logic[31:0]and_overwritten,input logic[4:0]read_index,output logic[31:0]read_data,status,
 output logic selected,trigger_pulse,protect_evidence,output logic[63:0]trigger_tick,
 output logic[63:0]capture_post_ticks,output logic[31:0]capture_post_limit);
 logic armed,waiting,running,fired,error_flag;logic[31:0]saved[0:31];logic[511:0]cause;
 logic take_rule,eligible;logic[63:0]arm_tick;
 logic pending,pending_rule;logic[511:0]pending_cause;logic[15:0]pending_evidence;
 logic agree_valid;logic[511:0]agree_cause;logic[255:0]agree_rule,pending_agree_rule;logic[15:0]agree_evidence;
 tg_ai_agreement agreement(.clk,.resetn,.clear(clear||capture_start),.enable(eligible&&mode==4),
  .window_valid,.window_begin,.window_end,.epoch,.generation,.cnn_valid,.cnn_packet,.cnn_evidence_id,
  .rule_valid,.rule_packet,.valid(agree_valid),.cause(agree_cause),.rule_cause(agree_rule),.evidence_id(agree_evidence),.overwritten(and_overwritten));
 wire rule_identity=rule_packet[95:64]==epoch&&rule_packet[127:96]==generation;
 wire cnn_identity=cnn_packet[95:64]==epoch&&cnn_packet[127:96]==generation;
 always_comb begin
  selected=mode!=0&&(armed||waiting||running||fired);
  eligible=armed&&running&&capture_run&&monitor_armed&&!config_fault&&!capture_start&&!clear&&!fired&&
   !(command_valid&&(command==0||command==2));
  take_rule=pending_rule;
  cause=pending_cause;
  trigger_pulse=eligible&&pending;
  protect_evidence=trigger_pulse&&!take_rule;
  trigger_tick=timestamp;
  status={27'b0,error_flag,fired,running,waiting,armed};
  read_data=read_index<32?saved[read_index]:0;
 end
 // AXI forbids identity/mode writes while capture is armed or running.
 // Qualify identities before the output stage; cancel/stop/fault still gate
 // the output in the current cycle. One additional 100 MHz clock of latency.
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin pending<=0;pending_rule<=0;end
  else begin
   pending<=eligible&&((mode==4&&agree_valid)||(mode[0]&&rule_valid&&rule_identity)||(mode[1]&&cnn_valid&&cnn_identity));
   if(eligible&&mode==4&&agree_valid)begin
    pending_rule<=0;pending_cause<=agree_cause;pending_cause[143:136]<=8'd5;pending_evidence<=agree_evidence;pending_agree_rule<=agree_rule;
   end else if(eligible&&mode[0]&&rule_valid&&rule_identity)begin
    pending_rule<=1;pending_cause<=rule_packet;pending_evidence<=0;
   end else if(eligible&&mode[1]&&cnn_valid&&cnn_identity)begin
    pending_rule<=0;pending_cause<=cnn_packet;pending_evidence<=cnn_evidence_id;
   end
  end
 end
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin
   armed<=0;waiting<=0;running<=0;fired<=0;error_flag<=0;arm_tick<=0;capture_post_ticks<=0;capture_post_limit<=0;
   for(integer i=0;i<32;i=i+1)saved[i]<=0;
  end else begin
   if(command_valid)case(command)
    0:begin armed<=0;waiting<=0;running<=0;end
    1:if(!capture_run&&monitor_armed&&mode!=0&&!config_fault&&epoch!=0&&generation!=0&&store_generation!=0)begin
     armed<=1;waiting<=1;running<=0;fired<=0;error_flag<=0;capture_post_ticks<=post_ticks;capture_post_limit<=post_limit;
     for(integer i=0;i<32;i=i+1)saved[i]<=0;
    end else error_flag<=1;
    2:begin armed<=0;waiting<=0;running<=0;fired<=0;error_flag<=0;for(integer i=0;i<32;i=i+1)saved[i]<=0;end
    default:error_flag<=1;
   endcase
   if(capture_start&&armed&&waiting)begin waiting<=0;running<=1;arm_tick<=timestamp;saved[20]<=timestamp[31:0];saved[21]<=timestamp[63:32];end
   if(running&&!capture_run&&!capture_start)begin running<=0;armed<=0;end
   if(trigger_pulse)begin
    fired<=1;armed<=0;running<=0;
    saved[0]<=32'h54523437;saved[1]<=epoch;saved[2]<=generation;saved[3]<=store_generation;
    saved[4]<=cause[159:128];saved[5]<=cause[191:160];saved[6]<=cause[223:192];saved[7]<=cause[255:224];
    saved[8]<=timestamp[31:0];saved[9]<=timestamp[63:32];saved[10]<=cause[287:256];saved[11]<=cause[319:288];
    saved[12]<=cause[351:320];saved[13]<=cause[383:352];saved[14]<=cause[415:384];saved[15]<=cause[447:416];
    saved[16]<=take_rule?0:{16'b0,pending_evidence};saved[17]<=cause[511:480];saved[18]<=1;saved[19]<=0;
    saved[20]<=arm_tick[31:0];saved[21]<=arm_tick[63:32];saved[22]<=0;saved[23]<={29'b0,mode};
    if(mode==4)begin
     for(integer j=0;j<7;j=j+1)saved[24+j]<=pending_agree_rule[j*32+:32];
     saved[31]<=and_overwritten;
    end
   end
  end
 end
endmodule
