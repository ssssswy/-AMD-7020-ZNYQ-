`timescale 1ns/1ps
// Exact closed-edge budget evidence; separate from neural classification.
// Canonical SPI map = SCK,CS,MOSI,MISO; I2C = SCL,SDA. Physical polarity is
// retained here. START/STOP, NACK and clock stretching are not rule faults.
module tg_ai_edge_rules(
 input logic clk,resetn,clear,armed,enable,capture_start,capture_run,input_ready,
 input logic[63:0]timestamp,input logic[7:0]levels,
 input logic[1:0]protocol_mode,input logic[2:0]channel,input logic[11:0]bus_map,
 input logic cpol,cpha,cs_high,input logic[31:0]epoch,config_generation,
 input logic[31:0]glitch_ticks,phase_ticks,setup_ticks,hold_ticks,
 output logic event_valid,output logic[511:0]event_data,input logic event_ready,
 output logic partial,pending,output logic[31:0]dropped);
 logic running,known;logic[7:0]previous;logic[1:0]mode;logic[2:0]map[0:3];logic[2:0]lines;
 logic cp,ca,cs;logic[31:0]ep,cg,gl,ph,su,ho;
 logic[63:0]edge_tick[0:3],data_tick[0:1],sample_tick;
 logic[3:0]edge_known;logic[1:0]data_known;logic sample_known;
 logic[7:0]stage_valid;logic[63:0]stage_begin[0:7],stage_end[0:7],stage_width[0:7];
 logic[7:0]qvalid,qready,put;logic[162:0]qdata[0:7],put_data[0:7];logic[31:0]qdropped[0:7];
 logic[2:0]round_robin,selected;logic found;
 logic[32:0]drop_pair[0:3];logic[33:0]drop_half[0:1];logic[34:0]drop_total;
 wire change_cs=(previous[map[1]]!=levels[map[1]]);
 wire selected_active=mode!=1||(previous[map[1]]==cs&&levels[map[1]]==cs);
 wire clock_changed=previous[map[0]]!=levels[map[0]];
 wire sample_edge=clock_changed&&((previous[map[0]]==cp)==!ca);
 wire cancel=clear||!armed||capture_start;
 always_comb begin
  pending=event_valid||(|qvalid)||(|stage_valid);
  found=0;selected=round_robin;qready=0;
  for(integer n=0;n<8;n=n+1)begin
   if(!found&&qvalid[(round_robin+n)&7])begin found=1;selected=(round_robin+n)&7;end
  end
  if(found&&(!event_valid||event_ready)&&!cancel)qready[selected]=1;
  put=0;
  for(integer j=0;j<8;j=j+1)begin
   put_data[j]=0;
   if(j<4)begin
    if(stage_valid[j]&&gl!=0&&stage_width[j]<{32'b0,gl})begin
     put[j]=1;put_data[j]={3'd1,stage_begin[j],stage_end[j],gl};
    end else if(j==0&&stage_valid[j]&&mode!=0&&ph!=0&&stage_width[j]<{32'b0,ph})begin
     put[j]=1;put_data[j]={3'd2,stage_begin[j],stage_end[j],ph};
    end
   end else if(j<6)begin
    put[j]=stage_valid[j]&&su!=0&&stage_width[j]<{32'b0,su};
    put_data[j]={3'd3,stage_begin[j],stage_end[j],su};
   end else begin
    put[j]=stage_valid[j]&&ho!=0&&stage_width[j]<{32'b0,ho};
    put_data[j]={3'd4,stage_begin[j],stage_end[j],ho};
   end
  end
 end
 // Statistics may lag producers by four clocks; no acquisition decision uses
 // this adder tree. Register every level instead of eight chained additions.
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin
   for(integer j=0;j<4;j=j+1)drop_pair[j]<=0;
   drop_half[0]<=0;drop_half[1]<=0;drop_total<=0;dropped<=0;
  end else begin
   for(integer j=0;j<4;j=j+1)drop_pair[j]<={1'b0,qdropped[2*j]}+{1'b0,qdropped[2*j+1]};
   drop_half[0]<={1'b0,drop_pair[0]}+{1'b0,drop_pair[1]};drop_half[1]<={1'b0,drop_pair[2]}+{1'b0,drop_pair[3]};
   drop_total<={1'b0,drop_half[0]}+{1'b0,drop_half[1]};
   dropped<=|drop_total[34:32]?32'hffffffff:drop_total[31:0];
  end
 end
 for(genvar j=0;j<8;j=j+1)begin:lanes
  tg_ai_rule_lane lane(.clk,.resetn(resetn&&!clear),.clear(cancel),.s_valid(put[j]),.s_data(put_data[j]),
   .m_valid(qvalid[j]),.m_data(qdata[j]),.m_ready(qready[j]),.dropped(qdropped[j]));
 end
 always_ff @(posedge clk)begin
  if(!resetn||clear)begin
   running<=0;known<=0;previous<=0;mode<=0;lines<=0;cp<=0;ca<=0;cs<=0;ep<=0;cg<=0;gl<=0;ph<=0;su<=0;ho<=0;
   edge_known<=0;data_known<=0;sample_known<=0;sample_tick<=0;stage_valid<=0;partial<=0;
   event_valid<=0;event_data<=0;round_robin<=0;
   for(integer j=0;j<4;j=j+1)begin map[j]<=0;edge_tick[j]<=0;end
   for(integer j=0;j<2;j=j+1)data_tick[j]<=0;
   for(integer j=0;j<8;j=j+1)begin stage_begin[j]<=0;stage_end[j]<=0;stage_width[j]<=0;end
  end else begin
   stage_valid<=0;
   if(dropped!=0)partial<=1;
   if(event_valid&&event_ready)event_valid<=0;
   if(found&&(!event_valid||event_ready)&&!cancel)begin
    event_valid<=1;event_data<=0;round_robin<=selected+1'b1;
    event_data[95:64]<=ep;event_data[127:96]<=cg;
    event_data[159:128]<={5'b0,(selected<4?map[selected]:map[2+selected[0]]),5'b0,qdata[selected][162:160],8'd4,(6'b0+mode+8'd1)};
    // Values for rules are unsigned physical ticks, NOT neural logits.
    event_data[191:160]<=qdata[selected][95:32]-qdata[selected][159:96];
    event_data[223:192]<=qdata[selected][31:0];event_data[255:224]<=32'h100;
    event_data[319:256]<=qdata[selected][159:96];event_data[383:320]<=qdata[selected][159:96];
    event_data[447:384]<=qdata[selected][95:32];event_data[479:448]<=32'h4a77aa8b;event_data[511:480]<=1;
   end
   if(cancel)begin running<=0;known<=0;stage_valid<=0;event_valid<=0;edge_known<=0;data_known<=0;sample_known<=0;end
   if(capture_start&&armed)begin
    running<=enable&&protocol_mode!=3;known<=input_ready;previous<=levels;mode<=protocol_mode;
    lines<=protocol_mode==0?1:protocol_mode==1?4:2;
    for(integer j=0;j<4;j=j+1)map[j]<=protocol_mode==0?channel:bus_map[j*3+:3];
    cp<=cpol;ca<=cpha;cs<=cs_high;ep<=epoch;cg<=config_generation;
    gl<=glitch_ticks;ph<=phase_ticks;su<=setup_ticks;ho<=hold_ticks;
   end else if(running&&armed&&capture_run)begin
    previous<=levels;
    if(!input_ready)begin known<=0;edge_known<=0;data_known<=0;sample_known<=0;partial<=1;end
    else if(!known)begin known<=1;edge_known<=0;data_known<=0;sample_known<=0;end
    else begin
     if(mode==1&&change_cs)begin edge_known<=0;data_known<=0;sample_known<=0;end
     for(integer j=0;j<4;j=j+1)begin
      if(j<lines&&previous[map[j]]!=levels[map[j]])begin
       stage_begin[j]<=edge_tick[j];stage_end[j]<=timestamp;stage_width[j]<=timestamp-edge_tick[j];
       stage_valid[j]<=selected_active&&edge_known[j]&&timestamp>edge_tick[j]&&!(mode==1&&change_cs)&&
        !(mode==2&&j==1&&(previous[map[0]]||levels[map[0]]));
       edge_tick[j]<=timestamp;
       edge_known[j]<=selected_active&&!(mode==2&&j==1&&(previous[map[0]]||levels[map[0]]));
      end
     end
     if(mode==2&&levels[map[0]])edge_known[1]<=0;
     if(mode==1&&selected_active)begin
      if(sample_edge)begin
       sample_tick<=timestamp;sample_known<=1;
       for(integer j=0;j<2;j=j+1)begin
        stage_valid[4+j]<=data_known[j]||(previous[map[2+j]]!=levels[map[2+j]]);
        stage_begin[4+j]<=previous[map[2+j]]!=levels[map[2+j]]?timestamp:data_tick[j];
        stage_end[4+j]<=timestamp;
        stage_width[4+j]<=previous[map[2+j]]!=levels[map[2+j]]?0:timestamp-data_tick[j];
       end
      end
      for(integer j=0;j<2;j=j+1)begin
       if(previous[map[2+j]]!=levels[map[2+j]])begin
        stage_valid[6+j]<=sample_known||sample_edge;
        stage_begin[6+j]<=sample_edge?timestamp:sample_tick;
        stage_end[6+j]<=timestamp;stage_width[6+j]<=sample_edge?0:timestamp-sample_tick;
        data_tick[j]<=timestamp;data_known[j]<=1;
       end
      end
     end
    end
   end
  end
 end
endmodule
