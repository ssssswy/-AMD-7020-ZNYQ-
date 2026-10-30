`timescale 1ns/1ps
module tracegpu_trigger_pulse(
 input wire clk, resetn, restart, enable, level, target_level,
 input wire [1:0] mode, input wire [63:0] timestamp, minimum, maximum,
 output reg hit, output reg [63:0] hit_tick, hit_start
);
 reg previous, measuring, reported;
 reg [63:0] start_tick;
 wire [63:0] width=timestamp-start_tick;
 wire closing=(level!=target_level);
 wire match_close=(mode==0 && width<minimum) || (mode==1 && width>minimum) || (mode==2 && width>=minimum && width<=maximum) || (mode==3 && (width<minimum || width>maximum));
 wire match_open=(mode==1 && width>minimum) || (mode==3 && width>maximum);
 always @(posedge clk)begin
  if(!resetn)begin previous<=0;measuring<=0;reported<=0;start_tick<=0;hit<=0;hit_tick<=0;hit_start<=0;end
  else begin
   hit<=0;previous<=level;
   if(restart || !enable)begin measuring<=0;reported<=0;end
   else if(!measuring)begin
    if(level==target_level && previous!=target_level)begin measuring<=1;reported<=0;start_tick<=timestamp;end
   end else begin
    if(!reported && ((closing && match_close) || (!closing && match_open)))begin
     hit<=1;hit_tick<=timestamp;hit_start<=start_tick;reported<=1;
    end
    if(closing)begin measuring<=0;reported<=0;end
   end
  end
 end
endmodule
