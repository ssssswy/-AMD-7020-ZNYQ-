
################################################################
# This is a generated script based on design: tracegpu_system
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2025.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   if { [string compare $scripts_vivado_version $current_vivado_version] > 0 } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2042 -severity "ERROR" " This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Sourcing the script failed since it was created with a future version of Vivado."}

   } else {
     catch {common::send_gid_msg -ssname BD::TCL -id 2041 -severity "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   }

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source tracegpu_system_script.tcl


# The design that will be created by this Tcl script contains the following 
# module references:
# tracegpu_ps_touch_io, tracegpu_i2c_testgen_v2_4, tracegpu_spi_testgen_v2_5, tracegpu_capture_path_select, tracegpu_vector_capture, tracegpu_logic_input_frontend, tg_ai_stream_axi_bd, tg_ai_capture_trigger_bridge

# Please add the sources of those modules before sourcing this Tcl script.

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xc7z020clg400-2
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name tracegpu_system

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_gid_msg -ssname BD::TCL -id 2001 -severity "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_gid_msg -ssname BD::TCL -id 2002 -severity "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_gid_msg -ssname BD::TCL -id 2003 -severity "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_gid_msg -ssname BD::TCL -id 2004 -severity "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_gid_msg -ssname BD::TCL -id 2005 -severity "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_gid_msg -ssname BD::TCL -id 2006 -severity "ERROR" $errMsg}
   return $nRet
}

set bCheckIPsPassed 1
##################################################################
# CHECK IPs
##################################################################
set bCheckIPs 1
if { $bCheckIPs == 1 } {
   set list_check_ips "\ 
xilinx.com:ip:processing_system7:5.5\
tracegpu.org:user:painterengine_gpu:1.2\
xilinx.com:ip:axis_data_fifo:2.0\
xilinx.com:ip:axi_dma:7.1\
xilinx.com:ip:xlconcat:2.1\
xilinx.com:ip:axi_vdma:6.3\
xilinx.com:ip:v_tc:6.2\
xilinx.com:ip:v_axi4s_vid_out:4.0\
xilinx.com:ip:clk_wiz:6.0\
xilinx.com:ip:proc_sys_reset:5.0\
xilinx.com:ip:util_vector_logic:2.0\
xilinx.com:ip:xlconstant:1.1\
tracegpu.org:user:tracegpu_gpu_batch:3.6\
tracegpu.org:user:tracegpu_argb_stream:1.9\
tracegpu.org:user:tracegpu_uart_event_merge:2.1\
xilinx.com:ip:axi_gpio:2.0\
xilinx.com:ip:xlslice:1.0\
tracegpu.org:user:tracegpu_uart_analyzer:3.2\
tracegpu.org:user:tracegpu_capture_control:3.2\
tracegpu.org:user:tracegpu_trigger_axi:3.2\
"

   set list_ips_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2011 -severity "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2012 -severity "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

}

##################################################################
# CHECK Modules
##################################################################
set bCheckModules 1
if { $bCheckModules == 1 } {
   set list_check_mods "\ 
tracegpu_ps_touch_io\
tracegpu_i2c_testgen_v2_4\
tracegpu_spi_testgen_v2_5\
tracegpu_capture_path_select\
tracegpu_vector_capture\
tracegpu_logic_input_frontend\
tg_ai_stream_axi_bd\
tg_ai_capture_trigger_bridge\
"

   set list_mods_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2020 -severity "INFO" "Checking if the following modules exist in the project's sources: $list_check_mods ."

   foreach mod_vlnv $list_check_mods {
      if { [can_resolve_reference $mod_vlnv] == 0 } {
         lappend list_mods_missing $mod_vlnv
      }
   }

   if { $list_mods_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2021 -severity "ERROR" "The following module(s) are not found in the project: $list_mods_missing" }
      common::send_gid_msg -ssname BD::TCL -id 2022 -severity "INFO" "Please add source files for the missing module(s) above."
      set bCheckIPsPassed 0
   }
}

