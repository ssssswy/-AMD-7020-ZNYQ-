`timescale 1ns/1ps
module tracegpu_trigger_core(
 input wire clk,resetn,clear,restart,enable,
 input wire [63:0] timestamp, input wire [3:0] levels,quality_fault,
 input wire [1:0] combine_mode,input wire [63:0] combine_window,
 input wire [3:0] level_mask,level_value,
 input wire [127:0] slot_cfg,
 input wire [255:0] slot_pattern,slot_mask,slot_minimum,slot_maximum,slot_gap,
 input wire [3:0] uart_valid,uart_frame_error,uart_parity_error,uart_false_start,
 input wire [31:0] uart_data, input wire [255:0] uart_tick,uart_frame_start,
 output wire trigger_pulse,trigger_sticky,
 output wire [63:0] trigger_tick,trigger_start_tick,
 output wire [7:0] trigger_reason,
 output wire [3:0] trigger_sources,trigger_predicates,trigger_levels,
 output wire [255:0] snapshot_ticks,snapshot_starts,
 output wire [31:0] snapshot_data,snapshot_details
);
 wire [3:0] enabled,hits;
 wire [255:0] ticks,starts;
 wire [31:0] reasons,data,details;
 wire [7:0] channels;
 // Register the per-slot channel mux before 64-bit interval arithmetic.
 // Every predicate and the level qualifier use the same delayed sample.
 reg [63:0] sample_tick;
 reg [3:0] sample_levels;
 reg sample_restart;
 reg [3:0] event_levels;
 always @(posedge clk)begin
  sample_tick<=timestamp;sample_levels<=levels;
  if(!resetn)begin sample_restart<=1;event_levels<=0;end
  else begin sample_restart<=restart;event_levels<=sample_levels;end
 end
 genvar g;
 generate for(g=0;g<4;g=g+1)begin:predicate
  wire [31:0] cfg=slot_cfg[g*32+:32];
  wire [3:0] kind=cfg[3:0];wire [1:0] ch=cfg[5:4];
  reg [63:0] bt,bs;
  reg [7:0] bd;
  reg good,broken,uv,fe,pe,fs;
  always @(posedge clk)begin
   bt<=uart_tick[ch*64+:64];bs<=uart_frame_start[ch*64+:64];bd<=uart_data[ch*8+:8];
   good<=uart_valid[ch]&&!uart_frame_error[ch]&&!uart_parity_error[ch]&&!quality_fault[ch];
   broken<=quality_fault[ch]||uart_false_start[ch]||(uart_valid[ch]&&(uart_frame_error[ch]||uart_parity_error[ch]));
   uv<=uart_valid[ch];fe<=uart_frame_error[ch];pe<=uart_parity_error[ch];fs<=uart_false_start[ch];
  end
  wire seq_hit,pulse_hit,timeout_hit;
  wire [63:0] seq_tick,seq_start,pulse_tick,pulse_start,timeout_tick,timeout_start;
  reg err_hit;reg [63:0] err_tick,err_start;reg [7:0] err_reason,tap_data,tap_flags;
  assign enabled[g]=(kind!=0);assign channels[g*2+:2]=ch;
  tracegpu_trigger_sequence seq(.clk(clk),.resetn(resetn),.restart(sample_restart||clear),.enable(enable&&kind==1),
   .length(cfg[15:12]),.pattern(slot_pattern[g*64+:64]),.mask(slot_mask[g*64+:64]),.max_gap(slot_gap[g*64+:64]),
   .byte_valid(good),.byte_data(bd),.break_sequence(broken),.byte_tick(bt),.frame_start(bs),.hit(seq_hit),.hit_tick(seq_tick),.hit_start(seq_start));
  tracegpu_trigger_pulse pulse(.clk(clk),.resetn(resetn),.restart(sample_restart||clear),.enable(enable&&kind==3),
   .level(sample_levels[ch]),.target_level(cfg[10]),.mode(cfg[9:8]),.timestamp(sample_tick),.minimum(slot_minimum[g*64+:64]),.maximum(slot_maximum[g*64+:64]),
   .hit(pulse_hit),.hit_tick(pulse_tick),.hit_start(pulse_start));
  tracegpu_trigger_timeout timeout_engine(.clk(clk),.resetn(resetn),.restart(sample_restart||clear),.enable(enable&&kind==4),
   .level(sample_levels[ch]),.mode(cfg[9:8]),.timestamp(sample_tick),.threshold(slot_minimum[g*64+:64]),.good_byte(good),.byte_tick(bt),
   .hit(timeout_hit),.hit_tick(timeout_tick),.hit_start(timeout_start));
  always @(posedge clk)begin
   if(!resetn||sample_restart||clear)begin err_hit<=0;err_tick<=0;err_start<=0;err_reason<=0;tap_data<=0;tap_flags<=0;end
   else begin
    err_hit<=0;tap_data<=bd;
    tap_flags<={5'd0,fs,uv&&pe,uv&&fe};
    if(enable&&kind==2)begin
     if((cfg[16]&&uv&&fe) || (cfg[17]&&uv&&pe) || (cfg[18]&&fs))begin
      err_hit<=1;err_tick<=bt;err_start<=bs;
      if(cfg[16]&&uv&&fe)err_reason<=2;
      else if(cfg[17]&&uv&&pe)err_reason<=3;
      else err_reason<=4;
     end
    end
   end
  end
  assign hits[g]=(kind==1)?seq_hit:(kind==2)?err_hit:(kind==3)?pulse_hit:(kind==4)?timeout_hit:1'b0;
  assign ticks[g*64+:64]=(kind==1)?seq_tick:(kind==2)?err_tick:(kind==3)?pulse_tick:timeout_tick;
  assign starts[g*64+:64]=(kind==1)?seq_start:(kind==2)?err_start:(kind==3)?pulse_start:timeout_start;
  assign reasons[g*8+:8]=(kind==1)?8'd1:(kind==2)?err_reason:(kind==3)?(8'd5+{6'd0,cfg[9:8]}):((cfg[9:8]==3)?8'd10:8'd9);
  assign data[g*8+:8]=(kind==1||kind==2)?tap_data:8'd0;
  assign details[g*8+:8]=(kind==2)?tap_flags:8'd0;
 end endgenerate
 tracegpu_trigger_combine combine(.clk(clk),.resetn(resetn),.clear(clear),.restart(restart||sample_restart),.enable(enable),
  .mode(combine_mode),.window_ticks(combine_window),.timestamp(sample_tick),.enabled(enabled),.hit(hits),.levels(sample_levels),.event_levels(event_levels),
  .level_mask(level_mask),.level_value(level_value),.hit_ticks(ticks),.hit_starts(starts),.hit_reasons(reasons),.hit_data(data),.hit_details(details),.channels(channels),
  .trigger_pulse(trigger_pulse),.trigger_sticky(trigger_sticky),.trigger_tick(trigger_tick),.trigger_start_tick(trigger_start_tick),
  .trigger_reason(trigger_reason),.trigger_sources(trigger_sources),.trigger_predicates(trigger_predicates),.trigger_levels(trigger_levels),
  .snapshot_ticks(snapshot_ticks),.snapshot_starts(snapshot_starts),.snapshot_data(snapshot_data),.snapshot_details(snapshot_details));
endmodule
