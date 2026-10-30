`timescale 1ns/1ps
// Configuration-time rational geometry. Tables are generated in PL, never
// supplied per window by ARM. Numerator is the 100 MHz sampling timebase.
module tg_ai_uart_geometry(
 input logic clk,resetn,start,input logic[31:0]unit_den,
 input logic data8,parity_enable,stop2,output logic busy,done,fault,
 output logic table_valid,output logic[4:0]table_addr,
 output logic[15:0]table_q,output logic[31:0]table_rem);
 typedef enum logic[2:0]{IDLE,PREP,SUBTRACT,ADJUST,ISSUE,WAIT_DIV,WRITE}state_t;
 state_t state;logic[31:0]den,divisor;logic[63:0]numerator,constant_q,scaled_q,constant_value;
 logic[3:0]base_slot;logic[1:0]roles;logic[4:0]item;
 logic db,dd,dz;logic[63:0]dq;logic[31:0]dr;logic[3:0]slot;
 tg_ai_udiv64 div(.clk,.resetn,.start(state==ISSUE),.numerator,.denominator(divisor),
  .busy(db),.done(dd),.divide_zero(dz),.quotient(dq),.remainder(dr));
 always_comb begin
  slot=base_slot+(item<13?item-10:item<16?item-13:item-16);
  // All multipliers by the fixed timebase are compile-time constants. Do not
  // infer an unregistered 64-bit DSP cascade from a small item/slot selector.
  constant_value=0;
  case(item)
   0:constant_value=64'd3200000000;1:constant_value=300000000;2:constant_value=500000000;
   3:constant_value=700000000;4:constant_value=900000000;5:constant_value=1100000000;
   6:constant_value=1300000000;7:constant_value=1500000000;8:constant_value=1700000000;9:constant_value=1900000000;
   default:begin
    if(item<13)case(slot)
     8:constant_value=64'd10400000000;9:constant_value=64'd12000000000;10:constant_value=64'd13600000000;11:constant_value=64'd15200000000;
     default:constant_value=0;
    endcase
    else if(item<16)case(slot)
     8:constant_value=64'd16700000000;9:constant_value=64'd18300000000;10:constant_value=64'd19900000000;11:constant_value=64'd21500000000;
     default:constant_value=0;
    endcase
    else case(slot)
     8:constant_value=1300000000;9:constant_value=1500000000;10:constant_value=1700000000;11:constant_value=1900000000;
     default:constant_value=0;
    endcase
   end
  endcase
  table_valid=state==WRITE;table_addr=item;table_q=dq[15:0];table_rem=dr;
 end
 always_ff @(posedge clk)begin
  if(!resetn)begin state<=IDLE;busy<=0;done<=0;fault<=0;den<=0;base_slot<=0;roles<=0;item<=0;
   numerator<=0;divisor<=0;constant_q<=0;scaled_q<=0;end
  else begin
   done<=0;
   if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     if(unit_den<3571429||unit_den>22222222)fault<=1;
     else begin busy<=1;den<=unit_den;base_slot<=data8?9:8;roles<=1+stop2+parity_enable;item<=0;state<=PREP;end
    end
    PREP:begin
     constant_q<=constant_value;divisor<=item>=10&&item<16 ? den<<4 : den<<1;
     scaled_q<=item<16 ? ({32'b0,den}<<13)+({32'b0,den}<<3) : ({32'b0,den}<<10)+({32'b0,den}<<9)+den;
     state<=SUBTRACT;
    end
    SUBTRACT:begin numerator<=item<13 ? constant_q : scaled_q-constant_q;state<=ADJUST;end
    ADJUST:begin if(item>=13)numerator<=numerator+divisor-1;state<=ISSUE;end
    ISSUE:state<=WAIT_DIV;
    WAIT_DIV:if(dd)begin
     if(dz||dq>65535)begin fault<=1;busy<=0;state<=IDLE;end else state<=WRITE;
    end
    WRITE:begin
     if(item==18)begin busy<=0;done<=1;state<=IDLE;end
     else begin item<=item+1;state<=PREP;end
    end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
  end
 end
endmodule
