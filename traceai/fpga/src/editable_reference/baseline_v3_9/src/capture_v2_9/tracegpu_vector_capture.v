`timescale 1ns/1ps
// Captures already synchronized/filtered inputs. No external pin is driven.
// Record: {type, flags, data, sequence, 8'hff, timestamp} (128 bits).
module tracegpu_vector_capture #(
 parameter FIFO_BITS=10, parameter WATERMARK_TICKS=100000,
 parameter PACKET_RECORDS=256
)(
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF m_axis, ASSOCIATED_RESET resetn" *)
 input wire clk, input wire resetn, input wire clear,
 input wire enable, input wire capture_start, input wire capture_run,
 input wire [63:0] timestamp, input wire [3:0] levels,
 output reg [127:0] m_axis_tdata, output reg m_axis_tvalid,
 input wire m_axis_tready, output wire [15:0] m_axis_tkeep,
 output reg m_axis_tlast,
 output reg [63:0] change_attempted, output reg [63:0] change_queued,
 output reg [63:0] change_dropped, output reg [63:0] first_loss_tick,
 output reg [31:0] event_sequence, output reg [31:0] rejected_start,
 output wire [31:0] status, output wire [15:0] fifo_level,
 output reg [15:0] fifo_peak,output wire [2:0] capture_event_count
);
 localparam DEPTH=(1<<FIFO_BITS);
 localparam INIT=8'h10, CHANGE=8'h11, END_REC=8'h12,
            GAP=8'h13, WATERMARK=8'h14, RESYNC=8'h15;
 (* ram_style="block" *) reg [128:0] queue[0:DEPTH-1];
 reg [FIFO_BITS-1:0] wr_ptr,rd_ptr;
 reg [FIFO_BITS:0] count;
 reg active,stopping,gap_pending,resync_pending;
 reg [3:0] previous,stop_levels;
 reg [63:0] stop_tick,gap_tick;
 reg [31:0] watermark_age;
 reg [15:0] packet_count;
 wire room=(count<DEPTH);
 wire pop=(count!=0)&&(!m_axis_tvalid||m_axis_tready);
 wire sampling=active&&!stopping&&capture_run;
 wire changed=sampling&&(levels!=previous);
 // Count every produced record, including heartbeat and loss-recovery records.
 // Attempted CHANGE is counted even when the queue reports a loss.
 reg push;
 reg [7:0] event_type,event_flags,event_data;
 reg [63:0] event_tick;
 assign capture_event_count={2'd0,changed}+{2'd0,(push&&event_type!=CHANGE)};
 wire event_last=(packet_count==PACKET_RECORDS-1)||event_type==END_REC||
                 event_type==WATERMARK||event_type==GAP;
 wire starting=enable&&capture_start&&!active&&(count==0)&&!m_axis_tvalid;
 assign m_axis_tkeep=16'hffff;
 assign fifo_level=count+m_axis_tvalid;
 assign status={25'd0,(rejected_start!=0),(change_dropped!=0),resync_pending,
                gap_pending,stopping,active,((count==0)&&!m_axis_tvalid)};

 // A CHANGE never yields its only write slot to a heartbeat. GAP and RESYNC
 // retain ordering behind all pre-loss records in the queue.
 always @* begin
  push=0;event_type=0;event_flags=8'h0f;event_data={4'd0,levels};event_tick=timestamp;
  if(starting)begin push=1;event_type=INIT;end
  else if(active&&room)begin
   if(gap_pending)begin push=1;event_type=GAP;event_flags=1;event_data=0;event_tick=gap_tick;end
   else if(resync_pending)begin
    push=1;event_type=RESYNC;
    if(stopping||!capture_run)begin event_tick=stopping?stop_tick:timestamp-64'd1;event_data={4'd0,(stopping?stop_levels:previous)};end
   end
   else if(changed)begin push=1;event_type=CHANGE;end
   else if(stopping)begin push=1;event_type=END_REC;event_tick=stop_tick;event_data={4'd0,stop_levels};end
   else if(capture_run&&watermark_age>=WATERMARK_TICKS-1)begin push=1;event_type=WATERMARK;end
  end
 end

 always @(posedge clk)begin
  if(!resetn||clear)begin
   wr_ptr<=0;rd_ptr<=0;count<=0;m_axis_tvalid<=0;m_axis_tdata<=0;m_axis_tlast<=0;
   active<=0;stopping<=0;gap_pending<=0;resync_pending<=0;
   previous<=0;stop_levels<=0;stop_tick<=0;gap_tick<=0;
   watermark_age<=0;packet_count<=0;event_sequence<=0;
   change_attempted<=0;change_queued<=0;change_dropped<=0;first_loss_tick<=0;
   rejected_start<=0;fifo_peak<=0;
  end else begin
   if(pop)begin
    {m_axis_tlast,m_axis_tdata}<=queue[rd_ptr];rd_ptr<=rd_ptr+1'b1;m_axis_tvalid<=1;
   end else if(m_axis_tready)m_axis_tvalid<=0;
   if(push)begin
    queue[wr_ptr]<={event_last,event_type,event_flags,event_data,event_sequence,8'hff,event_tick};
    wr_ptr<=wr_ptr+1'b1;event_sequence<=event_sequence+1'b1;
    packet_count<=event_last?0:packet_count+1'b1;
   end
   case({push,pop})2'b10:count<=count+1'b1;2'b01:count<=count-1'b1;default:;endcase
   if(fifo_level>fifo_peak)fifo_peak<=fifo_level;
   if(enable&&capture_start&&!starting)rejected_start<=rejected_start+1'b1;
   if(starting)begin
    active<=1;previous<=levels;watermark_age<=0;
   end else if(active)begin
    if(watermark_age<WATERMARK_TICKS)watermark_age<=watermark_age+1'b1;
    if(sampling)previous<=levels;
    if(!stopping&&!capture_run)begin stopping<=1;stop_tick<=timestamp-64'd1;stop_levels<=previous;end
    if(changed)begin
     change_attempted<=change_attempted+1'b1;
     if(push&&event_type==CHANGE)change_queued<=change_queued+1'b1;
     else begin
      change_dropped<=change_dropped+1'b1;
      if(change_dropped==0)first_loss_tick<=timestamp;
      // An existing recovery interval absorbs further losses. A change on
      // RESYNC's tick is included in that boundary, not a fabricated edge.
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
