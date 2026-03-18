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

void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    
    kinit();         // physical page allocator
    vminit();        // create kernel page table
    procinit();      // process table
    trapinit();      // trap vectors
    apic_init();     // set up LS7A1000 interrupt controller
    extioi_init();   // extended I/O interrupt controller
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    disk_init();     // virtio-pci disk (fallback to ramdisk)
    userinit();      // first user process
    printf("hart %d starting\n", cpuid());
  }
  trapinithart();
  scheduler();
}
