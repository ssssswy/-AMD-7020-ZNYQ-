`timescale 1ns/1ps
// Match the existing UART frontend exactly; no extra output pipeline stage.
module tracegpu_logic_input_frontend(
 input wire clk,resetn,input wire [3:0] logic_async,
 output wire [3:0] levels,output wire input_ready
);
 reg [4:0] settle;
 assign input_ready=settle[4];
 always @(posedge clk)begin
  if(!resetn)settle<=0;
  else if(!settle[4])settle<=settle+1'b1;
 end
 genvar n;
 generate for(n=0;n<4;n=n+1)begin:lane
  tracegpu_logic_input_sync sync(.clk(clk),.resetn(resetn),.async_pin(logic_async[n]),.level(levels[n]));
 end endgenerate
endmodule
module tracegpu_logic_input_sync(input wire clk,resetn,async_pin,output reg level);
 (* ASYNC_REG="TRUE",SHREG_EXTRACT="NO" *) reg sync_ff1,sync_ff2;
 reg [2:0] history;
 always @(posedge clk)begin
  if(!resetn)begin sync_ff1<=1;sync_ff2<=1;history<=3'b111;level<=1;end
  else begin
   sync_ff1<=async_pin;sync_ff2<=sync_ff1;history<={history[1:0],sync_ff2};
   level<=(history[2]&history[1])|(history[2]&history[0])|(history[1]&history[0]);
  end
 end
endmodule
