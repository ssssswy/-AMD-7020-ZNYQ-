`timescale 1ns/1ps
// UART tap times use the capture clock and precede FIFO/DMA back-pressure.
module tracegpu_trigger_sequence(
 input wire clk, resetn, restart, enable,
 input wire [3:0] length, input wire [63:0] pattern, mask, max_gap,
 input wire byte_valid, input wire [7:0] byte_data,
 input wire break_sequence, input wire [63:0] byte_tick, frame_start,
 output reg hit, output reg [63:0] hit_tick, hit_start
);
 // pattern, mask, length and max_gap must be constant during enable.
 reg [6:0] prefix;
 wire [7:0] byte_match;
 for(genvar m=0;m<8;m=m+1)begin:byte_comparators
  assign byte_match[m]=(byte_data & mask[m*8+:8])==(pattern[m*8+:8] & mask[m*8+:8]);
 end
 // Each lane has one variable tap; the valid count guards unfilled data.
 (* shreg_extract="yes" *) reg [6:0] start_bits[0:63];
 wire [2:0] start_index=length-2;
 wire [63:0] historical_start;
 for(genvar b=0;b<64;b=b+1)begin:start_taps
  assign historical_start[b]=start_bits[b][start_index];
 end
 integer k;
 always @(posedge clk)begin
  if(resetn&&!restart&&enable&&!break_sequence&&byte_valid)begin
   for(k=0;k<64;k=k+1)start_bits[k]<={start_bits[k][5:0],frame_start[k]};
  end
 end
 reg [3:0] count;
 reg [63:0] last_tick;
 integer i,j;
 reg match_now, expired;
 always @* begin
  expired=(max_gap!=0 && count!=0 && (byte_tick-last_tick)>max_gap);
  match_now=(length>=1 && length<=8) &&
    ((length==1 && byte_match[0]) ||
     (length>1 && !expired && count>=length-1 && prefix[length-2] && byte_match[length-1]));
 end
 always @(posedge clk)begin
  if(!resetn)begin
   hit<=0;hit_tick<=0;hit_start<=0;count<=0;last_tick<=0;
   prefix<=0;
  end else begin
   hit<=0;
   if(restart || !enable || break_sequence)begin count<=0;prefix<=0;end
   else if(byte_valid)begin
    if(match_now)begin hit<=1;hit_tick<=byte_tick;hit_start<=(length==1)?frame_start:historical_start;end
    prefix[0]<=byte_match[0];
    for(i=1;i<7;i=i+1)prefix[i]<=!expired&&prefix[i-1]&&byte_match[i];
    last_tick<=byte_tick;
    if(expired)count<=1;else if(count<7)count<=count+1'b1;
   end
  end
 end
endmodule
