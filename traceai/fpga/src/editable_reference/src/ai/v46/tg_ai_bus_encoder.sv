`timescale 1ns/1ps
// Frozen R2 SPI [43,64] / I2C [38,64] Q6 features. Whole 768-sample snapshot;
// lookahead stays inside the locked window. Canonical raw order as bus_context.
// period_code is the configuration-time PL value round(2*100MHz/(period*rate)).
module tg_ai_bus_encoder(
 input logic clk,resetn,wr,start,input logic[9:0]wr_addr,input logic[3:0]wr_data,
 input logic spi,cpol,cpha,word16,input logic[6:0]period_code,
 input logic[2:0]setup_budget,hold_budget,input logic[7:0]glitch_budget,clock_budget,timeout_budget,
 output logic busy,done,fault,output logic out_valid,input logic out_ready,
 output logic[11:0]out_addr,output logic signed[7:0]out_data);
 typedef enum logic[5:0]{IDLE,BEGIN_CONFIG,WAIT_CONFIG,INIT,DENSE_REQ,DENSE_WAIT,DENSE_WRITE,
  FWD_REQ,FWD_WAIT,WIDTH_SELECT,WIDTH_QREQ,WIDTH_QWAIT,WIDTH_NEXT,FWD_COMMIT,STATS_WRITE,NEXT_FWD,
  BWD_REQ,BWD_WAIT,SETUP_SELECT,SETUP_NEXT,BWD_COMMIT,MIN_REQ,MIN_WRITE,OUT_REQ,OUT_SEND}state_t;
 state_t state,min_return;
 logic cfg_spi,cfg_cpol,cfg_cpha,cfg_word16,context_complete;
 logic[6:0]cfg_period;logic[2:0]cfg_setup,cfg_hold;logic[7:0]cfg_glitch,cfg_clock,cfg_timeout;
 logic cb,cd,cf,cren,crv;logic[9:0]cra,cprefix_tick;logic[25:0]crdata;logic cpactive,cpknown;logic[3:0]cpphase;
 tg_ai_bus_context context_encoder(.clk,.resetn,.wr(wr&&!busy),.start(state==BEGIN_CONFIG),.wr_addr,.wr_data,
  .spi(cfg_spi),.cpol(cfg_cpol),.cpha(cfg_cpha),.word16(cfg_word16),.busy(cb),.done(cd),.fault(cf),
  .rd_en(cren),.rd_addr(cra),.rd_valid(crv),.rd_data(crdata),.last_prefix_tick(cprefix_tick),.prefix_active(cpactive),.prefix_known(cpknown),.prefix_phase(cpphase));
 logic wb,wrdy,wd,wf,wren,wrv;logic[9:0]wra;logic[6:0]wcode;
 tg_ai_bus_width_lut width_table(.clk,.resetn,.start(state==BEGIN_CONFIG),.glitch_budget(cfg_glitch),.clock_budget(cfg_clock),
  .busy(wb),.ready(wrdy),.done(wd),.fault(wf),.rd_en(wren),.rd_addr(wra),.rd_valid(wrv),.rd_code(wcode));
 (* ram_style="block" *)logic[7:0]features[0:4095];logic[7:0]fq,fwd_data;logic fwe;logic[11:0]fwa,fra;
 (* ram_style="block" *)logic[23:0]before_points[0:767];logic[23:0]before_q;
 logic[25:0]meta;logic[3:0]previous_raw,changed,run_active;
 logic previous_active;logic[9:0]index,run_start[0:3],last_before[0:1],next_data[0:1],next_clock;
 logic[3:0]live_changed;logic[19:0]live_before;
 logic[11:0]init_addr,min_addr,out_index;
 logic[5:0]dense_channel,bin,min_bin,width_last_bin;logic[2:0]width_feature,stat_index;
 logic[7:0]dense_value;logic[6:0]min_value;logic[7:0]stat_code[0:4];logic[2:0]sum_active,sum_known,sum_edge,sum_sample;logic[5:0]sum_phase;
 logic[1:0]data_line;logic obligation_hold;
 integer raw_channels,base,nwidth,stats_base,nsignals,width_line,width_length,width_limit,data_physical_line;
 integer before_edge,after_edge,distance;logic eligible_sample;
 function automatic[6:0]phase_code(input logic[3:0]p);
  if(cfg_spi)phase_code=cfg_word16 ? {p,2'b0} : {p,3'b0};
  else case(p)0:phase_code=0;1:phase_code=7;2:phase_code=14;3:phase_code=21;4:phase_code=28;5:phase_code=36;6:phase_code=43;7:phase_code=50;default:phase_code=57;endcase
 endfunction
 function automatic[6:0]prior_code(input logic[5:0]s);
  if(cfg_spi)prior_code=cfg_word16 ? s : {s,1'b0};
  else case(s)
   0:prior_code=0;1:prior_code=2;2:prior_code=4;3:prior_code=5;4:prior_code=7;5:prior_code=9;6:prior_code=11;7:prior_code=12;
   8:prior_code=14;9:prior_code=16;10:prior_code=18;11:prior_code=20;12:prior_code=21;13:prior_code=23;14:prior_code=25;15:prior_code=27;
   16:prior_code=28;17:prior_code=30;18:prior_code=32;19:prior_code=34;20:prior_code=36;21:prior_code=37;22:prior_code=39;23:prior_code=41;
   24:prior_code=43;25:prior_code=44;26:prior_code=46;27:prior_code=48;28:prior_code=50;29:prior_code=52;30:prior_code=53;31:prior_code=55;default:prior_code=57;
  endcase
 endfunction
 function automatic[6:0]timing_code(input integer d,input logic[2:0]b);
  if(d>=2*(b==0?1:b))timing_code=64;
  else case(b)
   0,1:timing_code=d*32;2:timing_code=d*16;4:timing_code=d*8;
   default:case(d)0:timing_code=0;1:timing_code=11;2:timing_code=21;3:timing_code=32;4:timing_code=43;default:timing_code=53;endcase
  endcase
 endfunction
 function automatic[7:0]context_code(input logic[2:0]j);
  integer age;
  begin
   age=511-cprefix_tick;if(age>256)age=256;
   if(cfg_spi)case(j)
    0:context_code=cfg_cpol?64:0;1:context_code=cfg_cpha?64:0;2:context_code=cfg_word16?64:0;3:context_code=cfg_period;
    4:context_code=cfg_setup*16;5:context_code=cfg_hold*16;6:context_code=cpknown?phase_code(cpphase):8'hc0;default:context_code=cpknown?64:0;
   endcase
   else case(j)
    0:context_code=cfg_period;1:context_code=cfg_setup*16;2:context_code=cfg_hold*16;3:context_code=cfg_timeout!=0?64:0;
    4:context_code=({1'b0,cfg_timeout}+2)/4;5:context_code=cpactive?(age+2)/4:0;6:context_code=cpknown?phase_code(cpphase):8'hc0;default:context_code=cpactive?64:0;
   endcase
  end
 endfunction
 always_comb begin
  raw_channels=cfg_spi?16:8;base=cfg_spi?32:28;nwidth=cfg_spi?4:3;stats_base=base+nwidth;nsignals=cfg_spi?2:1;
  width_line=width_feature==0||width_feature==nwidth-1 ? 0 : cfg_spi ? (width_feature==1?2:3) : 1;
  width_length=index-run_start[width_line];width_limit=width_feature==nwidth-1 ? cfg_clock : cfg_glitch;
  data_physical_line=cfg_spi?data_line+2:1;
  before_edge=data_line==1 ? before_q[19:10] : before_q[9:0];after_edge=data_line<2 ? (changed[data_physical_line] ? index : next_data[data_line[0]]) : 1023;
  distance=obligation_hold ? after_edge-index : index-before_edge;
  eligible_sample=index>=512&&meta[7]&&(cfg_spi||next_data[0]==1023||next_data[0]>=next_clock);
  live_changed=index!=0 ? crdata[3:0]^previous_raw : 0;
  live_before[9:0]=live_changed[cfg_spi?2:1] ? index : last_before[0];
  live_before[19:10]=live_changed[3] ? index : last_before[1];
  cren=state==FWD_REQ||state==BWD_REQ||(state==DENSE_REQ&&!(dense_channel>=raw_channels&&dense_channel<raw_channels+8));
  cra=state==DENSE_REQ ? 512+4*bin+dense_channel[1:0] : index;
  wren=state==WIDTH_QREQ;wra={width_feature==nwidth-1,width_length[8:0]};
  fwe=0;fwa=0;fwd_data=0;fra=state==OUT_REQ?out_index:min_addr;
  case(state)
   INIT:begin fwe=1;fwa=init_addr;fwd_data=64;end
   DENSE_WRITE:begin fwe=1;fwa={dense_channel,bin};fwd_data=dense_value;end
   MIN_WRITE:begin fwe=1;fwa=min_addr;fwd_data=min_value<fq?min_value:fq;end
   STATS_WRITE:begin fwe=1;fwa=(stats_base+(stat_index==4?6:stat_index))*64+bin;fwd_data=stat_code[stat_index];end
   default:begin end
  endcase
  out_valid=state==OUT_SEND;out_addr=out_index;out_data=fq;
 end
 always_ff @(posedge clk)begin
  if(fwe)features[fwa]<=fwd_data;
  if(state==MIN_REQ||state==OUT_REQ)fq<=features[fra];
  if(state==FWD_WAIT&&crv)before_points[index]<={live_changed,live_before};
  if(state==BWD_REQ)before_q<=before_points[index];
  if(!resetn)begin
   state<=IDLE;min_return<=IDLE;busy<=0;done<=0;fault<=0;context_complete<=0;
   cfg_spi<=0;cfg_cpol<=0;cfg_cpha<=0;cfg_word16<=0;cfg_period<=0;cfg_setup<=0;cfg_hold<=0;cfg_glitch<=0;cfg_clock<=0;cfg_timeout<=0;
   meta<=0;previous_raw<=0;changed<=0;run_active<=0;previous_active<=0;index<=0;next_clock<=1023;
   for(integer j=0;j<4;j=j+1)run_start[j]<=0;
   for(integer j=0;j<2;j=j+1)begin last_before[j]<=1023;next_data[j]<=1023;end
   for(integer j=0;j<5;j=j+1)stat_code[j]<=0;
   init_addr<=0;min_addr<=0;out_index<=0;dense_channel<=0;bin<=0;min_bin<=0;width_last_bin<=0;width_feature<=0;stat_index<=0;
   dense_value<=0;min_value<=0;sum_active<=0;sum_known<=0;sum_edge<=0;sum_sample<=0;sum_phase<=0;data_line<=0;obligation_hold<=0;
  end else begin
   done<=0;if(cd)context_complete<=1;
   if((start&&busy)||(wr&&busy))begin fault<=1;busy<=0;state<=IDLE;end
   else case(state)
    IDLE:if(start)begin
     if(fault||wr||glitch_budget==0||clock_budget==0||setup_budget>4||hold_budget>4||period_code<16||period_code>56)fault<=1;
     else begin
      busy<=1;context_complete<=0;cfg_spi<=spi;cfg_cpol<=cpol;cfg_cpha<=cpha;cfg_word16<=word16;cfg_period<=period_code;
      cfg_setup<=setup_budget;cfg_hold<=hold_budget;cfg_glitch<=glitch_budget;cfg_clock<=clock_budget;cfg_timeout<=timeout_budget;state<=BEGIN_CONFIG;
     end
    end
    BEGIN_CONFIG:state<=WAIT_CONFIG;
    WAIT_CONFIG:if((context_complete||cd)&&wrdy)begin init_addr<=0;state<=INIT;end
    INIT:if(init_addr==(cfg_spi?2751:2431))begin dense_channel<=0;bin<=0;state<=DENSE_REQ;end else init_addr<=init_addr+1;
    DENSE_REQ:if(dense_channel>=raw_channels&&dense_channel<raw_channels+8)begin dense_value<=context_code(dense_channel-raw_channels);state<=DENSE_WRITE;end else state<=DENSE_WAIT;
    DENSE_WAIT:if(crv)begin
     if(dense_channel<raw_channels)dense_value<=crdata[dense_channel>>2]?64:0;
     else if(dense_channel<raw_channels+12)dense_value<=phase_code(crdata[11:8]);
     else if(dense_channel<raw_channels+16)dense_value<=crdata[16]?64:0;
     else dense_value<=(crdata[25:17]+2)/4;
     state<=DENSE_WRITE;
    end
    DENSE_WRITE:if(bin==63)begin
     bin<=0;if(dense_channel==base-1)begin
      index<=0;previous_raw<=0;previous_active<=0;run_active<=0;for(integer j=0;j<4;j=j+1)run_start[j]<=0;
      last_before[0]<=1023;last_before[1]<=1023;sum_active<=0;sum_known<=0;sum_edge<=0;sum_sample<=0;sum_phase<=0;state<=FWD_REQ;
     end else begin dense_channel<=dense_channel+1;state<=DENSE_REQ;end
    end else begin bin<=bin+1;state<=DENSE_REQ;end
    FWD_REQ:state<=FWD_WAIT;
    FWD_WAIT:if(crv)begin
     meta<=crdata;changed<=live_changed;last_before[0]<=live_before[9:0];last_before[1]<=live_before[19:10];width_feature<=0;
     if(index>=512)begin
      if(index[1:0]==3)begin
       stat_code[0]<=(sum_active+crdata[4])*16;stat_code[1]<=(sum_known+crdata[5])*16;stat_code[2]<=(sum_edge+crdata[6])*16;stat_code[3]<=(sum_sample+crdata[7])*16;
       stat_code[4]<=prior_code(sum_phase+crdata[15:12]);sum_active<=0;sum_known<=0;sum_edge<=0;sum_sample<=0;sum_phase<=0;
      end else begin sum_active<=sum_active+crdata[4];sum_known<=sum_known+crdata[5];sum_edge<=sum_edge+crdata[6];sum_sample<=sum_sample+crdata[7];sum_phase<=sum_phase+crdata[15:12];end
     end
     state<=WIDTH_SELECT;
    end
    WIDTH_SELECT:begin
     if(width_feature>=nwidth)state<=FWD_COMMIT;
     else if(changed[width_line]&&run_start[width_line]!=0&&(run_active[width_line]||meta[4])&&index>512&&width_length<2*width_limit)begin
      min_bin<=run_start[width_line]<512 ? 0 : (run_start[width_line]-512)/4;width_last_bin<=(index-513)/4;state<=WIDTH_QREQ;
     end else width_feature<=width_feature+1;
    end
    WIDTH_QREQ:state<=WIDTH_QWAIT;
    WIDTH_QWAIT:if(wrv)begin min_value<=wcode;min_addr<=(base+width_feature)*64+min_bin;min_return<=WIDTH_NEXT;state<=MIN_REQ;end
    WIDTH_NEXT:if(min_bin==width_last_bin)begin width_feature<=width_feature+1;state<=WIDTH_SELECT;end
     else begin min_bin<=min_bin+1;min_addr<=min_addr+1;state<=MIN_REQ;end
    FWD_COMMIT:begin
     for(integer j=0;j<4;j=j+1)begin
      if(changed[j])begin run_start[j]<=index;run_active[j]<=previous_active||meta[4];end else run_active[j]<=run_active[j]||meta[4];
     end
     previous_raw<=meta[3:0];previous_active<=meta[4];
     if(index>=512&&index[1:0]==3)begin bin<=(index-512)/4;stat_index<=0;state<=STATS_WRITE;end else state<=NEXT_FWD;
    end
    STATS_WRITE:if(stat_index==4)state<=NEXT_FWD;else stat_index<=stat_index+1;
    NEXT_FWD:if(index==767)begin index<=767;next_data[0]<=1023;next_data[1]<=1023;next_clock<=1023;state<=BWD_REQ;end
     else begin index<=index+1;state<=FWD_REQ;end
    BWD_REQ:state<=BWD_WAIT;
    BWD_WAIT:if(crv)begin meta<=crdata;changed<=before_q[23:20];data_line<=0;obligation_hold<=0;state<=SETUP_SELECT;end
    SETUP_SELECT:begin
     if(!eligible_sample||data_line>=nsignals)state<=BWD_COMMIT;
     else if((!obligation_hold&&before_edge!=1023)||(obligation_hold&&cfg_hold!=0&&after_edge!=1023))begin
      min_value<=timing_code(distance,obligation_hold?cfg_hold:cfg_setup);min_addr<=(stats_base+(obligation_hold?5:4))*64+(index-512)/4;min_return<=SETUP_NEXT;state<=MIN_REQ;
     end else state<=SETUP_NEXT;
    end
    SETUP_NEXT:if(!obligation_hold)begin obligation_hold<=1;state<=SETUP_SELECT;end else begin data_line<=data_line+1;obligation_hold<=0;state<=SETUP_SELECT;end
    BWD_COMMIT:begin
     if(changed[cfg_spi?2:1])next_data[0]<=index;if(changed[3])next_data[1]<=index;if(meta[6])next_clock<=index;
     if(index==0)begin out_index<=0;state<=OUT_REQ;end else begin index<=index-1;state<=BWD_REQ;end
    end
    MIN_REQ:state<=MIN_WRITE;
    MIN_WRITE:state<=min_return;
    OUT_REQ:state<=OUT_SEND;
    OUT_SEND:if(out_ready)begin if(out_index==(cfg_spi?2751:2431))begin busy<=0;done<=1;state<=IDLE;end else begin out_index<=out_index+1;state<=OUT_REQ;end end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
   if(cf||wf)begin fault<=1;busy<=0;state<=IDLE;end
  end
 end
endmodule
