/* Add this file and its matching .tgf to the selected Vitis build. */
.section .rodata.tracegpu_font_ui_regular_16, "a", %progbits
.balign 4
.global tracegpu_font_ui_regular_16_start
.global tracegpu_font_ui_regular_16_end
tracegpu_font_ui_regular_16_start:
.incbin "tracegpu_font_ui_regular_16.tgf"
tracegpu_font_ui_regular_16_end:
.balign 4
