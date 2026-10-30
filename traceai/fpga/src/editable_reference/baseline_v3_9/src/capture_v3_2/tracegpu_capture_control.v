`timescale 1ns/1ps
module tracegpu_capture_control(
 input wire aclk,input wire aresetn,
 input wire [7:0] s_axi_awaddr,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire [31:0] s_axi_wdata,input wire [3:0] s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output wire [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,
 input wire [7:0] s_axi_araddr,input wire s_axi_arvalid,output wire s_axi_arready,
 output reg [31:0] s_axi_rdata,output wire [1:0] s_axi_rresp,output reg s_axi_rvalid,input wire s_axi_rready,
 input wire trigger0,input wire trigger1,input wire trigger2,input wire trigger3,
 output wire capture_run0,output wire capture_run1,output wire capture_run2,output wire capture_run3,
 output reg capture_run,output wire capture_clear,output reg capture_start,output reg capture_flush,
 output wire stream_resetn,output reg [63:0] timestamp,
 output reg vector_mode,output wire legacy_start,output wire legacy_flush,
 input wire [31:0] vector_status, input wire [31:0] vector_sequence,
 input wire [63:0] vector_attempted,vector_queued,vector_dropped,vector_first_loss,
 input wire [15:0] vector_fifo_level,vector_fifo_peak,
 input wire [31:0] vector_rejected_start,
 input wire advanced_active,input wire advanced_trigger,
 input wire [63:0] advanced_trigger_tick,input wire [63:0] advanced_post_ticks,
 input wire [31:0] advanced_post_limit,input wire [11:0] legacy_event_count,
 input wire [2:0] vector_event_count,input wire input_ready,
 output wire [7:0] capture_channels,output wire [7:0] vector_capture_mask
);
reg [31:0] snap_status,snap_sequence,snap_rejected;
reg [63:0] snap_attempted,snap_queued,snap_dropped,snap_first_loss;
reg [31:0] snap_fifo;
assign legacy_start=capture_start&&!vector_mode;
assign legacy_flush=capture_flush&&!vector_mode;
reg armed,triggered,complete;
reg [1:0] source_id;
reg [3:0] enable_mask;
reg [7:0] vector_mask;
reg [31:0] reject_code;
reg [31:0] post_ticks;
reg [63:0] trigger_time,deadline;
reg advanced_post;
reg [31:0] post_count[0:3];
reg [31:0] stop_reason;
reg [63:0] stop_tick;
// Break the producer's event qualification/timestamp arithmetic from the
// post-budget add/compare path. One clock of pending production is covered
// by the software's 256-record reserve; event tags themselves do not change.
reg [11:0] legacy_count_q;
reg [2:0] vector_count_q;
always @(posedge aclk)begin
 if(!aresetn)begin legacy_count_q<=0;vector_count_q<=0;end
 else begin legacy_count_q<=legacy_event_count;vector_count_q<=vector_event_count;end
end
integer ci;
wire advanced_hit=advanced_active&&advanced_trigger&&capture_run&&!triggered;
// capture_run falls before the producer queues END and before AXIS consumes
// the remaining records. Keep the session/configuration locked through both
// phases, otherwise an accepted START has no INIT or CLEAR discards evidence.
wire vector_busy=vector_mode&&(vector_status[1]||!vector_status[0]);
wire configuration_busy=capture_run||advanced_active||vector_busy;
wire time_done=advanced_post&&((timestamp-trigger_time)>=advanced_post_ticks);
wire count_done=advanced_post&&(
 (vector_mode&&({1'b0,post_count[0]}+vector_count_q>={1'b0,advanced_post_limit}))||
 (!vector_mode&&((enable_mask[0]&&({1'b0,post_count[0]}+legacy_count_q[2:0]>={1'b0,advanced_post_limit}))||
 (enable_mask[1]&&({1'b0,post_count[1]}+legacy_count_q[5:3]>={1'b0,advanced_post_limit}))||
 (enable_mask[2]&&({1'b0,post_count[2]}+legacy_count_q[8:6]>={1'b0,advanced_post_limit}))||
 (enable_mask[3]&&({1'b0,post_count[3]}+legacy_count_q[11:9]>={1'b0,advanced_post_limit})) )));
assign capture_channels=vector_mode?vector_mask:{4'd0,enable_mask};
assign vector_capture_mask=vector_mask;
reg [4:0] clear_count;
reg aw_pending,w_pending;
reg [7:0] aw_saved;
reg [31:0] w_saved;
reg [3:0] strb_saved;
wire write_fire=aw_pending&&w_pending&&!s_axi_bvalid;
wire [3:0] triggers={trigger3,trigger2,trigger1,trigger0};
wire chosen_trigger=triggers[source_id]&&enable_mask[source_id]&&capture_run;
assign capture_run0=capture_run&&!vector_mode&&enable_mask[0];
assign capture_run1=capture_run&&!vector_mode&&enable_mask[1];
assign capture_run2=capture_run&&!vector_mode&&enable_mask[2];
assign capture_run3=capture_run&&!vector_mode&&enable_mask[3];
assign s_axi_awready=!aw_pending&&!s_axi_bvalid;
assign s_axi_wready=!w_pending&&!s_axi_bvalid;
assign s_axi_bresp=0;assign s_axi_rresp=0;assign s_axi_arready=!s_axi_rvalid;
assign capture_clear=(clear_count!=0);
assign stream_resetn=aresetn&&!capture_clear;
always @(posedge aclk)begin
 if(!aresetn)begin
  capture_run<=0;capture_start<=0;capture_flush<=0;timestamp<=0;vector_mode<=0;
  snap_status<=0;snap_sequence<=0;snap_rejected<=0;snap_attempted<=0;snap_queued<=0;snap_dropped<=0;snap_first_loss<=0;snap_fifo<=0;
  armed<=0;triggered<=0;complete<=0;source_id<=0;enable_mask<=4'hf;post_ticks<=10000000;
  trigger_time<=0;deadline<=0;clear_count<=0;s_axi_bvalid<=0;s_axi_rvalid<=0;s_axi_rdata<=0;
  advanced_post<=0;stop_reason<=0;stop_tick<=0;
  vector_mask<=8'hff;reject_code<=0;aw_pending<=0;w_pending<=0;aw_saved<=0;w_saved<=0;strb_saved<=0;
  for(ci=0;ci<4;ci=ci+1)post_count[ci]<=0;
 end else begin
  timestamp<=timestamp+1'b1;capture_start<=0;capture_flush<=0;
  if(s_axi_awvalid&&s_axi_awready)begin aw_pending<=1;aw_saved<=s_axi_awaddr;end
  if(s_axi_wvalid&&s_axi_wready)begin w_pending<=1;w_saved<=s_axi_wdata;strb_saved<=s_axi_wstrb;end
  if(clear_count!=0)clear_count<=clear_count-1'b1;
  if(armed&&!triggered&&chosen_trigger&&!advanced_active)begin
   triggered<=1;trigger_time<=timestamp;deadline<=timestamp+post_ticks;
  end
  if(armed&&triggered&&!advanced_active&&timestamp>=deadline)begin
   capture_run<=0;armed<=0;complete<=1;capture_flush<=1;
   stop_reason<=1;stop_tick<=timestamp;
  end
  if(advanced_hit)begin
   triggered<=1;armed<=1;trigger_time<=advanced_trigger_tick;advanced_post<=1;
   for(ci=0;ci<4;ci=ci+1)post_count[ci]<=0;
   if(advanced_post_ticks==0||advanced_post_limit==0||(timestamp-advanced_trigger_tick)>=advanced_post_ticks)begin
    capture_run<=0;armed<=0;complete<=1;capture_flush<=1;advanced_post<=0;
    stop_reason<=advanced_post_limit==0?2:1;stop_tick<=timestamp;
   end
  end
  if(advanced_post&&capture_run)begin
   for(ci=0;ci<4;ci=ci+1)begin
    if(vector_mode)begin if(ci==0)post_count[ci]<=post_count[ci]+vector_count_q;end
    else post_count[ci]<=post_count[ci]+legacy_count_q[ci*3+:3];
   end
   if(time_done||count_done)begin
    capture_run<=0;armed<=0;complete<=1;capture_flush<=1;advanced_post<=0;
    stop_reason<=count_done?2:1;stop_tick<=timestamp;
   end
  end
  if(write_fire)begin
   s_axi_bvalid<=1;aw_pending<=0;w_pending<=0;
   case(aw_saved[7:2])
    0:if(strb_saved[0])begin
     if(w_saved[4])begin
      snap_status<=vector_status;snap_sequence<=vector_sequence;snap_rejected<=vector_rejected_start;
      snap_attempted<=vector_attempted;snap_queued<=vector_queued;snap_dropped<=vector_dropped;
      snap_first_loss<=vector_first_loss;snap_fifo<={vector_fifo_peak,vector_fifo_level};
     end else if(w_saved[3])capture_flush<=1;
     else if(w_saved[0]&&!w_saved[1]&&(!input_ready||clear_count!=0))reject_code<=1;
     else if(w_saved[0]&&(capture_run||vector_busy))reject_code<=2;
     else if(w_saved[1]&&(capture_run||vector_busy))reject_code<=2;
     else begin
      capture_run<=w_saved[0]&&!w_saved[1];
      capture_start<=w_saved[0]&&!w_saved[1];
      advanced_post<=0;
      if(w_saved[0]||w_saved[1])begin
       armed<=w_saved[2]||advanced_active;triggered<=0;complete<=0;reject_code<=0;
       for(ci=0;ci<4;ci=ci+1)post_count[ci]<=0;
       stop_reason<=0;stop_tick<=0;
      end
      if(w_saved[1])begin clear_count<=16;armed<=0;trigger_time<=0;vector_mode<=strb_saved[1]&&w_saved[8];end
      if(!w_saved[0]&&!w_saved[1])begin
       capture_flush<=1;armed<=0;
       // A drain request after automatic stop preserves the hardware reason.
       if(capture_run)begin stop_reason<=3;stop_tick<=timestamp;end
      end
     end
    end
    2:if(strb_saved[0]&&!configuration_busy)source_id<=w_saved[1:0];
    3:if(&strb_saved&&!configuration_busy)post_ticks<=w_saved;
    9:if(strb_saved[0])begin
     if(configuration_busy)reject_code<=2;
     else if(w_saved[7:4]!=0||w_saved[3:0]==0)reject_code<=3;
     else begin enable_mask<=w_saved[3:0];reject_code<=0;end
    end
    37:if(strb_saved[0])begin
     if(configuration_busy)reject_code<=2;
     else if(w_saved[7:0]==0)reject_code<=3;
     else begin vector_mask<=w_saved[7:0];reject_code<=0;end
    end
    default:;
   endcase
  end else if(s_axi_bready)s_axi_bvalid<=0;
  if(s_axi_arvalid&&s_axi_arready)begin
   s_axi_rvalid<=1;
   case(s_axi_araddr[7:2])
    0:s_axi_rdata<={31'd0,capture_run};
    1:s_axi_rdata<={27'd0,capture_clear,complete,triggered,armed,capture_run};
    2:s_axi_rdata<={30'd0,source_id};
    3:s_axi_rdata<=post_ticks;
    4:s_axi_rdata<=trigger_time[31:0];
    5:s_axi_rdata<=trigger_time[63:32];
    6:s_axi_rdata<=32'h00030200;
    7:s_axi_rdata<=timestamp[31:0];
    8:s_axi_rdata<=timestamp[63:32];
    9:s_axi_rdata<={28'd0,enable_mask};
    10:s_axi_rdata<=4;
    11:s_axi_rdata<={31'd0,vector_mode};
    12:s_axi_rdata<=snap_status;
    13:s_axi_rdata<=snap_sequence;
    14:s_axi_rdata<=snap_fifo;
    15:s_axi_rdata<=snap_rejected;
    16:s_axi_rdata<=snap_attempted[31:0];17:s_axi_rdata<=snap_attempted[63:32];
    18:s_axi_rdata<=snap_queued[31:0];19:s_axi_rdata<=snap_queued[63:32];
    20:s_axi_rdata<=snap_dropped[31:0];21:s_axi_rdata<=snap_dropped[63:32];
    22:s_axi_rdata<=snap_first_loss[31:0];23:s_axi_rdata<=snap_first_loss[63:32];
    24:s_axi_rdata<=32'h00000003;
    25:s_axi_rdata<=stop_reason;
    26:s_axi_rdata<=stop_tick[31:0];27:s_axi_rdata<=stop_tick[63:32];
    28:s_axi_rdata<=post_count[0];29:s_axi_rdata<=post_count[1];
    30:s_axi_rdata<=post_count[2];31:s_axi_rdata<=post_count[3];
    32:s_axi_rdata<=8;33:s_axi_rdata<=4;34:s_axi_rdata<=32'hff;
    35:s_axi_rdata<=32'h00030200;
    36:s_axi_rdata<={30'd0,!capture_clear,input_ready};
    37:s_axi_rdata<={24'd0,vector_mask};38:s_axi_rdata<=reject_code;
    default:s_axi_rdata<=0;
   endcase
  end else if(s_axi_rready)s_axi_rvalid<=0;
 end
end
endmodule
