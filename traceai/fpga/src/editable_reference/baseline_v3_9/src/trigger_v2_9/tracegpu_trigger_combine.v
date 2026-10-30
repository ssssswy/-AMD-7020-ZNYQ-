`timescale 1ns/1ps
// One candidate set per clock; balanced reductions, subtraction and comparison
// use separate registers. Tagged event times, not processing clocks, define ALL.
module tracegpu_trigger_combine(
 input wire clk, resetn, clear, restart, enable,
 input wire [1:0] mode, input wire [63:0] window_ticks, timestamp,
 input wire [3:0] enabled, hit, levels, event_levels, level_mask, level_value,
 input wire [255:0] hit_ticks, hit_starts,
 input wire [31:0] hit_reasons, hit_data,hit_details,
 input wire [7:0] channels,
 output reg trigger_pulse, trigger_sticky,
 output reg [63:0] trigger_tick, trigger_start_tick,
 output reg [7:0] trigger_reason,
 output reg [3:0] trigger_sources, trigger_predicates, trigger_levels,
 output reg [255:0] snapshot_ticks, snapshot_starts,
 output reg [31:0] snapshot_data,snapshot_details
);
 reg [3:0] seen;
 reg [63:0] seen_tick[0:3],seen_start[0:3];
 reg [7:0] seen_reason[0:3],seen_data[0:3],seen_detail[0:3];
 reg [3:0] participants,sources;
 reg [63:0] candidate_tick[0:3],candidate_start[0:3];
 reg [255:0] selected_ticks,selected_starts;
 reg [31:0] selected_data,selected_details;
 reg [7:0] reason;
 reg found,valid;
 reg [4:0] pipe_valid,pipe_all;
 reg [595:0] metadata[0:4];
 reg [63:0] window_pipe[0:4];
 reg [63:0] lo0[0:3],hi0[0:3],start0[0:3];
 reg [63:0] lo1[0:1],hi1[0:1],start1[0:1];
 reg [63:0] lo2,hi2,start2,hi3,start3,hi4,start4,span3;
 reg accept4;
 integer i,j;
 always @*begin
  participants=(mode==0)?(hit&enabled):((mode==1)?enabled:4'b0);
  sources=0;reason=0;found=0;
  selected_ticks=0;selected_starts=0;selected_data=0;selected_details=0;
  for(j=0;j<4;j=j+1)begin
   candidate_tick[j]=hit[j]?hit_ticks[j*64+:64]:seen_tick[j];
   candidate_start[j]=hit[j]?hit_starts[j*64+:64]:seen_start[j];
   if(participants[j])begin
    sources[channels[j*2+:2]]=1;
    if(!found)begin reason=hit[j]?hit_reasons[j*8+:8]:seen_reason[j];found=1;end
    selected_ticks[j*64+:64]=candidate_tick[j];selected_starts[j*64+:64]=candidate_start[j];
    selected_data[j*8+:8]=hit[j]?hit_data[j*8+:8]:seen_data[j];
    selected_details[j*8+:8]=hit[j]?hit_details[j*8+:8]:seen_detail[j];
   end
  end
  valid=(hit&enabled)!=0;
  if(mode==1)valid=valid&&enabled!=0&&((seen|hit)&enabled)==enabled;
  valid=valid&&((event_levels&level_mask)==(level_value&level_mask));
  if(mode==2)begin
   valid=(levels&level_mask)==(level_value&level_mask);
   reason=11;sources=level_mask;
  end
 end
 always @(posedge clk)begin
  // Valid bits guard unreset data registers: lower reset fanout/control sets.
  metadata[0]<={selected_ticks,selected_starts,selected_data,selected_details,reason,sources,participants,(mode==2)?levels:event_levels};
  window_pipe[0]<=window_ticks;
  for(i=0;i<4;i=i+1)begin
   lo0[i]<=participants[i]?candidate_tick[i]:64'hffffffffffffffff;
   hi0[i]<=participants[i]?candidate_tick[i]:64'd0;
   start0[i]<=participants[i]?candidate_start[i]:64'hffffffffffffffff;
  end
  if(mode==2)begin lo0[0]<=timestamp;hi0[0]<=timestamp;start0[0]<=timestamp;end
  for(i=0;i<2;i=i+1)begin
   lo1[i]<=(lo0[i*2]<lo0[i*2+1])?lo0[i*2]:lo0[i*2+1];
   hi1[i]<=(hi0[i*2]>hi0[i*2+1])?hi0[i*2]:hi0[i*2+1];
   start1[i]<=(start0[i*2]<start0[i*2+1])?start0[i*2]:start0[i*2+1];
  end
  lo2<=lo1[0]<lo1[1]?lo1[0]:lo1[1];
  hi2<=hi1[0]>hi1[1]?hi1[0]:hi1[1];
  start2<=start1[0]<start1[1]?start1[0]:start1[1];
  span3<=hi2-lo2;hi3<=hi2;start3<=start2;
  accept4<=!pipe_all[3]||(span3<=window_pipe[3]);hi4<=hi3;start4<=start3;
  for(i=1;i<5;i=i+1)begin metadata[i]<=metadata[i-1];window_pipe[i]<=window_pipe[i-1];end
  if(!resetn||clear||restart)begin
   seen<=0;pipe_valid<=0;pipe_all<=0;
   trigger_pulse<=0;trigger_sticky<=0;trigger_tick<=0;trigger_start_tick<=0;
   trigger_reason<=0;trigger_sources<=0;trigger_predicates<=0;trigger_levels<=0;
   snapshot_ticks<=0;snapshot_starts<=0;snapshot_data<=0;snapshot_details<=0;
   for(i=0;i<4;i=i+1)begin seen_tick[i]<=0;seen_start[i]<=0;seen_reason[i]<=0;seen_data[i]<=0;seen_detail[i]<=0;end
  end else begin
   trigger_pulse<=0;
   pipe_valid<={pipe_valid[3:0],valid&&enable&&!trigger_sticky};
   pipe_all<={pipe_all[3:0],mode==1};
   if(!enable)pipe_valid<=0;
   if(enable&&!trigger_sticky)begin
    // Last observations suffice: window comparison rejects stale entries
    // without a long combinational expiry feedback path.
    for(i=0;i<4;i=i+1)if(hit[i]&&enabled[i])begin
     seen[i]<=1;seen_tick[i]<=hit_ticks[i*64+:64];seen_start[i]<=hit_starts[i*64+:64];
     seen_reason[i]<=hit_reasons[i*8+:8];seen_data[i]<=hit_data[i*8+:8];seen_detail[i]<=hit_details[i*8+:8];
    end
    if(pipe_valid[4]&&accept4)begin
     trigger_sticky<=1;trigger_pulse<=1;trigger_tick<=hi4;trigger_start_tick<=start4;
     {snapshot_ticks,snapshot_starts,snapshot_data,snapshot_details,trigger_reason,trigger_sources,trigger_predicates,trigger_levels}<=metadata[4];
    end
   end
  end
 end
endmodule
