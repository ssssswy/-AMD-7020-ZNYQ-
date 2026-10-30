`timescale 1ns/1ps
// Single outstanding AXI4-Lite transaction per direction. AW and W are independent.
// No DDR master: PS owns a stopped snapshot and pushes one binary token at a time.
module tg_ai_axi #(parameter ROM_DIR="model/r11") (
 (* X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF s_axi, ASSOCIATED_RESET resetn, FREQ_HZ 100000000" *)
 input wire clk,
 (* X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *) input wire resetn,
 input wire [15:0] s_axi_awaddr,input wire [2:0] s_axi_awprot,input wire s_axi_awvalid,
 output wire s_axi_awready,
 input wire [31:0] s_axi_wdata,input wire [3:0] s_axi_wstrb,input wire s_axi_wvalid,
 output wire s_axi_wready,
 output reg [1:0] s_axi_bresp,output reg s_axi_bvalid,input wire s_axi_bready,
 input wire [15:0] s_axi_araddr,input wire [2:0] s_axi_arprot,input wire s_axi_arvalid,
 output wire s_axi_arready,
 output reg [31:0] s_axi_rdata,output reg [1:0] s_axi_rresp,
 output reg s_axi_rvalid,input wire s_axi_rready
);
 localparam [31:0] MAGIC=32'h54474149,ABI=32'h00040300;
 reg awheld,wheld;reg[15:0] wa;reg[31:0] wd;reg[3:0] ws;
 reg[2:0] resetting;
 reg core_wr,core_start;reg[12:0] core_addr;reg[7:0] core_data;reg[2:0] model;
 reg[1:0] read_pending;reg[6:0] result_addr;wire signed[31:0] result_data;
 wire[12:0] expected=model==0?6144:(model==3?384:(model==4?2752:(model==5?2432:1280)));
 wire core_busy,core_done,core_fault;wire[159:0] core_logits;
 reg running,done_sticky,fault_sticky;
 reg[7:0] error_code;reg[12:0] loaded;
 reg[31:0] job_id,active_id,result_id,limit_cycles,cycles,completions,timeouts;
 reg[159:0] result_logits;
 wire core_resetn=resetn&&(resetting==0);
 tg_ai_r11_core #(.ROM_DIR(ROM_DIR)) core(
  .clk(clk),.resetn(core_resetn),.wr(core_wr),.data(core_data),.model_id(model),
  .addr(core_addr),.start(core_start),
  .busy(core_busy),.done(core_done),.fault(core_fault),.logits(core_logits),.result_addr(result_addr),.result_data(result_data));
 assign s_axi_awready=resetn&&!awheld&&!s_axi_bvalid;
 assign s_axi_wready=resetn&&!wheld&&!s_axi_bvalid;
 assign s_axi_arready=resetn&&!s_axi_rvalid&&!read_pending;
 wire commit=awheld&&wheld&&!s_axi_bvalid;
 // State-changing commands are blocked at a terminal-event boundary. Soft reset always wins.
 wire terminal=running&&(core_done||core_fault||cycles>=limit_cycles-1);
 always @(posedge clk)begin
  if(!resetn)begin
   awheld<=0;wheld<=0;wa<=0;wd<=0;ws<=0;s_axi_bvalid<=0;s_axi_bresp<=0;read_pending<=0;result_addr<=0;
   s_axi_rvalid<=0;s_axi_rresp<=0;s_axi_rdata<=0;
   resetting<=3;core_wr<=0;core_start<=0;core_addr<=0;core_data<=0;model<=0;
   running<=0;done_sticky<=0;fault_sticky<=0;error_code<=0;loaded<=0;
   job_id<=0;active_id<=0;result_id<=0;limit_cycles<=5000000;cycles<=0;
   completions<=0;timeouts<=0;result_logits<=0;
  end else begin
   core_wr<=0;core_start<=0;
   if(resetting!=0)resetting<=resetting-1'b1;
   if(s_axi_bvalid&&s_axi_bready)s_axi_bvalid<=0;
   if(s_axi_rvalid&&s_axi_rready)s_axi_rvalid<=0;
   if(read_pending!=0)begin read_pending<=read_pending-1'b1;if(read_pending==1)begin s_axi_rvalid<=1;s_axi_rdata<=result_data;s_axi_rresp<=done_sticky?0:2;end end
   if(s_axi_awvalid&&s_axi_awready)begin awheld<=1;wa<=s_axi_awaddr;end
   if(s_axi_wvalid&&s_axi_wready)begin wheld<=1;wd<=s_axi_wdata;ws<=s_axi_wstrb;end
   if(running)begin
    cycles<=cycles+1'b1;
    if(core_done)begin
     running<=0;done_sticky<=1;result_logits<=core_logits;result_id<=active_id;
     completions<=completions+1'b1;
    end else if(core_fault||cycles>=limit_cycles-1)begin
     running<=0;done_sticky<=0;fault_sticky<=1;
     error_code<=core_fault?8'd5:8'd6;resetting<=3;loaded<=0;
     if(!core_fault)timeouts<=timeouts+1'b1;
    end
   end
   if(commit)begin
    awheld<=0;wheld<=0;s_axi_bvalid<=1;s_axi_bresp<=0;
    if(wa[1:0]!=0 || !(wa==16'h10||wa==16'h20||wa==16'h24||wa==16'h2c||wa==16'h3c))
     s_axi_bresp<=2;
    else if(ws==0)begin end // AXI all-zero strobe has no effect.
    else if(ws!=4'hf)s_axi_bresp<=2; // Atomic word ABI: partial writes rejected.
    else case(wa)
     16'h10:begin
      if(wd==2)begin
       resetting<=3;running<=0;loaded<=0;done_sticky<=0;fault_sticky<=0;
       error_code<=0;result_logits<=0;result_id<=0;cycles<=0;
      end else if(resetting!=0||terminal) s_axi_bresp<=2;
      else if(wd==4)begin
       if(running)s_axi_bresp<=2;
       else begin done_sticky<=0;result_logits<=0;result_id<=0;end
      end else if(wd==1)begin
       if(running||loaded!=expected||fault_sticky)begin
        s_axi_bresp<=2;
        // A rejected second start must not poison the legitimate in-flight job.
        if(!running)begin fault_sticky<=1;error_code<=2;end
       end else begin
        core_start<=1;running<=1;loaded<=0;active_id<=job_id;
        done_sticky<=0;result_logits<=0;result_id<=0;cycles<=0;
       end
      end else s_axi_bresp<=2;
     end
     16'h20:begin
      if(running||resetting!=0||fault_sticky||loaded>=expected||wd>255)begin
       s_axi_bresp<=2;
       if(!running&&resetting==0&&!terminal)begin fault_sticky<=1;error_code<=4;end
      end else begin core_wr<=1;core_addr<=loaded;core_data<=wd[7:0];loaded<=loaded+1'b1;end
     end
     16'h24:if(running||resetting!=0||terminal)s_axi_bresp<=2;else job_id<=wd;
     16'h3c:if(running||resetting!=0||terminal||wd>5)s_axi_bresp<=2;else begin model<=wd[2:0];loaded<=0;resetting<=3;fault_sticky<=0;done_sticky<=0;result_id<=0;end
     16'h2c:if(running||resetting!=0||terminal||wd<16||wd>10000000)s_axi_bresp<=2;else limit_cycles<=wd;
    endcase
   end
   if(s_axi_arvalid&&s_axi_arready)begin
    s_axi_rvalid<=1;s_axi_rresp<=0;s_axi_rdata<=0;
    if(s_axi_araddr[1:0]!=0)s_axi_rresp<=2;
    else if(s_axi_araddr>=16'h100 && s_axi_araddr<=16'h20c)begin
     if(!done_sticky||model<4)s_axi_rresp<=2;
     else begin s_axi_rvalid<=0;read_pending<=2;result_addr<=(s_axi_araddr-16'h100)>>2;end
    end else case(s_axi_araddr)
     16'h00:s_axi_rdata<=MAGIC;
     16'h04:s_axi_rdata<=ABI;
     16'h08:s_axi_rdata<=32'h4a77aa8b;
     16'h0c:s_axi_rdata<=3; // Six-model PL backend.
     16'h14:s_axi_rdata<={27'b0,(resetting!=0),(loaded==expected&&!fault_sticky&&!running),fault_sticky,done_sticky,running};
     16'h18:s_axi_rdata<={24'b0,error_code};
     16'h1c:s_axi_rdata<={19'b0,loaded};
     16'h24:s_axi_rdata<=job_id;
     16'h28:s_axi_rdata<=result_id;
     16'h2c:s_axi_rdata<=limit_cycles;
     16'h3c:s_axi_rdata<={29'b0,model};
     16'h54:s_axi_rdata<=model>=4?68:5;
     16'h30:s_axi_rdata<=cycles;
     16'h34:s_axi_rdata<=completions;
     16'h38:s_axi_rdata<=timeouts;
     16'h40:s_axi_rdata<=result_logits[31:0];
     16'h44:s_axi_rdata<=result_logits[63:32];
     16'h48:s_axi_rdata<=result_logits[95:64];
     16'h4c:s_axi_rdata<=result_logits[127:96];
     16'h50:s_axi_rdata<=result_logits[159:128];
     16'h80:s_axi_rdata<=32'h4a77aa8b;
     16'h84:s_axi_rdata<=32'h9a66af2c;
     16'h88:s_axi_rdata<=32'h74de721b;
     16'h8c:s_axi_rdata<=32'h5cae0075;
     16'h90:s_axi_rdata<=32'ha943bc0a;
     16'h94:s_axi_rdata<=32'hed3b5dc5;
     16'h98:s_axi_rdata<=32'ha5512052;
     16'h9c:s_axi_rdata<=32'h4f5a0cf0;
     default:s_axi_rresp<=2;
    endcase
   end
  end
 end
endmodule
