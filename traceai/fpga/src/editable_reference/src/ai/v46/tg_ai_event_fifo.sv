`timescale 1ns/1ps
// Synchronous memory and prefetched stable output. Capacity includes prefetched item.
module tg_ai_event_fifo #(parameter ADDR_BITS=6)(
 input logic clk,resetn,clear,input logic s_valid,input logic[511:0]s_data,
 output logic s_ready,output logic m_valid,output logic[511:0]m_data,input logic m_ready,
 output logic[ADDR_BITS:0]level,output logic overflow,output logic[31:0]dropped);
 localparam DEPTH=1<<ADDR_BITS;
 (* ram_style="block" *)logic[511:0]mem[0:DEPTH-1];
 logic[ADDR_BITS-1:0]wp,rp;logic[ADDR_BITS:0]stored;
 logic pending;logic[511:0]read_q;
 wire pop=m_valid&&m_ready;
 wire fetch=(!m_valid||m_ready)&&stored!=0&&!pending;
 wire put=s_valid&&s_ready;
 always_comb begin level=stored+m_valid+pending;s_ready=level<DEPTH;end
 always_ff @(posedge clk)begin
  if(put)mem[wp]<=s_data;if(fetch)read_q<=mem[rp];
  if(!resetn||clear)begin wp<=0;rp<=0;stored<=0;pending<=0;m_valid<=0;m_data<=0;overflow<=0;dropped<=0;end
  else begin
   pending<=fetch;
   if(pop)m_valid<=0;
   if(pending)begin m_data<=read_q;m_valid<=1;end
   if(put)wp<=wp+1;if(fetch)rp<=rp+1;
   case({put,fetch})2'b10:stored<=stored+1;2'b01:stored<=stored-1;default:stored<=stored;endcase
   if(s_valid&&!s_ready)begin overflow<=1;if(dropped!=32'hffffffff)dropped<=dropped+1;end
  end
 end
endmodule
