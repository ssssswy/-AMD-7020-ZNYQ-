# Navigator Bank13 J4/23..26. Actual VCCO=3.3V must be verified on the board.
set_property -dict {PACKAGE_PIN Y13 IOSTANDARD LVCMOS33 PULLUP true} [get_ports logic_in_ch5]
set_property -dict {PACKAGE_PIN Y12 IOSTANDARD LVCMOS33 PULLUP true} [get_ports logic_in_ch6]
set_property -dict {PACKAGE_PIN Y11 IOSTANDARD LVCMOS33 PULLUP true} [get_ports logic_in_ch7]
set_property -dict {PACKAGE_PIN W11 IOSTANDARD LVCMOS33 PULLUP true} [get_ports logic_in_ch8]
# Only the async pin -> first synchronizer D path is excepted, not filtering,
# capture, trigger or the second FF. ASYNC_REG remains on both genuine FFs.
set v32_first [get_cells -quiet -hier -filter {NAME =~ *logic_frontend*sync_ff1_reg}]
set_false_path -from [get_ports {logic_in_ch5 logic_in_ch6 logic_in_ch7 logic_in_ch8}] -to [get_pins -of_objects $v32_first -filter {REF_PIN_NAME == D}]
