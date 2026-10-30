`timescale 1ns / 1ps
module uart_baud_nco(input wire clk,input wire resetn,input wire clear,input wire [31:0] phase_increment,output reg sample_tick);
reg [31:0] phase_accumulator; wire [32:0] phase_sum={1'b0,phase_accumulator}+{1'b0,phase_increment};
always @(posedge clk) begin
 if(!resetn||clear) begin phase_accumulator<=32'd0;sample_tick<=1'b0;end
 else begin phase_accumulator<=phase_sum[31:0];sample_tick<=phase_sum[32];end
end
endmodule
