`timescale 1ns/1ps
// Synchronous read register is intentionally unreset for true BRAM inference.
// DEPTH RAM words plus one held output word: legacy fifo_level convention.
// Full RAM conservatively rejects a simultaneous push/read, as the baseline.
module tracegpu_vector_fifo #(parameter FIFO_BITS=10)(
 input wire clk,resetn,clear,
 input wire [128:0] s_data,input wire s_valid,output wire s_ready,
 output wire [128:0] m_data,output reg m_valid,input wire m_ready,
 output wire [15:0] level
);
 localparam DEPTH=(1<<FIFO_BITS);
 (* ram_style="block" *) reg [128:0] memory[0:DEPTH-1];
 reg [128:0] read_data;
 reg [FIFO_BITS-1:0] wr_ptr,rd_ptr;
 reg [FIFO_BITS:0] count;
 wire write_enable=s_valid&&s_ready;
 wire read_enable=resetn&&!clear&&(count!=0)&&(!m_valid||m_ready);
 assign s_ready=resetn&&!clear&&(count<DEPTH);
 assign m_data=read_data;
 assign level=count+m_valid;
 always @(posedge clk)begin
  if(write_enable)memory[wr_ptr]<=s_data;
  if(read_enable)read_data<=memory[rd_ptr];
 end
 always @(posedge clk)begin
  if(!resetn||clear)begin wr_ptr<=0;rd_ptr<=0;count<=0;m_valid<=0;end
  else begin
   if(write_enable)wr_ptr<=wr_ptr+1'b1;
   if(read_enable)begin rd_ptr<=rd_ptr+1'b1;m_valid<=1;end
   else if(m_ready)m_valid<=0;
   case({write_enable,read_enable})
    2'b10:count<=count+1'b1;
    2'b01:count<=count-1'b1;
    default:;
   endcase
  end
 end
endmodule
