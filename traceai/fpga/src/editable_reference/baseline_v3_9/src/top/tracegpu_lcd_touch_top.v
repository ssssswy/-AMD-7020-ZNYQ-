`timescale 1ns / 1ps

// TraceGPU board bring-up top for the ALIENTEK Navigator ZYNQ-7020.
// The signal names follow NAVIGATOR_ZYNQ_IO.xdc.  The copied ALIENTEK
// modules auto-detect the supported 4.3-inch 480x272/800x480 panels.
module tracegpu_lcd_touch_top (
    input             sys_clk,
    input             sys_rst_n,

    output            lcd_scl,
    inout             lcd_sda,
    inout             ct_int,
    output            ct_rst,

    output            lcd_de,
    output            lcd_hs,
    output            lcd_vs,
    output            lcd_bl,
    output            lcd_clk,
    output            lcd_rst_n,
    inout      [23:0] lcd_rgb
);

wire [15:0] lcd_id;
wire [31:0] touch_xy;

touch_top u_touch_top (
    .clk         (sys_clk),
    .rst_n       (sys_rst_n),
    .touch_rst_n (ct_rst),
    .touch_int   (ct_int),
    .touch_scl   (lcd_scl),
    .touch_sda   (lcd_sda),
    .lcd_id      (lcd_id),
    .data        (touch_xy)
);

// Version 1.1 uses the vendor coordinate-display demo as the hardware
// acceptance top.  The PainterEngine framebuffer/VDMA scanout will replace
// lcd_rgb_char in the next integration stage while retaining this pin layer.
lcd_rgb_char u_lcd_rgb_char (
    .sys_clk   (sys_clk),
    .sys_rst_n (sys_rst_n),
    .data      (touch_xy),
    .lcd_id    (lcd_id),
    .lcd_hs    (lcd_hs),
    .lcd_vs    (lcd_vs),
    .lcd_de    (lcd_de),
    .lcd_rgb   (lcd_rgb),
    .lcd_bl    (lcd_bl),
    .lcd_rst_n (lcd_rst_n),
    .lcd_clk   (lcd_clk)
);

endmodule
