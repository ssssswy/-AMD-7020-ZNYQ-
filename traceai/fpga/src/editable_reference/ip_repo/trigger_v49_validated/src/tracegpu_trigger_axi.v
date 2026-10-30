`timescale 1ns/1ps
// v2.9 independent AW/W AXI4-Lite slave. Shadow writes never alter a
// committed capture. The explicit capture_start starts all timeout clocks.
module tracegpu_trigger_axi(
 input wire aclk,aresetn,
 input wire [11:0] s_axi_awaddr,input wire s_axi_awvalid,output wire s_axi_awready,
 input wire [31:0] s_axi_wdata,input wire [3:0] s_axi_wstrb,input wire s_axi_wvalid,output wire s_axi_wready,
 output wire [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,
 input wire [11:0] s_axi_araddr,input wire s_axi_arvalid,output wire s_axi_arready,
 output reg [31:0] s_axi_rdata,output wire [1:0] s_axi_rresp,output reg s_axi_rvalid,input wire s_axi_rready,
 input wire [63:0] timestamp,input wire [7:0] levels,
 input wire capture_start,capture_run,input wire [7:0] capture_channels,
 input wire vector_mode,input wire [3:0] parity_enabled,quality_fault,
 input wire [3:0] uart_valid,uart_frame_error,uart_parity_error,uart_false_start,
 input wire [31:0] uart_data,input wire [255:0] uart_tick,uart_frame_start,
 output reg advanced_active,output wire trigger_pulse,trigger_sticky,
 output wire [63:0] trigger_tick,trigger_start_tick,output wire [31:0] trigger_reason,
 output wire [7:0] trigger_sources,output wire [3:0] trigger_predicates,
 output wire [63:0] configured_post_ticks,output wire [31:0] configured_post_limit
);
 localparam VERSION=32'h00030200;
 reg [31:0] shadow_global,shadow_generation,shadow_capture_id,shadow_limit;
 reg [63:0] shadow_window,shadow_post;
 reg [31:0] shadow_cfg[0:3];
 reg [63:0] shadow_pattern[0:3],shadow_mask[0:3],shadow_minimum[0:3],shadow_maximum[0:3],shadow_gap[0:3];
 reg [31:0] active_global,active_generation,snapshot_generation,active_capture_id,active_limit;
 reg [63:0] active_window,active_post,arm_tick;
 reg [127:0] active_cfg;
 reg [255:0] active_pattern,active_mask,active_minimum,active_maximum,active_gap;
 reg committed,wait_start,armed,config_reject,arm_reject,engine_clear;
 reg [31:0] reject_code;
 reg [3:0] quality_seen;
 reg aw_pending,w_pending;
 reg [11:0] aw_saved;
 reg [31:0] w_saved;
 reg [3:0] strb_saved;
 wire write_fire=aw_pending&&w_pending&&!s_axi_bvalid;
 wire [7:0] triggered_levels;
 wire [7:0] reason_byte;
 wire [255:0] snapshot_ticks,snapshot_starts;
 wire [31:0] snapshot_data,snapshot_details;
 wire restart=wait_start&&capture_start;
 // Decoder taps are registered with their decision-cycle timestamp. Delay
 // raw levels and the timebase equally so qualifiers/deadline activity use
 // that same physical sampling cycle, not the following processing cycle.
 reg [63:0] sample_tick;
 reg [7:0] sample_levels;
 reg sample_restart;
 always @(posedge aclk)begin
  if(!aresetn)begin sample_tick<=0;sample_levels<=0;sample_restart<=0;end
  else begin sample_tick<=timestamp;sample_levels<=levels;sample_restart<=restart;end
 end
 assign trigger_reason={24'd0,reason_byte};
 assign configured_post_ticks=active_post;
 assign configured_post_limit=active_limit;
 assign s_axi_awready=!aw_pending&&!s_axi_bvalid;
 assign s_axi_wready=!w_pending&&!s_axi_bvalid;
 assign s_axi_bresp=2'b00;
 assign s_axi_rresp=2'b00;
 assign s_axi_arready=!s_axi_rvalid;
 function [31:0] merge_strobes;
  input [31:0] original,replacement;input [3:0] strobes;
  integer k;
  begin
   merge_strobes=original;
   for(k=0;k<4;k=k+1)if(strobes[k])merge_strobes[k*8+:8]=replacement[k*8+:8];
  end
 endfunction
 reg [31:0] config_error,arm_error;
 // AXI accepts a new transaction only after the preceding write response.
 // Therefore a shadow update is visible to this register before COMMIT fires.
 reg [31:0] config_error_q;
 always @(posedge aclk)begin
  if(!aresetn)config_error_q<=5;
  else config_error_q<=config_error;
 end
 integer i,j;
 reg any_enabled,any_fixed;
 reg [3:0] kind;
 reg [2:0] channel;
 // COMMIT validates static fields; ARM additionally validates live capture
 // mode/channels/parity. All checks refer to the complete shadow image.
 always @*begin
  config_error=0;any_enabled=0;any_fixed=0;kind=0;channel=0;
  if((shadow_global&32'hff0000fc)!=0 || shadow_global[1:0]==3)config_error=4;
  if(shadow_global[1:0]==2 && shadow_global[15:8]==0)config_error=4;
  for(i=0;i<4;i=i+1)begin
   kind=shadow_cfg[i][3:0];
   if(kind!=0)begin
    any_enabled=1;
    if(kind>4 || (shadow_cfg[i]&32'hfff80880)!=0)config_error=5;
    if(kind==1)begin
     if(shadow_cfg[i][15:12]<1 || shadow_cfg[i][15:12]>8 || shadow_cfg[i][9:8]!=0)config_error=6;
     any_fixed=0;
     for(j=0;j<8;j=j+1)if(j<shadow_cfg[i][15:12])begin
      if(shadow_mask[i][j*8+:8]==8'hff)any_fixed=1;
      else if(shadow_mask[i][j*8+:8]!=0)config_error=7;
     end
     if(!any_fixed)config_error=7;
    end
    if(kind==2 && (shadow_cfg[i][18:16]==0 || shadow_cfg[i][9:8]!=0))config_error=5;
    if((kind==3 || kind==4) && shadow_minimum[i]==0)config_error=8;
    if(kind==3 && shadow_cfg[i][9] && shadow_maximum[i]<shadow_minimum[i])config_error=9;
   end
  end
  if(shadow_global[1:0]!=2 && !any_enabled)config_error=5;
  arm_error=0;
  if(!committed)arm_error=2;
  if(capture_run)arm_error=3;
  if(advanced_active)arm_error=1;
  if((active_global[15:8]&~capture_channels)!=0)arm_error=10;
  for(i=0;i<4;i=i+1)begin
   kind=active_cfg[i*32+:4];channel=active_cfg[i*32+4+:3];
   if(active_global[1:0]!=2 && kind!=0)begin
    if(!capture_channels[channel])arm_error=10;
    if((vector_mode||channel>=4) && (kind==1 || kind==2 || (kind==4 && active_cfg[i*32+8+:2]==3)))arm_error=11;
    if(kind==2 && active_cfg[i*32+17] && (channel>=4||!parity_enabled[channel[1:0]]))arm_error=12;
   end
  end
 end
 tracegpu_trigger_core core(.clk(aclk),.resetn(aresetn),.clear(engine_clear),.restart(sample_restart),.enable(armed&&capture_run),
  .timestamp(sample_tick),.levels(sample_levels),.quality_fault(quality_fault),.combine_mode(active_global[1:0]),.combine_window(active_window),
  .level_mask(active_global[15:8]),.level_value(active_global[23:16]),.slot_cfg(active_cfg),.slot_pattern(active_pattern),.slot_mask(active_mask),
  .slot_minimum(active_minimum),.slot_maximum(active_maximum),.slot_gap(active_gap),.uart_valid(uart_valid),.uart_frame_error(uart_frame_error),
  .uart_parity_error(uart_parity_error),.uart_false_start(uart_false_start),.uart_data(uart_data),.uart_tick(uart_tick),.uart_frame_start(uart_frame_start),
  .trigger_pulse(trigger_pulse),.trigger_sticky(trigger_sticky),.trigger_tick(trigger_tick),.trigger_start_tick(trigger_start_tick),.trigger_reason(reason_byte),
  .trigger_sources(trigger_sources),.trigger_predicates(trigger_predicates),.trigger_levels(triggered_levels),
  .snapshot_ticks(snapshot_ticks),.snapshot_starts(snapshot_starts),.snapshot_data(snapshot_data),.snapshot_details(snapshot_details));
 integer slot_index,slot_word,k;
 always @(posedge aclk)begin
  if(!aresetn)begin
   shadow_global<=0;shadow_generation<=0;shadow_capture_id<=0;shadow_window<=0;shadow_post<=5000000;shadow_limit<=65520;
   active_global<=0;active_generation<=0;snapshot_generation<=0;active_capture_id<=0;active_window<=0;active_post<=0;active_limit<=0;
   active_cfg<=0;active_pattern<=0;active_mask<=0;active_minimum<=0;active_maximum<=0;active_gap<=0;arm_tick<=0;
   committed<=0;wait_start<=0;armed<=0;advanced_active<=0;config_reject<=0;arm_reject<=0;reject_code<=0;quality_seen<=0;engine_clear<=0;
   aw_pending<=0;w_pending<=0;aw_saved<=0;w_saved<=0;strb_saved<=0;s_axi_bvalid<=0;s_axi_rvalid<=0;s_axi_rdata<=0;
   for(k=0;k<4;k=k+1)begin
    shadow_cfg[k]<=0;shadow_pattern[k]<=0;shadow_mask[k]<=0;shadow_minimum[k]<=100;shadow_maximum[k]<=1000;shadow_gap[k]<=0;
   end
  end else begin
   engine_clear<=0;
   if(advanced_active)quality_seen<=quality_seen|quality_fault;
   if(restart)begin wait_start<=0;armed<=1;arm_tick<=timestamp;end
   else if(armed&&!capture_run)armed<=0;
   if(s_axi_awvalid&&s_axi_awready)begin aw_pending<=1;aw_saved<=s_axi_awaddr;end
   if(s_axi_wvalid&&s_axi_wready)begin w_pending<=1;w_saved<=s_axi_wdata;strb_saved<=s_axi_wstrb;end
   if(s_axi_bvalid&&s_axi_bready)s_axi_bvalid<=0;
   if(write_fire)begin
    aw_pending<=0;w_pending<=0;s_axi_bvalid<=1;
    if(aw_saved[1:0]==0)begin
     case(aw_saved)
      12'h008:if(strb_saved[0])begin
       case(w_saved[7:0])
        8'h01:begin
         if(advanced_active || capture_run)begin config_reject<=1;reject_code<=1;end
         else if(config_error_q!=0)begin config_reject<=1;reject_code<=config_error_q;end
         else begin
          active_global<=shadow_global;active_generation<=shadow_generation;active_window<=shadow_window;active_post<=shadow_post;active_limit<=shadow_limit;
          for(k=0;k<4;k=k+1)begin
           active_cfg[k*32+:32]<=shadow_cfg[k];active_pattern[k*64+:64]<=shadow_pattern[k];active_mask[k*64+:64]<=shadow_mask[k];
           active_minimum[k*64+:64]<=shadow_minimum[k];active_maximum[k*64+:64]<=shadow_maximum[k];active_gap[k*64+:64]<=shadow_gap[k];
          end
          committed<=1;config_reject<=0;reject_code<=0;
         end
        end
        8'h02:begin
         if(arm_error!=0)begin arm_reject<=1;reject_code<=arm_error;end
         else begin
          wait_start<=1;armed<=0;advanced_active<=1;active_capture_id<=shadow_capture_id;snapshot_generation<=active_generation;arm_tick<=0;
          engine_clear<=1;quality_seen<=0;arm_reject<=0;reject_code<=0;
         end
        end
        8'h04:begin wait_start<=0;armed<=0;advanced_active<=0;end
        8'h08:begin
         wait_start<=0;armed<=0;advanced_active<=0;engine_clear<=1;quality_seen<=0;arm_reject<=0;config_reject<=0;reject_code<=0;
         arm_tick<=0;snapshot_generation<=0;active_capture_id<=0;
        end
        default:begin config_reject<=1;reject_code<=13;end
       endcase
      end
      12'h010:shadow_generation<=merge_strobes(shadow_generation,w_saved,strb_saved);
      12'h014:shadow_capture_id<=merge_strobes(shadow_capture_id,w_saved,strb_saved);
      12'h020:shadow_global<=merge_strobes(shadow_global,w_saved,strb_saved);
      12'h028:shadow_window[31:0]<=merge_strobes(shadow_window[31:0],w_saved,strb_saved);
      12'h02c:shadow_window[63:32]<=merge_strobes(shadow_window[63:32],w_saved,strb_saved);
      12'h030:shadow_post[31:0]<=merge_strobes(shadow_post[31:0],w_saved,strb_saved);
      12'h034:shadow_post[63:32]<=merge_strobes(shadow_post[63:32],w_saved,strb_saved);
      12'h038:shadow_limit<=merge_strobes(shadow_limit,w_saved,strb_saved);
      default:if(aw_saved>=12'h100 && aw_saved<12'h200)begin
       slot_index=aw_saved[7:6];slot_word=aw_saved[5:2];
       case(slot_word)
        0:shadow_cfg[slot_index]<=merge_strobes(shadow_cfg[slot_index],w_saved,strb_saved);
        1:shadow_pattern[slot_index][31:0]<=merge_strobes(shadow_pattern[slot_index][31:0],w_saved,strb_saved);
        2:shadow_pattern[slot_index][63:32]<=merge_strobes(shadow_pattern[slot_index][63:32],w_saved,strb_saved);
        3:shadow_mask[slot_index][31:0]<=merge_strobes(shadow_mask[slot_index][31:0],w_saved,strb_saved);
        4:shadow_mask[slot_index][63:32]<=merge_strobes(shadow_mask[slot_index][63:32],w_saved,strb_saved);
        5:shadow_minimum[slot_index][31:0]<=merge_strobes(shadow_minimum[slot_index][31:0],w_saved,strb_saved);
        6:shadow_minimum[slot_index][63:32]<=merge_strobes(shadow_minimum[slot_index][63:32],w_saved,strb_saved);
        7:shadow_maximum[slot_index][31:0]<=merge_strobes(shadow_maximum[slot_index][31:0],w_saved,strb_saved);
        8:shadow_maximum[slot_index][63:32]<=merge_strobes(shadow_maximum[slot_index][63:32],w_saved,strb_saved);
        9:shadow_gap[slot_index][31:0]<=merge_strobes(shadow_gap[slot_index][31:0],w_saved,strb_saved);
        10:shadow_gap[slot_index][63:32]<=merge_strobes(shadow_gap[slot_index][63:32],w_saved,strb_saved);
        default:;
       endcase
      end
     endcase
    end
   end
   if(s_axi_rvalid&&s_axi_rready)s_axi_rvalid<=0;
   if(s_axi_arvalid&&s_axi_arready)begin
    s_axi_rvalid<=1;s_axi_rdata<=0;
    if(s_axi_araddr[1:0]==0)begin
     case(s_axi_araddr)
      12'h000:s_axi_rdata<=VERSION;
      12'h004:s_axi_rdata<=32'h0000003f;
      12'h00c:s_axi_rdata<={24'd0,advanced_active,(quality_seen!=0),arm_reject,config_reject,trigger_sticky,armed,wait_start,committed};
      12'h010:s_axi_rdata<=shadow_generation;
      12'h014:s_axi_rdata<=shadow_capture_id;
      12'h018:s_axi_rdata<=reject_code;
      12'h020:s_axi_rdata<=shadow_global;
      12'h028:s_axi_rdata<=shadow_window[31:0];12'h02c:s_axi_rdata<=shadow_window[63:32];
      12'h030:s_axi_rdata<=shadow_post[31:0];12'h034:s_axi_rdata<=shadow_post[63:32];
      12'h038:s_axi_rdata<=shadow_limit;
      12'h040:s_axi_rdata<={4'd0,triggered_levels,trigger_predicates,trigger_sources,reason_byte};
      12'h044:s_axi_rdata<={28'd0,quality_seen};
      12'h048:s_axi_rdata<=trigger_tick[31:0];12'h04c:s_axi_rdata<=trigger_tick[63:32];
      12'h050:s_axi_rdata<=trigger_start_tick[31:0];12'h054:s_axi_rdata<=trigger_start_tick[63:32];
      12'h058:s_axi_rdata<=snapshot_generation;12'h05c:s_axi_rdata<=active_capture_id;
      12'h080:s_axi_rdata<=snapshot_data;
      12'h084:s_axi_rdata<=snapshot_details;
      12'h088:s_axi_rdata<=arm_tick[31:0];12'h08c:s_axi_rdata<=arm_tick[63:32];
      12'h0b0:s_axi_rdata<=8;12'h0b4:s_axi_rdata<=4;
      12'h0b8:s_axi_rdata<=4;12'h0bc:s_axi_rdata<=VERSION;
      default:begin
       if(s_axi_araddr>=12'h060 && s_axi_araddr<12'h080)s_axi_rdata<=snapshot_ticks[s_axi_araddr[4:2]*32+:32];
       else if(s_axi_araddr>=12'h090 && s_axi_araddr<12'h0b0)s_axi_rdata<=snapshot_starts[(s_axi_araddr-12'h090)*8+:32];
       else if(s_axi_araddr>=12'h100 && s_axi_araddr<12'h200)begin
        case(s_axi_araddr[5:2])
         0:s_axi_rdata<=shadow_cfg[s_axi_araddr[7:6]];
         1:s_axi_rdata<=shadow_pattern[s_axi_araddr[7:6]][31:0];2:s_axi_rdata<=shadow_pattern[s_axi_araddr[7:6]][63:32];
         3:s_axi_rdata<=shadow_mask[s_axi_araddr[7:6]][31:0];4:s_axi_rdata<=shadow_mask[s_axi_araddr[7:6]][63:32];
         5:s_axi_rdata<=shadow_minimum[s_axi_araddr[7:6]][31:0];6:s_axi_rdata<=shadow_minimum[s_axi_araddr[7:6]][63:32];
         7:s_axi_rdata<=shadow_maximum[s_axi_araddr[7:6]][31:0];8:s_axi_rdata<=shadow_maximum[s_axi_araddr[7:6]][63:32];
         9:s_axi_rdata<=shadow_gap[s_axi_araddr[7:6]][31:0];10:s_axi_rdata<=shadow_gap[s_axi_araddr[7:6]][63:32];
         default:s_axi_rdata<=0;
        endcase
       end
      end
     endcase
    end
   end
  end
 end
endmodule
