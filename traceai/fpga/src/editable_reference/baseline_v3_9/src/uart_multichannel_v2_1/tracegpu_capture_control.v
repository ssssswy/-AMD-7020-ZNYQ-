`timescale 1ns/1ps
module tracegpu_capture_control(
 input wire aclk,input wire aresetn,
 input wire [5:0] s_axi_awaddr,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire [31:0] s_axi_wdata,input wire [3:0] s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output wire [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,
 input wire [5:0] s_axi_araddr,input wire s_axi_arvalid,output wire s_axi_arready,
 output reg [31:0] s_axi_rdata,output wire [1:0] s_axi_rresp,output reg s_axi_rvalid,input wire s_axi_rready,
 input wire trigger0,input wire trigger1,input wire trigger2,input wire trigger3,
 output wire capture_run0,output wire capture_run1,output wire capture_run2,output wire capture_run3,
 output reg capture_run,output wire capture_clear,output reg capture_start,output reg capture_flush,
 output wire stream_resetn,output reg [63:0] timestamp
);
reg armed,triggered,complete;
reg [1:0] source_id;
reg [3:0] enable_mask;
reg [31:0] post_ticks;
reg [63:0] trigger_time,deadline;
reg [4:0] clear_count;
wire write_fire=s_axi_awvalid&&s_axi_wvalid&&!s_axi_bvalid;
wire [3:0] triggers={trigger3,trigger2,trigger1,trigger0};
wire chosen_trigger=triggers[source_id]&&enable_mask[source_id]&&capture_run;
assign capture_run0=capture_run&&enable_mask[0];
assign capture_run1=capture_run&&enable_mask[1];
assign capture_run2=capture_run&&enable_mask[2];
assign capture_run3=capture_run&&enable_mask[3];
assign s_axi_awready=write_fire;assign s_axi_wready=write_fire;
assign s_axi_bresp=0;assign s_axi_rresp=0;assign s_axi_arready=!s_axi_rvalid;
assign capture_clear=(clear_count!=0);
assign stream_resetn=aresetn&&!capture_clear;
always @(posedge aclk)begin
 if(!aresetn)begin
  capture_run<=0;capture_start<=0;capture_flush<=0;timestamp<=0;
  armed<=0;triggered<=0;complete<=0;source_id<=0;enable_mask<=4'hf;post_ticks<=10000000;
  trigger_time<=0;deadline<=0;clear_count<=0;s_axi_bvalid<=0;s_axi_rvalid<=0;s_axi_rdata<=0;
 end else begin
  timestamp<=timestamp+1'b1;capture_start<=0;capture_flush<=0;
  if(clear_count!=0)clear_count<=clear_count-1'b1;
  if(armed&&!triggered&&chosen_trigger)begin
   triggered<=1;trigger_time<=timestamp;deadline<=timestamp+post_ticks;
  end
  if(armed&&triggered&&timestamp>=deadline)begin
   capture_run<=0;armed<=0;complete<=1;capture_flush<=1;
  end
  if(write_fire)begin
   s_axi_bvalid<=1;
   case(s_axi_awaddr[5:2])
    0:if(s_axi_wstrb[0])begin
     if(s_axi_wdata[3])capture_flush<=1;
     else begin
      capture_run<=s_axi_wdata[0]&&!s_axi_wdata[1];
      capture_start<=s_axi_wdata[0]&&!s_axi_wdata[1];
      armed<=s_axi_wdata[2];triggered<=0;complete<=0;
      if(s_axi_wdata[1])begin clear_count<=16;armed<=0;trigger_time<=0;end
      if(!s_axi_wdata[0]&&!s_axi_wdata[1])capture_flush<=1;
     end
    end
    2:if(s_axi_wstrb[0]&&!capture_run)source_id<=s_axi_wdata[1:0];
    3:if(&s_axi_wstrb)post_ticks<=s_axi_wdata;
    9:if(s_axi_wstrb[0]&&!capture_run)enable_mask<=s_axi_wdata[3:0];
    default:;
   endcase
  end else if(s_axi_bready)s_axi_bvalid<=0;
  if(s_axi_arvalid&&s_axi_arready)begin
   s_axi_rvalid<=1;
   case(s_axi_araddr[5:2])
    0:s_axi_rdata<={31'd0,capture_run};
    1:s_axi_rdata<={27'd0,capture_clear,complete,triggered,armed,capture_run};
    2:s_axi_rdata<={30'd0,source_id};
    3:s_axi_rdata<=post_ticks;
    4:s_axi_rdata<=trigger_time[31:0];
    5:s_axi_rdata<=trigger_time[63:32];
    6:s_axi_rdata<=32'h00020100;
    7:s_axi_rdata<=timestamp[31:0];
    8:s_axi_rdata<=timestamp[63:32];
    9:s_axi_rdata<={28'd0,enable_mask};
    10:s_axi_rdata<=4;
    default:s_axi_rdata<=0;
   endcase
  end else if(s_axi_rready)s_axi_rvalid<=0;
 end
end
endmodule
