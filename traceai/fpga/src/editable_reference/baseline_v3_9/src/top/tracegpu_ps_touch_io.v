`timescale 1ns / 1ps

// Physical I/O bridge for the Zynq PS I2C0 and PS GPIO EMIO touch interface.
// All protocol transfers and touch parsing are performed by the PS.  The PL
// contains only the required bidirectional I/O buffers and pin routing.
module tracegpu_ps_touch_io (
    input  wire        i2c_scl_o,
    input  wire        i2c_scl_t,
    output wire        i2c_scl_i,
    input  wire        i2c_sda_o,
    input  wire        i2c_sda_t,
    output wire        i2c_sda_i,
    input  wire [63:0] gpio_o,
    input  wire [63:0] gpio_t,
    output wire [63:0] gpio_i,
    inout  wire        touch_scl,
    inout  wire        touch_sda,
    output wire        touch_rst,
    inout  wire        touch_int
);

wire touch_int_i;

IOBUF scl_iobuf (
    .I(i2c_scl_o), .T(i2c_scl_t), .O(i2c_scl_i), .IO(touch_scl)
);

IOBUF sda_iobuf (
    .I(i2c_sda_o), .T(i2c_sda_t), .O(i2c_sda_i), .IO(touch_sda)
);

// EMIO GPIO 0 is the active-low controller reset.  It is always used as an
// output by software; keeping the existing top-level port direction avoids an
// unnecessary bidirectional constraint on the reset pin.
assign touch_rst = gpio_o[0];

// EMIO GPIO 1 is normally the interrupt input.  Goodix controllers also use
// this pin as an output during reset to select I2C address 0x14 or 0x5d.
IOBUF int_iobuf (
    .I(gpio_o[1]), .T(gpio_t[1]), .O(touch_int_i), .IO(touch_int)
);

assign gpio_i = {62'd0, touch_int_i, touch_rst};

endmodule
