.global _start
_start:
    add a0, a0, a1
    sub a0, a0, a1
    # Check FDT magic
    lwu t1, 0(a1)
    li t0, 0xedfe0dd0
    bne t1, t0, fail
    li a0, 0
    ebreak
fail:
    li a0, 1
    ebreak
