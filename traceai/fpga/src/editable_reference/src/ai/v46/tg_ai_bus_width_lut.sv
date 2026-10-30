`timescale 1ns/1ps
// Configuration-time q(L,B)=min(64,floor(32*L/B+0.5)), B in [1,255].
// Incremental remainder normalization avoids a wide combinational divider.
// Bank 0=glitch, bank 1=clock. Length >=512 always saturates for B<=255.
module tg_ai_bus_width_lut(
 input logic clk,resetn,start,input logic[7:0]glitch_budget,clock_budget,
 output logic busy,ready,done,fault,
 input logic rd_en,input logic[9:0]rd_addr,output logic rd_valid,output logic[6:0]rd_code);
 typedef enum logic[1:0]{IDLE,NORMALIZE,STORE}state_t;state_t state;
 (* ram_style="block" *)logic[6:0]lut[0:1023];
 logic bank;logic[8:0]length,rem;logic[7:0]budget,budget_clock;logic[6:0]code;
 always_ff @(posedge clk)begin
  if(state==STORE)lut[{bank,length}]<=code;
  if(rd_en&&ready)rd_code<=lut[rd_addr];
  rd_valid<=rd_en&&ready&&resetn;
  if(!resetn)begin state<=IDLE;busy<=0;ready<=0;done<=0;fault<=0;bank<=0;length<=0;rem<=0;budget<=0;budget_clock<=0;code<=0;rd_valid<=0;end
  else begin
   done<=0;if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     ready<=0;
     if(glitch_budget==0||clock_budget==0)fault<=1;
     else begin busy<=1;bank<=0;length<=0;rem<=glitch_budget>>1;budget<=glitch_budget;budget_clock<=clock_budget;code<=0;state<=STORE;end
    end
    NORMALIZE:begin
     if(code>=64)begin code<=64;state<=STORE;end
     else if(rem>=budget)begin rem<=rem-budget;code<=code+1;end
     else state<=STORE;
    end
    STORE:begin
     if(length==511)begin
      if(bank)begin busy<=0;ready<=1;done<=1;state<=IDLE;end
      else begin bank<=1;length<=0;rem<=budget_clock>>1;budget<=budget_clock;code<=0;state<=STORE;end
     end else begin length<=length+1;rem<=rem+32;state<=NORMALIZE;end
    end
    default:begin fault<=1;busy<=0;ready<=0;state<=IDLE;end
   endcase
  end
 end
endmodule
