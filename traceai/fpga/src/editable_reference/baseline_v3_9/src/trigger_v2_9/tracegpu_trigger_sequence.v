`timescale 1ns/1ps
// UART tap times use the capture clock and precede FIFO/DMA back-pressure.
module tracegpu_trigger_sequence(
 input wire clk, resetn, restart, enable,
 input wire [3:0] length, input wire [63:0] pattern, mask, max_gap,
 input wire byte_valid, input wire [7:0] byte_data,
 input wire break_sequence, input wire [63:0] byte_tick, frame_start,
 output reg hit, output reg [63:0] hit_tick, hit_start
);
 reg [7:0] history[0:6];
 reg [63:0] starts[0:6];
 reg [3:0] count;
 reg [63:0] last_tick;
 integer i,j;
 reg match_now, expired;
 always @* begin
  expired=(max_gap!=0 && count!=0 && (byte_tick-last_tick)>max_gap);
  match_now=(length>=1 && length<=8 && (length==1 || (!expired && count>=length-1)));
  for(j=0;j<8;j=j+1)begin
   if(j<length)begin
    if(j==length-1)begin
     if((byte_data & mask[j*8+:8])!=(pattern[j*8+:8] & mask[j*8+:8]))match_now=0;
    end else if(j<7 && length>=2)begin
     if((history[length-2-j] & mask[j*8+:8])!=(pattern[j*8+:8] & mask[j*8+:8]))match_now=0;
    end
   end
  end
 end
 always @(posedge clk)begin
  if(!resetn)begin
   hit<=0;hit_tick<=0;hit_start<=0;count<=0;last_tick<=0;
   for(i=0;i<7;i=i+1)begin history[i]<=0;starts[i]<=0;end
  end else begin
   hit<=0;
   if(restart || !enable || break_sequence)count<=0;
   else if(byte_valid)begin
    if(match_now)begin hit<=1;hit_tick<=byte_tick;hit_start<=(length==1)?frame_start:starts[length-2];end
    for(i=6;i>0;i=i-1)begin history[i]<=history[i-1];starts[i]<=starts[i-1];end
    history[0]<=byte_data;starts[0]<=frame_start;last_tick<=byte_tick;
    if(expired)count<=1;else if(count<7)count<=count+1'b1;
   end
  end
 end
endmodule
