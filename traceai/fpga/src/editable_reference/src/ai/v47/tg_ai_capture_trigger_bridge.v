`timescale 1ns/1ps
// Existing native trigger remains intact when AI mode is not selected.
module tg_ai_capture_trigger_bridge(
 input wire ai_selected,ai_trigger,input wire[63:0]ai_tick,ai_post_ticks,input wire[31:0]ai_post_limit,
 input wire native_active,native_trigger,input wire[63:0]native_tick,native_post_ticks,input wire[31:0]native_post_limit,
 output wire advanced_active,advanced_trigger,output wire[63:0]advanced_trigger_tick,advanced_post_ticks,output wire[31:0]advanced_post_limit);
 assign advanced_active=ai_selected?1'b1:native_active;
 assign advanced_trigger=ai_selected?ai_trigger:native_trigger;
 assign advanced_trigger_tick=ai_selected?ai_tick:native_tick;
 assign advanced_post_ticks=ai_selected?ai_post_ticks:native_post_ticks;
 assign advanced_post_limit=ai_selected?ai_post_limit:native_post_limit;
endmodule
