
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
    EXTMEM (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

SECTIONS {
    .text : {
        *(.text)
    } > EXTMEM
}
