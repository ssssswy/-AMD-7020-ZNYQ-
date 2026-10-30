`timescale 1ns/1ps
module tracegpu_uart_event_merge #(parameter integer PACKET_RECORDS=256)(
 input wire aclk,input wire aresetn,
 input wire [127:0] s0_axis_tdata,input wire s0_axis_tvalid,output wire s0_axis_tready,input wire s0_axis_tlast,
 input wire [127:0] s1_axis_tdata,input wire s1_axis_tvalid,output wire s1_axis_tready,input wire s1_axis_tlast,
 input wire [127:0] s2_axis_tdata,input wire s2_axis_tvalid,output wire s2_axis_tready,input wire s2_axis_tlast,
 input wire [127:0] s3_axis_tdata,input wire s3_axis_tvalid,output wire s3_axis_tready,input wire s3_axis_tlast,
 output wire [127:0] m_axis_tdata,output wire m_axis_tvalid,input wire m_axis_tready,
 output wire [15:0] m_axis_tkeep,output wire m_axis_tlast
);
tracegpu_uart_event_merge_n #(.CHANNELS(4),.PACKET_RECORDS(PACKET_RECORDS)) core(
 .aclk(aclk),.aresetn(aresetn),.s_data({s3_axis_tdata,s2_axis_tdata,s1_axis_tdata,s0_axis_tdata}),
 .s_valid({s3_axis_tvalid,s2_axis_tvalid,s1_axis_tvalid,s0_axis_tvalid}),
 .s_ready({s3_axis_tready,s2_axis_tready,s1_axis_tready,s0_axis_tready}),
 .m_axis_tdata(m_axis_tdata),.m_axis_tvalid(m_axis_tvalid),.m_axis_tready(m_axis_tready),
 .m_axis_tkeep(m_axis_tkeep),.m_axis_tlast(m_axis_tlast));
endmodule
