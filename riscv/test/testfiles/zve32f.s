.text
.globl _start
_start:
    vsetivli t0, 4, e32, m1, ta, ma
    vfsqrt.v v1, v2
    vfadd.vv v1, v2, v3
