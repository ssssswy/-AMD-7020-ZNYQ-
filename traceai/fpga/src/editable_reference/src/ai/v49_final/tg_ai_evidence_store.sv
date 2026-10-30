`timescale 1ns/1ps
// Same four synchronous 32-word records as v4.7. Protect by explicit sequence
// after an AND decision, including after the record has finished writing.
module tg_ai_evidence_store(
 input logic clk,resetn,clear,new_capture,start,protect,
 input logic[15:0]protect_sequence_id,
 output logic[15:0]sequence_id,allocation_id,output logic[4:0]word_index,input logic[31:0]word_data,
 output logic busy,done,input logic read_enable,input logic[6:0]read_address,output logic[31:0]read_data);
 (* ram_style="block" *)logic[31:0]memory[0:127];
 logic[1:0]slot,locked_slot;logic locked;logic[15:0]next_sequence;logic[3:0]valid;
 logic[15:0]record_sequence[0:3];
 logic protect_found,protect_new,lock_now;logic[1:0]protect_slot,base_slot,allocate_slot,lock_slot;
 assign allocation_id=next_sequence;
 function automatic[1:0]next_slot(input[1:0]a,input l,input[1:0]b);
  logic[1:0]v;begin v=a+1;next_slot=l&&v==b?v+1:v;end
 endfunction
 always_comb begin
  base_slot=locked&&slot==locked_slot?next_slot(slot,locked,locked_slot):slot;
  protect_found=0;protect_slot=0;
  for(integer i=0;i<4;i=i+1)begin
   if(protect&&protect_sequence_id!=0&&record_sequence[i]==protect_sequence_id&&
      (valid[i]||(busy&&slot==i)))begin protect_found=1;protect_slot=i;end
  end
  protect_new=protect&&protect_sequence_id!=0&&start&&!busy&&protect_sequence_id==next_sequence;
  if(protect_new)begin protect_found=1;protect_slot=base_slot;end
  lock_now=locked||protect_found;lock_slot=protect_found?protect_slot:locked_slot;
  allocate_slot=protect_found&&!protect_new&&base_slot==protect_slot?next_slot(base_slot,1'b1,protect_slot):base_slot;
 end
 always_ff @(posedge clk)begin
  if(read_enable)read_data<=valid[read_address[6:5]]?memory[read_address]:0;
  if(busy)memory[{slot,word_index}]<=word_data;
  if(!resetn||clear)begin
   slot<=0;locked_slot<=0;locked<=0;sequence_id<=0;next_sequence<=1;word_index<=0;busy<=0;done<=0;valid<=0;
   for(integer i=0;i<4;i=i+1)record_sequence[i]<=0;
  end else begin
   done<=0;
   if(new_capture)begin locked<=0;valid<=0;end
   if(start&&!busy)begin
    slot<=allocate_slot;record_sequence[allocate_slot]<=next_sequence;
    sequence_id<=next_sequence;next_sequence<=next_sequence==65535?1:next_sequence+1;
    word_index<=0;busy<=1;valid[allocate_slot]<=0;
   end
   if(protect_found&&!new_capture)begin locked<=1;locked_slot<=protect_slot;end
   if(busy)begin
    if(word_index==31)begin busy<=0;done<=1;valid[slot]<=1;slot<=next_slot(slot,lock_now,lock_slot);end
    else word_index<=word_index+1;
   end
  end
 end
endmodule
