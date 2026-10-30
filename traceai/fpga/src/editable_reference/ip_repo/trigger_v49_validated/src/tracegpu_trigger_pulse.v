`timescale 1ns/1ps
module tracegpu_trigger_pulse(
 input wire clk, resetn, restart, enable, level, target_level,
 input wire [1:0] mode, input wire [63:0] timestamp, minimum, maximum,
 output reg hit, output reg [63:0] hit_tick, hit_start
);
 reg previous, measuring, reported;
 reg [63:0] start_tick;
 wire [63:0] width=timestamp-start_tick;
 // AXI active minimum/maximum remain constant while capture is enabled.
 // Compute unsigned 65-bit deadlines once, then compare in parallel. Carry
 // and start order preserve exact modulo-2^64 width semantics across wrap.
 reg[64:0] min_deadline,max_deadline;
 function automatic lt64(input[63:0]a,b);
  reg[3:0]lt,eq;
  begin
   for(integer k=0;k<4;k=k+1)begin lt[k]=a[k*16+:16]<b[k*16+:16];eq[k]=a[k*16+:16]==b[k*16+:16];end
   lt64=lt[3]|(eq[3]&lt[2])|(eq[3]&eq[2]&lt[1])|(eq[3]&eq[2]&eq[1]&lt[0]);
  end
 endfunction
 wire after_start=!lt64(timestamp,start_tick);
 wire min_before=lt64(timestamp,min_deadline[63:0]);
 wire max_before=lt64(timestamp,max_deadline[63:0]);
 wire less_min=min_deadline[64]?(after_start||min_before):(after_start&&min_before);
 wire less_max=max_deadline[64]?(after_start||max_before):(after_start&&max_before);
 wire equal_min=timestamp==min_deadline[63:0];
 wire equal_max=timestamp==max_deadline[63:0];
 wire greater_min=!less_min&&!equal_min;
 wire greater_max=!less_max&&!equal_max;
 wire closing=(level!=target_level);
 wire match_close=(mode==0 && less_min) || (mode==1 && greater_min) || (mode==2 && !less_min && !greater_max) || (mode==3 && (less_min || greater_max));
 wire match_open=(mode==1 && greater_min) || (mode==3 && greater_max);
 always @(posedge clk)begin
  if(!resetn)begin previous<=0;measuring<=0;reported<=0;start_tick<=0;min_deadline<=0;max_deadline<=0;hit<=0;hit_tick<=0;hit_start<=0;end
  else begin
   hit<=0;previous<=level;
   if(restart || !enable)begin measuring<=0;reported<=0;end
   else if(!measuring)begin
    if(level==target_level && previous!=target_level)begin measuring<=1;reported<=0;start_tick<=timestamp;min_deadline<={1'b0,timestamp}+{1'b0,minimum};max_deadline<={1'b0,timestamp}+{1'b0,maximum};end
   end else begin
    if(!reported && ((closing && match_close) || (!closing && match_open)))begin
     hit<=1;hit_tick<=timestamp;hit_start<=start_tick;reported<=1;
    end
    if(closing)begin measuring<=0;reported<=0;end
   end
  end
 end
endmodule
