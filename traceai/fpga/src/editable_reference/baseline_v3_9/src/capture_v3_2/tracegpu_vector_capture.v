`timescale 1ns/1ps
// ABI 0x00030200: {type8, mask_or_gap_reason8, levels8, seq32, ff8, tick64}.
module tracegpu_vector_capture #(
 parameter FIFO_BITS=10,parameter WATERMARK_TICKS=100000,parameter PACKET_RECORDS=256
)(
 (* X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF m_axis, ASSOCIATED_RESET resetn" *)
 input wire clk,input wire resetn,input wire clear,
 input wire enable,input wire capture_start,input wire capture_run,
 input wire [63:0] timestamp,input wire [7:0] levels,capture_mask,
 output wire [127:0] m_axis_tdata,output wire m_axis_tvalid,
 input wire m_axis_tready,output wire [15:0] m_axis_tkeep,output wire m_axis_tlast,
 output reg [63:0] change_attempted,change_queued,change_dropped,first_loss_tick,
 output reg [31:0] event_sequence,rejected_start,
 output wire [31:0] status,output wire [15:0] fifo_level,
 output reg [15:0] fifo_peak,output wire [2:0] capture_event_count
);
 localparam INIT=8'h10,CHANGE=8'h11,END_REC=8'h12,GAP=8'h13,WATERMARK=8'h14,RESYNC=8'h15;
 wire [7:0] masked_levels=levels&capture_mask;
 wire [128:0] output_word;
 wire room;
 reg active,stopping,gap_pending,resync_pending;
 reg [7:0] previous,stop_levels,active_mask;
 reg [63:0] stop_tick,gap_tick;
 reg [31:0] watermark_age;
 reg [15:0] packet_count;
 wire sampling=active&&!stopping&&capture_run;
 wire changed=sampling&&((levels&active_mask)!=previous);
 reg push;
 reg [7:0] event_type,event_flags,event_data;
 reg [63:0] event_tick;
 wire event_last=(packet_count==PACKET_RECORDS-1)||event_type==END_REC||event_type==WATERMARK||event_type==GAP;
 wire starting=enable&&capture_start&&!active&&(fifo_level==0)&&(capture_mask!=0);
 assign capture_event_count={2'd0,changed}+{2'd0,(push&&event_type!=CHANGE)};
 assign m_axis_tkeep=16'hffff;
 assign {m_axis_tlast,m_axis_tdata}=output_word;
 assign status={25'd0,(rejected_start!=0),(change_dropped!=0),resync_pending,gap_pending,stopping,active,(fifo_level==0)};
 tracegpu_vector_fifo #(.FIFO_BITS(FIFO_BITS)) fifo(
  .clk(clk),.resetn(resetn),.clear(clear),
  .s_data({event_last,event_type,event_flags,event_data,event_sequence,8'hff,event_tick}),
  .s_valid(push),.s_ready(room),.m_data(output_word),.m_valid(m_axis_tvalid),
  .m_ready(m_axis_tready),.level(fifo_level));
 always @*begin
  push=0;event_type=0;event_flags=active_mask;event_data=levels&active_mask;event_tick=timestamp;
  if(resetn&&!clear)begin
   if(starting&&room)begin push=1;event_type=INIT;event_flags=capture_mask;event_data=masked_levels;end
   else if(active&&room)begin
    if(gap_pending)begin push=1;event_type=GAP;event_flags=1;event_data=0;event_tick=gap_tick;end
    else if(resync_pending)begin
     push=1;event_type=RESYNC;
     if(stopping||!capture_run)begin event_tick=stopping?stop_tick:timestamp-64'd1;event_data=stopping?stop_levels:previous;end
    end else if(changed)begin push=1;event_type=CHANGE;end
    else if(stopping)begin push=1;event_type=END_REC;event_tick=stop_tick;event_data=stop_levels;end
    else if(capture_run&&watermark_age>=WATERMARK_TICKS-1)begin push=1;event_type=WATERMARK;end
   end
  end
 end
 always @(posedge clk)begin
  if(!resetn||clear)begin
   active<=0;stopping<=0;gap_pending<=0;resync_pending<=0;active_mask<=0;
   previous<=0;stop_levels<=0;stop_tick<=0;gap_tick<=0;
   watermark_age<=0;packet_count<=0;event_sequence<=0;
   change_attempted<=0;change_queued<=0;change_dropped<=0;first_loss_tick<=0;
   rejected_start<=0;fifo_peak<=0;
  end else begin
   if(push)begin event_sequence<=event_sequence+1'b1;packet_count<=event_last?0:packet_count+1'b1;end
   if(fifo_level>fifo_peak)fifo_peak<=fifo_level;
   if(enable&&capture_start&&!starting&&rejected_start!=32'hffffffff)rejected_start<=rejected_start+1'b1;
   if(starting)begin active<=1;active_mask<=capture_mask;previous<=masked_levels;watermark_age<=0;end
   else if(active)begin
    if(watermark_age<WATERMARK_TICKS)watermark_age<=watermark_age+1'b1;
    if(sampling)previous<=levels&active_mask;
    if(!stopping&&!capture_run)begin stopping<=1;stop_tick<=timestamp-64'd1;stop_levels<=previous;end
    if(changed)begin
     if(change_attempted!=64'hffffffffffffffff)change_attempted<=change_attempted+1'b1;
     if(push&&event_type==CHANGE)begin
      if(change_queued!=64'hffffffffffffffff)change_queued<=change_queued+1'b1;
     end else begin
      if(change_dropped!=64'hffffffffffffffff)change_dropped<=change_dropped+1'b1;
      if(change_dropped==0)first_loss_tick<=timestamp;
      if(!gap_pending&&!resync_pending)begin gap_pending<=1;gap_tick<=timestamp;end
     end
    end
    if(push)begin
     if(event_type==GAP)begin gap_pending<=0;resync_pending<=1;end
     if(event_type==RESYNC)resync_pending<=0;
     if(event_type==WATERMARK)watermark_age<=0;
     if(event_type==END_REC)begin active<=0;stopping<=0;end
    end
   end
  end
 end
endmodule
