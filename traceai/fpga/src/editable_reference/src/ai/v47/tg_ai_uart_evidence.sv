`timescale 1ns/1ps
// Observations, not another classifier. One synchronous raw BRAM and one serial
// divider. Raw input is a read-only tap of the existing PL-owned window.
module tg_ai_uart_evidence(
 input logic clk,resetn,clear,wr,input logic[9:0]wr_addr,input logic wr_data,
 input logic start,input logic[31:0]unit_den,period_ticks,baud,epoch,generation,window_id,
 input logic[63:0]origin,evidence_tick,input logic[31:0]identity,
 input logic point_valid,primary,input logic[9:0]anchor,input logic[3:0]slot,
 input logic signed[31:0]anchor_margin,input logic data8,parity_enable,parity_odd,stop2,partial,
 input logic publish,input logic[15:0]sequence_id,input logic[63:0]timestamp,
 input logic[4:0]word_index,output logic[31:0]word_data,
 output logic busy,done,fault);
 (* ram_style="block" *)logic raw[0:767];logic rd;logic[9:0]address,index;
 typedef enum logic[3:0]{IDLE,PERIOD_START,PERIOD_WAIT,SLOT_START,SLOT_WAIT,SLOT_READ,SLOT_USE,SCAN_READ,SCAN_USE,MEAN_START,MEAN_WAIT,READY}st_t;
 st_t state;logic[31:0]flags,min_width,min_begin;logic[12:0]expected_q8;logic[13:0]measured_q8;logic[26:0]max_phase,phase;logic[9:0]estimate_sum,estimate_count;
 logic[31:0]low_bounds,valid_mask;logic[7:0]payload;logic observed_parity,expected_parity;logic[1:0]stop_low;
 logic[3:0]s,last_slot;logic[9:0]sample_index,selected_index,last_center,run_begin;logic previous,have_previous;
 logic[31:0]width;logic[26:0]phase_error,phase_now;logic[63:0]decision_tick,frame_tick;
 logic[63:0]numerator,quotient;logic[31:0]denominator,remainder;logic db,dd,dz;
 logic[31:0]slot_constant;
 always_comb begin
  case(s)
   0:slot_constant=100000000;1:slot_constant=300000000;2:slot_constant=500000000;3:slot_constant=700000000;
   4:slot_constant=900000000;5:slot_constant=1100000000;6:slot_constant=1300000000;7:slot_constant=1500000000;
   8:slot_constant=1700000000;9:slot_constant=1900000000;10:slot_constant=2100000000;11:slot_constant=32'd2300000000;
   default:slot_constant=32'd2500000000;
  endcase
  numerator=state==PERIOD_START?64'd25600000000:state==MEAN_START?{46'b0,estimate_sum,8'b0}:{32'b0,slot_constant};
  denominator=state==PERIOD_START?unit_den:state==MEAN_START?estimate_count:(unit_den<<1);
  address=state==SLOT_READ?sample_index:index;
  width={22'b0,index}-{22'b0,run_begin};
  phase_now=index==anchor ? (unit_den>>1) : phase+unit_den[24:0]>=27'd100000000 ? phase+unit_den[24:0]-27'd100000000 : phase+unit_den[24:0];
  phase_error=phase>27'd50000000 ? 27'd100000000-phase : phase;
  busy=state!=IDLE&&state!=READY;done=state==READY;
  word_data=0;
  case(word_index)
   0:word_data=32'h45563437;1:word_data={16'b0,sequence_id};2:word_data=epoch;3:word_data=generation;
   4:word_data=window_id;5:word_data=identity;6:word_data=flags;
   7:word_data=period_ticks;8:word_data=origin[31:0];9:word_data=origin[63:32];
   10:word_data=decision_tick[31:0];11:word_data=decision_tick[63:32];12:word_data=evidence_tick[31:0];13:word_data=evidence_tick[63:32];
   14:word_data=frame_tick[31:0];15:word_data=frame_tick[63:32];16:word_data=expected_q8;17:word_data=measured_q8;
   18:word_data=estimate_count;19:word_data=estimate_sum;20:word_data={16'b0,stop_low,4'b0,expected_parity,observed_parity,payload};
   21:word_data=min_width;22:word_data=min_begin;23:word_data=max_phase;24:word_data=unit_den;
   25:word_data={12'b0,slot,6'b0,anchor};26:word_data={27'b0,stop2,parity_odd,parity_enable,data8,point_valid};
   27:word_data=low_bounds;28:word_data=anchor_margin;29:word_data=valid_mask;30:word_data=32'h00040700;31:word_data=baud;
  endcase
 end
 tg_ai_udiv64 divider(.clk,.resetn(resetn&&!clear),.start(state==PERIOD_START||state==SLOT_START||state==MEAN_START),
  .numerator,.denominator,.busy(db),.done(dd),.divide_zero(dz),.quotient,.remainder);
 always_ff @(posedge clk)begin
  if(wr&&wr_addr<768)raw[wr_addr]<=wr_data;
  rd<=raw[address];
  if(!resetn||clear)begin
   state<=IDLE;fault<=0;index<=0;s<=0;sample_index<=0;selected_index<=0;last_center<=0;last_slot<=0;flags<=0;
   expected_q8<=0;measured_q8<=0;min_width<=32'hffffffff;min_begin<=0;max_phase<=0;phase<=0;
   estimate_sum<=0;estimate_count<=0;payload<=0;observed_parity<=0;expected_parity<=0;stop_low<=0;
   valid_mask<=0;low_bounds<=32'hffffffff;decision_tick<=0;frame_tick<=0;run_begin<=0;previous<=0;have_previous<=0;
  end else begin
   if(publish)decision_tick<=timestamp;
   case(state)
    IDLE,READY:if(start)begin
     flags<=32'h1|(point_valid?32'h2:32'h8000)|(primary&&point_valid?32'h4:0)|(point_valid&&anchor_margin>0?32'h8:0)|
      (parity_enable?32'h40:0)|(partial?32'h4000:0);
     fault<=0;expected_q8<=0;measured_q8<=0;min_width<=32'hffffffff;min_begin<=0;max_phase<=0;phase<=0;
     estimate_sum<=0;estimate_count<=0;payload<=0;observed_parity<=0;expected_parity<=parity_odd;
     stop_low<=0;valid_mask<=0;low_bounds<=32'hffffffff;s<=0;index<=0;have_previous<=0;run_begin<=0;
     last_slot<=(data8?9:8)+parity_enable+stop2;
     frame_tick<=point_valid?(origin+anchor*period_ticks-(period_ticks>>1)):0;
     state<=PERIOD_START;
    end
    PERIOD_START:state<=PERIOD_WAIT;
    PERIOD_WAIT:if(dd)begin
     if(dz||quotient==0||quotient>8191||unit_den<3571429||unit_den>22222222)begin fault<=1;flags[14]<=1;state<=READY;end
     else begin expected_q8<=quotient[12:0];state<=point_valid?SLOT_START:SCAN_READ;end
    end
    SLOT_START:state<=SLOT_WAIT;
    SLOT_WAIT:if(dd)begin
     // nearest sample to a-.5 + (slot+.5)*samples_per_bit
     if(dz||quotient+anchor>=768)begin flags[14]<=1;state<=SCAN_READ;end
     else begin sample_index<=anchor+quotient[9:0];last_center<=anchor+quotient[9:0];state<=SLOT_READ;end
    end
    SLOT_READ:state<=SLOT_USE;
    SLOT_USE:begin
     valid_mask[s]<=1;
     if(s>=1&&s<=(data8?8:7))begin payload[s-1]<=rd;expected_parity<=expected_parity^rd;end
     if(parity_enable&&s==(data8?9:8))begin observed_parity<=rd;flags[7]<=1;flags[8]<=rd!=expected_parity;end
     if(s==(data8?9:8)+parity_enable)begin stop_low[0]<=!rd;flags[12]<=!rd;end
     if(stop2&&s==last_slot)begin stop_low[1]<=!rd;flags[13]<=!rd;end
     if(s==slot)begin flags[4]<=1;flags[5]<=!rd;selected_index<=sample_index;end
     if(s==last_slot)begin index<=0;state<=SCAN_READ;end else begin s<=s+1;state<=SLOT_START;end
    end
    SCAN_READ:begin if(index>=anchor)phase<=phase_now;state<=SCAN_USE;end
    SCAN_USE:begin
     // The first and last open runs are excluded. Long UART data runs are
     // legal and never used as single-bit estimates.
     if(!have_previous)begin previous<=rd;run_begin<=index;have_previous<=1;end
     else if(rd!=previous)begin
      if(run_begin!=0&&index>=512)begin
       if(width<min_width)begin min_width<=width;min_begin<=run_begin;end
       if((width<<8)<(expected_q8>>2))flags[9]<=1;
       if((width<<8)>=(expected_q8-(expected_q8>>2))&&(width<<8)<=(expected_q8+(expected_q8>>2)))begin
        estimate_sum<=estimate_sum+width;estimate_count<=estimate_count+1;
       end
      end
      if(point_valid&&index>anchor&&index<=last_center)begin
       // Phase is advanced in the BRAM-read cycle, before abs/max comparison.
       // One-sample uncertainty remains; sign is relative to a candidate grid.
       flags[17]<=1;
       if(phase_error>max_phase)begin max_phase<=phase_error;flags[16]<=phase>27'd50000000;end
       if(phase_error>unit_den+4000000)flags[10]<=1;
      end
      if(point_valid&&flags[4]&&flags[5]&&!previous&&run_begin<=selected_index&&index>selected_index)low_bounds<={6'b0,index,6'b0,run_begin};
      previous<=rd;run_begin<=index;
     end
     if(index==767&&point_valid&&flags[4]&&flags[5]&&!rd&&run_begin<=selected_index)low_bounds<={6'b0,index,6'b0,run_begin};
     if(index==767)state<=MEAN_START;
     else begin index<=index+1;state<=SCAN_READ;end
    end
    MEAN_START:state<=estimate_count>=3?MEAN_WAIT:READY;
    MEAN_WAIT:if(dd)begin if(!dz)begin measured_q8<=quotient[13:0];flags[11]<=1;end state<=READY;end
    default:begin fault<=1;state<=READY;end
   endcase
  end
 end
endmodule
