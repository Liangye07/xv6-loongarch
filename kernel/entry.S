#include "asm.h"

.section .text
.global _entry
_entry:

    la.global   $t0, 1f
    jirl        $zero, $t0, 0

1:

    li.d        $t0, 0x9000000000000011   # CA(Cached), PLV0, 0x9000...
    csrwr       $t0, LOONGARCH_CSR_DMWIN0

    li.d        $t0, 0x9100000000000010
    csrwr       $t0, LOONGARCH_CSR_DMWIN1
    
    li.d        $t0, 0
    csrwr       $t0, LOONGARCH_CSR_DMWIN2
    csrwr       $t0, LOONGARCH_CSR_DMWIN3

    # 配置 CRMD：开启分页(PG=1)，设置 PLV0，关闭中断(IE=0)
    # 0xb0 = 10110000b (取决于具体架构定义，通常包含 DA=0, PG=1)
    li.w        $t0, 0xb0
    csrwr       $t0, LOONGARCH_CSR_CRMD

    # 彻底清空 TLB
    invtlb      0x0, $zero, $zero

    # 设置栈指针
    la          $sp, stack0
    li.d        $a0, 4096             # 4KB per CPU
    csrrd       $a1, LOONGARCH_CSR_CPUID
    # 顺手把 CPUID 给 $tp，虽然 start.c 还会设一遍，但这能保证 bl start 前 $tp 可用
    add.d       $tp, $a1, $zero       
    
    addi.d      $a1, $a1, 1
    mul.d       $a0, $a0, $a1
    add.d       $sp, $sp, $a0


    bl          start

spin:
    b           spin