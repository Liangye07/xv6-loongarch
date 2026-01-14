#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

void main();
void timerinit();

__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void
start()
{
  // ertn 返回后的状态
  uint64 x = csrrd_prmd();
  x &= ~CSR_CRMD_PLV;     // 返回后处于 PLV0
  x |= CSR_CRMD_IE;       // 返回后开启全局中断
  csrwr_prmd(x);

  // 设置返回地址为 main
  csrwr_era((uint64)main);

  // 基础硬件初始化
  timerinit();

  // 将 hartid 写入 $tp 寄存器
  uint64 id = r_cpuid();
  asm volatile("move $tp, %0" : : "r" (id));

  // 执行 ertn 跳转至 main
  asm volatile("ertn");
}

void
timerinit()
{
  // 设置 10ms 定时器频率
  uint64 interval = 1000000;
  csrwr_tcfg(interval | CSR_TCFG_EN | CSR_TCFG_PER);

  // 开启定时器中断使能
  uint64 ecfg = csrrd_ecfg();
  ecfg |= ECFG_LIE_TI; 
  csrwr_ecfg(ecfg);
}