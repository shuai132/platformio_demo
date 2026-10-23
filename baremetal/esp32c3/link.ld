ENTRY(_start)
MEMORY {
  iram (rx) : ORIGIN = 0x40380400, LENGTH = 0xfc00
  dram (rw) : ORIGIN = 0x3fc90000, LENGTH = 0x40000
}
SECTIONS {
  .iram0.text : ALIGN(16) {
    KEEP(*(.text.start))
    *(.text .text.* .gnu.linkonce.t.*)
    . = ALIGN(16);
  } > iram
  .dram0.data : ALIGN(16) {
    *(.rodata .rodata.* .srodata .srodata.* .gnu.linkonce.r.*)
    *(.data .data.* .sdata .sdata.* .gnu.linkonce.d.* .gnu.linkonce.s.*)
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
    . = ALIGN(16);
  } > dram
  __global_pointer$ = ADDR(.dram0.data) + 0x800;
  .dram0.bss (NOLOAD) : ALIGN(16) {
    __bss_start = .;
    *(.bss .bss.* .sbss .sbss.* .gnu.linkonce.b.* .gnu.linkonce.sb.* COMMON)
    . = ALIGN(16);
    __bss_end = .;
  } > dram
  .heap (NOLOAD) : ALIGN(16) {
    __heap_start = .;
    . += 0x8000;
    __heap_end = .;
  } > dram
  .stack ORIGIN(dram) + LENGTH(dram) - 0x4000 (NOLOAD) : {
    __stack_bottom = .;
    . += 0x4000;
    __stack_top = .;
  } > dram
  ASSERT(__heap_end <= __stack_bottom, "RAM overflow: heap overlaps stack")
  /DISCARD/ : { *(.eh_frame* .gcc_except_table* .comment .note*) }
}
PROVIDE(ets_printf = 0x40000040);
PROVIDE(uart_tx_one_char = 0x40000068);
PROVIDE(uart_div_modify = 0x40000088);
PROVIDE(ets_update_cpu_frequency = 0x40000588);
