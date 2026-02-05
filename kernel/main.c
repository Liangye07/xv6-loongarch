/*#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"


*/
//volatile static int started = 0;

/*
// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
*/
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

void
main(void)
{
  // 1. 基础输出初始化 (Week 3)
  consoleinit();
  printfinit();
  printf("\n");
  printf("xv6-loongarch: booting Week 6 (Trap/Interrupt) test...\n");

  // 2. 中断与异常初始化 (Week 6 核心)
  // trapinit 负责将 kernelvec 挂载到 CSR.EENTRY
  trapinit(); 
  printf("trap: exception entry (EENTRY) set to kernelvec.\n");

  // trapinithart 负责设置当前 CPU 的 TCFG、开启 ECFG.TI 并开启全局中断 CSR.CRMD.IE
  trapinithart(); 
  printf("trap: hart initialized, timer started, interrupts enabled.\n");

  // 3. 验证阶段
  printf("Testing: Waiting for timer interrupts (should see dots below)...\n");

  // 如果一切正常，由于 trapinithart 开启了中断，
  // CPU 会定期触发时钟中断，跳入 kernelvec.S，再进入 trap.c 的 kerneltrap()。
  // 根据之前写的 trap.c 逻辑，你应该能看到屏幕上不断跳出 "."
  asm volatile("syscall 0");
  for(;;) {
    // 你也可以在这里手动触发一个异常来测试系统调用入口：
    // static int done = 0;
    // if(!done) {
    //   done = 1;
    //   asm volatile("syscall 0"); 
    // }
  }
}