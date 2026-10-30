// Vivado BD requires a Verilog (not SystemVerilog) module-reference top.
// This layer is wires only; maintained AXI and CNN implementations stay separate.
module tg_ai_axi_bd #(parameter ROM_DIR="model/r11") (
 (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 clk CLK", X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF s_axi, ASSOCIATED_RESET resetn, FREQ_HZ 100000000" *)
 input wire clk,
 (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 resetn RST", X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *)
 input wire resetn,
 input wire[15:0]s_axi_awaddr,input wire[2:0]s_axi_awprot,input wire s_axi_awvalid,
 output wire s_axi_awready,input wire[31:0]s_axi_wdata,input wire[3:0]s_axi_wstrb,
 input wire s_axi_wvalid,output wire s_axi_wready,output wire[1:0]s_axi_bresp,
 output wire s_axi_bvalid,input wire s_axi_bready,input wire[15:0]s_axi_araddr,
 input wire[2:0]s_axi_arprot,input wire s_axi_arvalid,output wire s_axi_arready,
 output wire[31:0]s_axi_rdata,output wire[1:0]s_axi_rresp,output wire s_axi_rvalid,
 input wire s_axi_rready
);
 tg_ai_axi #(.ROM_DIR(ROM_DIR)) impl(
  .clk(clk),.resetn(resetn),.s_axi_awaddr(s_axi_awaddr),.s_axi_awprot(s_axi_awprot),
  .s_axi_awvalid(s_axi_awvalid),.s_axi_awready(s_axi_awready),.s_axi_wdata(s_axi_wdata),
  .s_axi_wstrb(s_axi_wstrb),.s_axi_wvalid(s_axi_wvalid),.s_axi_wready(s_axi_wready),
  .s_axi_bresp(s_axi_bresp),.s_axi_bvalid(s_axi_bvalid),.s_axi_bready(s_axi_bready),
  .s_axi_araddr(s_axi_araddr),.s_axi_arprot(s_axi_arprot),.s_axi_arvalid(s_axi_arvalid),
  .s_axi_arready(s_axi_arready),.s_axi_rdata(s_axi_rdata),.s_axi_rresp(s_axi_rresp),
  .s_axi_rvalid(s_axi_rvalid),.s_axi_rready(s_axi_rready));
endmodule
