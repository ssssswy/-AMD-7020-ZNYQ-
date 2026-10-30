`timescale 1ns/1ps
// Passive analyzer test source: never drives external board pins.
// control: 0=mux enable, 1=start toggle, 2=CPOL, 3=CPHA, 4=LSB,
// 5=CS high, 7:6=speed (100k/500k/1M), 10:8=pattern,
// 23:16=bytes-1. Config changes are accepted only while idle.
// pattern 0: ramp/complement; 1:00/00; 2:FF/FF; 3:AA/55;
// 4: four-byte CS groups; 5: missing final CS; 6: 11 sample edges;
// 7: deliberately change MOSI on first sampling edge (AMBIG).
module tracegpu_spi_testgen_v2_5(
 input wire clk,input wire resetn,input wire [31:0] control,
 input wire [3:0] external_lines,output wire [3:0] sampled_lines,
 output wire [31:0] status);
reg [3:0] lines;
reg busy,done,seen,cpol,cpha,lsb,cs_high;
reg [2:0] pattern;
reg [9:0] half_ticks,timer;
reg [8:0] words,word_no;
reg [3:0] bit_no;
reg [2:0] state;
reg leading;
localparam IDLE=0,SELECT=1,CLOCK=2,DESELECT=3,GAP=4;
wire trigger=control[1]!=seen;
assign sampled_lines=control[0]?lines:external_lines;
assign status={16'h2501,8'd0,pattern,control[0],2'd0,done,busy};
function [1:0] value_bit;
 input [8:0] w;input [3:0] b;input [2:0] p;input order;
 reg [7:0] tx,rx;reg [3:0] ix;
 begin
  case(p)
   1:begin tx=0;rx=0;end
   2:begin tx=255;rx=255;end
   3:begin tx=8'haa;rx=8'h55;end
   default:begin tx=w[7:0];rx=~w[7:0];end
  endcase
  ix=order?b:7-b;value_bit={rx[ix],tx[ix]};
 end
endfunction
always @(posedge clk)begin
 if(!resetn)begin lines<=4'b1000;busy<=0;done<=0;seen<=0;cpol<=0;cpha<=0;lsb<=0;cs_high<=0;pattern<=0;half_ticks<=500;timer<=0;words<=256;word_no<=0;bit_no<=0;state<=IDLE;leading<=1;end
 else begin
  seen<=control[1];
  if(!control[0])begin busy<=0;done<=0;state<=IDLE;lines<={!control[5],2'b00,control[2]};end
  else if(!busy)begin
   // Preserve the asserted CS for the missing-end injection until source off
   // or a new trigger. This makes the incomplete transaction observable.
   if(!(done&&pattern==5))lines<={!control[5],2'b00,control[2]};
   if(trigger)begin
    busy<=1;done<=0;cpol<=control[2];cpha<=control[3];lsb<=control[4];cs_high<=control[5];pattern<=control[10:8];
    half_ticks<=(control[7:6]==0)?500:(control[7:6]==1)?100:50;
    timer<=(control[7:6]==0)?499:(control[7:6]==1)?99:49;
    words<={1'b0,control[23:16]}+1'b1;word_no<=0;bit_no<=0;leading<=1;state<=SELECT;
    lines<={!control[5],2'b00,control[2]};
   end
  end else if(timer!=0)timer<=timer-1'b1;
  else begin
   timer<=half_ticks-1'b1;
   case(state)
    SELECT:begin
     lines[3]<=cs_high;lines[0]<=cpol;
     lines[2:1]<=value_bit(word_no,0,pattern,lsb);bit_no<=0;leading<=1;state<=CLOCK;
    end
    CLOCK:begin
     lines[0]<=leading?!cpol:cpol;leading<=!leading;
     if(leading)begin
      if(cpha)lines[2:1]<=value_bit(word_no,bit_no,pattern,lsb);
      if(pattern==7&&!cpha&&word_no==0&&bit_no==0)lines[1]<=!lines[1];
     end else begin
      if(pattern==7&&cpha&&word_no==0&&bit_no==0)lines[1]<=!lines[1];
      if((pattern==6&&word_no==1&&bit_no==2)||bit_no==7)begin
       bit_no<=0;
       if(word_no+1==words||(pattern==6&&word_no==1&&bit_no==2))state<=DESELECT;
       else begin
        word_no<=word_no+1'b1;
        if(pattern==4&&word_no[1:0]==3)state<=GAP;
        else if(!cpha)lines[2:1]<=value_bit(word_no+1'b1,0,pattern,lsb);
       end
      end else begin
       bit_no<=bit_no+1'b1;
       if(!cpha)lines[2:1]<=value_bit(word_no,bit_no+1'b1,pattern,lsb);
      end
     end
    end
    GAP:begin lines[3]<=!cs_high;state<=SELECT;end
    DESELECT:begin if(pattern!=5)lines[3]<=!cs_high;busy<=0;done<=1;state<=IDLE;end
    default:begin busy<=0;state<=IDLE;end
   endcase
  end
 end
end
endmodule
