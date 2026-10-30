`timescale 1ns/1ps
// AXI4-Lite config/status/events only. Deliberately no PS waveform/feature port.
`include "v46_uart_contract.vh"
module tg_ai_stream_axi #(parameter ROM_DIR="model/v46_uart")(
 (* X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF s_axi, ASSOCIATED_RESET resetn, FREQ_HZ 100000000" *)input logic clk,
 (* X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *)input logic resetn,
 input logic[15:0]s_axi_awaddr,input logic[2:0]s_axi_awprot,input logic s_axi_awvalid,output logic s_axi_awready,
 input logic[31:0]s_axi_wdata,input logic[3:0]s_axi_wstrb,input logic s_axi_wvalid,output logic s_axi_wready,
 output logic[1:0]s_axi_bresp,output logic s_axi_bvalid,input logic s_axi_bready,
 input logic[15:0]s_axi_araddr,input logic[2:0]s_axi_arprot,input logic s_axi_arvalid,output logic s_axi_arready,
 output logic[31:0]s_axi_rdata,output logic[1:0]s_axi_rresp,output logic s_axi_rvalid,input logic s_axi_rready,
 input logic capture_clear,capture_start,capture_run,capture_vector_mode,input_ready,
 input logic[63:0]timestamp,input logic[7:0]levels,capture_mask,
 output logic ai_selected,ai_trigger,output logic[63:0]ai_tick,ai_post_ticks,output logic[31:0]ai_post_limit);
 localparam[31:0]MAGIC=32'h41493436,ABI=32'h00040801,CAPS=32'h00003fff;
 logic awheld,wheld;logic[15:0]wa;logic[31:0]wd;logic[3:0]ws;
 logic armed,committed,soft_clear;logic[31:0]shadow[0:13],cfg[0:13];
 logic[31:0]snapshot[0:17],snapshot_id,rejected;
 logic ev,epop,eo,active,locked,partial,cf;logic[511:0]event_data;logic[6:0]event_level;
 logic[63:0]seen,dropped,skipped;logic[31:0]checked,failed,window_cycles,last_error,events_dropped,rules_dropped,events_merged;
 logic[31:0]status;logic shadow_valid,writable,config_write_allowed;
 wire commit_write=awheld&&wheld&&!s_axi_bvalid;
 wire full_write=ws==4'hf&&wa[1:0]==0;
 logic[2:0]trigger_mode;logic[63:0]post_ticks;logic[31:0]post_limit,store_generation;
 logic[31:0]and_overwritten;logic window_valid;logic[63:0]window_begin,window_end;
 logic trigger_rule_valid,trigger_cnn_valid,protect_evidence;logic[511:0]trigger_rule_packet,trigger_cnn_packet;
 logic[15:0]trigger_evidence_id;logic[31:0]trigger_status,trigger_read_data,evidence_read_data;
 logic evidence_pending,evidence_read_enable,trigger_write_allowed;
 wire trigger_command=commit_write&&full_write&&wa==16'h338&&wd<=2&&
  (wd==0||(!capture_run&&!locked&&!active))&&(wd!=1||trigger_mode!=4||(cfg[0]==0&&cfg[9][0]));
 assign evidence_read_enable=s_axi_arvalid&&s_axi_arready&&s_axi_araddr[1:0]==0&&s_axi_araddr>=16'h400&&s_axi_araddr<=16'h5fc;
 tg_ai_trigger_gate trigger_gate(.clk,.resetn,.clear(capture_clear||soft_clear),.capture_start(capture_start&&capture_vector_mode),
  .capture_run(capture_run&&capture_vector_mode),.monitor_armed(armed),.config_fault(cf),.timestamp,.epoch(cfg[7]),.generation(cfg[8]),.store_generation,
  .mode(trigger_mode),.post_ticks,.post_limit,.command_valid(trigger_command),.command(wd[1:0]),
  .rule_valid(trigger_rule_valid),.rule_packet(trigger_rule_packet),.cnn_valid(trigger_cnn_valid),.cnn_packet(trigger_cnn_packet),
  .window_valid,.window_begin,.window_end,.and_overwritten,.cnn_evidence_id(trigger_evidence_id),.read_index((s_axi_araddr-16'h340)>>2),.read_data(trigger_read_data),.status(trigger_status),
  .selected(ai_selected),.trigger_pulse(ai_trigger),.protect_evidence,.trigger_tick(ai_tick),.capture_post_ticks(ai_post_ticks),.capture_post_limit(ai_post_limit));
 always_comb begin
  s_axi_awready=resetn&&!awheld&&!s_axi_bvalid;s_axi_wready=resetn&&!wheld&&!s_axi_bvalid;
  s_axi_arready=resetn&&!s_axi_rvalid&&!evidence_pending;
  trigger_write_allowed=!capture_run&&!locked&&!active&&!(trigger_status&7);
  status={22'b0,capture_run,input_ready,capture_vector_mode,eo,ev,partial,cf,locked,active,armed};
  config_write_allowed=!armed&&!capture_run&&!locked&&!active;
  writable=(wa>=16'h324&&wa<=16'h338&&wa[1:0]==0)||wa==16'h10||wa==16'h18||wa==16'h44||wa==16'h244||
   (((wa>=16'h20&&wa<=16'h40)||(wa>=16'h48&&wa<=16'h58))&&wa[1:0]==0);
  shadow_valid=shadow[0]<=2&&shadow[1]>0&&shadow[1]<=65535&&shadow[2]>0&&shadow[2]<=12500000&&
   shadow[3][31:15]==0&&shadow[4][31:4]==0&&shadow[5][31:4]==0&&shadow[6][31:30]==0&&
   !(shadow[4][2]&&!shadow[4][1])&&shadow[9]<=1;
  if(shadow[0]==0)shadow_valid=shadow_valid&&shadow[2]>=4800&&shadow[2]<=921600;
  else shadow_valid=shadow_valid&&shadow[6][7:0]!=0&&shadow[6][15:8]!=0&&shadow[6][18:16]<=4&&shadow[6][21:19]<=4;
  epop=commit_write&&full_write&&wa==16'h244&&ev&&wd==event_data[31:0]&&!capture_clear&&!soft_clear;
 end
 tg_ai_stream_top #(.ROM_DIR(ROM_DIR))monitor(.clk,.resetn,.clear(capture_clear||soft_clear),.armed,
  .capture_start(capture_start&&capture_vector_mode),.capture_run(capture_run&&capture_vector_mode),.input_ready,
  .timestamp,.levels,.capture_mask,.period_ticks(cfg[1]),.rate_hz(cfg[2]),.epoch(cfg[7]),.config_generation(cfg[8]),
  .channel(cfg[3][2:0]),.protocol_mode(cfg[0][1:0]),.bus_map(cfg[3][14:3]),
  .bus_cpol(cfg[5][0]),.bus_cpha(cfg[5][1]),.bus_word16(cfg[5][2]),.spi_cs_high(cfg[5][3]),
  .bus_setup(cfg[6][18:16]),.bus_hold(cfg[6][21:19]),.bus_glitch(cfg[6][7:0]),.bus_clock(cfg[6][15:8]),.bus_timeout(cfg[6][29:22]),
  .data8(cfg[4][0]),.parity_enable(cfg[4][1]),.parity_odd(cfg[4][2]),.stop2(cfg[4][3]),
  .rules_enable(cfg[9][0]),.rule_glitch_ticks(cfg[10]),.rule_phase_ticks(cfg[11]),.rule_setup_ticks(cfg[12]),.rule_hold_ticks(cfg[13]),
  .event_valid(ev),.event_data,.event_ready(epop),.event_level,.event_overflow(eo),.events_dropped,
  .samples_seen(seen),.samples_dropped(dropped),.windows_skipped(skipped),.windows_checked(checked),.windows_failed(failed),
  .last_window_cycles(window_cycles),.last_error,.active,.window_locked(locked),.partial,.config_fault(cf),.rules_dropped,.events_merged,
  .trigger_window_valid(window_valid),.trigger_window_begin(window_begin),.trigger_window_end(window_end),
  .trigger_rule_valid,.trigger_cnn_valid,.trigger_rule_packet,.trigger_cnn_packet,.trigger_evidence_id,.protect_evidence,
  .evidence_read_enable,.evidence_read_address((s_axi_araddr-16'h400)>>2),.evidence_read_data);
 always_ff @(posedge clk)begin
  if(!resetn)begin
   awheld<=0;wheld<=0;wa<=0;wd<=0;ws<=0;s_axi_bvalid<=0;s_axi_bresp<=0;s_axi_rvalid<=0;s_axi_rresp<=0;s_axi_rdata<=0;
   trigger_mode<=0;post_ticks<=0;post_limit<=0;store_generation<=0;evidence_pending<=0;
   armed<=0;committed<=0;soft_clear<=0;snapshot_id<=0;rejected<=0;
   for(integer i=0;i<14;i=i+1)begin shadow[i]<=0;cfg[i]<=0;end
   for(integer i=0;i<18;i=i+1)snapshot[i]<=0;
  end else begin
   soft_clear<=0;
   if(evidence_pending)begin evidence_pending<=0;s_axi_rvalid<=1;s_axi_rresp<=0;s_axi_rdata<=evidence_read_data;end
   if(s_axi_bvalid&&s_axi_bready)s_axi_bvalid<=0;
   if(s_axi_rvalid&&s_axi_rready)s_axi_rvalid<=0;
   if(s_axi_awvalid&&s_axi_awready)begin awheld<=1;wa<=s_axi_awaddr;end
   if(s_axi_wvalid&&s_axi_wready)begin wheld<=1;wd<=s_axi_wdata;ws<=s_axi_wstrb;end
   if(commit_write)begin
    awheld<=0;wheld<=0;s_axi_bvalid<=1;s_axi_bresp<=0;
    if(wa[1:0]!=0||!writable)begin s_axi_bresp<=2;rejected<=rejected+1;end
    else if(ws==0)begin end
    else if(ws!=4'hf)begin s_axi_bresp<=2;rejected<=rejected+1;end
    else if(wa>=16'h324&&wa<=16'h338)begin
     if(wa==16'h338)begin
      if(!trigger_command)begin s_axi_bresp<=2;rejected<=rejected+1;end
     end else if(!trigger_write_allowed)begin s_axi_bresp<=2;rejected<=rejected+1;end
     else case(wa)
      16'h324:if(wd<=4)trigger_mode<=wd[2:0];else begin s_axi_bresp<=2;rejected<=rejected+1;end
      16'h328:post_ticks[31:0]<=wd;16'h32c:post_ticks[63:32]<=wd;
      16'h330:post_limit<=wd;16'h334:store_generation<=wd;
      default:begin s_axi_bresp<=2;rejected<=rejected+1;end
     endcase
    end else if((wa>=16'h20&&wa<=16'h40)||(wa>=16'h48&&wa<=16'h58))begin
     if(!config_write_allowed)begin s_axi_bresp<=2;rejected<=rejected+1;end
     else if(wa<=16'h40)shadow[(wa-16'h20)>>2]<=wd;
     else shadow[(wa-16'h24)>>2]<=wd;
    end else case(wa)
     16'h10:begin
      if(wd==0)armed<=0;
      else if(wd==2)begin soft_clear<=1;armed<=0;end
      else if(wd==1&&committed&&!capture_run&&!locked&&!cf)armed<=1;
      else begin s_axi_bresp<=2;rejected<=rejected+1;end
     end
     16'h18:if(wd!=1)begin s_axi_bresp<=2;rejected<=rejected+1;end
      else begin
       snapshot_id<=snapshot_id+1;snapshot[0]<=seen[31:0];snapshot[1]<=seen[63:32];snapshot[2]<=dropped[31:0];snapshot[3]<=dropped[63:32];
       snapshot[4]<=skipped[31:0];snapshot[5]<=skipped[63:32];snapshot[6]<=checked;snapshot[7]<=failed;snapshot[8]<=window_cycles;snapshot[9]<=last_error;
       snapshot[10]<={25'b0,event_level};snapshot[11]<=events_dropped;snapshot[12]<=status;snapshot[13]<=cfg[8];snapshot[14]<=cfg[7];snapshot[15]<=snapshot_id+1;
       snapshot[16]<=rules_dropped;snapshot[17]<=events_merged;
      end
     16'h44:if(wd!=1||!config_write_allowed||!shadow_valid)begin s_axi_bresp<=2;rejected<=rejected+1;end
      else begin for(integer i=0;i<14;i=i+1)cfg[i]<=shadow[i];committed<=1;end
     // Stale ACKs are normal asynchronous clear races, not AXI bus faults.
     16'h244:if(!ev||wd!=event_data[31:0]||capture_clear||soft_clear)rejected<=rejected+1;
     default:begin s_axi_bresp<=2;rejected<=rejected+1;end
    endcase
   end
   if(s_axi_arvalid&&s_axi_arready)begin
    s_axi_rvalid<=1;s_axi_rresp<=0;s_axi_rdata<=0;
    if(s_axi_araddr[1:0]!=0)s_axi_rresp<=2;
    else if(evidence_read_enable)begin s_axi_rvalid<=0;evidence_pending<=1;end
    else if(s_axi_araddr>=16'h340&&s_axi_araddr<=16'h3bc)s_axi_rdata<=trigger_read_data;
    else if(s_axi_araddr>=16'h200&&s_axi_araddr<=16'h23c)begin
     // Empty/cleared head reads return zero so ARM polling cannot data-abort.
     if(ev&&!capture_clear&&!soft_clear)s_axi_rdata<=event_data[((s_axi_araddr-16'h200)>>2)*32+:32];
    end else if(s_axi_araddr>=16'h80&&s_axi_araddr<=16'hc4)s_axi_rdata<=snapshot[(s_axi_araddr-16'h80)>>2];
    else if(s_axi_araddr>=16'h120&&s_axi_araddr<=16'h140)s_axi_rdata<=cfg[(s_axi_araddr-16'h120)>>2];
    else if(s_axi_araddr>=16'h20&&s_axi_araddr<=16'h40)s_axi_rdata<=shadow[(s_axi_araddr-16'h20)>>2];
    else if(s_axi_araddr>=16'h148&&s_axi_araddr<=16'h158)s_axi_rdata<=cfg[(s_axi_araddr-16'h124)>>2];
    else if(s_axi_araddr>=16'h48&&s_axi_araddr<=16'h58)s_axi_rdata<=shadow[(s_axi_araddr-16'h24)>>2];
    else case(s_axi_araddr)
     16'h00:s_axi_rdata<=MAGIC;16'h04:s_axi_rdata<=ABI;16'h08:s_axi_rdata<=CAPS;16'h0c:s_axi_rdata<=4;
     16'h10:s_axi_rdata<={23'b0,committed,7'b0,armed};16'h14:s_axi_rdata<=status;16'h18:s_axi_rdata<=snapshot_id;16'h1c:s_axi_rdata<=rejected;
     16'h248:s_axi_rdata<={25'b0,event_level};16'h24c:s_axi_rdata<=events_dropped;
     16'h260:s_axi_rdata<=rules_dropped;16'h264:s_axi_rdata<=events_merged;
     16'h320:s_axi_rdata<=32'h00040801;16'h324:s_axi_rdata<={29'b0,trigger_mode};
     16'h3c0:s_axi_rdata<=and_overwritten;
     16'h328:s_axi_rdata<=post_ticks[31:0];16'h32c:s_axi_rdata<=post_ticks[63:32];16'h330:s_axi_rdata<=post_limit;
     16'h334:s_axi_rdata<=store_generation;16'h338:s_axi_rdata<=0;16'h33c:s_axi_rdata<=trigger_status;
     16'h300:s_axi_rdata<=`TG_AI_UART_HASH0;16'h304:s_axi_rdata<=`TG_AI_UART_HASH1;16'h308:s_axi_rdata<=`TG_AI_UART_HASH2;16'h30c:s_axi_rdata<=`TG_AI_UART_HASH3;
     16'h310:s_axi_rdata<=`TG_AI_UART_HASH4;16'h314:s_axi_rdata<=`TG_AI_UART_HASH5;16'h318:s_axi_rdata<=`TG_AI_UART_HASH6;16'h31c:s_axi_rdata<=`TG_AI_UART_HASH7;
     default:s_axi_rresp<=2;
    endcase
   end
   if(capture_clear||soft_clear)begin snapshot_id<=snapshot_id+1;for(integer i=0;i<18;i=i+1)snapshot[i]<=0;end
  end
 end
endmodule
