// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// kernel/kalloc.c  — LoongArch-safe kalloc (带 KADDR/PADDR 辅助，带调试输出)
// kernel/kalloc.c — LoongArch-safe kalloc (修正版：统一物理/虚拟地址比较)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
#include "defs.h"

void freerange(uint64 pa_start, uint64 pa_end);

extern char end[]; // first address after kernel (KVA)

// 辅助：KVA <-> PA 转换（基于 DMWIN0_MASK）
static inline void *KADDR(uint64 pa) { return (void *)(pa | DMWIN0_MASK); }
static inline uint64 PADDR(void *va) { return ((uint64)va & ~DMWIN0_MASK); }

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// kinit: 初始化 kmem，注意把 end/PHYSTOP 都转为物理地址再处理
void
kinit()
{
  initlock(&kmem.lock, "kmem");

  // 计算内核结束的物理地址（end 可能是 KVA）
  uint64 end_u = (uint64)end;
  uint64 end_pa = (end_u & DMWIN0_MASK) ? (end_u & ~DMWIN0_MASK) : end_u;

  // 计算 PHYSTOP 的物理地址（可能包含 DMW mask）
  uint64 phystop_u = (uint64)PHYSTOP;
  uint64 phystop_pa = (phystop_u & DMWIN0_MASK) ? (phystop_u & ~DMWIN0_MASK) : phystop_u;

  printf("[kinit] start: end=%p end_pa=0x%lx PHYSTOP=0x%lx phystop_pa=0x%lx\n",
         end, end_u, phystop_u, phystop_pa);

  freerange(end_pa, phystop_pa);

  printf("[kinit] done\n");
}

// freerange: 接受物理地址区间 [pa_start, pa_end)
void
freerange(uint64 pa_start, uint64 pa_end)
{
  uint64 pa = PGROUNDUP(pa_start);
  for(; pa + PGSIZE <= pa_end; pa += PGSIZE){
    void *kva = KADDR(pa); // 转为内核可访问虚拟地址再释放
    kfree(kva);
  }
}

// kfree: 接受一个内核虚拟地址 (KVA)，但用物理地址做边界检查
void
kfree(void *va)
{
  struct run *r;
  uint64 pa = PADDR(va); // 将 KVA -> PA 以便比较

  // 边界检查：都用物理地址进行比较
  // 注意：计算 end_pa 与 phystop_pa 在 kinit 已处理（这里直接比较）
  // 为了安全，如果 end 包含 DMW mask，PADDR(end) 可用；但 end 是 extern char[], 取其物理值如下：
  uint64 end_u = (uint64)end;
  uint64 end_pa = (end_u & DMWIN0_MASK) ? (end_u & ~DMWIN0_MASK) : end_u;
  uint64 phystop_u = (uint64)PHYSTOP;
  uint64 phystop_pa = (phystop_u & DMWIN0_MASK) ? (phystop_u & ~DMWIN0_MASK) : phystop_u;

  if((pa % PGSIZE) != 0 || pa < end_pa || pa >= phystop_pa)
    panic("kfree");

  // 填充垃圾数据以捕捉悬空引用
  memset(va, 1, PGSIZE);

  r = (struct run*)va;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// kalloc: 返回一个内核可访问的虚拟地址 (KVA)
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}
