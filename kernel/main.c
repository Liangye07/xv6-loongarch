#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"
#include "proc.h"      // struct proc, allocproc, killed 等
#include "spinlock.h"  // struct spinlock, acquire/release

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

  // 4. 启用分页机制
  // 必须在开启中断前完成，因为中断处理程序可能需要页表环境
  printf("[3] main: enabling paging (kvminithart)...\n");
  kvminithart();
  printf("[3] main: paging enabled!\n");

  // 5. 初始化 Trap (中断/异常)
  // 设置异常向量入口 EENTRY
  printf("[4] main: calling trapinit()...\n");
  trapinit();

  // 6. 开启当前 CPU 的时钟中断
  // 设置定时器并开启 CRMD.IE 允许中断触发
  printf("[5] main: calling trapinithart()...\n");
  trapinithart();
  printf("[5] main: interrupts are now enabled!\n");

  // 7. 分页开启后验证内存分配是否可用
  printf("[Test] Paging and memory allocator test...\n");
  char *mem = kalloc();
  if(mem != 0){
    printf("[Test] kalloc() success: VA=%p\n", mem);
    *(volatile uint64*)mem = 0x12345678abcdef00ULL;
    uint64 val = *(uint64*)mem;
    if(val == 0x12345678abcdef00ULL) {
      printf("[Test] memory write/read OK!\n");
    }
    kfree(mem);
  }
 /*
  // 8. 等待并观察时钟中断
  // 在 trap.c 的 clockintr 中，每隔 10 个 tick 会打印一个 "."
  printf("[Info] Waiting for timer interrupts (watch for dots)... \n");
  printf("================================================\n");

  // 进入死循环，等待中断打断此处执行并进入 kerneltrap -> devintr -> clockintr
  for(;;) {
    // 可以在这里增加一点延迟打印，或者单纯空转
  }*/
}