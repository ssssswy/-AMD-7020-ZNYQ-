`timescale 1ns/1ps
// Four 32-word records. Sequence+epoch+generation must be verified by reader.
// Protect the first-trigger slot until the next capture; no 1024-bit FF copy.
module tg_ai_evidence_store(
 input logic clk,resetn,clear,new_capture,start,protect,
 output logic[15:0]sequence_id,allocation_id,output logic[4:0]word_index,input logic[31:0]word_data,
 output logic busy,done,input logic read_enable,input logic[6:0]read_address,output logic[31:0]read_data);
 (* ram_style="block" *)logic[31:0]memory[0:127];
 logic[1:0]slot,locked_slot;logic locked;logic[15:0]next_sequence;logic[3:0]valid;
 assign allocation_id=next_sequence;
 function automatic[1:0]next_slot(input[1:0]a,input l,input[1:0]b);
  logic[1:0]v;begin v=a+1;next_slot=l&&v==b?v+1:v;end
 endfunction
 always_ff @(posedge clk)begin
  if(read_enable)read_data<=valid[read_address[6:5]]?memory[read_address]:0;
  if(busy)memory[{slot,word_index}]<=word_data;
  if(!resetn||clear)begin slot<=0;locked_slot<=0;locked<=0;sequence_id<=0;next_sequence<=1;word_index<=0;busy<=0;done<=0;valid<=0;end
  else begin
   done<=0;
   if(new_capture)begin locked<=0;valid<=0;end
   if(start&&!busy)begin sequence_id<=next_sequence;next_sequence<=next_sequence==65535?1:next_sequence+1;word_index<=0;busy<=1;valid[slot]<=0;end
   if(start&&protect)begin locked<=1;locked_slot<=slot;end
   if(busy)begin
    if(word_index==31)begin busy<=0;done<=1;valid[slot]<=1;slot<=next_slot(slot,locked,locked_slot);end
    else word_index<=word_index+1;
   end
  end
 end
endmodule
