`timescale 1ns / 1ps
module uart_input_sync(input wire clk,input wire resetn,input wire uart_rx_async,output reg uart_rx);
(* ASYNC_REG="TRUE",SHREG_EXTRACT="NO" *) reg sync_ff1,sync_ff2; reg [2:0] history;
always @(posedge clk) begin
 if(!resetn) begin sync_ff1<=1'b1;sync_ff2<=1'b1;history<=3'b111;uart_rx<=1'b1;end
 else begin sync_ff1<=uart_rx_async;sync_ff2<=sync_ff1;history<={history[1:0],sync_ff2};uart_rx<=(history[2]&history[1])|(history[2]&history[0])|(history[1]&history[0]);end
end
endmodule
