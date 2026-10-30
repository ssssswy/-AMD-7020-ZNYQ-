`timescale 1ns/1ps
// Canonical SPI raw order SCK,CS,MOSI,MISO; I2C SCL,SDA. Selected CS is low.
// Observed prefix parser only; no injected labels or transmitter state.
module tg_ai_bus_context(
 input logic clk,resetn,wr,start,input logic[9:0]wr_addr,input logic[3:0]wr_data,
 input logic spi,cpol,cpha,word16,output logic busy,done,fault,
 input logic rd_en,input logic[9:0]rd_addr,output logic rd_valid,output logic[25:0]rd_data,
 output logic[9:0]last_prefix_tick,output logic prefix_active,prefix_known,output logic[3:0]prefix_phase);
 typedef enum logic[1:0]{IDLE,READ,ENCODE}state_t;state_t state;
 (* ram_style="block" *)logic[3:0]raw[0:767];
 (* ram_style="block" *)logic[25:0]metadata[0:767];
 logic[3:0]raw_q,previous;logic[9:0]index,loaded,last;
 logic cfg_spi,cfg_cpol,cfg_cpha,cfg_word16,on,known;
 logic[3:0]phase;logic next_on,next_known,clock_edge,sample_edge,bus_event;logic[3:0]next_phase;
 logic[9:0]next_last;logic[8:0]age;logic[25:0]packed_meta;
 always_comb begin
  clock_edge=index!=0&&raw_q[0]!=previous[0];bus_event=index!=0&&raw_q[1]!=previous[1]&&raw_q[0]&&previous[0];
  next_on=on;next_known=known;next_phase=phase;
  if(cfg_spi)begin
   next_on=!raw_q[1];
   if(index!=0&&previous[1]&&!raw_q[1])begin next_known=1;next_phase=0;end
   if(clock_edge&&raw_q[0]==(cfg_cpha?cfg_cpol:!cfg_cpol)&&next_on)next_phase=next_phase== (cfg_word16?15:7) ? 0 : next_phase+1;
   if(index!=0&&!previous[1]&&raw_q[1])next_known=0;
  end else begin
   if(bus_event)begin if(!raw_q[1])begin next_on=1;next_known=1;next_phase=0;end else begin next_on=0;next_known=0;end end
   if(index!=0&&!previous[0]&&raw_q[0]&&next_on)next_phase=next_phase==8 ? 0 : next_phase+1;
  end
  sample_edge=clock_edge&&raw_q[0]==(cfg_spi?(cfg_cpha?cfg_cpol:!cfg_cpol):1'b1)&&next_on&&next_known;
  next_last=index!=0&&(clock_edge||next_on!=on) ? index : last;
  age=!next_on ? 0 : index-next_last>256 ? 256 : index-next_last;
  packed_meta={age,known,phase,next_phase,sample_edge,clock_edge,next_known,next_on,raw_q};
 end
 always_ff @(posedge clk)begin
  if(wr&&!busy&&wr_addr==loaded&&wr_addr<768)raw[wr_addr]<=wr_data;
  if(state==READ)raw_q<=raw[index];
  if(state==ENCODE)metadata[index]<=packed_meta;
  if(rd_en&&rd_addr<768)rd_data<=metadata[rd_addr];
  rd_valid<=rd_en&&rd_addr<768&&resetn&&!busy;
  if(!resetn)begin
   state<=IDLE;busy<=0;done<=0;fault<=0;index<=0;loaded<=0;last<=0;previous<=0;
   cfg_spi<=0;cfg_cpol<=0;cfg_cpha<=0;cfg_word16<=0;on<=0;known<=0;phase<=0;
   last_prefix_tick<=0;prefix_active<=0;prefix_known<=0;prefix_phase<=0;rd_valid<=0;
  end else begin
   done<=0;if(start&&busy)fault<=1;
   if(wr)begin if(busy||wr_addr!=loaded||wr_addr>=768)fault<=1;else loaded<=loaded+1;end
   case(state)
    IDLE:if(start)begin
     if(fault||wr||loaded!=768)fault<=1;
     else begin busy<=1;loaded<=0;index<=0;last<=0;previous<=0;on<=0;known<=0;phase<=0;
      last_prefix_tick<=0;cfg_spi<=spi;cfg_cpol<=cpol;cfg_cpha<=cpha;cfg_word16<=word16;state<=READ;end
    end
    READ:state<=ENCODE;
    ENCODE:begin
     on<=next_on;known<=next_known;phase<=next_phase;last<=next_last;previous<=raw_q;
     if(index!=0&&index<512&&(clock_edge||(!cfg_spi&&bus_event)))last_prefix_tick<=index;
     if(index==511)begin prefix_active<=next_on;prefix_known<=next_known;prefix_phase<=next_phase;end
     if(index==767)begin busy<=0;done<=1;state<=IDLE;end else begin index<=index+1;state<=READ;end
    end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
  end
 end
endmodule
