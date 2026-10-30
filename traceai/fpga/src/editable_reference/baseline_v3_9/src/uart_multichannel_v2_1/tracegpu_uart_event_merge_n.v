`timescale 1ns/1ps
// One registered output slot; advance fairness only when accepting an input.
module tracegpu_uart_event_merge_n #(
 parameter integer CHANNELS=4, parameter integer PACKET_RECORDS=256
)(
 input wire aclk, input wire aresetn,
 input wire [CHANNELS*128-1:0] s_data,
 input wire [CHANNELS-1:0] s_valid, output reg [CHANNELS-1:0] s_ready,
 output reg [127:0] m_axis_tdata, output reg m_axis_tvalid,
 input wire m_axis_tready, output wire [15:0] m_axis_tkeep,
 output reg m_axis_tlast
);
// The runtime channel index is not a Verilog integer: a 32-bit index makes
// round-robin wrap arithmetic part of the FIFO READY -> BRAM enable path.
// Keep one extra candidate bit so non-power-of-two CHANNELS also wrap correctly.
localparam integer CHANNEL_WIDTH = CHANNELS > 1 ? $clog2(CHANNELS) : 1;
localparam integer PACKET_WIDTH = PACKET_RECORDS > 1 ? $clog2(PACKET_RECORDS) : 1;
reg [CHANNEL_WIDTH-1:0] prefer, selected;
reg [CHANNEL_WIDTH:0] candidate;
integer scan;
reg found;
reg [PACKET_WIDTH-1:0] packet_count;
wire slot=!m_axis_tvalid||m_axis_tready;
assign m_axis_tkeep=16'hffff;
always @* begin
 selected=0;found=0;s_ready=0;candidate=0;
 for(scan=0;scan<CHANNELS;scan=scan+1)begin
  candidate=prefer+scan;
  if(candidate>=CHANNELS)candidate=candidate-CHANNELS;
  if(!found&&s_valid[candidate])begin selected=candidate;found=1;end
 end
 if(aresetn&&slot&&found)s_ready[selected]=1;
end
always @(posedge aclk)begin
 if(!aresetn)begin
  prefer<=0;packet_count<=0;m_axis_tdata<=0;m_axis_tvalid<=0;m_axis_tlast<=0;
 end else if(slot)begin
  m_axis_tvalid<=found;
  if(found)begin
   m_axis_tdata<=s_data[selected*128+:128];
   m_axis_tdata[71:64]<=selected;
   m_axis_tlast<=(packet_count>=PACKET_RECORDS-1)||(s_data[selected*128+120+:8]==8'h04);
   if((packet_count>=PACKET_RECORDS-1)||(s_data[selected*128+120+:8]==8'h04))packet_count<=0;
   else packet_count<=packet_count+1'b1;
   prefer<=selected==CHANNELS-1?0:selected+1;
  end else m_axis_tlast<=0;
 end
end
endmodule
