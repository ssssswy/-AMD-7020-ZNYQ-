`timescale 1ns/1ps
// Complete observed UART candidate enumeration and R9/R8/C feature generation.
// One window stays owned until done. No candidate score or primary hard filter.
module tg_ai_uart_local(
 input logic clk,resetn,wr,start,input logic[9:0]wr_addr,input logic wr_data,
 input logic[31:0]unit_den,input logic data8,parity_enable,parity_odd,stop2,
 output logic busy,done,fault,output logic token_valid,
 input logic emit_local,emit_stop,advance,input logic signed[31:0]anchor_margin,
 output logic[9:0]anchor_index,output logic[3:0]slot_index,
 output logic is_parity,primary,role_index,output logic[8:0]token_count,
 output logic[47:0]center_numerator,output logic[31:0]center_denominator,
 output logic out_valid,input logic out_ready,output logic[10:0]out_addr,output logic signed[7:0]out_data,
 output logic stream_done);
 typedef enum logic[4:0]{IDLE,GEOM_START,GEOM_WAIT,SCAN_READ,SCAN_ENCODE,FLAG_REQ,COUNT,FIND,CENTER_A,CENTER_B,
  DATA_READ,DATA_ACC,PARITY_READ,PARITY_ACC,POINT_INIT,FINE_NORM,COARSE_NORM,
  FINE_READ,FINE_SAVE,COARSE_READ,COARSE_SAVE,STOP_READ,POINT_STORE,READY,OUT_READ,OUT_SEND}state_t;
 state_t state;
 (* ram_style="block" *)logic raw[0:767];
 (* ram_style="block" *)logic[19:0]local_points[0:63];
 (* ram_style="block" *)logic[2:0]stop_points[0:63];
 logic raw_q;logic[19:0]lq;logic[2:0]sq;
 // A candidate's primary flag is set at the SAME low-run qualification as
 // the all-observed detector. Store both bits in one synchronous RAM rather
 // than two 768-bit dynamically updated register maps.
 (* ram_style="block" *)logic[1:0]flags[0:767];logic[1:0]flag_q,flag_data;
 logic flag_we,flags_cleared,geometry_complete,counting;logic[9:0]flag_addr;
 logic[9:0]loaded,index,scan_candidate,scan_direct;logic[1:0]role;
 logic[31:0]den,phase_end,phase_reopen;logic cfg_data8,cfg_parity,cfg_odd,cfg_stop2;
 (* use_dsp="yes" *)logic[26:0]center_low,center_high;
 logic[31:0]center_constant;
 logic active,candidate,direct_candidate,previous,direct_eligible,candidate_eligible;
 logic[31:0]phase,cphase,dphase,high_phase;logic signed[35:0]remaining;
 logic[15:0]gq[0:18];logic[31:0]gr[0:18];logic gb,gd,gf,gv;
 logic[4:0]ga;logic[15:0]gvalue;logic[31:0]grem;
 tg_ai_uart_geometry geometry(.clk,.resetn,.start(state==GEOM_START),.unit_den(den),
  .data8(cfg_data8),.parity_enable(cfg_parity),.stop2(cfg_stop2),.busy(gb),.done(gd),.fault(gf),
  .table_valid(gv),.table_addr(ga),.table_q(gvalue),.table_rem(grem));
 integer data_bits,frame_bits,roles;logic na,nc,nd,nprev,nce,nde,mark_primary,mark_qualified;
 logic[31:0]np,ncp,ndp,nh;logic[9:0]nca,nda;logic eligible;
 logic touched;integer slot;
 logic[3:0]dk;logic[3:0]data_sum;logic data_xor,data_valid,expected,observed_parity;
 logic signed[7:0]mean_code,match_code;logic[6:0]qualification;
 logic[5:0]point;logic[4:0]channel;logic stop_stream;
 logic signed[15:0]fine_offset,coarse_offset;logic[31:0]fine_rem;logic[32:0]coarse_rem;
 logic fine_value,fine_valid,coarse_value,coarse_valid;logic[19:0]point_bits;
 integer fine_index,coarse_index,stop_index,data_index,read_index;logic[10:0]out_index;
 logic[31:0]fine_den,coarse_den;logic signed[31:0]clipped_margin;
 always_comb begin
  data_bits=cfg_data8?8:7;frame_bits=1+data_bits+cfg_parity+1+cfg_stop2;roles=1+cfg_stop2+cfg_parity;
  slot=1+data_bits+role;
  touched=index>=gq[13+role]&&index<gq[16+role];
  eligible=remaining<$signed({4'b0,phase_end});
  na=active;nc=candidate;nd=direct_candidate;nprev=raw_q;np=phase;ncp=cphase;ndp=dphase;
  nca=scan_candidate;nda=scan_direct;nce=candidate_eligible;nde=direct_eligible;
  mark_primary=0;mark_qualified=0;
  nh=raw_q ? (high_phase>=32'd100000000 ? 32'd100000000 : high_phase+den+den) : 0;
  if(na&&np>=phase_end)na=0;
  if(na&&previous&&!raw_q&&high_phase>=32'd100000000&&np>=phase_reopen)na=0;
  if(!na)begin
   if(!nc&&previous&&!raw_q&&high_phase>=32'd100000000)begin nc=1;ncp=den;nca=index;nce=eligible;end
   if(nc)begin
    if(raw_q)nc=0;
    else if(ncp>=32'd100000000)begin na=1;np=ncp;nc=0;mark_primary=nce;end
   end
  end
  if(index!=0&&previous&&!raw_q&&high_phase>=32'd100000000)begin nd=1;ndp=den;nda=index;nde=eligible;end
  if(nd)begin
   if(raw_q)nd=0;
   else if(ndp>=32'd100000000)begin nd=0;mark_qualified=nde;end
  end
  fine_den=den<<4;coarse_den=den<<1;
  fine_index=$signed({1'b0,anchor_index})+fine_offset;
  coarse_index=$signed({1'b0,anchor_index})+coarse_offset;
  stop_index=fine_index+(fine_rem>(fine_den>>1));
  data_index=anchor_index+gq[1+dk];read_index=0;
  case(state)
   SCAN_READ:read_index=index;
   DATA_READ:read_index=data_index;
   PARITY_READ:read_index=anchor_index+gq[1+data_bits];
   FINE_READ:read_index=fine_index;
   COARSE_READ:read_index=coarse_index;
   STOP_READ:read_index=stop_index;
   default:read_index=0;
  endcase
  point_bits=0;
  point_bits[0]=fine_value;point_bits[1]=fine_valid;point_bits[2]=fine_valid&&fine_index>=512;
  point_bits[3]=is_parity?expected:1;point_bits[4]=!is_parity||data_valid;
  point_bits[5]=is_parity;point_bits[6]=!is_parity;point_bits[7]=primary;
  point_bits[8]=coarse_value;point_bits[9]=coarse_valid;point_bits[10]=coarse_valid&&coarse_index>=512;
  point_bits[11]=$signed({1'b0,point})>=32&&$signed({1'b0,point})<34;
  point_bits[12]=point>=34&&point<2*(1+data_bits)+32;
  point_bits[13]=cfg_parity&&point>=2*(1+data_bits)+32&&point<2*(2+data_bits)+32;
  point_bits[14]=point>=2*(1+data_bits+cfg_parity)+32&&point<2*(1+data_bits+cfg_parity+1+cfg_stop2)+32;
  point_bits[15]=cfg_parity;point_bits[16]=cfg_data8;point_bits[17]=cfg_stop2;
  point_bits[18]=!is_parity&&role_index;point_bits[19]=1;
  token_valid=state==READY;out_valid=state==OUT_SEND;out_addr=out_index;
  out_data=0;
  if(!stop_stream)out_data=lq[channel]?8'sd64:8'sd0;
  else case(channel)
   0,1,2:out_data=sq[channel]?8'sd64:8'sd0;
   3:out_data=$signed({1'b0,qualification});4:out_data=mean_code;5:out_data=match_code;
   default:out_data=0;
  endcase
  clipped_margin=anchor_margin;
  if(anchor_margin< -16384)clipped_margin=-16384;
  if(anchor_margin>16384)clipped_margin=16384;
  flag_we=0;flag_addr=index;flag_data=0;
  if(resetn)begin
   if(state==GEOM_WAIT&&!flags_cleared)flag_we=1;
   else if(state==SCAN_ENCODE&&mark_qualified)begin flag_we=1;flag_addr=nda;flag_data={mark_primary,1'b1};end
  end
 end
 function automatic[31:0]end_phase(input logic d,p,s);
  case({d,p,s})
   0:end_phase=1800000000;1,2,4:end_phase=2000000000;
   3,5,6:end_phase=2200000000;7:end_phase=2400000000;
  endcase
 endfunction
 function automatic[7:0]mean7(input logic[3:0]s);
  case(s)0:mean7=0;1:mean7=9;2:mean7=18;3:mean7=27;4:mean7=37;5:mean7=46;6:mean7=55;default:mean7=64;endcase
 endfunction
 always_ff @(posedge clk)begin
  if(wr&&!busy&&wr_addr==loaded&&wr_addr<768)raw[wr_addr]<=wr_data;
  if(flag_we)flags[flag_addr]<=flag_data;
  if(state==FLAG_REQ)flag_q<=flags[index];
  if(state==SCAN_READ||state==DATA_READ||state==PARITY_READ||state==FINE_READ||state==COARSE_READ||state==STOP_READ)
   raw_q<=read_index>=0&&read_index<768 ? raw[read_index] : 1'b0;
  if(gv)begin gq[ga]<=gvalue;gr[ga]<=grem;end
  if(state==POINT_STORE)begin local_points[point]<=point_bits;
   stop_points[point]<={stop_index>=512&&stop_index<768,stop_index>=0&&stop_index<768,raw_q};end
  if(state==OUT_READ)begin lq<=local_points[point];sq<=stop_points[point];end
  if(!resetn)begin
   state<=IDLE;loaded<=0;busy<=0;done<=0;fault<=0;stream_done<=0;index<=0;role<=0;den<=0;
   phase_end<=0;phase_reopen<=0;center_numerator<=0;center_denominator<=0;center_low<=0;center_high<=0;center_constant<=0;
   cfg_data8<=0;cfg_parity<=0;cfg_odd<=0;cfg_stop2<=0;active<=0;candidate<=0;direct_candidate<=0;previous<=1;
   scan_candidate<=0;scan_direct<=0;candidate_eligible<=0;direct_eligible<=0;
   phase<=0;cphase<=0;dphase<=0;high_phase<=0;remaining<=0;flags_cleared<=0;geometry_complete<=0;counting<=1;
   anchor_index<=0;slot_index<=0;is_parity<=0;primary<=0;role_index<=0;token_count<=1;
   dk<=0;data_sum<=0;data_xor<=0;data_valid<=0;expected<=0;observed_parity<=0;mean_code<=0;match_code<=0;qualification<=0;
   point<=0;channel<=0;stop_stream<=0;out_index<=0;fine_offset<=0;coarse_offset<=0;fine_rem<=0;coarse_rem<=0;
   fine_value<=0;fine_valid<=0;coarse_value<=0;coarse_valid<=0;
  end else begin
   done<=0;stream_done<=0;
   if(wr)begin if(busy||wr_addr!=loaded||wr_addr>=768)fault<=1;else loaded<=loaded+1;end
   if(start&&busy)fault<=1;
   case(state)
    IDLE:if(start)begin
     if(fault||wr||loaded!=768||unit_den<3571429||unit_den>22222222||(!parity_enable&&parity_odd))fault<=1;
     else begin
      busy<=1;loaded<=0;den<=unit_den;cfg_data8<=data8;cfg_parity<=parity_enable;cfg_odd<=parity_odd;cfg_stop2<=stop2;
      phase_end<=end_phase(data8,parity_enable,stop2);phase_reopen<=end_phase(data8,parity_enable,stop2)-80000000;
      center_denominator<=unit_den<<1;
      active<=0;candidate<=0;direct_candidate<=0;previous<=1;phase<=0;cphase<=0;dphase<=0;high_phase<=0;
      flags_cleared<=0;geometry_complete<=0;counting<=1;index<=0;role<=0;token_count<=1;
      remaining<=$signed({4'b0,unit_den})*36'd1025;state<=GEOM_START;
     end
    end
    GEOM_START:state<=GEOM_WAIT;
    GEOM_WAIT:begin
     if(!flags_cleared)begin if(index==767)begin flags_cleared<=1;index<=0;end else index<=index+1;end
     if(gd)geometry_complete<=1;
     if(gf)begin fault<=1;busy<=0;state<=IDLE;end
     else if(flags_cleared&&(gd||geometry_complete))begin index<=0;state<=SCAN_READ;end
    end
    SCAN_READ:state<=SCAN_ENCODE;
    SCAN_ENCODE:begin
     active<=na;candidate<=nc;direct_candidate<=nd;previous<=nprev;
     phase<=np+den+den;cphase<=ncp+den+den;dphase<=ndp+den+den;high_phase<=nh;
     scan_candidate<=nca;scan_direct<=nda;candidate_eligible<=nce;direct_eligible<=nde;
     remaining<=remaining-$signed({4'b0,den})-$signed({4'b0,den});
     if(index==767)begin index<=0;role<=0;state<=FLAG_REQ;end else begin index<=index+1;state<=SCAN_READ;end
    end
    FLAG_REQ:state<=counting?COUNT:FIND;
    COUNT:begin
     if(flag_q[0]&&touched)begin
      if(token_count==256)begin fault<=1;busy<=0;state<=IDLE;end else token_count<=token_count+1;
     end
     if(!(flag_q[0]&&touched&&token_count==256))begin
      if(role+1<roles)role<=role+1;
      else begin role<=0;state<=FLAG_REQ;if(index==767)begin index<=0;counting<=0;end else index<=index+1;end
     end
    end
    FIND:begin
     if(flag_q[0]&&touched)begin
      anchor_index<=index;slot_index<=slot;is_parity<=cfg_parity&&role==0;primary<=flag_q[1];role_index<=cfg_parity ? (role==0 ? 0 : role-1) : role;
      dk<=0;data_sum<=0;data_xor<=0;data_valid<=1;state<=CENTER_A;
     end else if(role+1<roles)role<=role+1;
     else if(index==767)begin busy<=0;done<=1;state<=IDLE;end else begin index<=index+1;role<=0;state<=FLAG_REQ;end
    end
    CENTER_A:begin
     center_low<=anchor_index*den[15:0];center_high<=anchor_index*den[31:16];
     case(slot_index)
      8:center_constant<=1700000000;9:center_constant<=1900000000;10:center_constant<=2100000000;11:center_constant<=2300000000;
      default:center_constant<=0;
     endcase
     state<=CENTER_B;
    end
    CENTER_B:begin center_numerator<=({21'b0,center_high}<<17)+({21'b0,center_low}<<1)-den+center_constant;state<=DATA_READ;end
    DATA_READ:state<=DATA_ACC;
    DATA_ACC:begin
     if(data_index<0||data_index>=768)data_valid<=0;
     else begin data_sum<=data_sum+raw_q;data_xor<=data_xor^raw_q;end
     if(dk+1<data_bits)begin dk<=dk+1;state<=DATA_READ;end else state<=PARITY_READ;
    end
    PARITY_READ:state<=PARITY_ACC;
    PARITY_ACC:begin
     expected<=data_valid&&cfg_parity ? data_xor^cfg_odd : 1'b0;
     mean_code<=!data_valid ? -8'sd64 : cfg_data8 ? {data_sum,3'b0} : mean7(data_sum);
     match_code<=data_valid&&cfg_parity&&anchor_index+gq[1+data_bits]<768 ? (raw_q==(data_xor^cfg_odd) ? 8'sd64 : 8'sd0) : -8'sd64;
     point<=0;fine_offset<=$signed(gq[10+role]);fine_rem<=gr[10+role];
     coarse_offset<=-$signed(gq[0])-(gr[0]!=0);coarse_rem<=gr[0]==0 ? 0 : coarse_den-gr[0];state<=POINT_INIT;
    end
    POINT_INIT:state<=FINE_NORM;
    FINE_NORM:if(fine_rem>=fine_den)begin fine_rem<=fine_rem-fine_den;fine_offset<=fine_offset+1;end else state<=COARSE_NORM;
    COARSE_NORM:if(coarse_rem>=coarse_den)begin coarse_rem<=coarse_rem-coarse_den;coarse_offset<=coarse_offset+1;end else state<=FINE_READ;
    FINE_READ:state<=FINE_SAVE;
    FINE_SAVE:begin fine_value<=raw_q;fine_valid<=fine_index>=0&&fine_index<768;state<=COARSE_READ;end
    COARSE_READ:state<=COARSE_SAVE;
    COARSE_SAVE:begin coarse_value<=raw_q;coarse_valid<=coarse_index>=0&&coarse_index<768;state<=STOP_READ;end
    STOP_READ:state<=POINT_STORE;
    POINT_STORE:begin
     if(point==63)state<=READY;
     else begin point<=point+1;fine_rem<=fine_rem+32'd100000000;coarse_rem<=coarse_rem+33'd100000000;state<=FINE_NORM;end
    end
    READY:begin
     if((emit_local&&emit_stop)||(advance&&(emit_local||emit_stop))||(emit_stop&&is_parity))begin fault<=1;busy<=0;state<=IDLE;end
     else if(emit_local||emit_stop)begin
      stop_stream<=emit_stop;qualification<=(clipped_margin+16384+256)/512;point<=0;channel<=0;out_index<=0;state<=OUT_READ;
     end else if(advance)begin
      if(role+1<roles)begin role<=role+1;state<=FIND;end
      else if(index==767)begin busy<=0;done<=1;state<=IDLE;end
      else begin index<=index+1;role<=0;state<=FLAG_REQ;end
     end
    end
    OUT_READ:state<=OUT_SEND;
    OUT_SEND:if(out_ready)begin
     out_index<=out_index+1;
     if(point==63)begin point<=0;if(channel==(stop_stream?5:19))begin stream_done<=1;state<=READY;end else begin channel<=channel+1;state<=OUT_READ;end end
     else begin point<=point+1;state<=OUT_READ;end
    end
    default:begin fault<=1;busy<=0;state<=IDLE;end
   endcase
   // This invariant follows from the identical start edge/low qualification;
   // fail rather than silently losing a primary identity if it is violated.
   if(state==SCAN_ENCODE&&mark_primary&&(!mark_qualified||nca!=nda))begin fault<=1;busy<=0;state<=IDLE;end
  end
 end
endmodule
