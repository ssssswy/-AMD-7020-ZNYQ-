`timescale 1ns/1ps
// Bounded PL coalescing cache. Queue sequence (word0) is unique per message;
// physical event ID (word1) survives UPDATE messages. Word14 is hit count.
// The eight-entry cache is not a claim of unbounded event history retention.
module tg_ai_event_merge(
 input logic clk,resetn,clear,input logic s_valid,input logic[511:0]s_data,output logic s_ready,
 output logic m_valid,output logic[511:0]m_data,input logic m_ready,
 output logic busy,output logic[31:0]merged);
 typedef enum logic[3:0]{IDLE,READ,CHECK,DISTANCE,MATCH,NEXT,BUILD,WRITE,EMIT}st_t;
 st_t state;
 (* ram_style="distributed" *)logic[511:0]cache[0:7];
 logic[7:0]valid;logic[2:0]index,replace_index,write_index;
 logic[511:0]incoming,old,built;logic candidate,close_point,is_update;
 logic[63:0]distance;logic[31:0]resolution,event_id,queue_sequence,next_id,next_sequence;
 wire selected_better=incoming[143:136]==8'd4 ? incoming[191:160]<old[191:160] :
  $signed(incoming[191:160])>$signed(old[191:160]);
 assign s_ready=state==IDLE&&!clear&&resetn;
 assign m_valid=state==EMIT&&!clear&&resetn;assign m_data=built;
 assign busy=state!=IDLE;
 always_comb begin
  next_id=event_id==32'hffffffff ? 1 : event_id+1;
  next_sequence=queue_sequence==32'hffffffff ? 1 : queue_sequence+1;
 end
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin
   state<=IDLE;valid<=0;index<=0;replace_index<=0;write_index<=0;incoming<=0;old<=0;built<=0;
   candidate<=0;close_point<=0;is_update<=0;distance<=0;resolution<=0;event_id<=0;queue_sequence<=0;merged<=0;
  end else case(state)
   IDLE:if(s_valid)begin incoming<=s_data;index<=0;is_update<=0;state<=READ;end
   READ:begin old<=cache[index];state<=CHECK;end
   CHECK:begin
    candidate<=valid[index]&&incoming[159:64]==old[159:64]&&incoming[383:320]<=old[447:384]&&old[383:320]<=incoming[447:384];
    close_point<=incoming[232]&&old[232];
    resolution<=incoming[511:480]>old[511:480]?incoming[511:480]:old[511:480];
    state<=DISTANCE;
   end
   DISTANCE:begin
    distance<=incoming[319:256]>=old[319:256]?incoming[319:256]-old[319:256]:old[319:256]-incoming[319:256];
    state<=MATCH;
   end
   MATCH:if(candidate&&(!close_point||distance<={32'b0,resolution}))begin
    is_update<=1;write_index<=index;state<=BUILD;
   end else state<=NEXT;
   NEXT:if(index==7)begin write_index<=replace_index;state<=BUILD;end else begin index<=index+1'b1;state<=READ;end
   BUILD:begin
    built<=is_update&&!selected_better?old:incoming;
    built[31:0]<=next_sequence;
    built[63:32]<=is_update?old[63:32]:next_id;
    built[479:448]<=is_update ? (old[479:448]==32'hffffffff ? old[479:448] : old[479:448]+1) : 1;
    built[236]<=is_update; // word7 bit12 = UPDATE
    if(is_update)begin
     built[235]<=incoming[235]||old[235];
     built[383:320]<=incoming[383:320]<old[383:320]?incoming[383:320]:old[383:320];
     built[447:384]<=incoming[447:384]>old[447:384]?incoming[447:384]:old[447:384];
     if(merged!=32'hffffffff)merged<=merged+1;
    end else begin event_id<=next_id;replace_index<=replace_index+1'b1;end
    queue_sequence<=next_sequence;state<=WRITE;
   end
   WRITE:begin cache[write_index]<=built;valid[write_index]<=1;state<=EMIT;end
   EMIT:if(m_ready)state<=IDLE;
   default:state<=IDLE;
  endcase
 end
endmodule
