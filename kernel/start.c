#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

void main();
void timerinit();

// 这里的 stack0 会被 entry.S 使用
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// start 函数在 DMW 直接映射模式下运行
void
start()
{
  // 1. 设置 PRMD (即执行 ertn 后的状态)
  uint64 x = csrrd_prmd();
  x &= ~CSR_CRMD_PLV;     // 设置返回后处于 PLV0 (最高特权级)
  
  // 保持 IE=0 (关闭中断)，直到内核初始化后期再开启
  x &= ~CSR_CRMD_IE;      
  csrwr_prmd(x);

  // 2. 设置返回地址 (ERA) 为 main 函数
  csrwr_era((uint64)main);

  // 3. 基础硬件初始化 (时钟)
  // timerinit();

  // 4. 将 hartid 写入 $tp 寄存器
  // 使用 move 指令将 cpuid 放入 $tp
  uint64 id = r_cpuid();
  asm volatile("move $tp, %0" : : "r" (id));

  // 5. 执行 ertn
  // 此时 CRMD 仍然是 DA=1, PG=0 (直接映射)
  // 跳转到 main 后，main 会调用 kvminithart 切换到分页模式
  asm volatile("ertn");
}
/*
void
timerinit()
{
  // 设置恒定频率定时器 (10MHz 假设)
  uint64 interval = 10000000; 
  csrwr_tcfg(interval | CSR_TCFG_EN | CSR_TCFG_PER);

  // 开启定时器中断位 (TI) 的局部使能
  uint64 ecfg = csrrd_ecfg();
  ecfg |= ECFG_LIE_TI; 
  csrwr_ecfg(ecfg);
}*/