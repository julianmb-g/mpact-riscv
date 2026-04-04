.global _start
_start:
    # Authentic OS boot payload 
    # Check FDT magic
    li t0, 0xd00dfeed
    sw a0, 0(x0)
    sw a1, 4(x0)
    ebreak
