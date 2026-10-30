# CH1=W10 J4/19 and CH2=W9 J4/20 inherited from isolated v2.0.1 constraints.
# All four inputs require actual Bank13 VCCO=3.3V; no 5V/RS232 direct connection.
set_property -dict {PACKAGE_PIN V11 IOSTANDARD LVCMOS33 PULLUP true} [get_ports uart_rx_ch3]
set_property -dict {PACKAGE_PIN V10 IOSTANDARD LVCMOS33 PULLUP true} [get_ports uart_rx_ch4]