if { $bCheckIPsPassed != 1 } {
  common::send_gid_msg -ssname BD::TCL -id 2023 -severity "WARNING" "Will not continue with creation of design due to the error(s) above."
  return 3
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder
  variable design_name

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2090 -severity "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2091 -severity "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set DDR_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 DDR_0 ]

  set FIXED_IO_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_processing_system7:fixedio_rtl:1.0 FIXED_IO_0 ]


  # Create ports
  set lcd_rgb [ create_bd_port -dir O -from 23 -to 0 lcd_rgb ]
  set lcd_de [ create_bd_port -dir O lcd_de ]
  set lcd_hs [ create_bd_port -dir O -from 0 -to 0 lcd_hs ]
  set lcd_vs [ create_bd_port -dir O -from 0 -to 0 lcd_vs ]
  set lcd_bl [ create_bd_port -dir O -from 0 -to 0 lcd_bl ]
  set lcd_clk [ create_bd_port -dir O lcd_clk ]
  set lcd_rst_n [ create_bd_port -dir O -from 0 -to 0 lcd_rst_n ]
  set lcd_scl [ create_bd_port -dir IO lcd_scl ]
  set lcd_sda [ create_bd_port -dir IO lcd_sda ]
  set ct_rst [ create_bd_port -dir O ct_rst ]
  set ct_int [ create_bd_port -dir IO ct_int ]
  set uart_analyzer_rx [ create_bd_port -dir I uart_analyzer_rx ]
  set uart_rx_ch2 [ create_bd_port -dir I uart_rx_ch2 ]
  set uart_rx_ch3 [ create_bd_port -dir I uart_rx_ch3 ]
  set uart_rx_ch4 [ create_bd_port -dir I uart_rx_ch4 ]
  set logic_in_ch5 [ create_bd_port -dir I logic_in_ch5 ]
  set logic_in_ch6 [ create_bd_port -dir I logic_in_ch6 ]
  set logic_in_ch7 [ create_bd_port -dir I logic_in_ch7 ]
  set logic_in_ch8 [ create_bd_port -dir I logic_in_ch8 ]

  # Create instance: ps7_0, and set properties
  set ps7_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7_0 ]
  set_property -dict [list \
    CONFIG.PCW_ACT_APU_PERIPHERAL_FREQMHZ {666.666687} \
    CONFIG.PCW_ACT_CAN0_PERIPHERAL_FREQMHZ {23.8095} \
    CONFIG.PCW_ACT_CAN1_PERIPHERAL_FREQMHZ {23.8095} \
    CONFIG.PCW_ACT_CAN_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_DCI_PERIPHERAL_FREQMHZ {10.158730} \
    CONFIG.PCW_ACT_ENET0_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_ENET1_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_FPGA0_PERIPHERAL_FREQMHZ {100.000000} \
    CONFIG.PCW_ACT_FPGA1_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_FPGA2_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_FPGA3_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_I2C_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_ACT_PCAP_PERIPHERAL_FREQMHZ {200.000000} \
    CONFIG.PCW_ACT_QSPI_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_SDIO_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_SMC_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_SPI_PERIPHERAL_FREQMHZ {10.000000} \
    CONFIG.PCW_ACT_TPIU_PERIPHERAL_FREQMHZ {200.000000} \
    CONFIG.PCW_ACT_TTC0_CLK0_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC0_CLK1_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC0_CLK2_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC1_CLK0_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC1_CLK1_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC1_CLK2_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_ACT_TTC_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_ACT_UART_PERIPHERAL_FREQMHZ {100.000000} \
    CONFIG.PCW_ACT_USB0_PERIPHERAL_FREQMHZ {60} \
    CONFIG.PCW_ACT_USB1_PERIPHERAL_FREQMHZ {60} \
    CONFIG.PCW_ACT_WDT_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_APU_CLK_RATIO_ENABLE {6:2:1} \
    CONFIG.PCW_APU_PERIPHERAL_FREQMHZ {666.666666} \
    CONFIG.PCW_CAN0_PERIPHERAL_CLKSRC {External} \
    CONFIG.PCW_CAN0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_CAN1_PERIPHERAL_CLKSRC {External} \
    CONFIG.PCW_CAN1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_CAN_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_CAN_PERIPHERAL_VALID {0} \
    CONFIG.PCW_CLK0_FREQ {100000000} \
    CONFIG.PCW_CLK1_FREQ {10000000} \
    CONFIG.PCW_CLK2_FREQ {10000000} \
    CONFIG.PCW_CLK3_FREQ {10000000} \
    CONFIG.PCW_CORE0_FIQ_INTR {0} \
    CONFIG.PCW_CORE0_IRQ_INTR {0} \
    CONFIG.PCW_CORE1_FIQ_INTR {0} \
    CONFIG.PCW_CORE1_IRQ_INTR {0} \
    CONFIG.PCW_CPU_CPU_6X4X_MAX_RANGE {767} \
    CONFIG.PCW_CPU_PERIPHERAL_CLKSRC {ARM PLL} \
    CONFIG.PCW_CRYSTAL_PERIPHERAL_FREQMHZ {33.333333} \
    CONFIG.PCW_DCI_PERIPHERAL_CLKSRC {DDR PLL} \
    CONFIG.PCW_DCI_PERIPHERAL_FREQMHZ {10.159} \
    CONFIG.PCW_DDR_PERIPHERAL_CLKSRC {DDR PLL} \
    CONFIG.PCW_DDR_RAM_BASEADDR {0x00100000} \
    CONFIG.PCW_DDR_RAM_HIGHADDR {0x3FFFFFFF} \
    CONFIG.PCW_DM_WIDTH {4} \
    CONFIG.PCW_DQS_WIDTH {4} \
    CONFIG.PCW_DQ_WIDTH {32} \
    CONFIG.PCW_ENET0_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_ENET0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_ENET1_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_ENET1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_ENET_RESET_POLARITY {Active Low} \
    CONFIG.PCW_EN_4K_TIMER {0} \
    CONFIG.PCW_EN_CAN0 {0} \
    CONFIG.PCW_EN_CAN1 {0} \
    CONFIG.PCW_EN_CLK0_PORT {1} \
    CONFIG.PCW_EN_CLK1_PORT {0} \
    CONFIG.PCW_EN_CLK2_PORT {0} \
    CONFIG.PCW_EN_CLK3_PORT {0} \
    CONFIG.PCW_EN_CLKTRIG0_PORT {0} \
    CONFIG.PCW_EN_CLKTRIG1_PORT {0} \
    CONFIG.PCW_EN_CLKTRIG2_PORT {0} \
    CONFIG.PCW_EN_CLKTRIG3_PORT {0} \
    CONFIG.PCW_EN_DDR {1} \
    CONFIG.PCW_EN_EMIO_CAN0 {0} \
    CONFIG.PCW_EN_EMIO_CAN1 {0} \
    CONFIG.PCW_EN_EMIO_CD_SDIO0 {0} \
    CONFIG.PCW_EN_EMIO_CD_SDIO1 {0} \
    CONFIG.PCW_EN_EMIO_ENET0 {0} \
    CONFIG.PCW_EN_EMIO_ENET1 {0} \
    CONFIG.PCW_EN_EMIO_GPIO {1} \
    CONFIG.PCW_EN_EMIO_I2C0 {1} \
    CONFIG.PCW_EN_EMIO_I2C1 {0} \
    CONFIG.PCW_EN_EMIO_MODEM_UART0 {0} \
    CONFIG.PCW_EN_EMIO_MODEM_UART1 {0} \
    CONFIG.PCW_EN_EMIO_PJTAG {0} \
    CONFIG.PCW_EN_EMIO_SDIO0 {0} \
    CONFIG.PCW_EN_EMIO_SDIO1 {0} \
    CONFIG.PCW_EN_EMIO_SPI0 {0} \
    CONFIG.PCW_EN_EMIO_SPI1 {0} \
    CONFIG.PCW_EN_EMIO_SRAM_INT {0} \
    CONFIG.PCW_EN_EMIO_TRACE {0} \
    CONFIG.PCW_EN_EMIO_TTC0 {0} \
    CONFIG.PCW_EN_EMIO_TTC1 {0} \
    CONFIG.PCW_EN_EMIO_UART0 {0} \
    CONFIG.PCW_EN_EMIO_UART1 {0} \
    CONFIG.PCW_EN_EMIO_WDT {0} \
    CONFIG.PCW_EN_EMIO_WP_SDIO0 {0} \
    CONFIG.PCW_EN_EMIO_WP_SDIO1 {0} \
    CONFIG.PCW_EN_ENET0 {0} \
    CONFIG.PCW_EN_ENET1 {0} \
    CONFIG.PCW_EN_GPIO {0} \
    CONFIG.PCW_EN_I2C0 {1} \
    CONFIG.PCW_EN_I2C1 {0} \
    CONFIG.PCW_EN_MODEM_UART0 {0} \
    CONFIG.PCW_EN_MODEM_UART1 {0} \
    CONFIG.PCW_EN_PJTAG {0} \
    CONFIG.PCW_EN_PTP_ENET0 {0} \
    CONFIG.PCW_EN_PTP_ENET1 {0} \
    CONFIG.PCW_EN_QSPI {0} \
    CONFIG.PCW_EN_RST0_PORT {1} \
    CONFIG.PCW_EN_RST1_PORT {0} \
    CONFIG.PCW_EN_RST2_PORT {0} \
    CONFIG.PCW_EN_RST3_PORT {0} \
    CONFIG.PCW_EN_SDIO0 {0} \
    CONFIG.PCW_EN_SDIO1 {0} \
    CONFIG.PCW_EN_SMC {0} \
    CONFIG.PCW_EN_SPI0 {0} \
    CONFIG.PCW_EN_SPI1 {0} \
    CONFIG.PCW_EN_TRACE {0} \
    CONFIG.PCW_EN_TTC0 {0} \
    CONFIG.PCW_EN_TTC1 {0} \
    CONFIG.PCW_EN_UART0 {1} \
    CONFIG.PCW_EN_UART1 {0} \
    CONFIG.PCW_EN_USB0 {0} \
    CONFIG.PCW_EN_USB1 {0} \
    CONFIG.PCW_EN_WDT {0} \
    CONFIG.PCW_FCLK0_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_FCLK1_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_FCLK2_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_FCLK3_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_FCLK_CLK0_BUF {TRUE} \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} \
    CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_FPGA2_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_FPGA3_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_FPGA_FCLK0_ENABLE {1} \
    CONFIG.PCW_GP0_EN_MODIFIABLE_TXN {1} \
    CONFIG.PCW_GP0_NUM_READ_THREADS {4} \
    CONFIG.PCW_GP0_NUM_WRITE_THREADS {4} \
    CONFIG.PCW_GP1_EN_MODIFIABLE_TXN {1} \
    CONFIG.PCW_GP1_NUM_READ_THREADS {4} \
    CONFIG.PCW_GP1_NUM_WRITE_THREADS {4} \
    CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {1} \
    CONFIG.PCW_GPIO_EMIO_GPIO_IO {64} \
    CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH {64} \
    CONFIG.PCW_GPIO_MIO_GPIO_ENABLE {0} \
    CONFIG.PCW_GPIO_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_I2C0_BASEADDR {0xE0004000} \
    CONFIG.PCW_I2C0_HIGHADDR {0xE0004FFF} \
    CONFIG.PCW_I2C0_I2C0_IO {EMIO} \
    CONFIG.PCW_I2C0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_I2C1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_I2C_PERIPHERAL_FREQMHZ {111.111115} \
    CONFIG.PCW_I2C_RESET_POLARITY {Active Low} \
    CONFIG.PCW_IMPORT_BOARD_PRESET {None} \
    CONFIG.PCW_INCLUDE_ACP_TRANS_CHECK {0} \
    CONFIG.PCW_IRQ_F2P_INTR {1} \
    CONFIG.PCW_IRQ_F2P_MODE {DIRECT} \
    CONFIG.PCW_MIO_14_IOTYPE {LVCMOS 3.3V} \
    CONFIG.PCW_MIO_14_PULLUP {enabled} \
    CONFIG.PCW_MIO_14_SLEW {slow} \
    CONFIG.PCW_MIO_15_IOTYPE {LVCMOS 3.3V} \
    CONFIG.PCW_MIO_15_PULLUP {enabled} \
    CONFIG.PCW_MIO_15_SLEW {slow} \
    CONFIG.PCW_MIO_PRIMITIVE {54} \
    CONFIG.PCW_MIO_TREE_PERIPHERALS {unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#UART 0#UART\
0#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned}\
\
    CONFIG.PCW_MIO_TREE_SIGNALS {unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#rx#tx#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned#unassigned}\
\
    CONFIG.PCW_M_AXI_GP0_ENABLE_STATIC_REMAP {0} \
    CONFIG.PCW_M_AXI_GP0_ID_WIDTH {12} \
    CONFIG.PCW_M_AXI_GP0_SUPPORT_NARROW_BURST {0} \
    CONFIG.PCW_M_AXI_GP0_THREAD_ID_WIDTH {12} \
    CONFIG.PCW_NAND_CYCLES_T_AR {1} \
    CONFIG.PCW_NAND_CYCLES_T_CLR {1} \
    CONFIG.PCW_NAND_CYCLES_T_RC {11} \
    CONFIG.PCW_NAND_CYCLES_T_REA {1} \
    CONFIG.PCW_NAND_CYCLES_T_RR {1} \
    CONFIG.PCW_NAND_CYCLES_T_WC {11} \
    CONFIG.PCW_NAND_CYCLES_T_WP {1} \
    CONFIG.PCW_NAND_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_NOR_CS0_T_CEOE {1} \
    CONFIG.PCW_NOR_CS0_T_PC {1} \
    CONFIG.PCW_NOR_CS0_T_RC {11} \
    CONFIG.PCW_NOR_CS0_T_TR {1} \
    CONFIG.PCW_NOR_CS0_T_WC {11} \
    CONFIG.PCW_NOR_CS0_T_WP {1} \
    CONFIG.PCW_NOR_CS0_WE_TIME {0} \
    CONFIG.PCW_NOR_CS1_T_CEOE {1} \
    CONFIG.PCW_NOR_CS1_T_PC {1} \
    CONFIG.PCW_NOR_CS1_T_RC {11} \
    CONFIG.PCW_NOR_CS1_T_TR {1} \
    CONFIG.PCW_NOR_CS1_T_WC {11} \
    CONFIG.PCW_NOR_CS1_T_WP {1} \
    CONFIG.PCW_NOR_CS1_WE_TIME {0} \
    CONFIG.PCW_NOR_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_NOR_SRAM_CS0_T_CEOE {1} \
    CONFIG.PCW_NOR_SRAM_CS0_T_PC {1} \
    CONFIG.PCW_NOR_SRAM_CS0_T_RC {11} \
    CONFIG.PCW_NOR_SRAM_CS0_T_TR {1} \
    CONFIG.PCW_NOR_SRAM_CS0_T_WC {11} \
    CONFIG.PCW_NOR_SRAM_CS0_T_WP {1} \
    CONFIG.PCW_NOR_SRAM_CS0_WE_TIME {0} \
    CONFIG.PCW_NOR_SRAM_CS1_T_CEOE {1} \
    CONFIG.PCW_NOR_SRAM_CS1_T_PC {1} \
    CONFIG.PCW_NOR_SRAM_CS1_T_RC {11} \
    CONFIG.PCW_NOR_SRAM_CS1_T_TR {1} \
    CONFIG.PCW_NOR_SRAM_CS1_T_WC {11} \
    CONFIG.PCW_NOR_SRAM_CS1_T_WP {1} \
    CONFIG.PCW_NOR_SRAM_CS1_WE_TIME {0} \
    CONFIG.PCW_OVERRIDE_BASIC_CLOCK {0} \
    CONFIG.PCW_P2F_I2C0_INTR {0} \
    CONFIG.PCW_P2F_UART0_INTR {0} \
    CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY0 {0.089} \
    CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY1 {0.075} \
    CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY2 {0.085} \
    CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY3 {0.092} \
    CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_0 {-0.025} \
    CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_1 {0.014} \
    CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_2 {-0.009} \
    CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_3 {-0.033} \
    CONFIG.PCW_PACKAGE_NAME {clg400} \
    CONFIG.PCW_PCAP_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_PCAP_PERIPHERAL_FREQMHZ {200} \
    CONFIG.PCW_PERIPHERAL_BOARD_PRESET {None} \
    CONFIG.PCW_PJTAG_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_PLL_BYPASSMODE_ENABLE {0} \
    CONFIG.PCW_PRESET_BANK0_VOLTAGE {LVCMOS 3.3V} \
    CONFIG.PCW_PRESET_BANK1_VOLTAGE {LVCMOS 1.8V} \
    CONFIG.PCW_PS7_SI_REV {PRODUCTION} \
    CONFIG.PCW_QSPI_INTERNAL_HIGHADDRESS {0xFCFFFFFF} \
    CONFIG.PCW_QSPI_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_SD0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_SD1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_SDIO_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_SDIO_PERIPHERAL_VALID {0} \
    CONFIG.PCW_SMC_CYCLE_T0 {NA} \
    CONFIG.PCW_SMC_CYCLE_T1 {NA} \
    CONFIG.PCW_SMC_CYCLE_T2 {NA} \
    CONFIG.PCW_SMC_CYCLE_T3 {NA} \
    CONFIG.PCW_SMC_CYCLE_T4 {NA} \
    CONFIG.PCW_SMC_CYCLE_T5 {NA} \
    CONFIG.PCW_SMC_CYCLE_T6 {NA} \
    CONFIG.PCW_SMC_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_SMC_PERIPHERAL_VALID {0} \
    CONFIG.PCW_SPI0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_SPI1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_SPI_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_SPI_PERIPHERAL_VALID {0} \
    CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {64} \
    CONFIG.PCW_S_AXI_HP0_ID_WIDTH {6} \
    CONFIG.PCW_S_AXI_HP1_DATA_WIDTH {64} \
    CONFIG.PCW_S_AXI_HP1_ID_WIDTH {6} \
    CONFIG.PCW_S_AXI_HP2_DATA_WIDTH {64} \
    CONFIG.PCW_S_AXI_HP2_ID_WIDTH {6} \
    CONFIG.PCW_TPIU_PERIPHERAL_CLKSRC {External} \
    CONFIG.PCW_TRACE_INTERNAL_WIDTH {2} \
    CONFIG.PCW_TRACE_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_TTC0_CLK0_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC0_CLK0_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC0_CLK1_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC0_CLK1_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC0_CLK2_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC0_CLK2_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_TTC1_CLK0_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC1_CLK0_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC1_CLK1_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC1_CLK1_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC1_CLK2_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_TTC1_CLK2_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_TTC1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_UART0_BASEADDR {0xE0000000} \
    CONFIG.PCW_UART0_BAUD_RATE {115200} \
    CONFIG.PCW_UART0_GRP_FULL_ENABLE {0} \
    CONFIG.PCW_UART0_HIGHADDR {0xE0000FFF} \
    CONFIG.PCW_UART0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_UART0_UART0_IO {MIO 14 .. 15} \
    CONFIG.PCW_UART1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_UART_PERIPHERAL_CLKSRC {IO PLL} \
    CONFIG.PCW_UART_PERIPHERAL_FREQMHZ {100} \
    CONFIG.PCW_UART_PERIPHERAL_VALID {1} \
    CONFIG.PCW_UIPARAM_ACT_DDR_FREQ_MHZ {533.333374} \
    CONFIG.PCW_UIPARAM_DDR_ADV_ENABLE {0} \
    CONFIG.PCW_UIPARAM_DDR_AL {0} \
    CONFIG.PCW_UIPARAM_DDR_BL {8} \
    CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY0 {0.25} \
    CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY1 {0.25} \
    CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY2 {0.25} \
    CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY3 {0.25} \
    CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {32 Bit} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_0_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PACKAGE_LENGTH {80.4535} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_1_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PACKAGE_LENGTH {80.4535} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_2_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PACKAGE_LENGTH {80.4535} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_3_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PACKAGE_LENGTH {80.4535} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_CLOCK_STOP_EN {0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_0_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_0_PACKAGE_LENGTH {105.056} \
    CONFIG.PCW_UIPARAM_DDR_DQS_0_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQS_1_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_1_PACKAGE_LENGTH {66.904} \
    CONFIG.PCW_UIPARAM_DDR_DQS_1_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQS_2_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_2_PACKAGE_LENGTH {89.1715} \
    CONFIG.PCW_UIPARAM_DDR_DQS_2_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQS_3_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_3_PACKAGE_LENGTH {113.63} \
    CONFIG.PCW_UIPARAM_DDR_DQS_3_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_0 {0.0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_1 {0.0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_2 {0.0} \
    CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_3 {0.0} \
    CONFIG.PCW_UIPARAM_DDR_DQ_0_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQ_0_PACKAGE_LENGTH {98.503} \
    CONFIG.PCW_UIPARAM_DDR_DQ_0_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQ_1_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQ_1_PACKAGE_LENGTH {68.5855} \
    CONFIG.PCW_UIPARAM_DDR_DQ_1_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQ_2_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQ_2_PACKAGE_LENGTH {90.295} \
    CONFIG.PCW_UIPARAM_DDR_DQ_2_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_DQ_3_LENGTH_MM {0} \
    CONFIG.PCW_UIPARAM_DDR_DQ_3_PACKAGE_LENGTH {103.977} \
    CONFIG.PCW_UIPARAM_DDR_DQ_3_PROPOGATION_DELAY {160} \
    CONFIG.PCW_UIPARAM_DDR_ENABLE {1} \
    CONFIG.PCW_UIPARAM_DDR_FREQ_MHZ {533.333333} \
    CONFIG.PCW_UIPARAM_DDR_HIGH_TEMP {Normal (0-85)} \
    CONFIG.PCW_UIPARAM_DDR_MEMORY_TYPE {DDR 3} \
    CONFIG.PCW_UIPARAM_DDR_PARTNO {MT41J256M16 RE-125} \
    CONFIG.PCW_UIPARAM_DDR_TRAIN_DATA_EYE {1} \
    CONFIG.PCW_UIPARAM_DDR_TRAIN_READ_GATE {1} \
    CONFIG.PCW_UIPARAM_DDR_TRAIN_WRITE_LEVEL {1} \
    CONFIG.PCW_UIPARAM_DDR_USE_INTERNAL_VREF {0} \
    CONFIG.PCW_UIPARAM_GENERATE_SUMMARY {NA} \
    CONFIG.PCW_USB0_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_USB1_PERIPHERAL_ENABLE {0} \
    CONFIG.PCW_USB_RESET_POLARITY {Active Low} \
    CONFIG.PCW_USE_AXI_FABRIC_IDLE {0} \
    CONFIG.PCW_USE_AXI_NONSECURE {0} \
    CONFIG.PCW_USE_CORESIGHT {0} \
    CONFIG.PCW_USE_CROSS_TRIGGER {0} \
    CONFIG.PCW_USE_CR_FABRIC {1} \
    CONFIG.PCW_USE_DDR_BYPASS {0} \
    CONFIG.PCW_USE_DEBUG {0} \
    CONFIG.PCW_USE_DMA0 {0} \
    CONFIG.PCW_USE_DMA1 {0} \
    CONFIG.PCW_USE_DMA2 {0} \
    CONFIG.PCW_USE_DMA3 {0} \
    CONFIG.PCW_USE_EXPANDED_IOP {0} \
    CONFIG.PCW_USE_FABRIC_INTERRUPT {1} \
    CONFIG.PCW_USE_HIGH_OCM {0} \
    CONFIG.PCW_USE_M_AXI_GP0 {1} \
    CONFIG.PCW_USE_M_AXI_GP1 {0} \
    CONFIG.PCW_USE_PROC_EVENT_BUS {0} \
    CONFIG.PCW_USE_PS_SLCR_REGISTERS {0} \
    CONFIG.PCW_USE_S_AXI_ACP {0} \
    CONFIG.PCW_USE_S_AXI_GP0 {0} \
    CONFIG.PCW_USE_S_AXI_GP1 {0} \
    CONFIG.PCW_USE_S_AXI_HP0 {1} \
    CONFIG.PCW_USE_S_AXI_HP1 {1} \
    CONFIG.PCW_USE_S_AXI_HP2 {1} \
    CONFIG.PCW_USE_S_AXI_HP3 {0} \
    CONFIG.PCW_USE_TRACE {0} \
    CONFIG.PCW_VALUE_SILVERSION {3} \
    CONFIG.PCW_WDT_PERIPHERAL_CLKSRC {CPU_1X} \
    CONFIG.PCW_WDT_PERIPHERAL_DIVISOR0 {1} \
    CONFIG.PCW_WDT_PERIPHERAL_ENABLE {0} \
    CONFIG.preset {None} \
  ] $ps7_0


  # Create instance: axi_ctrl, and set properties
  set axi_ctrl [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ctrl ]
  set_property -dict [list \
    CONFIG.NUM_MI {14} \
    CONFIG.NUM_SI {1} \
    CONFIG.S00_HAS_REGSLICE {1} \
  ] $axi_ctrl


  # Create instance: axi_gpu_mem, and set properties
  set axi_gpu_mem [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_gpu_mem ]
  set_property -dict [list \
    CONFIG.NUM_MI {1} \
    CONFIG.NUM_SI {2} \
  ] $axi_gpu_mem


  # Create instance: axi_vdma_mem, and set properties
  set axi_vdma_mem [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_vdma_mem ]
  set_property -dict [list \
    CONFIG.NUM_MI {1} \
    CONFIG.NUM_SI {1} \
  ] $axi_vdma_mem


  # Create instance: axi_uart_mem, and set properties
  set axi_uart_mem [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_uart_mem ]
  set_property -dict [list \
    CONFIG.NUM_MI {1} \
    CONFIG.NUM_SI {2} \
  ] $axi_uart_mem


  # Create instance: painterengine_gpu_0, and set properties
  set painterengine_gpu_0 [ create_bd_cell -type ip -vlnv tracegpu.org:user:painterengine_gpu:1.2 painterengine_gpu_0 ]

  # Create instance: uart_event_fifo, and set properties
  set uart_event_fifo [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 uart_event_fifo ]
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {4096} \
    CONFIG.HAS_TKEEP {1} \
    CONFIG.HAS_TLAST {1} \
    CONFIG.TDATA_NUM_BYTES {16} \
  ] $uart_event_fifo


  # Create instance: uart_capture_dma, and set properties
  set uart_capture_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 uart_capture_dma ]
  set_property -dict [list \
    CONFIG.c_include_mm2s {0} \
    CONFIG.c_include_s2mm {1} \
    CONFIG.c_include_sg {1} \
    CONFIG.c_m_axi_s2mm_data_width {128} \
    CONFIG.c_s_axis_s2mm_tdata_width {128} \
    CONFIG.c_sg_include_stscntrl_strm {0} \
    CONFIG.c_sg_length_width {23} \
  ] $uart_capture_dma


  # Create instance: uart_irq_concat, and set properties
  set uart_irq_concat [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 uart_irq_concat ]
  set_property CONFIG.NUM_PORTS {2} $uart_irq_concat


  # Create instance: axi_vdma_0, and set properties
  set axi_vdma_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_vdma:6.3 axi_vdma_0 ]
  set_property -dict [list \
    CONFIG.c_dynamic_resolution {1} \
    CONFIG.c_include_mm2s {1} \
    CONFIG.c_include_s2mm {0} \
    CONFIG.c_include_sg {0} \
    CONFIG.c_m_axi_mm2s_data_width {32} \
    CONFIG.c_m_axis_mm2s_tdata_width {32} \
    CONFIG.c_mm2s_max_burst_length {16} \
    CONFIG.c_num_fstores {2} \
    CONFIG.c_use_fsync {1} \
  ] $axi_vdma_0


  # Create instance: v_tc_0, and set properties
  set v_tc_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:v_tc:6.2 v_tc_0 ]
  set_property -dict [list \
    CONFIG.GEN_F0_VBLANK_HEND {800} \
    CONFIG.GEN_F0_VBLANK_HSTART {800} \
    CONFIG.GEN_F0_VFRAME_SIZE {525} \
    CONFIG.GEN_F0_VSYNC_VEND {492} \
    CONFIG.GEN_F0_VSYNC_VSTART {490} \
    CONFIG.GEN_HACTIVE_SIZE {800} \
    CONFIG.GEN_HFRAME_SIZE {1056} \
    CONFIG.GEN_HSYNC_END {968} \
    CONFIG.GEN_HSYNC_START {840} \
    CONFIG.GEN_VACTIVE_SIZE {480} \
    CONFIG.HAS_AXI4_LITE {false} \
    CONFIG.VIDEO_MODE {Custom} \
    CONFIG.enable_detection {false} \
    CONFIG.enable_generation {true} \
  ] $v_tc_0


  # Create instance: v_axi4s_vid_out_0, and set properties
  set v_axi4s_vid_out_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:v_axi4s_vid_out:4.0 v_axi4s_vid_out_0 ]
  set_property -dict [list \
    CONFIG.C_HAS_ASYNC_CLK {1} \
    CONFIG.C_NATIVE_COMPONENT_WIDTH {8} \
    CONFIG.C_PIXELS_PER_CLOCK {1} \
    CONFIG.C_S_AXIS_VIDEO_DATA_WIDTH {8} \
    CONFIG.C_S_AXIS_VIDEO_FORMAT {2} \
  ] $v_axi4s_vid_out_0


  # Create instance: clk_pixel_25m, and set properties
  set clk_pixel_25m [ create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:6.0 clk_pixel_25m ]
  set_property -dict [list \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {25.000} \
    CONFIG.NUM_OUT_CLKS {1} \
    CONFIG.PRIM_IN_FREQ {100.000} \
    CONFIG.PRIM_SOURCE {No_buffer} \
    CONFIG.USE_LOCKED {true} \
    CONFIG.USE_RESET {false} \
  ] $clk_pixel_25m


  # Create instance: rst_100m, and set properties
  set rst_100m [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_100m ]

  # Create instance: rst_25m, and set properties
  set rst_25m [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_25m ]

  # Create instance: ps_reset_not, and set properties
  set ps_reset_not [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 ps_reset_not ]
  set_property -dict [list \
    CONFIG.C_OPERATION {not} \
    CONFIG.C_SIZE {1} \
  ] $ps_reset_not


  # Create instance: ps_touch_io_0, and set properties
  set block_name tracegpu_ps_touch_io
  set block_cell_name ps_touch_io_0
  if { [catch {set ps_touch_io_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $ps_touch_io_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: logic_one, and set properties
  set logic_one [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 logic_one ]
  set_property -dict [list \
    CONFIG.CONST_VAL {1} \
    CONFIG.CONST_WIDTH {1} \
  ] $logic_one


  # Create instance: frame_ptr_zero, and set properties
  set frame_ptr_zero [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 frame_ptr_zero ]
  set_property -dict [list \
    CONFIG.CONST_VAL {0} \
    CONFIG.CONST_WIDTH {6} \
  ] $frame_ptr_zero


  # Create instance: pixel_reset_not, and set properties
  set pixel_reset_not [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 pixel_reset_not ]
  set_property -dict [list \
    CONFIG.C_OPERATION {not} \
    CONFIG.C_SIZE {1} \
  ] $pixel_reset_not


  # Create instance: gpu_batch_0, and set properties
  set gpu_batch_0 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_gpu_batch:3.6 gpu_batch_0 ]

  # Create instance: argb_stream_0, and set properties
  set argb_stream_0 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_argb_stream:1.9 argb_stream_0 ]

  # Create instance: uart_channel_fifo_0, and set properties
  set uart_channel_fifo_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 uart_channel_fifo_0 ]
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {2048} \
    CONFIG.HAS_TKEEP {1} \
    CONFIG.HAS_TLAST {1} \
    CONFIG.TDATA_NUM_BYTES {16} \
  ] $uart_channel_fifo_0


  # Create instance: uart_channel_fifo_1, and set properties
  set uart_channel_fifo_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 uart_channel_fifo_1 ]
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {2048} \
    CONFIG.HAS_TKEEP {1} \
    CONFIG.HAS_TLAST {1} \
    CONFIG.TDATA_NUM_BYTES {16} \
  ] $uart_channel_fifo_1


  # Create instance: uart_merge, and set properties
  set uart_merge [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_uart_event_merge:2.1 uart_merge ]

  # Create instance: uart_channel_fifo_2, and set properties
  set uart_channel_fifo_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 uart_channel_fifo_2 ]
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {2048} \
    CONFIG.HAS_TKEEP {1} \
    CONFIG.HAS_TLAST {1} \
    CONFIG.TDATA_NUM_BYTES {16} \
  ] $uart_channel_fifo_2


  # Create instance: uart_channel_fifo_3, and set properties
  set uart_channel_fifo_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 uart_channel_fifo_3 ]
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {2048} \
    CONFIG.HAS_TKEEP {1} \
    CONFIG.HAS_TLAST {1} \
    CONFIG.TDATA_NUM_BYTES {16} \
  ] $uart_channel_fifo_3


  # Create instance: i2c_test_control, and set properties
  set i2c_test_control [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 i2c_test_control ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS_2 {1} \
    CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_DOUT_DEFAULT {0x00000000} \
    CONFIG.C_GPIO2_WIDTH {32} \
    CONFIG.C_GPIO_WIDTH {32} \
    CONFIG.C_IS_DUAL {1} \
  ] $i2c_test_control


  # Create instance: i2c_test_source, and set properties
  set block_name tracegpu_i2c_testgen_v2_4
  set block_cell_name i2c_test_source
  if { [catch {set i2c_test_source [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $i2c_test_source eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: capture_levels, and set properties
  set capture_levels [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 capture_levels ]
  set_property CONFIG.NUM_PORTS {4} $capture_levels


  # Create instance: spi_test_control, and set properties
  set spi_test_control [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 spi_test_control ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS_2 {1} \
    CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_DOUT_DEFAULT {0x00000000} \
    CONFIG.C_GPIO2_WIDTH {32} \
    CONFIG.C_GPIO_WIDTH {32} \
    CONFIG.C_IS_DUAL {1} \
  ] $spi_test_control


  # Create instance: spi_test_source, and set properties
  set block_name tracegpu_spi_testgen_v2_5
  set block_cell_name spi_test_source
  if { [catch {set spi_test_source [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $spi_test_source eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: spi_external_lines, and set properties
  set spi_external_lines [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 spi_external_lines ]
  set_property CONFIG.NUM_PORTS {4} $spi_external_lines


  # Create instance: spi_line_0, and set properties
  set spi_line_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 spi_line_0 ]
  set_property -dict [list \
    CONFIG.DIN_FROM {0} \
    CONFIG.DIN_TO {0} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $spi_line_0


  # Create instance: spi_line_1, and set properties
  set spi_line_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 spi_line_1 ]
  set_property -dict [list \
    CONFIG.DIN_FROM {1} \
    CONFIG.DIN_TO {1} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $spi_line_1


  # Create instance: spi_line_2, and set properties
  set spi_line_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 spi_line_2 ]
  set_property -dict [list \
    CONFIG.DIN_FROM {2} \
    CONFIG.DIN_TO {2} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $spi_line_2


  # Create instance: spi_line_3, and set properties
  set spi_line_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 spi_line_3 ]
  set_property -dict [list \
    CONFIG.DIN_FROM {3} \
    CONFIG.DIN_TO {3} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $spi_line_3


  # Create instance: capture_path, and set properties
  set block_name tracegpu_capture_path_select
  set block_cell_name capture_path
  if { [catch {set capture_path [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $capture_path eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: trigger_valid, and set properties
  set trigger_valid [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_valid ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_valid


  # Create instance: trigger_data, and set properties
  set trigger_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_data ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {8} \
    CONFIG.IN1_WIDTH {8} \
    CONFIG.IN2_WIDTH {8} \
    CONFIG.IN3_WIDTH {8} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_data


  # Create instance: trigger_frame, and set properties
  set trigger_frame [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_frame ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_frame


  # Create instance: trigger_parity, and set properties
  set trigger_parity [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_parity ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_parity


  # Create instance: trigger_false_start, and set properties
  set trigger_false_start [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_false_start ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_false_start


  # Create instance: trigger_tick, and set properties
  set trigger_tick [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_tick ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {64} \
    CONFIG.IN1_WIDTH {64} \
    CONFIG.IN2_WIDTH {64} \
    CONFIG.IN3_WIDTH {64} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_tick


  # Create instance: trigger_start, and set properties
  set trigger_start [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_start ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {64} \
    CONFIG.IN1_WIDTH {64} \
    CONFIG.IN2_WIDTH {64} \
    CONFIG.IN3_WIDTH {64} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_start


  # Create instance: trigger_parity_enabled, and set properties
  set trigger_parity_enabled [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_parity_enabled ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_parity_enabled


  # Create instance: trigger_quality, and set properties
  set trigger_quality [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_quality ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {1} \
    CONFIG.IN1_WIDTH {1} \
    CONFIG.IN2_WIDTH {1} \
    CONFIG.IN3_WIDTH {1} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_quality


  # Create instance: trigger_count, and set properties
  set trigger_count [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 trigger_count ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {3} \
    CONFIG.IN1_WIDTH {3} \
    CONFIG.IN2_WIDTH {3} \
    CONFIG.IN3_WIDTH {3} \
    CONFIG.NUM_PORTS {4} \
  ] $trigger_count


  # Create instance: uart_analyzer_0, and set properties
  set uart_analyzer_0 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_uart_analyzer:3.2 uart_analyzer_0 ]

  # Create instance: uart_ch2, and set properties
  set uart_ch2 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_uart_analyzer:3.2 uart_ch2 ]

  # Create instance: uart_ch3, and set properties
  set uart_ch3 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_uart_analyzer:3.2 uart_ch3 ]

  # Create instance: uart_ch4, and set properties
  set uart_ch4 [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_uart_analyzer:3.2 uart_ch4 ]

  # Create instance: capture_control, and set properties
  set capture_control [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_capture_control:3.2 capture_control ]

  # Create instance: advanced_trigger, and set properties
  set advanced_trigger [ create_bd_cell -type ip -vlnv tracegpu.org:user:tracegpu_trigger_axi:3.2 advanced_trigger ]

  # Create instance: vector_capture, and set properties
  set block_name tracegpu_vector_capture
  set block_cell_name vector_capture
  if { [catch {set vector_capture [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $vector_capture eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: logic_frontend, and set properties
  set block_name tracegpu_logic_input_frontend
  set block_cell_name logic_frontend
  if { [catch {set logic_frontend [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $logic_frontend eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: logic_async_high, and set properties
  set logic_async_high [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 logic_async_high ]
  set_property CONFIG.NUM_PORTS {4} $logic_async_high


  # Create instance: capture_levels8, and set properties
  set capture_levels8 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 capture_levels8 ]
  set_property -dict [list \
    CONFIG.IN0_WIDTH {4} \
    CONFIG.IN1_WIDTH {4} \
    CONFIG.NUM_PORTS {2} \
  ] $capture_levels8


  # Create instance: ai_inference, and set properties
  set block_name tg_ai_stream_axi_bd
  set block_cell_name ai_inference
  if { [catch {set ai_inference [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $ai_inference eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
    set_property CONFIG.ROM_DIR {D:/FPGA_gpu/TraceGPU_2025_2/v4.9_full/model/v46_uart} $ai_inference


  # Create instance: ai_trigger_bridge, and set properties
  set block_name tg_ai_capture_trigger_bridge
  set block_cell_name ai_trigger_bridge
  if { [catch {set ai_trigger_bridge [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $ai_trigger_bridge eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create interface connections
  connect_bd_intf_net -intf_net argb_stream_0_m_axis [get_bd_intf_pins argb_stream_0/m_axis] [get_bd_intf_pins v_axi4s_vid_out_0/video_in]
  connect_bd_intf_net -intf_net axi_ctrl_M00_AXI [get_bd_intf_pins axi_ctrl/M00_AXI] [get_bd_intf_pins axi_vdma_0/S_AXI_LITE]
  connect_bd_intf_net -intf_net axi_ctrl_M01_AXI [get_bd_intf_pins axi_ctrl/M01_AXI] [get_bd_intf_pins painterengine_gpu_0/S_AXI]
  connect_bd_intf_net -intf_net axi_ctrl_M02_AXI [get_bd_intf_pins axi_ctrl/M02_AXI] [get_bd_intf_pins uart_analyzer_0/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M03_AXI [get_bd_intf_pins axi_ctrl/M03_AXI] [get_bd_intf_pins uart_capture_dma/S_AXI_LITE]
  connect_bd_intf_net -intf_net axi_ctrl_M04_AXI [get_bd_intf_pins axi_ctrl/M04_AXI] [get_bd_intf_pins gpu_batch_0/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M05_AXI [get_bd_intf_pins axi_ctrl/M05_AXI] [get_bd_intf_pins argb_stream_0/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M06_AXI [get_bd_intf_pins axi_ctrl/M06_AXI] [get_bd_intf_pins uart_ch2/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M07_AXI [get_bd_intf_pins axi_ctrl/M07_AXI] [get_bd_intf_pins capture_control/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M08_AXI [get_bd_intf_pins axi_ctrl/M08_AXI] [get_bd_intf_pins uart_ch3/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M09_AXI [get_bd_intf_pins axi_ctrl/M09_AXI] [get_bd_intf_pins uart_ch4/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M10_AXI [get_bd_intf_pins axi_ctrl/M10_AXI] [get_bd_intf_pins i2c_test_control/S_AXI]
  connect_bd_intf_net -intf_net axi_ctrl_M11_AXI [get_bd_intf_pins axi_ctrl/M11_AXI] [get_bd_intf_pins spi_test_control/S_AXI]
  connect_bd_intf_net -intf_net axi_ctrl_M12_AXI [get_bd_intf_pins axi_ctrl/M12_AXI] [get_bd_intf_pins advanced_trigger/s_axi]
  connect_bd_intf_net -intf_net axi_ctrl_M13_AXI [get_bd_intf_pins axi_ctrl/M13_AXI] [get_bd_intf_pins ai_inference/s_axi]
  connect_bd_intf_net -intf_net axi_gpu_mem_M00_AXI [get_bd_intf_pins axi_gpu_mem/M00_AXI] [get_bd_intf_pins ps7_0/S_AXI_HP0]
  connect_bd_intf_net -intf_net axi_uart_mem_M00_AXI [get_bd_intf_pins axi_uart_mem/M00_AXI] [get_bd_intf_pins ps7_0/S_AXI_HP2]
  connect_bd_intf_net -intf_net axi_vdma_0_M_AXIS_MM2S [get_bd_intf_pins axi_vdma_0/M_AXIS_MM2S] [get_bd_intf_pins argb_stream_0/s_axis]
  connect_bd_intf_net -intf_net axi_vdma_0_M_AXI_MM2S [get_bd_intf_pins axi_vdma_0/M_AXI_MM2S] [get_bd_intf_pins axi_vdma_mem/S00_AXI]
  connect_bd_intf_net -intf_net axi_vdma_mem_M00_AXI [get_bd_intf_pins axi_vdma_mem/M00_AXI] [get_bd_intf_pins ps7_0/S_AXI_HP1]
  connect_bd_intf_net -intf_net capture_path_m_axis [get_bd_intf_pins capture_path/m_axis] [get_bd_intf_pins uart_event_fifo/S_AXIS]
  connect_bd_intf_net -intf_net gpu_batch_0_m_axi [get_bd_intf_pins gpu_batch_0/m_axi] [get_bd_intf_pins axi_gpu_mem/S01_AXI]
  connect_bd_intf_net -intf_net painterengine_gpu_0_m00_axi [get_bd_intf_pins painterengine_gpu_0/m00_axi] [get_bd_intf_pins axi_gpu_mem/S00_AXI]
  connect_bd_intf_net -intf_net ps7_0_DDR [get_bd_intf_ports DDR_0] [get_bd_intf_pins ps7_0/DDR]
  connect_bd_intf_net -intf_net ps7_0_FIXED_IO [get_bd_intf_ports FIXED_IO_0] [get_bd_intf_pins ps7_0/FIXED_IO]
  connect_bd_intf_net -intf_net ps7_0_M_AXI_GP0 [get_bd_intf_pins ps7_0/M_AXI_GP0] [get_bd_intf_pins axi_ctrl/S00_AXI]
  connect_bd_intf_net -intf_net uart_analyzer_0_m_axis [get_bd_intf_pins uart_merge/m_axis] [get_bd_intf_pins capture_path/s0_axis]
  connect_bd_intf_net -intf_net uart_analyzer_0_m_axis1 [get_bd_intf_pins uart_analyzer_0/m_axis] [get_bd_intf_pins uart_channel_fifo_0/S_AXIS]
  connect_bd_intf_net -intf_net uart_capture_dma_M_AXI_S2MM [get_bd_intf_pins uart_capture_dma/M_AXI_S2MM] [get_bd_intf_pins axi_uart_mem/S00_AXI]
  connect_bd_intf_net -intf_net uart_capture_dma_M_AXI_SG [get_bd_intf_pins uart_capture_dma/M_AXI_SG] [get_bd_intf_pins axi_uart_mem/S01_AXI]
  connect_bd_intf_net -intf_net uart_ch2_m_axis [get_bd_intf_pins uart_ch2/m_axis] [get_bd_intf_pins uart_channel_fifo_1/S_AXIS]
  connect_bd_intf_net -intf_net uart_ch3_m_axis [get_bd_intf_pins uart_ch3/m_axis] [get_bd_intf_pins uart_channel_fifo_2/S_AXIS]
  connect_bd_intf_net -intf_net uart_ch4_m_axis [get_bd_intf_pins uart_ch4/m_axis] [get_bd_intf_pins uart_channel_fifo_3/S_AXIS]
  connect_bd_intf_net -intf_net uart_channel_fifo_0_M_AXIS [get_bd_intf_pins uart_channel_fifo_0/M_AXIS] [get_bd_intf_pins uart_merge/s0_axis]
  connect_bd_intf_net -intf_net uart_channel_fifo_1_M_AXIS [get_bd_intf_pins uart_channel_fifo_1/M_AXIS] [get_bd_intf_pins uart_merge/s1_axis]
  connect_bd_intf_net -intf_net uart_channel_fifo_2_M_AXIS [get_bd_intf_pins uart_channel_fifo_2/M_AXIS] [get_bd_intf_pins uart_merge/s2_axis]
  connect_bd_intf_net -intf_net uart_channel_fifo_3_M_AXIS [get_bd_intf_pins uart_channel_fifo_3/M_AXIS] [get_bd_intf_pins uart_merge/s3_axis]
  connect_bd_intf_net -intf_net uart_event_fifo_M_AXIS [get_bd_intf_pins uart_event_fifo/M_AXIS] [get_bd_intf_pins uart_capture_dma/S_AXIS_S2MM]
  connect_bd_intf_net -intf_net v_tc_0_vtiming_out [get_bd_intf_pins v_tc_0/vtiming_out] [get_bd_intf_pins v_axi4s_vid_out_0/vtiming_in]
  connect_bd_intf_net -intf_net vector_capture_m_axis [get_bd_intf_pins vector_capture/m_axis] [get_bd_intf_pins capture_path/s1_axis]

  # Create port connections
  connect_bd_net -net Net  [get_bd_ports lcd_scl] \
  [get_bd_pins ps_touch_io_0/touch_scl]
  connect_bd_net -net Net1  [get_bd_ports lcd_sda] \
  [get_bd_pins ps_touch_io_0/touch_sda]
  connect_bd_net -net Net2  [get_bd_ports ct_int] \
  [get_bd_pins ps_touch_io_0/touch_int]
  connect_bd_net -net advanced_trigger_advanced_active  [get_bd_pins advanced_trigger/advanced_active] \
  [get_bd_pins ai_trigger_bridge/native_active]
  connect_bd_net -net advanced_trigger_configured_post_limit  [get_bd_pins advanced_trigger/configured_post_limit] \
  [get_bd_pins ai_trigger_bridge/native_post_limit]
  connect_bd_net -net advanced_trigger_configured_post_ticks  [get_bd_pins advanced_trigger/configured_post_ticks] \
  [get_bd_pins ai_trigger_bridge/native_post_ticks]
  connect_bd_net -net advanced_trigger_trigger_pulse  [get_bd_pins advanced_trigger/trigger_pulse] \
  [get_bd_pins ai_trigger_bridge/native_trigger]
  connect_bd_net -net advanced_trigger_trigger_tick  [get_bd_pins advanced_trigger/trigger_tick] \
  [get_bd_pins ai_trigger_bridge/native_tick]
  connect_bd_net -net ai_inference_ai_post_limit  [get_bd_pins ai_inference/ai_post_limit] \
  [get_bd_pins ai_trigger_bridge/ai_post_limit]
  connect_bd_net -net ai_inference_ai_post_ticks  [get_bd_pins ai_inference/ai_post_ticks] \
  [get_bd_pins ai_trigger_bridge/ai_post_ticks]
  connect_bd_net -net ai_inference_ai_selected  [get_bd_pins ai_inference/ai_selected] \
  [get_bd_pins ai_trigger_bridge/ai_selected]
  connect_bd_net -net ai_inference_ai_tick  [get_bd_pins ai_inference/ai_tick] \
  [get_bd_pins ai_trigger_bridge/ai_tick]
  connect_bd_net -net ai_inference_ai_trigger  [get_bd_pins ai_inference/ai_trigger] \
  [get_bd_pins ai_trigger_bridge/ai_trigger]
  connect_bd_net -net ai_trigger_bridge_advanced_active  [get_bd_pins ai_trigger_bridge/advanced_active] \
  [get_bd_pins capture_control/advanced_active]
  connect_bd_net -net ai_trigger_bridge_advanced_post_limit  [get_bd_pins ai_trigger_bridge/advanced_post_limit] \
  [get_bd_pins capture_control/advanced_post_limit]
  connect_bd_net -net ai_trigger_bridge_advanced_post_ticks  [get_bd_pins ai_trigger_bridge/advanced_post_ticks] \
  [get_bd_pins capture_control/advanced_post_ticks]
  connect_bd_net -net ai_trigger_bridge_advanced_trigger  [get_bd_pins ai_trigger_bridge/advanced_trigger] \
  [get_bd_pins capture_control/advanced_trigger]
  connect_bd_net -net ai_trigger_bridge_advanced_trigger_tick  [get_bd_pins ai_trigger_bridge/advanced_trigger_tick] \
  [get_bd_pins capture_control/advanced_trigger_tick]
  connect_bd_net -net capture_control_capture_channels  [get_bd_pins capture_control/capture_channels] \
  [get_bd_pins advanced_trigger/capture_channels]
  connect_bd_net -net capture_control_capture_clear1  [get_bd_pins capture_control/capture_clear] \
  [get_bd_pins uart_analyzer_0/capture_clear] \
  [get_bd_pins uart_ch2/capture_clear] \
  [get_bd_pins uart_ch3/capture_clear] \
  [get_bd_pins uart_ch4/capture_clear] \
  [get_bd_pins vector_capture/clear] \
  [get_bd_pins ai_inference/capture_clear]
  connect_bd_net -net capture_control_capture_flush1  [get_bd_pins capture_control/capture_flush]
  connect_bd_net -net capture_control_capture_run  [get_bd_pins capture_control/capture_run] \
  [get_bd_pins advanced_trigger/capture_run] \
  [get_bd_pins vector_capture/capture_run] \
  [get_bd_pins ai_inference/capture_run]
  connect_bd_net -net capture_control_capture_run0  [get_bd_pins capture_control/capture_run0] \
  [get_bd_pins uart_analyzer_0/capture_run]
  connect_bd_net -net capture_control_capture_run1  [get_bd_pins capture_control/capture_run1] \
  [get_bd_pins uart_ch2/capture_run]
  connect_bd_net -net capture_control_capture_run2  [get_bd_pins capture_control/capture_run2] \
  [get_bd_pins uart_ch3/capture_run]
  connect_bd_net -net capture_control_capture_run3  [get_bd_pins capture_control/capture_run3] \
  [get_bd_pins uart_ch4/capture_run]
  connect_bd_net -net capture_control_capture_start1  [get_bd_pins capture_control/capture_start] \
  [get_bd_pins advanced_trigger/capture_start] \
  [get_bd_pins vector_capture/capture_start] \
  [get_bd_pins ai_inference/capture_start]
  connect_bd_net -net capture_control_legacy_flush  [get_bd_pins capture_control/legacy_flush] \
  [get_bd_pins uart_analyzer_0/capture_flush] \
  [get_bd_pins uart_ch2/capture_flush] \
  [get_bd_pins uart_ch3/capture_flush] \
  [get_bd_pins uart_ch4/capture_flush]
  connect_bd_net -net capture_control_legacy_start  [get_bd_pins capture_control/legacy_start] \
  [get_bd_pins uart_analyzer_0/capture_start] \
  [get_bd_pins uart_ch2/capture_start] \
  [get_bd_pins uart_ch3/capture_start] \
  [get_bd_pins uart_ch4/capture_start]
  connect_bd_net -net capture_control_stream_resetn  [get_bd_pins capture_control/stream_resetn] \
  [get_bd_pins uart_channel_fifo_0/s_axis_aresetn] \
  [get_bd_pins uart_channel_fifo_1/s_axis_aresetn] \
  [get_bd_pins uart_event_fifo/s_axis_aresetn] \
  [get_bd_pins uart_channel_fifo_2/s_axis_aresetn] \
  [get_bd_pins uart_channel_fifo_3/s_axis_aresetn] \
  [get_bd_pins uart_merge/aresetn]
  connect_bd_net -net capture_control_timestamp1  [get_bd_pins capture_control/timestamp] \
  [get_bd_pins uart_analyzer_0/shared_timestamp] \
  [get_bd_pins uart_ch2/shared_timestamp] \
  [get_bd_pins uart_ch3/shared_timestamp] \
  [get_bd_pins uart_ch4/shared_timestamp] \
  [get_bd_pins advanced_trigger/timestamp] \
  [get_bd_pins vector_capture/timestamp] \
  [get_bd_pins ai_inference/timestamp]
  connect_bd_net -net capture_control_vector_capture_mask  [get_bd_pins capture_control/vector_capture_mask] \
  [get_bd_pins vector_capture/capture_mask] \
  [get_bd_pins ai_inference/capture_mask]
  connect_bd_net -net capture_control_vector_mode  [get_bd_pins capture_control/vector_mode] \
  [get_bd_pins advanced_trigger/vector_mode] \
  [get_bd_pins capture_path/select_vector] \
  [get_bd_pins vector_capture/enable] \
  [get_bd_pins ai_inference/capture_vector_mode]
  connect_bd_net -net capture_levels8_dout  [get_bd_pins capture_levels8/dout] \
  [get_bd_pins vector_capture/levels] \
  [get_bd_pins advanced_trigger/levels] \
  [get_bd_pins ai_inference/levels]
  connect_bd_net -net capture_levels_dout  [get_bd_pins capture_levels/dout] \
  [get_bd_pins uart_analyzer_0/shared_levels] \
  [get_bd_pins uart_ch2/shared_levels] \
  [get_bd_pins uart_ch3/shared_levels] \
  [get_bd_pins uart_ch4/shared_levels] \
  [get_bd_pins capture_levels8/In0]
  connect_bd_net -net clk_pixel_25m_clk_out1  [get_bd_pins clk_pixel_25m/clk_out1] \
  [get_bd_pins v_tc_0/clk] \
  [get_bd_pins v_axi4s_vid_out_0/vid_io_out_clk] \
  [get_bd_pins rst_25m/slowest_sync_clk] \
  [get_bd_ports lcd_clk]
  connect_bd_net -net clk_pixel_25m_locked  [get_bd_pins clk_pixel_25m/locked] \
  [get_bd_pins rst_100m/dcm_locked] \
  [get_bd_pins rst_25m/dcm_locked] \
  [get_bd_pins pixel_reset_not/Op1]
  connect_bd_net -net frame_ptr_zero_dout  [get_bd_pins frame_ptr_zero/dout] \
  [get_bd_pins axi_vdma_0/mm2s_frame_ptr_in]
  connect_bd_net -net i2c_test_control_gpio_io_o  [get_bd_pins i2c_test_control/gpio_io_o] \
  [get_bd_pins i2c_test_source/control]
  connect_bd_net -net i2c_test_source_sampled_scl  [get_bd_pins i2c_test_source/sampled_scl] \
  [get_bd_pins spi_external_lines/In0]
  connect_bd_net -net i2c_test_source_sampled_sda  [get_bd_pins i2c_test_source/sampled_sda] \
  [get_bd_pins spi_external_lines/In1]
  connect_bd_net -net i2c_test_source_status  [get_bd_pins i2c_test_source/status] \
  [get_bd_pins i2c_test_control/gpio2_io_i]
  connect_bd_net -net logic_async_high_dout  [get_bd_pins logic_async_high/dout] \
  [get_bd_pins logic_frontend/logic_async]
  connect_bd_net -net logic_frontend_input_ready  [get_bd_pins logic_frontend/input_ready] \
  [get_bd_pins capture_control/input_ready] \
  [get_bd_pins ai_inference/input_ready]
  connect_bd_net -net logic_frontend_levels  [get_bd_pins logic_frontend/levels] \
  [get_bd_pins capture_levels8/In1]
  connect_bd_net -net logic_in_ch5_1  [get_bd_ports logic_in_ch5] \
  [get_bd_pins logic_async_high/In0]
  connect_bd_net -net logic_in_ch6_1  [get_bd_ports logic_in_ch6] \
  [get_bd_pins logic_async_high/In1]
  connect_bd_net -net logic_in_ch7_1  [get_bd_ports logic_in_ch7] \
  [get_bd_pins logic_async_high/In2]
  connect_bd_net -net logic_in_ch8_1  [get_bd_ports logic_in_ch8] \
  [get_bd_pins logic_async_high/In3]
  connect_bd_net -net logic_one_dout  [get_bd_pins logic_one/dout] \
  [get_bd_pins v_axi4s_vid_out_0/aclken] \
  [get_bd_pins v_axi4s_vid_out_0/vid_io_out_ce] \
  [get_bd_pins v_tc_0/clken] \
  [get_bd_ports lcd_hs] \
  [get_bd_ports lcd_vs] \
  [get_bd_ports lcd_bl] \
  [get_bd_ports lcd_rst_n]
  connect_bd_net -net pixel_reset_not_Res  [get_bd_pins pixel_reset_not/Res] \
  [get_bd_pins v_axi4s_vid_out_0/vid_io_out_reset]
  connect_bd_net -net ps7_0_FCLK_CLK0  [get_bd_pins ps7_0/FCLK_CLK0] \
  [get_bd_pins ps7_0/M_AXI_GP0_ACLK] \
  [get_bd_pins ps7_0/S_AXI_HP0_ACLK] \
  [get_bd_pins ps7_0/S_AXI_HP1_ACLK] \
  [get_bd_pins ps7_0/S_AXI_HP2_ACLK] \
  [get_bd_pins axi_ctrl/ACLK] \
  [get_bd_pins axi_ctrl/S00_ACLK] \
  [get_bd_pins axi_ctrl/M00_ACLK] \
  [get_bd_pins axi_ctrl/M01_ACLK] \
  [get_bd_pins axi_ctrl/M02_ACLK] \
  [get_bd_pins axi_ctrl/M03_ACLK] \
  [get_bd_pins axi_gpu_mem/ACLK] \
  [get_bd_pins axi_gpu_mem/S00_ACLK] \
  [get_bd_pins axi_gpu_mem/M00_ACLK] \
  [get_bd_pins axi_vdma_mem/ACLK] \
  [get_bd_pins axi_vdma_mem/S00_ACLK] \
  [get_bd_pins axi_vdma_mem/M00_ACLK] \
  [get_bd_pins axi_uart_mem/ACLK] \
  [get_bd_pins axi_uart_mem/S00_ACLK] \
  [get_bd_pins axi_uart_mem/M00_ACLK] \
  [get_bd_pins painterengine_gpu_0/S_AXI_ACLK] \
  [get_bd_pins painterengine_gpu_0/i_wire_clock] \
  [get_bd_pins axi_vdma_0/s_axi_lite_aclk] \
  [get_bd_pins axi_vdma_0/m_axi_mm2s_aclk] \
  [get_bd_pins axi_vdma_0/m_axis_mm2s_aclk] \
  [get_bd_pins uart_event_fifo/s_axis_aclk] \
  [get_bd_pins uart_capture_dma/s_axi_lite_aclk] \
  [get_bd_pins uart_capture_dma/m_axi_s2mm_aclk] \
  [get_bd_pins v_axi4s_vid_out_0/aclk] \
  [get_bd_pins clk_pixel_25m/clk_in1] \
  [get_bd_pins rst_100m/slowest_sync_clk] \
  [get_bd_pins axi_ctrl/M04_ACLK] \
  [get_bd_pins axi_gpu_mem/S01_ACLK] \
  [get_bd_pins argb_stream_0/aclk] \
  [get_bd_pins axi_ctrl/M05_ACLK] \
  [get_bd_pins axi_ctrl/M06_ACLK] \
  [get_bd_pins axi_ctrl/M07_ACLK] \
  [get_bd_pins uart_channel_fifo_0/s_axis_aclk] \
  [get_bd_pins uart_channel_fifo_1/s_axis_aclk] \
  [get_bd_pins axi_uart_mem/S01_ACLK] \
  [get_bd_pins uart_capture_dma/m_axi_sg_aclk] \
  [get_bd_pins uart_channel_fifo_2/s_axis_aclk] \
  [get_bd_pins uart_channel_fifo_3/s_axis_aclk] \
  [get_bd_pins axi_ctrl/M08_ACLK] \
  [get_bd_pins axi_ctrl/M09_ACLK] \
  [get_bd_pins axi_ctrl/M10_ACLK] \
  [get_bd_pins i2c_test_control/s_axi_aclk] \
  [get_bd_pins axi_ctrl/M11_ACLK] \
  [get_bd_pins spi_test_control/s_axi_aclk] \
  [get_bd_pins axi_ctrl/M12_ACLK] \
  [get_bd_pins uart_merge/aclk] \
  [get_bd_pins uart_analyzer_0/s_axi_aclk] \
  [get_bd_pins uart_ch2/s_axi_aclk] \
  [get_bd_pins uart_ch3/s_axi_aclk] \
  [get_bd_pins uart_ch4/s_axi_aclk] \
  [get_bd_pins capture_control/aclk] \
  [get_bd_pins advanced_trigger/aclk] \
  [get_bd_pins i2c_test_source/clk] \
  [get_bd_pins spi_test_source/clk] \
  [get_bd_pins capture_path/clk] \
  [get_bd_pins vector_capture/clk] \
  [get_bd_pins logic_frontend/clk] \
  [get_bd_pins gpu_batch_0/s_axi_aclk] \
  [get_bd_pins axi_ctrl/M13_ACLK] \
  [get_bd_pins ai_inference/clk]
  connect_bd_net -net ps7_0_FCLK_RESET0_N  [get_bd_pins ps7_0/FCLK_RESET0_N] \
  [get_bd_pins ps_reset_not/Op1]
  connect_bd_net -net ps7_0_GPIO_O  [get_bd_pins ps7_0/GPIO_O] \
  [get_bd_pins ps_touch_io_0/gpio_o]
  connect_bd_net -net ps7_0_GPIO_T  [get_bd_pins ps7_0/GPIO_T] \
  [get_bd_pins ps_touch_io_0/gpio_t]
  connect_bd_net -net ps7_0_I2C0_SCL_O  [get_bd_pins ps7_0/I2C0_SCL_O] \
  [get_bd_pins ps_touch_io_0/i2c_scl_o]
  connect_bd_net -net ps7_0_I2C0_SCL_T  [get_bd_pins ps7_0/I2C0_SCL_T] \
  [get_bd_pins ps_touch_io_0/i2c_scl_t]
  connect_bd_net -net ps7_0_I2C0_SDA_O  [get_bd_pins ps7_0/I2C0_SDA_O] \
  [get_bd_pins ps_touch_io_0/i2c_sda_o]
  connect_bd_net -net ps7_0_I2C0_SDA_T  [get_bd_pins ps7_0/I2C0_SDA_T] \
  [get_bd_pins ps_touch_io_0/i2c_sda_t]
  connect_bd_net -net ps_reset_not_Res  [get_bd_pins ps_reset_not/Res] \
  [get_bd_pins rst_100m/ext_reset_in] \
  [get_bd_pins rst_25m/ext_reset_in]
  connect_bd_net -net ps_touch_io_0_gpio_i  [get_bd_pins ps_touch_io_0/gpio_i] \
  [get_bd_pins ps7_0/GPIO_I]
  connect_bd_net -net ps_touch_io_0_i2c_scl_i  [get_bd_pins ps_touch_io_0/i2c_scl_i] \
  [get_bd_pins ps7_0/I2C0_SCL_I]
  connect_bd_net -net ps_touch_io_0_i2c_sda_i  [get_bd_pins ps_touch_io_0/i2c_sda_i] \
  [get_bd_pins ps7_0/I2C0_SDA_I]
  connect_bd_net -net ps_touch_io_0_touch_rst  [get_bd_pins ps_touch_io_0/touch_rst] \
  [get_bd_ports ct_rst]
  connect_bd_net -net rst_100m_peripheral_aresetn  [get_bd_pins rst_100m/peripheral_aresetn] \
  [get_bd_pins axi_ctrl/ARESETN] \
  [get_bd_pins axi_ctrl/S00_ARESETN] \
  [get_bd_pins axi_ctrl/M00_ARESETN] \
  [get_bd_pins axi_ctrl/M01_ARESETN] \
  [get_bd_pins axi_ctrl/M02_ARESETN] \
  [get_bd_pins axi_ctrl/M03_ARESETN] \
  [get_bd_pins axi_gpu_mem/ARESETN] \
  [get_bd_pins axi_gpu_mem/S00_ARESETN] \
  [get_bd_pins axi_gpu_mem/M00_ARESETN] \
  [get_bd_pins axi_vdma_mem/ARESETN] \
  [get_bd_pins axi_vdma_mem/S00_ARESETN] \
  [get_bd_pins axi_vdma_mem/M00_ARESETN] \
  [get_bd_pins axi_uart_mem/ARESETN] \
  [get_bd_pins axi_uart_mem/S00_ARESETN] \
  [get_bd_pins axi_uart_mem/M00_ARESETN] \
  [get_bd_pins painterengine_gpu_0/i_wire_resetn] \
  [get_bd_pins axi_vdma_0/axi_resetn] \
  [get_bd_pins uart_capture_dma/axi_resetn] \
  [get_bd_pins v_axi4s_vid_out_0/aresetn] \
  [get_bd_pins axi_ctrl/M04_ARESETN] \
  [get_bd_pins axi_gpu_mem/S01_ARESETN] \
  [get_bd_pins argb_stream_0/aresetn] \
  [get_bd_pins axi_ctrl/M05_ARESETN] \
  [get_bd_pins axi_ctrl/M06_ARESETN] \
  [get_bd_pins axi_ctrl/M07_ARESETN] \
  [get_bd_pins axi_uart_mem/S01_ARESETN] \
  [get_bd_pins axi_ctrl/M08_ARESETN] \
  [get_bd_pins axi_ctrl/M09_ARESETN] \
  [get_bd_pins axi_ctrl/M10_ARESETN] \
  [get_bd_pins i2c_test_control/s_axi_aresetn] \
  [get_bd_pins axi_ctrl/M11_ARESETN] \
  [get_bd_pins spi_test_control/s_axi_aresetn] \
  [get_bd_pins axi_ctrl/M12_ARESETN] \
  [get_bd_pins uart_analyzer_0/s_axi_aresetn] \
  [get_bd_pins uart_ch2/s_axi_aresetn] \
  [get_bd_pins uart_ch3/s_axi_aresetn] \
  [get_bd_pins uart_ch4/s_axi_aresetn] \
  [get_bd_pins capture_control/aresetn] \
  [get_bd_pins advanced_trigger/aresetn] \
  [get_bd_pins i2c_test_source/resetn] \
  [get_bd_pins spi_test_source/resetn] \
  [get_bd_pins capture_path/resetn] \
  [get_bd_pins vector_capture/resetn] \
  [get_bd_pins logic_frontend/resetn] \
  [get_bd_pins gpu_batch_0/s_axi_aresetn] \
  [get_bd_pins axi_ctrl/M13_ARESETN] \
  [get_bd_pins ai_inference/resetn]
  connect_bd_net -net rst_25m_peripheral_aresetn  [get_bd_pins rst_25m/peripheral_aresetn] \
  [get_bd_pins v_tc_0/resetn]
  connect_bd_net -net spi_external_lines_dout  [get_bd_pins spi_external_lines/dout] \
  [get_bd_pins spi_test_source/external_lines]
  connect_bd_net -net spi_line_0_Dout  [get_bd_pins spi_line_0/Dout] \
  [get_bd_pins uart_analyzer_0/uart_rx]
  connect_bd_net -net spi_line_1_Dout  [get_bd_pins spi_line_1/Dout] \
  [get_bd_pins uart_ch2/uart_rx]
  connect_bd_net -net spi_line_2_Dout  [get_bd_pins spi_line_2/Dout] \
  [get_bd_pins uart_ch3/uart_rx]
  connect_bd_net -net spi_line_3_Dout  [get_bd_pins spi_line_3/Dout] \
  [get_bd_pins uart_ch4/uart_rx]
  connect_bd_net -net spi_test_control_gpio_io_o  [get_bd_pins spi_test_control/gpio_io_o] \
  [get_bd_pins spi_test_source/control]
  connect_bd_net -net spi_test_source_sampled_lines  [get_bd_pins spi_test_source/sampled_lines] \
  [get_bd_pins spi_line_0/Din] \
  [get_bd_pins spi_line_1/Din] \
  [get_bd_pins spi_line_2/Din] \
  [get_bd_pins spi_line_3/Din]
  connect_bd_net -net spi_test_source_status  [get_bd_pins spi_test_source/status] \
  [get_bd_pins spi_test_control/gpio2_io_i]
  connect_bd_net -net trigger_count_dout  [get_bd_pins trigger_count/dout] \
  [get_bd_pins capture_control/legacy_event_count]
  connect_bd_net -net trigger_data_dout  [get_bd_pins trigger_data/dout] \
  [get_bd_pins advanced_trigger/uart_data]
  connect_bd_net -net trigger_false_start_dout  [get_bd_pins trigger_false_start/dout] \
  [get_bd_pins advanced_trigger/uart_false_start]
  connect_bd_net -net trigger_frame_dout  [get_bd_pins trigger_frame/dout] \
  [get_bd_pins advanced_trigger/uart_frame_error]
  connect_bd_net -net trigger_parity_dout  [get_bd_pins trigger_parity/dout] \
  [get_bd_pins advanced_trigger/uart_parity_error]
  connect_bd_net -net trigger_parity_enabled_dout  [get_bd_pins trigger_parity_enabled/dout] \
  [get_bd_pins advanced_trigger/parity_enabled]
  connect_bd_net -net trigger_quality_dout  [get_bd_pins trigger_quality/dout] \
  [get_bd_pins advanced_trigger/quality_fault]
  connect_bd_net -net trigger_start_dout  [get_bd_pins trigger_start/dout] \
  [get_bd_pins advanced_trigger/uart_frame_start]
  connect_bd_net -net trigger_tick_dout  [get_bd_pins trigger_tick/dout] \
  [get_bd_pins advanced_trigger/uart_tick]
  connect_bd_net -net trigger_valid_dout  [get_bd_pins trigger_valid/dout] \
  [get_bd_pins advanced_trigger/uart_valid]
  connect_bd_net -net uart_analyzer_0_irq  [get_bd_pins uart_analyzer_0/irq] \
  [get_bd_pins uart_irq_concat/In0]
  connect_bd_net -net uart_analyzer_0_sampled_level  [get_bd_pins uart_analyzer_0/sampled_level] \
  [get_bd_pins capture_levels/In0]
  connect_bd_net -net uart_analyzer_0_tap_data  [get_bd_pins uart_analyzer_0/tap_data] \
  [get_bd_pins trigger_data/In0]
  connect_bd_net -net uart_analyzer_0_tap_event_count  [get_bd_pins uart_analyzer_0/tap_event_count] \
  [get_bd_pins trigger_count/In0]
  connect_bd_net -net uart_analyzer_0_tap_false_start  [get_bd_pins uart_analyzer_0/tap_false_start] \
  [get_bd_pins trigger_false_start/In0]
  connect_bd_net -net uart_analyzer_0_tap_frame_error  [get_bd_pins uart_analyzer_0/tap_frame_error] \
  [get_bd_pins trigger_frame/In0]
  connect_bd_net -net uart_analyzer_0_tap_frame_start  [get_bd_pins uart_analyzer_0/tap_frame_start] \
  [get_bd_pins trigger_start/In0]
  connect_bd_net -net uart_analyzer_0_tap_parity_enabled  [get_bd_pins uart_analyzer_0/tap_parity_enabled] \
  [get_bd_pins trigger_parity_enabled/In0]
  connect_bd_net -net uart_analyzer_0_tap_parity_error  [get_bd_pins uart_analyzer_0/tap_parity_error] \
  [get_bd_pins trigger_parity/In0]
  connect_bd_net -net uart_analyzer_0_tap_quality_fault  [get_bd_pins uart_analyzer_0/tap_quality_fault] \
  [get_bd_pins trigger_quality/In0]
  connect_bd_net -net uart_analyzer_0_tap_tick  [get_bd_pins uart_analyzer_0/tap_tick] \
  [get_bd_pins trigger_tick/In0]
  connect_bd_net -net uart_analyzer_0_tap_valid  [get_bd_pins uart_analyzer_0/tap_valid] \
  [get_bd_pins trigger_valid/In0]
  connect_bd_net -net uart_analyzer_0_trigger_event1  [get_bd_pins uart_analyzer_0/trigger_event] \
  [get_bd_pins capture_control/trigger0]
  connect_bd_net -net uart_analyzer_rx_1  [get_bd_ports uart_analyzer_rx] \
  [get_bd_pins i2c_test_source/external_scl]
  connect_bd_net -net uart_capture_dma_s2mm_introut  [get_bd_pins uart_capture_dma/s2mm_introut] \
  [get_bd_pins uart_irq_concat/In1]
  connect_bd_net -net uart_ch2_sampled_level  [get_bd_pins uart_ch2/sampled_level] \
  [get_bd_pins capture_levels/In1]
  connect_bd_net -net uart_ch2_tap_data  [get_bd_pins uart_ch2/tap_data] \
  [get_bd_pins trigger_data/In1]
  connect_bd_net -net uart_ch2_tap_event_count  [get_bd_pins uart_ch2/tap_event_count] \
  [get_bd_pins trigger_count/In1]
  connect_bd_net -net uart_ch2_tap_false_start  [get_bd_pins uart_ch2/tap_false_start] \
  [get_bd_pins trigger_false_start/In1]
  connect_bd_net -net uart_ch2_tap_frame_error  [get_bd_pins uart_ch2/tap_frame_error] \
  [get_bd_pins trigger_frame/In1]
  connect_bd_net -net uart_ch2_tap_frame_start  [get_bd_pins uart_ch2/tap_frame_start] \
  [get_bd_pins trigger_start/In1]
  connect_bd_net -net uart_ch2_tap_parity_enabled  [get_bd_pins uart_ch2/tap_parity_enabled] \
  [get_bd_pins trigger_parity_enabled/In1]
  connect_bd_net -net uart_ch2_tap_parity_error  [get_bd_pins uart_ch2/tap_parity_error] \
  [get_bd_pins trigger_parity/In1]
  connect_bd_net -net uart_ch2_tap_quality_fault  [get_bd_pins uart_ch2/tap_quality_fault] \
  [get_bd_pins trigger_quality/In1]
  connect_bd_net -net uart_ch2_tap_tick  [get_bd_pins uart_ch2/tap_tick] \
  [get_bd_pins trigger_tick/In1]
  connect_bd_net -net uart_ch2_tap_valid  [get_bd_pins uart_ch2/tap_valid] \
  [get_bd_pins trigger_valid/In1]
  connect_bd_net -net uart_ch2_trigger_event1  [get_bd_pins uart_ch2/trigger_event] \
  [get_bd_pins capture_control/trigger1]
  connect_bd_net -net uart_ch3_sampled_level  [get_bd_pins uart_ch3/sampled_level] \
  [get_bd_pins capture_levels/In2]
  connect_bd_net -net uart_ch3_tap_data  [get_bd_pins uart_ch3/tap_data] \
  [get_bd_pins trigger_data/In2]
  connect_bd_net -net uart_ch3_tap_event_count  [get_bd_pins uart_ch3/tap_event_count] \
  [get_bd_pins trigger_count/In2]
  connect_bd_net -net uart_ch3_tap_false_start  [get_bd_pins uart_ch3/tap_false_start] \
  [get_bd_pins trigger_false_start/In2]
  connect_bd_net -net uart_ch3_tap_frame_error  [get_bd_pins uart_ch3/tap_frame_error] \
  [get_bd_pins trigger_frame/In2]
  connect_bd_net -net uart_ch3_tap_frame_start  [get_bd_pins uart_ch3/tap_frame_start] \
  [get_bd_pins trigger_start/In2]
  connect_bd_net -net uart_ch3_tap_parity_enabled  [get_bd_pins uart_ch3/tap_parity_enabled] \
  [get_bd_pins trigger_parity_enabled/In2]
  connect_bd_net -net uart_ch3_tap_parity_error  [get_bd_pins uart_ch3/tap_parity_error] \
  [get_bd_pins trigger_parity/In2]
  connect_bd_net -net uart_ch3_tap_quality_fault  [get_bd_pins uart_ch3/tap_quality_fault] \
  [get_bd_pins trigger_quality/In2]
  connect_bd_net -net uart_ch3_tap_tick  [get_bd_pins uart_ch3/tap_tick] \
  [get_bd_pins trigger_tick/In2]
  connect_bd_net -net uart_ch3_tap_valid  [get_bd_pins uart_ch3/tap_valid] \
  [get_bd_pins trigger_valid/In2]
  connect_bd_net -net uart_ch3_trigger_event  [get_bd_pins uart_ch3/trigger_event] \
  [get_bd_pins capture_control/trigger2]
  connect_bd_net -net uart_ch4_sampled_level  [get_bd_pins uart_ch4/sampled_level] \
  [get_bd_pins capture_levels/In3]
  connect_bd_net -net uart_ch4_tap_data  [get_bd_pins uart_ch4/tap_data] \
  [get_bd_pins trigger_data/In3]
  connect_bd_net -net uart_ch4_tap_event_count  [get_bd_pins uart_ch4/tap_event_count] \
  [get_bd_pins trigger_count/In3]
  connect_bd_net -net uart_ch4_tap_false_start  [get_bd_pins uart_ch4/tap_false_start] \
  [get_bd_pins trigger_false_start/In3]
  connect_bd_net -net uart_ch4_tap_frame_error  [get_bd_pins uart_ch4/tap_frame_error] \
  [get_bd_pins trigger_frame/In3]
  connect_bd_net -net uart_ch4_tap_frame_start  [get_bd_pins uart_ch4/tap_frame_start] \
  [get_bd_pins trigger_start/In3]
  connect_bd_net -net uart_ch4_tap_parity_enabled  [get_bd_pins uart_ch4/tap_parity_enabled] \
  [get_bd_pins trigger_parity_enabled/In3]
  connect_bd_net -net uart_ch4_tap_parity_error  [get_bd_pins uart_ch4/tap_parity_error] \
  [get_bd_pins trigger_parity/In3]
  connect_bd_net -net uart_ch4_tap_quality_fault  [get_bd_pins uart_ch4/tap_quality_fault] \
  [get_bd_pins trigger_quality/In3]
  connect_bd_net -net uart_ch4_tap_tick  [get_bd_pins uart_ch4/tap_tick] \
  [get_bd_pins trigger_tick/In3]
  connect_bd_net -net uart_ch4_tap_valid  [get_bd_pins uart_ch4/tap_valid] \
  [get_bd_pins trigger_valid/In3]
  connect_bd_net -net uart_ch4_trigger_event  [get_bd_pins uart_ch4/trigger_event] \
  [get_bd_pins capture_control/trigger3]
  connect_bd_net -net uart_irq_concat_dout  [get_bd_pins uart_irq_concat/dout] \
  [get_bd_pins ps7_0/IRQ_F2P]
  connect_bd_net -net uart_rx_ch2_1  [get_bd_ports uart_rx_ch2] \
  [get_bd_pins i2c_test_source/external_sda]
  connect_bd_net -net uart_rx_ch3_1  [get_bd_ports uart_rx_ch3] \
  [get_bd_pins spi_external_lines/In2]
  connect_bd_net -net uart_rx_ch4_1  [get_bd_ports uart_rx_ch4] \
  [get_bd_pins spi_external_lines/In3]
  connect_bd_net -net v_axi4s_vid_out_0_vid_active_video  [get_bd_pins v_axi4s_vid_out_0/vid_active_video] \
  [get_bd_ports lcd_de]
  connect_bd_net -net v_axi4s_vid_out_0_vid_data  [get_bd_pins v_axi4s_vid_out_0/vid_data] \
  [get_bd_ports lcd_rgb]
  connect_bd_net -net v_axi4s_vid_out_0_vtg_ce  [get_bd_pins v_axi4s_vid_out_0/vtg_ce] \
  [get_bd_pins v_tc_0/gen_clken]
  connect_bd_net -net vector_capture_capture_event_count  [get_bd_pins vector_capture/capture_event_count] \
  [get_bd_pins capture_control/vector_event_count]
  connect_bd_net -net vector_capture_change_attempted  [get_bd_pins vector_capture/change_attempted] \
  [get_bd_pins capture_control/vector_attempted]
  connect_bd_net -net vector_capture_change_dropped  [get_bd_pins vector_capture/change_dropped] \
  [get_bd_pins capture_control/vector_dropped]
  connect_bd_net -net vector_capture_change_queued  [get_bd_pins vector_capture/change_queued] \
  [get_bd_pins capture_control/vector_queued]
  connect_bd_net -net vector_capture_event_sequence  [get_bd_pins vector_capture/event_sequence] \
  [get_bd_pins capture_control/vector_sequence]
  connect_bd_net -net vector_capture_fifo_level  [get_bd_pins vector_capture/fifo_level] \
  [get_bd_pins capture_control/vector_fifo_level]
  connect_bd_net -net vector_capture_fifo_peak  [get_bd_pins vector_capture/fifo_peak] \
  [get_bd_pins capture_control/vector_fifo_peak]
  connect_bd_net -net vector_capture_first_loss_tick  [get_bd_pins vector_capture/first_loss_tick] \
  [get_bd_pins capture_control/vector_first_loss]
  connect_bd_net -net vector_capture_rejected_start  [get_bd_pins vector_capture/rejected_start] \
  [get_bd_pins capture_control/vector_rejected_start]
  connect_bd_net -net vector_capture_status  [get_bd_pins vector_capture/status] \
  [get_bd_pins capture_control/vector_status]

  # Create address segments
  assign_bd_address -offset 0x45300000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs advanced_trigger/s_axi/reg0] -force
  assign_bd_address -offset 0x45400000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs ai_inference/s_axi/reg0] -force
  assign_bd_address -offset 0x44C00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs argb_stream_0/s_axi/reg0] -force
  assign_bd_address -offset 0x43000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs axi_vdma_0/S_AXI_LITE/Reg] -force
  assign_bd_address -offset 0x44E00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs capture_control/s_axi/reg0] -force
  assign_bd_address -offset 0x44B00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs gpu_batch_0/s_axi/reg0] -force
  assign_bd_address -offset 0x45100000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs i2c_test_control/S_AXI/Reg] -force
  assign_bd_address -offset 0x60000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs painterengine_gpu_0/S_AXI/reg0] -force
  assign_bd_address -offset 0x45200000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs spi_test_control/S_AXI/Reg] -force
  assign_bd_address -offset 0x44A00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs uart_analyzer_0/s_axi/reg0] -force
  assign_bd_address -offset 0x40400000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs uart_capture_dma/S_AXI_LITE/Reg] -force
  assign_bd_address -offset 0x44D00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs uart_ch2/s_axi/reg0] -force
  assign_bd_address -offset 0x44F00000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs uart_ch3/s_axi/reg0] -force
  assign_bd_address -offset 0x45000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces ps7_0/Data] [get_bd_addr_segs uart_ch4/s_axi/reg0] -force
  assign_bd_address -offset 0x00000000 -range 0x40000000 -target_address_space [get_bd_addr_spaces painterengine_gpu_0/m00_axi] [get_bd_addr_segs ps7_0/S_AXI_HP0/HP0_DDR_LOWOCM] -force
  assign_bd_address -offset 0x00000000 -range 0x40000000 -target_address_space [get_bd_addr_spaces uart_capture_dma/Data_S2MM] [get_bd_addr_segs ps7_0/S_AXI_HP2/HP2_DDR_LOWOCM] -force
  assign_bd_address -offset 0x00000000 -range 0x40000000 -target_address_space [get_bd_addr_spaces uart_capture_dma/Data_SG] [get_bd_addr_segs ps7_0/S_AXI_HP2/HP2_DDR_LOWOCM] -force
  assign_bd_address -offset 0x00000000 -range 0x40000000 -target_address_space [get_bd_addr_spaces axi_vdma_0/Data_MM2S] [get_bd_addr_segs ps7_0/S_AXI_HP1/HP1_DDR_LOWOCM] -force
  assign_bd_address -offset 0x00000000 -range 0x40000000 -target_address_space [get_bd_addr_spaces gpu_batch_0/m_axi] [get_bd_addr_segs ps7_0/S_AXI_HP0/HP0_DDR_LOWOCM] -force


  # Restore current instance
  current_bd_instance $oldCurInst

  validate_bd_design
  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


