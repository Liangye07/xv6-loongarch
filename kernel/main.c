#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

static volatile int started;

void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();

    kinit();          // physical page allocator
    vminit();         // create kernel page table
    procinit();       // process table
    trapinit();       // trap vectors
    ls7a_intc_init(); // set up LS7A1000 interrupt controller
    extioi_init();    // extended I/O interrupt controller
    binit();          // buffer cache
    iinit();          // inode table
    fileinit();       // file table
    disk_init();      // virtio-pci disk (fallback to ramdisk)
    userinit();       // first user process
    printf("hart %d starting\n", cpuid());

    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
  }

  kvminithart();
  trapinithart();
  scheduler();
}
