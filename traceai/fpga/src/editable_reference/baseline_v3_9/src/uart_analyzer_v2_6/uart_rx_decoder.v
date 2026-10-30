`timescale 1ns / 1ps
module uart_rx_decoder(
 input wire clk,input wire resetn,input wire clear,input wire enable,input wire sample_tick,input wire uart_rx,
 input wire [3:0] data_bits,input wire [1:0] parity_mode,input wire two_stop_bits,
 output reg start_valid,output reg byte_valid,output reg [7:0] byte_data,
 output reg framing_error,output reg parity_error,output reg false_start);
localparam ST_IDLE=3'd0,ST_START=3'd1,ST_DATA=3'd2,ST_PARITY=3'd3,ST_STOP1=3'd4,ST_STOP2=3'd5;
reg [2:0] state;reg [3:0] sample_count;reg [3:0] bit_index;reg [7:0] data_shift;reg parity_xor;reg parity_bad;reg frame_bad;
wire [3:0] effective_data_bits=(data_bits==4'd7)?4'd7:4'd8;
always @(posedge clk) begin
 if(!resetn||clear) begin state<=ST_IDLE;sample_count<=0;bit_index<=0;data_shift<=0;parity_xor<=0;parity_bad<=0;frame_bad<=0;start_valid<=0;byte_valid<=0;byte_data<=0;framing_error<=0;parity_error<=0;false_start<=0;end
 else begin
  start_valid<=0;byte_valid<=0;false_start<=0;
  if(!enable) begin state<=ST_IDLE;sample_count<=0;end else case(state)
   ST_IDLE: begin sample_count<=0;if(!uart_rx) begin state<=ST_START;sample_count<=0;end end
   ST_START: if(sample_tick) begin if(sample_count==4'd7) begin sample_count<=0;if(!uart_rx) begin state<=ST_DATA;bit_index<=0;data_shift<=0;parity_xor<=0;parity_bad<=0;frame_bad<=0;start_valid<=1;end else begin state<=ST_IDLE;false_start<=1;end end else sample_count<=sample_count+1'b1;end
   ST_DATA: if(sample_tick) begin if(sample_count==4'd15) begin sample_count<=0;data_shift[bit_index]<=uart_rx;parity_xor<=parity_xor^uart_rx;if(bit_index==(effective_data_bits-1'b1)) begin bit_index<=0;state<=(parity_mode==0)?ST_STOP1:ST_PARITY;end else bit_index<=bit_index+1'b1;end else sample_count<=sample_count+1'b1;end
   ST_PARITY: if(sample_tick) begin if(sample_count==4'd15) begin sample_count<=0;if(parity_mode==2'd1) parity_bad<=(uart_rx!=parity_xor);else if(parity_mode==2'd2) parity_bad<=(uart_rx!=~parity_xor);state<=ST_STOP1;end else sample_count<=sample_count+1'b1;end
   ST_STOP1: if(sample_tick) begin if(sample_count==4'd15) begin sample_count<=0;frame_bad<=frame_bad|~uart_rx;if(two_stop_bits) state<=ST_STOP2;else begin byte_data<=data_shift;parity_error<=parity_bad;framing_error<=frame_bad|~uart_rx;byte_valid<=1;state<=ST_IDLE;end end else sample_count<=sample_count+1'b1;end
   ST_STOP2: if(sample_tick) begin if(sample_count==4'd15) begin sample_count<=0;byte_data<=data_shift;parity_error<=parity_bad;framing_error<=frame_bad|~uart_rx;byte_valid<=1;state<=ST_IDLE;end else sample_count<=sample_count+1'b1;end
   default: state<=ST_IDLE;
  endcase
 end
end
endmodule
