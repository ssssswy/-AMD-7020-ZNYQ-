`timescale 1ns/1ps
// Configuration-time radix-2 divider, 64 clocks. No combinational divide operator.
module tg_ai_udiv64(input logic clk,resetn,start,input logic[63:0]numerator,input logic[31:0]denominator,
 output logic busy,done,divide_zero,output logic[63:0]quotient,output logic[31:0]remainder);
 logic[63:0]bits;logic[31:0]divisor;logic[32:0]rem;logic[6:0]left;
 wire[32:0]trial={rem[31:0],bits[63]};wire subtract=trial>={1'b0,divisor};
 always_ff @(posedge clk)begin
  if(!resetn)begin busy<=0;done<=0;divide_zero<=0;quotient<=0;remainder<=0;bits<=0;divisor<=0;rem<=0;left<=0;end
  else begin
   done<=0;
   if(start&&!busy)begin
    divide_zero<=denominator==0;quotient<=0;remainder<=0;
    if(denominator==0)begin done<=1;busy<=0;end
    else begin bits<=numerator;divisor<=denominator;rem<=0;left<=64;busy<=1;end
   end else if(busy)begin
    bits<={bits[62:0],1'b0};rem<=subtract?trial-{1'b0,divisor}:trial;
    quotient<={quotient[62:0],subtract};left<=left-1;
    if(left==1)begin remainder<=subtract?trial-{1'b0,divisor}:trial;busy<=0;done<=1;end
   end
  end
 end
endmodule
