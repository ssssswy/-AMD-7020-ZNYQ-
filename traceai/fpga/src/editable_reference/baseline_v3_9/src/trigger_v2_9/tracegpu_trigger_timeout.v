`timescale 1ns/1ps
module tracegpu_trigger_timeout(
 input wire clk, resetn, restart, enable, level,
 input wire [1:0] mode, input wire [63:0] timestamp, threshold,
 input wire good_byte, input wire [63:0] byte_tick,
 output reg hit, output reg [63:0] hit_tick, hit_start
);
 reg previous, reported;
 reg [63:0] last_activity;
 wire edge_selected=(mode==0 && level!=previous) || (mode==1 && level&&!previous) || (mode==2 && !level&&previous);
 wire activity=(mode==3)?good_byte:edge_selected;
 always @(posedge clk)begin
  if(!resetn)begin previous<=0;reported<=0;last_activity<=0;hit<=0;hit_tick<=0;hit_start<=0;end
  else begin
   previous<=level;hit<=0;
   if(restart || !enable)begin last_activity<=timestamp;reported<=0;end
   else if(activity)begin last_activity<=(mode==3)?byte_tick:timestamp;reported<=0;end
   else if(!reported && timestamp-last_activity>=threshold)begin
    hit<=1;hit_tick<=timestamp;hit_start<=last_activity;reported<=1;
   end
  end
 end
endmodule
