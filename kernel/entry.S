#include "asm.h"

.section .text
.global _entry
_entry:
    # DMWIN0
    li.d        $t0, 0x9000000000000011   
    csrwr       $t0, LOONGARCH_CSR_DMWIN0

    #  跳转
    la.global   $t0, 1f
    jirl        $zero, $t0, 0

1:
    #  设置栈 
    la          $sp, stack0
    li.d        $a0, 4096             # 每个栈的大小
    
    # 获取 CPU ID
    csrrd       $a1, LOONGARCH_CSR_CPUID
    add.d       $tp, $a1, $zero       # tp = hartid
    
    # 计算当前 CPU 的栈顶
    addi.d      $a1, $a1, 1           # a1 = hartid + 1
    mul.d       $a0, $a0, $a1
    add.d       $sp, $sp, $a0

    bl          main

spin:
    b           spin

.section .data
.globl stack0
.align 16
stack0:
    .space 4096 * 4