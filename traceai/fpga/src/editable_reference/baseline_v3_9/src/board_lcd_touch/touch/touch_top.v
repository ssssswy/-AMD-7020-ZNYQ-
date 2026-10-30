//****************************************Copyright (c)***********************************//
//原子哥在线教学平台：www.yuanzige.com
//技术支持：http://www.openedv.com/forum.php
//淘宝店铺：https://zhengdianyuanzi.tmall.com
//关注微信公众平台微信号："正点原子"，免费获取ZYNQ & FPGA & STM32 & LINUX资料。
//版权所有，盗版必究。
//Copyright(C) 正点原子 2023-2033
//All rights reserved                                  
//----------------------------------------------------------------------------------------
// File name:           touch_top
// Created by:          正点原子
// Created date:        2025年10月14日14:17:02
// Version:             V1.0
// Descriptions:        LCD触摸顶层模块
//
//----------------------------------------------------------------------------------------
//****************************************************************************************//

module touch_top(
    input             clk        , //系统时钟信号
    input             rst_n      , //系统复位信号
    //LCD触摸相关信号            
    output            touch_rst_n, //触摸屏复位
    inout             touch_int  , //TOUCH INT信号
    output            touch_scl  , //I2C的SCL时钟信号
    inout             touch_sda,   //TOUCH IIC数据
    //用户端口
    input     [15:0]  lcd_id     , //LCD ID
    output    [31:0]  data         , //触摸点坐标
    output            touch_valid  , //当前触摸有效电平
    output            sample_toggle, //新坐标样本翻转标志
    output   [13:0]   touch_debug     //触摸控制器诊断状态
);

//parameter define
parameter CLK_FREQ    = 50_000_000   ;  //i2c_dri模块的驱动时钟频率(CLK_FREQ)
parameter I2C_FREQ    = 250_000      ;  //I2C的SCL时钟频率
parameter REG_NUM_WID = 8            ;  //一次读写寄存器的个数的位宽

//wire define
wire  [6:0]             slave_addr     ;  //器件地址
wire                    i2c_exec       ;  //I2C触发执行信号
wire                    i2c_rh_wl      ;  //I2C读写控制信号
wire  [15:0]            i2c_addr       ;  //I2C器件内地址
wire  [7:0]             i2c_data_w     ;  //I2C要写的数据
wire                    bit_ctrl       ;  //字地址位控制(0:8b,1:16b)
wire  [REG_NUM_WID-1:0] reg_num        ;  //一次读写寄存器的个数
wire  [7:0]             i2c_data_r     ;  //I2C读出的数据
wire                    i2c_done       ;  //I2C操作完成
wire                    once_byte_done ;  ////一字节数据读/写操作完成
wire                    i2c_ack        ;  //应答标志
wire                    dri_clk        ;  //I2C驱动时钟
wire                    touch_sda_i;
wire                    touch_sda_o;
wire                    touch_sda_t;
wire                    touch_int_i;
wire                    touch_int_o;
wire                    touch_int_t;

//*****************************************************
//**                    main code
//*****************************************************

// 与器件引脚连接的层级显式放置双向缓冲，避免 BD module reference
// 只生成输出缓冲而丢失 SDA/INT 输入通路。
IOBUF u_touch_sda_iobuf(
    .I(touch_sda_o), .O(touch_sda_i), .T(touch_sda_t), .IO(touch_sda)
);

IOBUF u_touch_int_iobuf(
    .I(touch_int_o), .O(touch_int_i), .T(touch_int_t), .IO(touch_int)
);

//I2C驱动模块
i2c_dri  #(
    .CLK_FREQ      (CLK_FREQ     ),
    .I2C_FREQ      (I2C_FREQ     ),
    .WIDTH         (REG_NUM_WID  )
    )
    u_i2c_dri(   
    .clk           (clk          ),
    .rst_n         (rst_n        ),
                                 
    .slave_addr    (slave_addr    ),
    .i2c_exec      (i2c_exec      ),
    .i2c_rh_wl     (i2c_rh_wl     ),
    .i2c_addr      (i2c_addr      ),
    .i2c_data_w    (i2c_data_w    ),
    .bit_ctrl      (bit_ctrl      ),
    .reg_num       (reg_num       ),
    .i2c_data_r    (i2c_data_r    ),
    .i2c_done      (i2c_done      ),
    .once_byte_done(once_byte_done),
    .scl           (touch_scl     ),
    .sda_i         (touch_sda_i   ),
    .sda_o         (touch_sda_o   ),
    .sda_t         (touch_sda_t   ),
    .ack           (i2c_ack       ),

    .dri_clk       (dri_clk       ) 
    );

//触摸驱动模块
touch_dri  #(
    .WIDTH         (REG_NUM_WID   )
     )
    u_touch_dri(
    .clk           (dri_clk       ),
    .rst_n         (rst_n         ),
                                  
    .slave_addr    (slave_addr    ),
    .i2c_exec      (i2c_exec      ),
    .i2c_rh_wl     (i2c_rh_wl     ),
    .i2c_addr      (i2c_addr      ),
    .i2c_data_w    (i2c_data_w    ),
    .bit_ctrl      (bit_ctrl      ),
    .reg_num       (reg_num       ),
                                  
    .i2c_data_r    (i2c_data_r    ),
    .i2c_ack       (i2c_ack       ),
    .i2c_done      (i2c_done      ),
    .once_byte_done(once_byte_done),
     
    .lcd_id        (lcd_id        ),
    .data          (data          ),
    .touch_valid   (touch_valid   ),
    .sample_toggle (sample_toggle ),
    .debug_status  (touch_debug   ),
    .touch_rst_n   (touch_rst_n   ),
    .touch_int_i   (touch_int_i   ),
    .touch_int_o   (touch_int_o   ),
    .touch_int_t   (touch_int_t   )
    );

endmodule
