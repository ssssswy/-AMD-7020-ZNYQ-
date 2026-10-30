// Vivado module-reference boundary. Maintained implementation remains editable SV.
module tg_ai_stream_axi_bd #(parameter ROM_DIR="model/v46_uart")(
 (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 clk CLK",X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF s_axi, ASSOCIATED_RESET resetn, FREQ_HZ 100000000" *)input wire clk,
 (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 resetn RST",X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *)input wire resetn,
 input wire[15:0]s_axi_awaddr,input wire[2:0]s_axi_awprot,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire[31:0]s_axi_wdata,input wire[3:0]s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output wire[1:0]s_axi_bresp,output wire s_axi_bvalid,input wire s_axi_bready,
 input wire[15:0]s_axi_araddr,input wire[2:0]s_axi_arprot,input wire s_axi_arvalid,output wire s_axi_arready,
 output wire[31:0]s_axi_rdata,output wire[1:0]s_axi_rresp,output wire s_axi_rvalid,input wire s_axi_rready,
 input wire capture_clear,capture_start,capture_run,capture_vector_mode,input_ready,
 input wire[63:0]timestamp,input wire[7:0]levels,capture_mask,
 output wire ai_selected,ai_trigger,output wire[63:0]ai_tick,ai_post_ticks,output wire[31:0]ai_post_limit);
 tg_ai_stream_axi #(.ROM_DIR(ROM_DIR))impl(
  .clk(clk),.resetn(resetn),.s_axi_awaddr(s_axi_awaddr),.s_axi_awprot(s_axi_awprot),.s_axi_awvalid(s_axi_awvalid),.s_axi_awready(s_axi_awready),
  .s_axi_wdata(s_axi_wdata),.s_axi_wstrb(s_axi_wstrb),.s_axi_wvalid(s_axi_wvalid),.s_axi_wready(s_axi_wready),
  .s_axi_bresp(s_axi_bresp),.s_axi_bvalid(s_axi_bvalid),.s_axi_bready(s_axi_bready),
  .s_axi_araddr(s_axi_araddr),.s_axi_arprot(s_axi_arprot),.s_axi_arvalid(s_axi_arvalid),.s_axi_arready(s_axi_arready),
  .s_axi_rdata(s_axi_rdata),.s_axi_rresp(s_axi_rresp),.s_axi_rvalid(s_axi_rvalid),.s_axi_rready(s_axi_rready),
  .capture_clear(capture_clear),.capture_start(capture_start),.capture_run(capture_run),.capture_vector_mode(capture_vector_mode),.input_ready(input_ready),
  .timestamp(timestamp),.levels(levels),.capture_mask(capture_mask),.ai_selected(ai_selected),.ai_trigger(ai_trigger),.ai_tick(ai_tick),.ai_post_ticks(ai_post_ticks),.ai_post_limit(ai_post_limit));
endmodule
