
.global _start
.section .text
_start:
    # a0 = hartid
    # a1 = dtb pointer
    # read FDT magic at (a1)
    lw t0, 0(a1)
    li t1, 0xedfe0dd0
    bne t0, t1, fail
    
pass:
    li a0, 0
    ebreak
    
fail:
    li a0, 1
    ebreak
