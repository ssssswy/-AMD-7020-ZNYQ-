`timescale 1ns/1ps
// Small distributed queue: each edge rule can produce one item every clock.
// Backpressure is absorbed locally, never propagated to capture inputs.
module tg_ai_rule_lane #(parameter WIDTH=163,parameter AB=3)(
 input logic clk,resetn,clear,input logic s_valid,input logic[WIDTH-1:0]s_data,
 output logic m_valid,output logic[WIDTH-1:0]m_data,input logic m_ready,
 output logic[31:0]dropped);
 localparam DEPTH=1<<AB;
 (* ram_style="distributed" *)logic[WIDTH-1:0]mem[0:DEPTH-1];
 logic[AB-1:0]head,tail;logic[AB:0]count;
 wire pop=m_valid&&m_ready;
 wire put=s_valid&&(count<DEPTH||pop);
 assign m_valid=count!=0;assign m_data=mem[head];
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin head<=0;tail<=0;count<=0;end
  else begin
   if(put)begin mem[tail]<=s_data;tail<=tail+1'b1;end
   if(pop)head<=head+1'b1;
   case({put,pop})2'b10:count<=count+1'b1;2'b01:count<=count-1'b1;default:begin end endcase
  end
  if(!resetn)dropped<=0;
  else if(s_valid&&!put&&!clear&&dropped!=32'hffffffff)dropped<=dropped+1'b1;
 end
endmodule
