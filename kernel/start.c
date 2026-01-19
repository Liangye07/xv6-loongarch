#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

void main();
void timerinit();

// 这里的 stack0 会被 entry.S 使用
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void
start()
{
  // 1. 设置 PRMD (即执行 ertn 后的状态)
  uint64 x = csrrd_prmd();
  x &= ~CSR_CRMD_PLV;     // 设置返回后处于 PLV0 (最高特权级)
  
  // 优化：在 start 阶段先保持 IE=0 (关闭中断)
  // 等 main.c 完成所有初始化后再由具体的函数开启
  x &= ~CSR_CRMD_IE;      
  csrwr_prmd(x);

  // 2. 设置返回地址 (ERA) 为 main 函数
  csrwr_era((uint64)main);

  // 3. 基础硬件初始化 (时钟)
  timerinit();

  // 4. 将 hartid 写入 $tp 寄存器
  // xv6 很多宏 (如 cpuid) 依赖 $tp 寄存器
  uint64 id = r_cpuid();
  asm volatile("move $tp, %0" : : "r" (id));

  // 5. 执行 ertn
  // 该指令会将 PRMD 里的 PLV/IE 应用到当前状态，并跳转到 ERA 指向的地址
  asm volatile("ertn");
}

void
timerinit()
{
  // 设置恒定频率定时器
  // interval = 时钟频率 * 时间(s)。这里预设一个值，后续根据 QEMU 频率调整
  uint64 interval = 10000000; 
  csrwr_tcfg(interval | CSR_TCFG_EN | CSR_TCFG_PER);

  // 开启定时器中断位 (TI) 的局部使能
  uint64 ecfg = csrrd_ecfg();
  ecfg |= ECFG_LIE_TI; 
  csrwr_ecfg(ecfg);
}