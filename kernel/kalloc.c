#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

// First address after the kernel image; provided by kernel.ld.
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
  freerange(end, (void*)RAMSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;

  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa.
void
kfree(void *pa)
{
  struct run *r;

  // The allocator only manages the direct-mapped RAM window [RAMBASE, RAMSTOP).
  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < (uint64)end ||
     (uint64)pa < RAMBASE || (uint64)pa >= RAMSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
    // Fill with junk to catch stale assumptions about page contents.
    memset((char*)r, 5, PGSIZE);
  } else {
    printf("kalloc: out of memory!\n");
  }
  return (void*)r;
}
