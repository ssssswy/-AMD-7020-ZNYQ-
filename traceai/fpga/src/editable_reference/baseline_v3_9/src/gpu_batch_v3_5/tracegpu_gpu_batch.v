`timescale 1ns/1ps
// v3.5 phase 1: one outstanding AXI burst; ordered per-descriptor colors;
// synchronous simple-dual-port BRAM FIFO; frozen, saturating job counters.
module tracegpu_gpu_batch #(parameter C_S_AXI_ADDR_WIDTH=8)(
 input wire s_axi_aclk,input wire s_axi_aresetn,
 input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_awaddr,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire [31:0] s_axi_wdata,input wire [3:0] s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output reg [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,
 input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_araddr,input wire s_axi_arvalid,output wire s_axi_arready,
 output reg [31:0] s_axi_rdata,output wire [1:0] s_axi_rresp,output reg s_axi_rvalid,input wire s_axi_rready,
 output reg [31:0] m_axi_awaddr,output reg [7:0] m_axi_awlen,output wire [2:0] m_axi_awsize,
 output wire [1:0] m_axi_awburst,output reg m_axi_awvalid,input wire m_axi_awready,
 output reg [31:0] m_axi_wdata,output reg [3:0] m_axi_wstrb,output reg m_axi_wlast,
 output reg m_axi_wvalid,input wire m_axi_wready,input wire [1:0] m_axi_bresp,
 input wire m_axi_bvalid,output reg m_axi_bready);
localparam VERSION=32'h00030500,CAPABILITY=32'h0000003f;
localparam ST_IDLE=0,ST_LOAD=1,ST_PREP=2,ST_ADDR=3,ST_AW=4,ST_W=5,ST_B=6,ST_FETCH=7;
reg [31:0] canvas,stride,color,expected,frame_id,mode,desc_color;
reg [31:0] active_canvas,active_stride,active_color,active_expected,active_frame;
reg active_colored;
reg [15:0] desc_x,desc_y,desc_w,desc_h;
reg [31:0] error_code,cycles,accepted,executed;
reg busy,done,sealed;reg [2:0] state;
reg [C_S_AXI_ADDR_WIDTH-1:0] write_addr;
reg [31:0] write_data;reg [3:0] write_strobe;reg aw_held,w_held;
wire write_fire=aw_held&&w_held&&!s_axi_bvalid;
wire [5:0] write_index=write_addr[7:2];
wire any_strobe=|write_strobe;
wire start_request=write_fire&&write_index==2&&write_strobe[0]&&write_data[0];
wire start_legal=!busy&&expected!=0&&expected<=2187&&canvas[1:0]==0&&stride>=800;
wire start_fire=start_request&&start_legal;
wire seal_fire=write_fire&&write_index==2&&write_strobe[0]&&write_data[1]&&busy;
wire config_write=write_index==3||write_index==4||write_index==5||write_index==6||write_index==9||write_index==17;
wire commit_request=write_fire&&write_index==10&&any_strobe;
wire geometry_legal=desc_w!=0&&desc_h!=0&&({1'b0,desc_x}+{1'b0,desc_w})<=17'd800&&({1'b0,desc_y}+{1'b0,desc_h})<=17'd480;
reg [8:0] wr_ptr,rd_ptr;reg [9:0] fifo_count;
wire push_fire=commit_request&&busy&&!sealed&&fifo_count<512&&accepted<active_expected&&geometry_legal;
wire pop_fire=busy&&state==ST_LOAD&&fifo_count!=0;
wire bad_write=(start_request&&!start_legal)||(write_fire&&any_strobe&&config_write&&busy)||(commit_request&&!push_fire);
wire [10:0] next_fifo_count={1'b0,fifo_count}+(push_fire?11'd1:11'd0)-(pop_fire?11'd1:11'd0);
// No reset on memory or its read port: this is deliberately a BRAM inference
// template, not 512 individually reset registers or asynchronous LUT RAM.
(* ram_style="block" *) reg [95:0] fifo_mem[0:511];
reg [95:0] fifo_read;
always @(posedge s_axi_aclk) begin
 if(push_fire)fifo_mem[wr_ptr]<={active_colored?desc_color:active_color,desc_h,desc_w,desc_y,desc_x};
 if(pop_fire)fifo_read<=fifo_mem[rd_ptr];
end
reg [15:0] x,y,w,h,row,remaining,xoff,burst_words,beat;
reg [31:0] rectangle_color,pixel_base,row_address;
wire [31:0] next_address=row_address+({16'd0,xoff}<<2);
wire [10:0] words_to_4k=(13'h1000-{1'b0,next_address[11:0]})>>2;
wire [15:0] capped_burst=remaining>256?16'd256:remaining;
wire [15:0] next_burst=capped_burst>words_to_4k?{5'd0,words_to_4k}:capped_burst;
reg [31:0] fifo_peak,fifo_empty,aw_stall,w_stall,b_wait,write_bytes,bursts,w_active;
reg perf_overflow;
wire perf_valid=done&&!busy&&!perf_overflow&&error_code==0;
assign s_axi_awready=!aw_held&&!s_axi_bvalid;
assign s_axi_wready=!w_held&&!s_axi_bvalid;
assign s_axi_arready=!s_axi_rvalid;
assign s_axi_rresp=2'b00;
assign m_axi_awsize=3'd2;assign m_axi_awburst=2'b01;
integer lane;
always @(posedge s_axi_aclk) begin
 if(!s_axi_aresetn)begin
  aw_held<=0;w_held<=0;write_addr<=0;write_data<=0;write_strobe<=0;s_axi_bvalid<=0;s_axi_bresp<=0;
  canvas<=0;stride<=800;color<=32'hff2dd2ff;expected<=0;frame_id<=0;mode<=0;
  desc_x<=0;desc_y<=0;desc_w<=0;desc_h<=0;desc_color<=0;
 end else begin
  if(s_axi_awvalid&&s_axi_awready)begin write_addr<=s_axi_awaddr;aw_held<=1;end
  if(s_axi_wvalid&&s_axi_wready)begin write_data<=s_axi_wdata;write_strobe<=s_axi_wstrb;w_held<=1;end
  if(s_axi_bvalid&&s_axi_bready)s_axi_bvalid<=0;
  if(write_fire)begin
   aw_held<=0;w_held<=0;s_axi_bvalid<=1;s_axi_bresp<=bad_write?2'b10:2'b00;
   if(!busy)begin
    case(write_index)
     3:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])canvas[lane*8+:8]<=write_data[lane*8+:8];
     4:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])stride[lane*8+:8]<=write_data[lane*8+:8];
     5:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])color[lane*8+:8]<=write_data[lane*8+:8];
     6:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])expected[lane*8+:8]<=write_data[lane*8+:8];
     9:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])frame_id[lane*8+:8]<=write_data[lane*8+:8];
     17:if(write_strobe[0])mode<={31'd0,write_data[0]};
     default:;
    endcase
   end
   case(write_index)
    7:begin if(write_strobe[0])desc_x[7:0]<=write_data[7:0];if(write_strobe[1])desc_x[15:8]<=write_data[15:8];if(write_strobe[2])desc_y[7:0]<=write_data[23:16];if(write_strobe[3])desc_y[15:8]<=write_data[31:24];end
    8:begin if(write_strobe[0])desc_w[7:0]<=write_data[7:0];if(write_strobe[1])desc_w[15:8]<=write_data[15:8];if(write_strobe[2])desc_h[7:0]<=write_data[23:16];if(write_strobe[3])desc_h[15:8]<=write_data[31:24];end
    16:for(lane=0;lane<4;lane=lane+1)if(write_strobe[lane])desc_color[lane*8+:8]<=write_data[lane*8+:8];
    default:;
   endcase
  end
 end
end
always @(posedge s_axi_aclk) begin
 if(!s_axi_aresetn)begin s_axi_rvalid<=0;s_axi_rdata<=0;end
 else begin
  if(s_axi_rvalid&&s_axi_rready)s_axi_rvalid<=0;
  if(s_axi_arvalid&&s_axi_arready)begin
   case(s_axi_araddr[7:2])
    0:s_axi_rdata<=VERSION;
    1:s_axi_rdata<={3'd0,fifo_count,15'd0,sealed,(error_code!=0),done,busy};
    3:s_axi_rdata<=canvas;4:s_axi_rdata<=stride;5:s_axi_rdata<=color;6:s_axi_rdata<=expected;
    7:s_axi_rdata<={desc_y,desc_x};8:s_axi_rdata<={desc_h,desc_w};
    9:s_axi_rdata<=busy?active_frame:frame_id;11:s_axi_rdata<=accepted;12:s_axi_rdata<=executed;13:s_axi_rdata<=error_code;
    14:s_axi_rdata<=cycles;15:s_axi_rdata<=CAPABILITY;16:s_axi_rdata<=desc_color;17:s_axi_rdata<=mode;
    18:s_axi_rdata<={28'd0,(error_code!=0),busy,perf_overflow,perf_valid};
    19:s_axi_rdata<=fifo_peak;20:s_axi_rdata<=cycles;21:s_axi_rdata<=fifo_empty;
    22:s_axi_rdata<=aw_stall;23:s_axi_rdata<=w_stall;24:s_axi_rdata<=b_wait;
    25:s_axi_rdata<=write_bytes;26:s_axi_rdata<=bursts;27:s_axi_rdata<=w_active;
    default:s_axi_rdata<=0;
   endcase
   s_axi_rvalid<=1;
  end
 end
end
always @(posedge s_axi_aclk)begin
 if(!s_axi_aresetn)begin
  cycles<=0;fifo_peak<=0;fifo_empty<=0;aw_stall<=0;w_stall<=0;b_wait<=0;write_bytes<=0;bursts<=0;w_active<=0;perf_overflow<=0;
 end else if(start_fire)begin
  cycles<=0;fifo_peak<=0;fifo_empty<=0;aw_stall<=0;w_stall<=0;b_wait<=0;write_bytes<=0;bursts<=0;w_active<=0;perf_overflow<=0;
 end else if(busy)begin
  if(cycles!=32'hffffffff)cycles<=cycles+1'b1;else perf_overflow<=1;
  if(next_fifo_count>fifo_peak)fifo_peak<={21'd0,next_fifo_count};
  if(state==ST_LOAD&&fifo_count==0&&!sealed)begin if(fifo_empty!=32'hffffffff)fifo_empty<=fifo_empty+1'b1;else perf_overflow<=1;end
  if(m_axi_awvalid&&!m_axi_awready)begin if(aw_stall!=32'hffffffff)aw_stall<=aw_stall+1'b1;else perf_overflow<=1;end
  if(m_axi_wvalid&&!m_axi_wready)begin if(w_stall!=32'hffffffff)w_stall<=w_stall+1'b1;else perf_overflow<=1;end
  if(m_axi_bready&&!m_axi_bvalid)begin if(b_wait!=32'hffffffff)b_wait<=b_wait+1'b1;else perf_overflow<=1;end
  if(m_axi_wvalid&&m_axi_wready)begin
   if(write_bytes<=32'hfffffffb)write_bytes<=write_bytes+4;else begin write_bytes<=32'hffffffff;perf_overflow<=1;end
   if(w_active!=32'hffffffff)w_active<=w_active+1'b1;else perf_overflow<=1;
  end
  if(m_axi_awvalid&&m_axi_awready)begin if(bursts!=32'hffffffff)bursts<=bursts+1'b1;else perf_overflow<=1;end
 end
end
always @(posedge s_axi_aclk)begin
 if(!s_axi_aresetn)begin
  busy<=0;done<=0;sealed<=0;state<=ST_IDLE;error_code<=0;accepted<=0;executed<=0;wr_ptr<=0;rd_ptr<=0;fifo_count<=0;
  active_canvas<=0;active_stride<=0;active_color<=0;active_expected<=0;active_frame<=0;active_colored<=0;
  m_axi_awvalid<=0;m_axi_wvalid<=0;m_axi_bready<=0;m_axi_wstrb<=4'hf;m_axi_wlast<=0;
  m_axi_awaddr<=0;m_axi_awlen<=0;m_axi_wdata<=0;
  x<=0;y<=0;w<=0;h<=0;row<=0;remaining<=0;xoff<=0;burst_words<=0;beat<=0;rectangle_color<=0;pixel_base<=0;row_address<=0;
 end else if(start_fire)begin
  busy<=1;done<=0;sealed<=0;error_code<=0;accepted<=0;executed<=0;wr_ptr<=0;rd_ptr<=0;fifo_count<=0;state<=ST_LOAD;
  active_canvas<=canvas;active_stride<=stride;active_color<=color;active_expected<=expected;active_frame<=frame_id;active_colored<=mode[0];
 end else begin
  if(bad_write)begin error_code<=32'h02;done<=0;end
  if(seal_fire)sealed<=1;
  if(push_fire)begin wr_ptr<=wr_ptr+1'b1;accepted<=accepted+1'b1;end
  if(pop_fire)rd_ptr<=rd_ptr+1'b1;
  if(push_fire||pop_fire)fifo_count<=next_fifo_count[9:0];
  case(state)
   ST_IDLE:;
   ST_LOAD:begin
    if(fifo_count!=0)state<=ST_FETCH;
    else if(sealed)begin
     busy<=0;done<=executed==active_expected&&accepted==active_expected&&error_code==0&&!bad_write;
     if(error_code==0&&!bad_write&&(executed!=active_expected||accepted!=active_expected))error_code<=32'h03;
     state<=ST_IDLE;
    end
   end
   ST_FETCH:begin x<=fifo_read[15:0];y<=fifo_read[31:16];w<=fifo_read[47:32];h<=fifo_read[63:48];rectangle_color<=fifo_read[95:64];state<=ST_PREP;end
   ST_PREP:begin pixel_base<=y*active_stride+x;row<=0;xoff<=0;remaining<=w;state<=ST_ADDR;end
   ST_ADDR:begin row_address<=active_canvas+(pixel_base<<2);state<=ST_AW;end
   ST_AW:begin
    if(!m_axi_awvalid)begin burst_words<=next_burst;beat<=0;m_axi_awaddr<=next_address;m_axi_awlen<=next_burst-1'b1;m_axi_awvalid<=1;end
    else if(m_axi_awready)begin m_axi_awvalid<=0;m_axi_wdata<=rectangle_color;m_axi_wstrb<=4'hf;m_axi_wvalid<=1;m_axi_wlast<=burst_words==1;state<=ST_W;end
   end
   ST_W:if(m_axi_wvalid&&m_axi_wready)begin
    if(m_axi_wlast)begin m_axi_wvalid<=0;m_axi_wlast<=0;m_axi_bready<=1;state<=ST_B;end
    else begin beat<=beat+1'b1;m_axi_wlast<=beat+2>=burst_words;end
   end
   ST_B:if(m_axi_bvalid&&m_axi_bready)begin
    m_axi_bready<=0;
    if(m_axi_bresp!=0)begin error_code<=32'h04;busy<=0;done<=0;state<=ST_IDLE;end
    else if(remaining>burst_words)begin remaining<=remaining-burst_words;xoff<=xoff+burst_words;state<=ST_AW;end
    else if(row+1<h)begin row<=row+1'b1;row_address<=row_address+(active_stride<<2);xoff<=0;remaining<=w;state<=ST_AW;end
    else begin executed<=executed+1'b1;state<=ST_LOAD;end
   end
   default:begin error_code<=32'h05;done<=0;busy<=0;state<=ST_IDLE;end
  endcase
 end
end
endmodule
