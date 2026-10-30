`timescale 1ns / 1ps
module uart_edge_capture(input wire clk,input wire resetn,input wire clear,input wire enable,input wire uart_rx,output reg edge_valid,output reg edge_level);
reg uart_rx_d;
always @(posedge clk) begin
 if(!resetn||clear) begin uart_rx_d<=1'b1;edge_valid<=1'b0;edge_level<=1'b1;end
 else begin edge_valid<=1'b0;if(enable&&(uart_rx!=uart_rx_d)) begin edge_valid<=1'b1;edge_level<=uart_rx;end uart_rx_d<=uart_rx;end
end
endmodule
