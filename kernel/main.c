#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"
#include "proc.h"      // struct proc, allocproc, killed 等
#include "spinlock.h"  // struct spinlock, acquire/release

#include "sleeplock.h"  // <--- 必须在 file.h 之前！
#include "fs.h"
#include "file.h" // 确保能看到 NDEV 的定义（如果 NDEV 在 param.h 里，也要包含）

struct devsw devsw[NDEV]; // 去掉 extern！这就是“生出”这个变量
/*
 * LoongArch xv6 内核主入口
 * 增加了时钟中断测试逻辑：开启中断后，系统应能周期性打印 "."
 */
void
main(void)
{
  // 1. 基础输出初始化
  consoleinit();
  printfinit();
  printf("\n\n==============================\n");
  printf("xv6-loongarch: booting...\n");
  printf("==============================\n");

  // 2. 初始化物理内存分配器
  printf("[1] main: calling kinit()...\n");
  kinit();
  printf("[1] main: kinit done\n");

  // 3. 初始化内核页表
  printf("[2] main: calling kvminit()...\n");
  kvminit();
  printf("[2] main: kvminit done\n");

 printf("[4] main: calling trapinit()...\n");
  trapinit();

  // --- 暴力测试开始 ---
  printf("[Test] Direct Interrupt Test: enabling interrupts and spinning...\n");
  
  intr_on(); // 手动开启全局中断开关 (修改 CSR_CRMD 的 IE 位)

  for(;;) {
    // 啥也不干，就在这等中断
    // 如果时钟中断配置成功，它会强行打断这个循环，跳到 kerneltrap
  }
  // --- 暴力测试结束 ---
}