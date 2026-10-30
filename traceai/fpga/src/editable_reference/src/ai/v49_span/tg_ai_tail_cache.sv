`timescale 1ns/1ps
// Timestamped selected-line changes, independent of the uniform live grid.
// END is timestamp-1 on capture_run falling, matching tracegpu_vector_capture.
// The cache reconstructs an exact end-aligned 768-point window or rejects it.
module tg_ai_tail_cache #(parameter ADDR_BITS=10)(
 input logic clk,resetn,clear,armed,capture_start,capture_run,
 input logic[63:0]timestamp,input logic[7:0]levels,selected_mask,input logic[31:0]period_ticks,epoch,config_generation,
 output logic pending,input logic replay_start,discard,output logic busy,done,rejected,
 output logic uncovered,output logic[7:0]reject_code,
 output logic[63:0]tail_origin,tail_end,output logic[31:0]tail_period,tail_epoch,tail_generation,output logic[7:0]tail_mask,
 output logic out_valid,input logic out_ready,output logic[9:0]out_index,output logic[7:0]out_data);
 localparam DEPTH=1<<ADDR_BITS;
 typedef enum logic[2:0]{IDLE,FIRST_REQ,FIRST_WAIT,NEXT_REQ,NEXT_WAIT,EMIT}state_t;state_t state;
 (* ram_style="block" *)logic[71:0]edges[0:DEPTH-1];logic[71:0]edge_q;
 logic[ADDR_BITS-1:0]wp,oldest,rp;logic[ADDR_BITS:0]count,unread;
 logic active;logic[63:0]capture_origin,next_tick,target_tick;logic[41:0]active_span;logic[31:0]active_period,active_epoch,active_generation;
 logic[7:0]mask,previous,current_level,next_level;logic next_valid;
 logic mem_we;logic[ADDR_BITS-1:0]mem_addr;logic[71:0]mem_data;
 always_comb begin
  mem_we=0;mem_addr=wp;mem_data={timestamp,levels&mask};
  if(resetn&&!clear&&armed)begin
   if(capture_start&&!busy&&selected_mask!=0&&period_ticks!=0)begin mem_we=1;mem_addr=0;mem_data={timestamp,levels&selected_mask};end
   else if(active&&capture_run&&(levels&mask)!=previous)mem_we=1;
  end
  out_valid=resetn&&!clear&&armed&&state==EMIT&&(!next_valid||$signed(next_tick-target_tick)>0);out_data=current_level;
 end
 always_ff @(posedge clk)begin
  if(mem_we)edges[mem_addr]<=mem_data;
  if(state==FIRST_REQ||state==NEXT_REQ)edge_q<=edges[rp];
  if(!resetn||clear)begin
   state<=IDLE;wp<=0;oldest<=0;rp<=0;count<=0;unread<=0;active<=0;capture_origin<=0;active_period<=0;active_span<=0;active_epoch<=0;active_generation<=0;
   mask<=0;previous<=0;current_level<=0;next_level<=0;next_tick<=0;target_tick<=0;next_valid<=0;
   pending<=0;busy<=0;done<=0;rejected<=0;uncovered<=0;reject_code<=0;tail_origin<=0;tail_end<=0;
   tail_period<=0;tail_epoch<=0;tail_generation<=0;tail_mask<=0;out_index<=0;
  end else begin
   done<=0;rejected<=0;
   if(discard&&!busy)pending<=0;
   if(capture_start&&armed)begin
    if(busy||selected_mask==0||period_ticks==0)begin rejected<=1;uncovered<=1;reject_code<=1;end
    else begin
     wp<=1;oldest<=0;count<=1;active<=1;capture_origin<=timestamp;active_period<=period_ticks;active_span<=42'd767*period_ticks;active_epoch<=epoch;active_generation<=config_generation;
     mask<=selected_mask;previous<=levels&selected_mask;pending<=0;uncovered<=0;reject_code<=0;
    end
   end else if(active)begin
    if(!armed)begin active<=0;pending<=0;end
    else if(!capture_run)begin
     active<=0;tail_end<=timestamp-1;tail_period<=active_period;tail_epoch<=active_epoch;tail_generation<=active_generation;tail_mask<=mask;
     if(timestamp-1-capture_origin<{22'b0,active_span})begin pending<=0;rejected<=1;uncovered<=1;reject_code<=2;end
     else begin tail_origin<=timestamp-1-{22'b0,active_span};pending<=1;end
    end else if(mem_we)begin
     previous<=levels&mask;wp<=wp+1;if(count==DEPTH)oldest<=oldest+1;else count<=count+1;
    end
   end
   case(state)
    IDLE:if(replay_start)begin
     if(!pending||active||!armed||count==0)begin rejected<=1;uncovered<=1;reject_code<=3;end
     else begin busy<=1;rp<=oldest;unread<=count;out_index<=0;target_tick<=tail_origin;next_valid<=0;state<=FIRST_REQ;end
    end
    FIRST_REQ:state<=FIRST_WAIT;
    FIRST_WAIT:begin
     if($signed(edge_q[71:8]-tail_origin)>0)begin busy<=0;pending<=0;rejected<=1;uncovered<=1;reject_code<=4;state<=IDLE;end
     else begin current_level<=edge_q[7:0];rp<=rp+1;unread<=unread-1;state<=NEXT_REQ;end
    end
    NEXT_REQ:if(unread==0)begin next_valid<=0;state<=EMIT;end else state<=NEXT_WAIT;
    NEXT_WAIT:begin next_tick<=edge_q[71:8];next_level<=edge_q[7:0];next_valid<=1;rp<=rp+1;unread<=unread-1;state<=EMIT;end
    EMIT:begin
     if(next_valid&&$signed(next_tick-target_tick)<=0)begin current_level<=next_level;next_valid<=0;state<=NEXT_REQ;end
     else if(out_ready)begin
      if(out_index==767)begin busy<=0;pending<=0;done<=1;state<=IDLE;end
      else begin out_index<=out_index+1;target_tick<=target_tick+tail_period;end
     end
    end
    default:begin busy<=0;pending<=0;rejected<=1;uncovered<=1;reject_code<=5;state<=IDLE;end
   endcase
   if(!armed&&(busy||pending))begin state<=IDLE;pending<=0;busy<=0;rejected<=1;uncovered<=1;reject_code<=6;end
   if(discard)begin state<=IDLE;pending<=0;busy<=0;end
  end
 end
endmodule
