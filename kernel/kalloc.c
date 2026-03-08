#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

// end[] is defined by kernel.ld, indicating the end of the kernel
extern char end[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  
  // 1. Precise RAM limit based on your 128MB allocation.
  // Physical RAM: 0x0 to 0x08000000.
  // DMW0 Virtual Address: DMWIN0_MASK to DMWIN0_MASK + 0x08000000.
  
  // To avoid hitting the very edge of physical memory which might cause issues in QEMU,
  // we leave a small safety gap (e.g., 1MB) at the end.
  uint64 max_pa = (128 - 1) * 1024 * 1024; 
  uint64 safe_ram_stop = DMWIN0_MASK + max_pa;

  printf("kinit: kernel ends at %p\n", end);
  printf("kinit: freeing from %p to %p (Safety limit: 127MB)\n", end, (void*)safe_ram_stop);
  
  freerange(end, (void*)safe_ram_stop);
  //freerange((void*)0x9000000090000000, (void*)0x9000000098000000);
  printf("kinit: done\n");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  uint64 count = 0;
  // Align up to page boundary
  p = (char*)PGROUNDUP((uint64)pa_start);
  //printf("start freerange\n");
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
    count++;
    // Increased frequency of progress reports to catch the exact stall point
    if(count % 2000 == 0) {
      //printf("freerange: freed %p pages...\n", count);
    }
  }
  //printf("freerange: total freed %p pages.\n", count);
}

// Free the page of physical memory pointed at by pa.
void
kfree(void *pa)
{
  struct run *r;

  // Strict physical range check: 128MB limit
  // Note: ensure limit matches the memory actually provided by QEMU
//  uint64 limit = DMWIN0_MASK + 128 * 1024 * 1024;

//  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < (uint64)end || (uint64)pa >= limit)
//    panic("kfree");

  // Fill with junk to catch dangling refs.
  //memset(pa, 1, PGSIZE);
  //printf("kfree start\n");
  r = (struct run*)pa;

  acquire(&kmem.lock);
 // printf("1\n");
  r->next = kmem.freelist;
 // printf("2\n");
  kmem.freelist = r;
  //printf("3\n");
  release(&kmem.lock);
  //printf("kfree finished\n");
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
  } else {
    // If we reach here, walk() will typically panic.
    // Adding a debug message to confirm kalloc exhaustion.
    printf("kalloc: out of memory!\n");
  }
  return (void*)r;
}