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

__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
void
main()
{
   if(cpuid() == 0){
    consoleinit();
    printfinit();
    
    kinit();         // physical page allocator
printf("kinit\n");
    vminit();        // create kernel page table
printf("vminit\n");
    procinit();      // process table
printf("procinit\n");
    trapinit();      // trap vectors
printf("trapinit\n");
    apic_init();     // set up LS7A1000 interrupt controller
//printf("apicinit\n");
    extioi_init();   // extended I/O interrupt controller
//printf("extioi_init\n");
    binit();         // buffer cache
//printf("binit\n");
    iinit();         // inode table
//printf("iinit\n");
    fileinit();      // file table
//printf("fileinit\n");
    disk_init();     // virtio-pci disk (fallback to ramdisk)
printf("disk_init\n");
    userinit();      // first user process
printf("userinit\n");
    printf("hart %d starting\n", cpuid());
  }
  printf("start scheduler\n");
    scheduler(); 
}