`timescale 1ns/1ps
// Select is changed only with the upstream producers and downstream FIFO held
// in clear by capture_control. This module does not switch an active packet.
module tracegpu_capture_path_select(
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF s0_axis:s1_axis:m_axis, ASSOCIATED_RESET resetn" *)
 input wire clk,input wire resetn,
 input wire select_vector,
 input wire [127:0] s0_axis_tdata,input wire [15:0] s0_axis_tkeep,
 input wire s0_axis_tvalid,input wire s0_axis_tlast,output wire s0_axis_tready,
 input wire [127:0] s1_axis_tdata,input wire [15:0] s1_axis_tkeep,
 input wire s1_axis_tvalid,input wire s1_axis_tlast,output wire s1_axis_tready,
 output wire [127:0] m_axis_tdata,output wire [15:0] m_axis_tkeep,
 output wire m_axis_tvalid,output wire m_axis_tlast,input wire m_axis_tready
);
 assign m_axis_tdata=select_vector?s1_axis_tdata:s0_axis_tdata;
 assign m_axis_tkeep=select_vector?s1_axis_tkeep:s0_axis_tkeep;
 assign m_axis_tvalid=select_vector?s1_axis_tvalid:s0_axis_tvalid;
 assign m_axis_tlast=select_vector?s1_axis_tlast:s0_axis_tlast;
 assign s0_axis_tready=m_axis_tready&&!select_vector;
 assign s1_axis_tready=m_axis_tready&&select_vector;
endmodule
