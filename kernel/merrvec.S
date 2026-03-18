#include "asm.h"

.section merrentry
.globl merrvec
.align 0x4
merrvec:
    # Machine errors are fatal. Switch to a known-good per-hart stack
    # before entering C so we do not depend on the interrupted $sp.
    csrwr   $t0, LOONGARCH_CSR_MERRSAVE
    csrrd   $t0, LOONGARCH_CSR_CPUID
    add.d   $tp, $t0, $zero

    la      $sp, stack0
    li.d    $t1, 4096
    addi.d  $t0, $t0, 1
    mul.d   $t1, $t1, $t0
    add.d   $sp, $sp, $t1

    bl      merrtrap

1:
    b       1b
