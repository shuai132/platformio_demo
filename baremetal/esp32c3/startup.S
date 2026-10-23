.section .text.start,"ax",@progbits
.global _start
.type _start,@function
_start:
  csrci mstatus, 8
  /* C3 uses a memory-mapped interrupt controller, not the standard mie CSR. */
  li t0, 0x600c2104
  sw zero, 0(t0)
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop
  la sp, __stack_top
  la t0, trap_entry
  csrw mtvec, t0
  la t0, __bss_start
  la t1, __bss_end
1:
  bgeu t0, t1, 2f
  sw zero, 0(t0)
  addi t0, t0, 4
  j 1b
2:
  call baremetal_start
3:
  j 3b
.size _start, .-_start
.balign 256
trap_entry:
  la sp, __stack_top
  csrr a0, mcause
  csrr a1, mepc
  csrr a2, mtval
  call baremetal_trap
4:
  j 4b
