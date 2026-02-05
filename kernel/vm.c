#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "loongarch.h"
#include "defs.h"
#include "fs.h"

/*
 * 内核全局页表指针
 */
pagetable_t kernel_pagetable;

extern char etext[];      
extern char trampoline[]; 

// 定义 DMW 掩码，优先使用 memlayout.h 或 loongarch.h 中的定义
#ifndef DMWIN_MASK
#ifdef DMWIN0_MASK
#define DMWIN_MASK DMWIN0_MASK
#else
#define DMWIN_MASK 0x9000000000000000UL
#endif
#endif

// 初始化 TLB 相关寄存器
void
tlbinit(void)
{
  // 1. 无效化所有 TLB 条目 (LoongArch 的 invtlb 0)
  flush_tlb_all();
  
  // 2. 配置 STLB 页大小为 4KB (PS=12=0xC)
  csrwr_stlbps(0xcU);
  
  // 3. 设置 ASID 为 0
  csrwr_asid(0x0U);

  // 根据建议，不在 tlbinit 中写 tlbrehi
}

// 初始化内核页表
void
kvminit(void)
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // 映射内核栈 (在页表中进行映射)
  // proc_mapstacks(kernel_pagetable);
}

// 切换并真正开启硬件页表遍历
void
kvminithart()
{
  // 1. 设置页目录表基址 (PGDL 对应用户态/低半空间)
  csrwr_pgdl((uint64)kernel_pagetable);

  // 2. 初始化 TLB 硬件状态
  tlbinit();

  // 3. 配置 PWCL (3级页表: 9-9-9 宽度)
  // 对应 loongarch.h 中的 PTBASE, PTWIDTH 等定义
  uint64 pwcl = (0UL << 30) |           // PTEWidth (64-bit)
                (DIR2WIDTH << 25) | (DIR2BASE << 20) | 
                (DIR1WIDTH << 15) | (DIR1BASE << 10) | 
                (PTWIDTH << 5)    | (PTBASE << 0);
  csrwr_pwcl(pwcl);

  // 4. 配置 PWCH (对于 Sv39, 高级页表设为 0)
  csrwr_pwch(0x0UL);
  
  // 5. 从直接映射模式切换到分页模式
  // 依据 loongarch.h 中的 CSR_CRMD_DA 和 CSR_CRMD_PG
  uint64 crmd = csrrd_crmd();
  crmd &= ~CSR_CRMD_DA; // 清除 DA 位，关闭直接映射
  crmd |=  CSR_CRMD_PG; // 设置 PG 位，开启分页
  csrwr_crmd(crmd);

  // 6. 切换后刷新 TLB
  flush_tlb_all();
}

// 查找页表项逻辑 (适配 DMW 访问)
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      // 物理地址转为 DMW 虚拟地址，确保内核能直接读写下级页表页
      pagetable = (pagetable_t)(PTE2PA(*pte) | DMWIN_MASK);      
    } else {
      if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      // 中间层级：有效位 + 存在位
      *pte = PA2PTE((uint64)pagetable) | PTE_V | PTE_P;
    }
  }
  return &pagetable[PX(0, va)];
}

// 建立映射
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    
    // LoongArch 特有处理：
    // PTE_MAT: 必须设为 Cached (0x1 << 4)
    // PTE_D: Dirty 位作为写使能硬件检查的一部分
    uint64 flags = perm | PTE_V | PTE_P | PTE_MAT;
    if(perm & PTE_W)
      flags |= PTE_D;

    *pte = PA2PTE(pa) | flags;
    
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 查找物理地址
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    return 0;
  
  pa = PTE2PA(*pte);
  return pa;
}

// 释放并可选地回收物理内存
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
      
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)(pa | DMWIN_MASK));
    }
    *pte = 0;
  }
}

pagetable_t
uvmcreate()
{
  pagetable_t pagetable = (pagetable_t) kalloc();
  if(pagetable == 0) return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  if(sz >= PGSIZE) panic("inituvm: size");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  // 用户初始进程映射：权限设为用户态+可写
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_U | PTE_W);
  memmove(mem, src, sz);
}

uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;
  if(newsz < oldsz) return oldsz;
  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_U | PTE_W) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz) return oldsz;
  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }
  return newsz;
}

void
freewalk(pagetable_t pagetable)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // 判断是否是子页表项：有效且非叶子节点（简单判断：无 W/U 权限位的通常是中间层）
    if((pte & PTE_V) && (pte & PTE_P) && !(pte & (PTE_W | PTE_U))){
      uint64 child = (PTE2PA(pte) | DMWIN_MASK);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    }
  }
  kfree((void*)pagetable);
}

void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i, flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0) panic("uvmcopy: walk");
    if((*pte & PTE_V) == 0) panic("uvmcopy: not mapped");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0) goto err;
    memmove(mem, (char*)(pa | DMWIN_MASK), PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;
err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0) panic("uvmclear");
  *pte &= ~PTE_U;
}

int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len) n = len;
    memmove((void *)((pa0 + (dstva - va0)) | DMWIN_MASK), src, n);
    len -= n; src += n; dstva = va0 + PGSIZE;
  }
  return 0;
}

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len) n = len;
    memmove(dst, (void *)((pa0 + (srcva - va0)) | DMWIN_MASK), n);
    len -= n; dst += n; srcva = va0 + PGSIZE;
  }
  return 0;
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;
  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max) n = max;
    char *p = (char *) ((pa0 + (srcva - va0)) | DMWIN_MASK);
    while(n > 0){
      if(*p == '\0'){ *dst = '\0'; got_null = 1; break; }
      else { *dst = *p; }
      --n; --max; p++; dst++;
    }
    srcva = va0 + PGSIZE;
  }
  return got_null ? 0 : -1;
}