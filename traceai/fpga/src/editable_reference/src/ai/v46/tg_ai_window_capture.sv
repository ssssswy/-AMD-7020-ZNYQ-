`timescale 1ns/1ps
// Read-only tap. One protected window; overload is explicit, never capture backpressure.
module tg_ai_window_capture(
 input logic clk,resetn,clear,armed,capture_start,capture_run,input_ready,
 input logic[63:0]timestamp,input logic[7:0]levels,capture_mask,
 input logic[31:0]period_ticks,epoch,config_generation,
 output logic window_valid,input logic window_take,window_release,
 output logic[63:0]window_origin,window_end,
 output logic[31:0]window_epoch,window_generation,window_period,window_id,
 output logic[7:0]window_mask,
 input logic rd_en,input logic[9:0]rd_offset,output logic rd_valid,output logic[7:0]rd_data,
 output logic[63:0]samples_seen,samples_dropped,windows_skipped,
 output logic active,locked,overflow,config_fault);
 (* ram_style="block" *)logic[7:0]mem[0:2047];
 logic[10:0]write_idx,window_base;logic[9:0]primed;logic[7:0]hop;
 logic[31:0]downcounter,active_period,active_epoch,active_generation;
 logic[7:0]active_mask;logic dropping;logic[31:0]next_id;
 wire free_window=!locked||window_release;
 wire[10:0]distance=write_idx-window_base;
 wire collision=locked&&!window_release&&distance<768;
 wire sample_now=active&&capture_run&&armed&&downcounter==0;
 wire due=primed==767||(primed==768&&hop==255);
 wire[10:0]read_index=window_base+{1'b0,rd_offset};
 logic mem_we;logic[10:0]mem_addr;logic[7:0]mem_data;
 // One explicit synchronous write port. Separate seed/normal writes in the
 // FSM can prevent BRAM inference and expand this ring into 16,384 FFs.
 always_comb begin
  mem_we=0;mem_addr=0;mem_data=levels&active_mask;
  if(resetn&&!clear)begin
   if(capture_start&&armed)begin
    if(input_ready&&period_ticks!=0&&capture_mask!=0&&!locked)begin mem_we=1;mem_data=levels&capture_mask;end
   end else if(active&&capture_run&&armed&&downcounter==0&&!((dropping&&!window_release)||collision))begin
    mem_we=1;mem_addr=dropping&&window_release ? 0 : write_idx;
   end
  end
 end
 always_ff @(posedge clk)begin
  if(mem_we)mem[mem_addr]<=mem_data;
  rd_valid<=rd_en&&locked&&rd_offset<768&&resetn&&!clear;
  if(rd_en&&locked&&rd_offset<768)rd_data<=mem[read_index];
  if(!resetn||clear)begin
   window_valid<=0;window_origin<=0;window_end<=0;window_epoch<=0;window_generation<=0;
   window_period<=0;window_id<=0;window_mask<=0;window_base<=0;write_idx<=0;primed<=0;hop<=0;
   downcounter<=0;active_period<=0;active_epoch<=0;active_generation<=0;active_mask<=0;
   samples_seen<=0;samples_dropped<=0;windows_skipped<=0;active<=0;locked<=0;overflow<=0;config_fault<=0;dropping<=0;next_id<=1;rd_valid<=0;
  end else begin
   if(window_valid&&window_take)window_valid<=0;
   if(window_release)begin locked<=0;window_valid<=0;if(dropping)begin dropping<=0;primed<=0;hop<=0;write_idx<=0;end end
   if(active&&(!capture_run||!armed))active<=0;
   if(capture_start&&armed)begin
    if(!input_ready||!period_ticks||capture_mask==0||locked)begin config_fault<=1;active<=0;end
    else begin
     active<=1;active_period<=period_ticks;active_epoch<=epoch;active_generation<=config_generation;active_mask<=capture_mask;
     downcounter<=period_ticks-1;primed<=1;hop<=0;write_idx<=1;
     samples_seen<=samples_seen+1;dropping<=0;
    end
   end else if(active&&capture_run&&armed)begin
    if(downcounter!=0)downcounter<=downcounter-1;
    else begin
     downcounter<=active_period-1;samples_seen<=samples_seen+1;
     if((dropping&&!window_release)||collision)begin
      samples_dropped<=samples_dropped+1;overflow<=1;dropping<=1;
      if(due)begin windows_skipped<=windows_skipped+1;hop<=0;end else if(primed==768)hop<=hop+1;
     end else if(dropping&&window_release)begin
      // Restart priming after the protected reader releases; never bridge the gap.
      write_idx<=1;primed<=1;hop<=0;dropping<=0;
     end else begin
      write_idx<=write_idx+1;
      if(primed<768)primed<=primed+1;
      if(due)begin
       hop<=0;
       if(free_window)begin
        window_valid<=1;locked<=1;window_base<=write_idx-11'd767;
        window_end<=timestamp;window_origin<=timestamp-64'd767*active_period;
        window_epoch<=active_epoch;window_generation<=active_generation;window_period<=active_period;window_mask<=active_mask;
        window_id<=next_id;next_id<=next_id+1;
       end else windows_skipped<=windows_skipped+1;
      end else if(primed==768)hop<=hop+1;
     end
    end
   end
  end
 end
endmodule
