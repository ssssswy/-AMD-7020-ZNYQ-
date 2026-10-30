`timescale 1ns/1ps
// Frozen thresholds, staged arithmetic, and canonical C IEEE754 boundary behavior.
module tg_ai_decision #(parameter ROM_FILE="model/v46_p16/global_decision.mem")(
 input logic clk,resetn,start,input logic[2:0]model_id,input logic[159:0]logits,
 input logic signed[31:0]anchor_margin,
 output logic busy,done,fault,abnormal,output logic[2:0]class_id,
 output logic signed[31:0]margin,cutoff,output logic[4:0]fraction_bits);
 typedef enum logic[2:0]{IDLE,CHOOSE,DIFF,LUT_READ,CALC,FINISH}state_t;state_t state;
 (* rom_style="block" *)logic[17:0]rom[0:4095];logic[17:0]table_q;
 initial $readmemh(ROM_FILE,rom);
 logic[2:0]mid,head;logic signed[31:0]z[0:4],am,gmax,pair02,pair34,rival;
 logic signed[32:0]difference,k;logic[32:0]distance;
 logic signed[36:0]effective_base,effective;logic signed[33:0]global_score;
 logic global_positive;logic[2:0]bus_head;
 always_comb begin
  distance=difference<0 ? -difference : difference;
  bus_head=1;for(integer i=2;i<4;i=i+1)if(z[i]>z[bus_head])bus_head=i;
 end
 function automatic logic critical_c_decision(input logic signed[31:0]m);
  // Only (d=2358,k=607) is closer to the original boundary than binary64
  // cancellation error over INT32 logits. In scaled coordinates, the C
  // expression is fl(fl(M+H)-(M-607)), H=256*log(1+exp(-2358/256)).
  // Rounding depends on the binary exponent of M+H. Integers M are exact;
  // negative powers of two cross into the previous exponent interval.
  logic[32:0]magnitude;integer exponent;
  logic[31:0]bits;
  begin
   magnitude=m<0 ? -$signed({m[31],m}) : {1'b0,m};exponent=-1;
   for(integer j=0;j<32;j=j+1)if(magnitude[j])exponent=j;
   if(m<0&&(magnitude&(magnitude-1))==0)exponent=exponent-1;
   bits=32'hb934ffff;
   critical_c_decision=m==0 ? 1'b1 : bits[exponent+1];
  end
 endfunction
 always_ff @(posedge clk)begin
  if(state==LUT_READ)table_q<=distance<4096 ? rom[distance[11:0]] : 18'd607;
  if(!resetn)begin
   state<=IDLE;mid<=0;head<=0;am<=0;busy<=0;done<=0;fault<=0;abnormal<=0;class_id<=0;margin<=0;cutoff<=0;fraction_bits<=0;
   gmax<=0;pair02<=0;pair34<=0;rival<=0;difference<=0;k<=0;effective_base<=0;effective<=0;global_score<=0;global_positive<=0;
   for(integer i=0;i<5;i=i+1)z[i]<=0;
  end else begin
   done<=0;if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     abnormal<=0;class_id<=0;margin<=0;
     if(fault||model_id>3)fault<=1;
     else begin mid<=model_id;am<=anchor_margin;for(integer i=0;i<5;i=i+1)z[i]<=$signed(logits[i*32+:32]);busy<=1;state<=CHOOSE;end
    end
    CHOOSE:begin
     gmax<=z[1]>=z[2] ? z[1] : z[2];pair02<=z[0]>=z[2] ? z[0] : z[2];pair34<=z[3]>=z[4] ? z[3] : z[4];
     head<=mid>=4 ? bus_head : mid==1 ? 1 : mid==3 ? 2 : 0;state<=DIFF;
    end
    DIFF:begin
     rival<=pair02>=pair34 ? pair02 : pair34;
     difference<=$signed({z[1][31],z[1]})-$signed({z[2][31],z[2]});
     k<=$signed({gmax[31],gmax})-$signed({z[0][31],z[0]});state<=LUT_READ;
    end
    LUT_READ:begin
     case(mid)
      1:effective_base<=($signed({{5{z[1][31]}},z[1]})-$signed({{5{rival[31]}},rival}))*8;
      2:effective_base<=$signed({{5{z[1][31]}},z[1]})-$signed({{5{z[0][31]}},z[0]});
      3:effective_base<=($signed({{5{z[1][31]}},z[1]})-$signed({{5{z[0][31]}},z[0]}))*2;
      4,5:effective_base<=$signed(z[head]);default:effective_base<=0;
     endcase
     state<=CALC;
    end
    CALC:begin
     effective<=effective_base+((mid==1||mid==3)&&am<0 ? $signed(am) : 37'sd0);
     global_score<=$signed(k)+$signed({1'b0,table_q[17:10]});
     global_positive<=distance==2358&&k==607 ? critical_c_decision(gmax) : k>$signed({1'b0,table_q[9:0]});
     state<=FINISH;
    end
    FINISH:begin
     busy<=0;done<=1;state<=IDLE;
     if(mid==0)begin
      fraction_bits<=8;cutoff<=607;
      margin<=global_score>34'sd2147483647 ? 32'sh7fffffff : global_score< -34'sd2147483648 ? 32'sh80000000 : global_score[31:0];
      if(global_positive)begin abnormal<=1;class_id<=z[1]>=z[2]?3:4;end
     end else begin
      fraction_bits<=mid>=4 ? 9 : 11;
      cutoff<=mid==1 ? 1920 : mid==3 ? 9856 : mid==4 ? -773 : mid==5 ? -589 : 0;
      if(effective>37'sd2147483647||effective< -37'sd2147483648)begin fault<=1;abnormal<=0;class_id<=0;end
      else begin
       margin<=effective[31:0];
       if(mid!=2&&effective>(mid==1 ? 1920 : mid==3 ? 9856 : mid==4 ? -773 : -589))begin abnormal<=1;class_id<=head;end
      end
     end
    end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
  end
 end
endmodule
