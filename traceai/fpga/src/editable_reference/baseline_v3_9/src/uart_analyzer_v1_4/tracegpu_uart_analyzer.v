`timescale 1ns / 1ps
module tracegpu_uart_analyzer #(parameter integer C_S_AXI_DATA_WIDTH=32,parameter integer C_S_AXI_ADDR_WIDTH=8)(
 input wire s_axi_aclk,input wire s_axi_aresetn,input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_awaddr,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire [C_S_AXI_DATA_WIDTH-1:0] s_axi_wdata,input wire [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output wire [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_araddr,input wire s_axi_arvalid,output wire s_axi_arready,
 output reg [C_S_AXI_DATA_WIDTH-1:0] s_axi_rdata,output wire [1:0] s_axi_rresp,output reg s_axi_rvalid,input wire s_axi_rready,
 input wire uart_rx,output wire irq,output reg [127:0] m_axis_tdata,output reg m_axis_tvalid,input wire m_axis_tready,output wire [15:0] m_axis_tkeep,output wire m_axis_tlast);
localparam [7:0] EVT_RAW_EDGE=8'h01,EVT_UART_BYTE=8'h02,EVT_TRIGGER=8'h03;
localparam [31:0] CORE_VERSION=32'h00010400,DEFAULT_PHASE_INCREMENT=32'd79164837;
reg capture_enable,clear_pulse,arm_pulse,flush_pulse;reg [31:0] phase_increment,format_reg,trigger_cfg,trigger_value_mask,event_enable,event_sequence;reg [15:0] packet_records,packet_record_count;reg m_axis_tlast_reg;reg [63:0] timestamp;
reg [31:0] byte_count,frame_error_count,parity_error_count,overflow_count;reg [7:0] latest_byte;reg latest_frame_error,latest_parity_error,triggered,trigger_pulse;
wire uart_rx_sync,sample_tick,edge_valid,edge_level,start_valid,byte_valid,framing_error,parity_error,false_start;wire [7:0] byte_data;
wire write_fire=s_axi_awvalid&&s_axi_wvalid&&!s_axi_bvalid;wire read_fire=s_axi_arvalid&&!s_axi_rvalid;wire stream_busy=m_axis_tvalid&&!m_axis_tready;
wire [1:0] parity_mode=format_reg[5:4];wire [3:0] configured_data_bits=format_reg[3:0];wire configured_two_stop_bits=format_reg[6];wire [1:0] trigger_mode=trigger_cfg[1:0];
wire byte_match=((byte_data&trigger_value_mask[15:8])==(trigger_value_mask[7:0]&trigger_value_mask[15:8]));
wire [15:0] effective_packet_records=(packet_records==0)?16'd1:packet_records;
wire pending_byte=byte_valid&&capture_enable&&event_enable[1];
wire pending_trigger=trigger_pulse&&event_enable[2];
wire pending_edge=edge_valid&&capture_enable&&event_enable[0];
wire [2:0] pending_count={2'd0,pending_byte}+{2'd0,pending_trigger}+{2'd0,pending_edge};
assign s_axi_awready=write_fire;assign s_axi_wready=write_fire;assign s_axi_bresp=2'b00;assign s_axi_arready=read_fire;assign s_axi_rresp=2'b00;
assign m_axis_tkeep=16'hffff;assign m_axis_tlast=m_axis_tlast_reg;assign irq=trigger_pulse|(stream_busy&&(pending_count!=0));
uart_input_sync u_sync(.clk(s_axi_aclk),.resetn(s_axi_aresetn),.uart_rx_async(uart_rx),.uart_rx(uart_rx_sync));
uart_baud_nco u_nco(.clk(s_axi_aclk),.resetn(s_axi_aresetn),.clear(clear_pulse),.phase_increment(phase_increment),.sample_tick(sample_tick));
uart_edge_capture u_edge(.clk(s_axi_aclk),.resetn(s_axi_aresetn),.clear(clear_pulse),.enable(capture_enable),.uart_rx(uart_rx_sync),.edge_valid(edge_valid),.edge_level(edge_level));
uart_rx_decoder u_rx(.clk(s_axi_aclk),.resetn(s_axi_aresetn),.clear(clear_pulse),.enable(capture_enable),.sample_tick(sample_tick),.uart_rx(uart_rx_sync),.data_bits(configured_data_bits),.parity_mode(parity_mode),.two_stop_bits(configured_two_stop_bits),.start_valid(start_valid),.byte_valid(byte_valid),.byte_data(byte_data),.framing_error(framing_error),.parity_error(parity_error),.false_start(false_start));
integer lane;
always @(posedge s_axi_aclk) begin
 if(!s_axi_aresetn) begin capture_enable<=0;clear_pulse<=0;arm_pulse<=0;flush_pulse<=0;phase_increment<=DEFAULT_PHASE_INCREMENT;format_reg<=32'h8;trigger_cfg<=0;trigger_value_mask<=32'h0000ff55;event_enable<=32'h0000000f;packet_records<=16'd1024;s_axi_bvalid<=0;end
 else begin clear_pulse<=0;arm_pulse<=0;flush_pulse<=0;if(write_fire) begin case(s_axi_awaddr[7:2])
  6'h00:if(s_axi_wstrb[0]) begin capture_enable<=s_axi_wdata[0];clear_pulse<=s_axi_wdata[1];arm_pulse<=s_axi_wdata[2];flush_pulse<=s_axi_wdata[3];end
  6'h02:for(lane=0;lane<4;lane=lane+1)if(s_axi_wstrb[lane])phase_increment[lane*8+:8]<=s_axi_wdata[lane*8+:8];
  6'h03:for(lane=0;lane<4;lane=lane+1)if(s_axi_wstrb[lane])format_reg[lane*8+:8]<=s_axi_wdata[lane*8+:8];
  6'h04:for(lane=0;lane<4;lane=lane+1)if(s_axi_wstrb[lane])trigger_cfg[lane*8+:8]<=s_axi_wdata[lane*8+:8];
  6'h05:for(lane=0;lane<4;lane=lane+1)if(s_axi_wstrb[lane])trigger_value_mask[lane*8+:8]<=s_axi_wdata[lane*8+:8];
  6'h0e:begin if(s_axi_wstrb[0])packet_records[7:0]<=s_axi_wdata[7:0];if(s_axi_wstrb[1])packet_records[15:8]<=s_axi_wdata[15:8];end
  6'h0f:for(lane=0;lane<4;lane=lane+1)if(s_axi_wstrb[lane])event_enable[lane*8+:8]<=s_axi_wdata[lane*8+:8];
  default:;endcase s_axi_bvalid<=1;end else if(s_axi_bready)s_axi_bvalid<=0;end
end
always @(posedge s_axi_aclk) begin
 if(!s_axi_aresetn)begin s_axi_rvalid<=0;s_axi_rdata<=0;end else begin if(read_fire)begin case(s_axi_araddr[7:2])
  6'h00:s_axi_rdata<={29'd0,2'd0,capture_enable};6'h01:s_axi_rdata<={27'd0,false_start,uart_rx_sync,(overflow_count!=0),triggered,capture_enable};
  6'h02:s_axi_rdata<=phase_increment;6'h03:s_axi_rdata<=format_reg;6'h04:s_axi_rdata<=trigger_cfg;6'h05:s_axi_rdata<=trigger_value_mask;
  6'h06:s_axi_rdata<=byte_count;6'h07:s_axi_rdata<=frame_error_count;6'h08:s_axi_rdata<=parity_error_count;6'h09:s_axi_rdata<=overflow_count;
  6'h0a:s_axi_rdata<={22'd0,latest_parity_error,latest_frame_error,latest_byte};6'h0b:s_axi_rdata<=timestamp[31:0];6'h0c:s_axi_rdata<=timestamp[63:32];6'h0d:s_axi_rdata<=CORE_VERSION;6'h0e:s_axi_rdata<={16'd0,packet_records};6'h0f:s_axi_rdata<=event_enable;6'h10:s_axi_rdata<=event_sequence;default:s_axi_rdata<=0;endcase s_axi_rvalid<=1;end else if(s_axi_rready)s_axi_rvalid<=0;end
end
always @(posedge s_axi_aclk) begin
 if(!s_axi_aresetn)begin timestamp<=0;byte_count<=0;frame_error_count<=0;parity_error_count<=0;overflow_count<=0;latest_byte<=0;latest_frame_error<=0;latest_parity_error<=0;triggered<=0;trigger_pulse<=0;packet_record_count<=0;event_sequence<=0;m_axis_tlast_reg<=0;m_axis_tdata<=0;m_axis_tvalid<=0;end
 else begin timestamp<=timestamp+1'b1;trigger_pulse<=0;
  if(clear_pulse)begin byte_count<=0;frame_error_count<=0;parity_error_count<=0;overflow_count<=0;latest_byte<=0;latest_frame_error<=0;latest_parity_error<=0;triggered<=0;packet_record_count<=0;event_sequence<=0;m_axis_tlast_reg<=0;m_axis_tvalid<=0;end
  else begin
   if(arm_pulse)begin triggered<=0;if(trigger_mode==0)begin triggered<=1;trigger_pulse<=1;end end
   else if(!triggered&&capture_enable&&((trigger_mode==1&&start_valid)||(trigger_mode==2&&byte_valid&&byte_match)||(trigger_mode==3&&byte_valid&&(framing_error||parity_error))))begin triggered<=1;trigger_pulse<=1;end
   if(byte_valid&&capture_enable)begin byte_count<=byte_count+1'b1;latest_byte<=byte_data;latest_frame_error<=framing_error;latest_parity_error<=parity_error;if(framing_error)frame_error_count<=frame_error_count+1'b1;if(parity_error)parity_error_count<=parity_error_count+1'b1;end
   if(m_axis_tvalid&&m_axis_tready)m_axis_tvalid<=0;
   if(!stream_busy)begin
    if(pending_byte)begin m_axis_tdata<={EVT_UART_BYTE,6'd0,parity_error,framing_error,byte_data,event_sequence,8'd0,timestamp};m_axis_tvalid<=1;m_axis_tlast_reg<=(packet_record_count==effective_packet_records-1'b1);event_sequence<=event_sequence+1'b1;if(packet_record_count==effective_packet_records-1'b1)packet_record_count<=0;else packet_record_count<=packet_record_count+1'b1;end
    else if(pending_trigger)begin m_axis_tdata<={EVT_TRIGGER,8'd0,8'd0,event_sequence,8'd0,timestamp};m_axis_tvalid<=1;m_axis_tlast_reg<=(packet_record_count==effective_packet_records-1'b1);event_sequence<=event_sequence+1'b1;if(packet_record_count==effective_packet_records-1'b1)packet_record_count<=0;else packet_record_count<=packet_record_count+1'b1;end
    else if(pending_edge)begin m_axis_tdata<={EVT_RAW_EDGE,7'd0,edge_level,8'd0,event_sequence,8'd0,timestamp};m_axis_tvalid<=1;m_axis_tlast_reg<=(packet_record_count==effective_packet_records-1'b1);event_sequence<=event_sequence+1'b1;if(packet_record_count==effective_packet_records-1'b1)packet_record_count<=0;else packet_record_count<=packet_record_count+1'b1;end
    else if(flush_pulse)begin m_axis_tdata<={8'h04,8'd0,8'd0,event_sequence,8'd0,timestamp};m_axis_tvalid<=1;m_axis_tlast_reg<=1;event_sequence<=event_sequence+1'b1;packet_record_count<=0;end
    if(pending_count>1)overflow_count<=overflow_count+pending_count-1'b1;
   end else if(pending_count!=0)overflow_count<=overflow_count+pending_count;
  end
 end
end
endmodule
