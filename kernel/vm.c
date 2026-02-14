#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"
#include "fs.h"

// 全局内核页表，开启分页后硬件通过此表翻译低半部分虚拟地址
pagetable_t kernel_pagetable;

/* * LoongArch TLB 硬件初始化
 * 设置页大小为 4KB (2^12)，清空 TLB，初始化 ASID
 */
void
tlbinit(void)
{
  flush_tlb_all();
  // STLB 页面大小配置：12 代表 2^12 = 4KB
  csrwr_stlbps(PGSHIFT);     
  csrwr_asid(0);         
  csrwr_tlbrentry(0);    
  csrwr_tlbrehi(PGSHIFT);    
}

/*
 * 内核虚拟内存初始化 (主 CPU 调用一次)
 * 建立内核页表并映射必要的内核地址空间
 */
void
kvminit(void)
{
  // 分配一页作为一级页表根节点
  kernel_pagetable = (pagetable_t) kalloc();
  if(kernel_pagetable == 0)
    panic("kvminit: kalloc failed");
  memset(kernel_pagetable, 0, PGSIZE);

  // 1. 映射 UART0 串口寄存器 (MMIO)
  // 使用 memlayout.h 中的 UART0 (VA) 和 UART0_PHYS (PA)
  // 权限：物理存在(P), 可写(W), 缓存访问(MAT)
  //mappages(kernel_pagetable, UART0, PGSIZE, UART0_PHYS, PTE_P | PTE_W | PTE_MAT);

  // 2. 映射内核代码、数据和 RAM 区
  // 使用 memlayout.h 中的 RAMBASE (VA) 和 DMW2PA(RAMBASE) (PA)
  //mappages(kernel_pagetable, RAMBASE, RAMSTOP - RAMBASE, DMW2PA(RAMBASE), PTE_P | PTE_W | PTE_MAT);

  // 3. 映射内核栈 (由 procinit 准备，映射到内核页表)
  proc_mapstacks(kernel_pagetable);
}

/*
 * 切换到硬件页表行走模式 (每个 CPU 调用一次)
 */
void
kvminithart(void)
{
  // 写入页表根基址寄存器 PGDL (硬件需要物理地址)
  csrwr_pgdl(DMW2PA(kernel_pagetable));
  
  // 初始化 TLB 寄存器状态
  tlbinit();

  // 配置 PWCL，使硬件了解 Sv39 的 9-9-9-12 结构
  // PTBASE: 12, PTWIDTH: 9
  // DIR1BASE: 21, DIR1WIDTH: 9
  // DIR2BASE: 30, DIR2WIDTH: 9
  // PTEWIDTH: 3 (代表 2^3 = 8 字节)
  uint64 pwcl = (uint64)PTBASE | 
                ((uint64)PTWIDTH << 5) | 
                ((uint64)DIR1BASE << 10) | 
                ((uint64)DIR1WIDTH << 15) | 
                ((uint64)DIR2BASE << 20) | 
                ((uint64)DIR2WIDTH << 25) | 
                (3UL << 30);   
  
  csrwr_pwcl(pwcl);
  csrwr_pwch(0);

  // 注意：此时 CRMD 的 PG 位通常在后续由 trapret 或手动设置开启
}

/*
 * 在页表中寻找虚拟地址 va 对应的页表项 (PTE)
 */
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      // 提取物理地址并转换为内核可访问的 DMW 地址
      pagetable = (pagetable_t)PA2DMW(PTE2PA(*pte));      
    } else {
      if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      // 存储子页表的物理地址，并标记有效位
      *pte = PA2PTE(DMW2PA(pagetable)) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

/*
 * 查找虚拟地址对应的物理地址 (PA)
 */
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
  // walkaddr 通常用于用户地址，检查是否存在
  pa = PTE2PA(*pte);
  return pa;
}

/*
 * 创建虚拟地址 va 到物理地址 pa 的映射
 */
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    
    // LoongArch 标准叶子节点标志：V (有效), P (物理页存在), MAT (Cached)
    *pte = PA2PTE(pa) | perm | PTE_V;
    
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

/*
 * 移除映射并根据需要释放物理页
 */
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
      kfree((void*)PA2DMW(pa));
    }
    *pte = 0;
  }
}

/*
 * 创建一个新的空用户页表
 */
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

/*
 * 加载初始程序到用户地址 0 处
 */
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvminit: more than a page");
  mem = kalloc();
  if(mem == 0)
    panic("uvminit: kalloc failed");
  memset(mem, 0, PGSIZE);
  // 权限：物理存在(P), 用户态(PLV=3), 缓存(MAT), 可写(W)
  mappages(pagetable, 0, PGSIZE, DMW2PA(mem), PTE_P | PTE_PLV | PTE_MAT | PTE_W);
  memmove(mem, src, sz);
}

/*
 * 为进程分配内存，将 oldsz 扩展到 newsz
 */
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    // LoongArch 用户页映射：V, P, PLV=3, W, MAT, D(表示可写成功)
    if(mappages(pagetable, a, PGSIZE, DMW2PA(mem), PTE_P | PTE_W | PTE_PLV | PTE_MAT | PTE_D) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

/*
 * 释放进程内存
 */
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

/*
 * 递归释放页表页面
 */
void
freewalk(pagetable_t pagetable)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (PTE_FLAGS(pte) == PTE_V)){
      // 指向下一级子页表
      uint64 child = PA2DMW(PTE2PA(pte));
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // 叶子节点不应在 freewalk 中处理，除非已经 uvmunmap
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

/*
 * 释放用户页表及关联物理内存
 */
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

/*
 * 复制父进程页表给子进程 (fork)
 */
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint64 flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)PA2DMW(pa), PGSIZE);
    if(mappages(new, i, PGSIZE, DMW2PA(mem), flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

/*
 * 清除 PTE_PLV 位以禁止用户访问某页（例如在 exec 时的过渡）
 */
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_PLV; 
}

/*
 * 从内核 src 拷贝到用户 dstva
 */
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    
    memmove((void *)(PA2DMW(pa0) + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

/*
 * 从用户 srcva 拷贝到内核 dst
 */
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    
    memmove(dst, (void *)(PA2DMW(pa0) + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

/*
 * 从用户 srcva 拷贝字符串到内核 dst
 */
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (PA2DMW(pa0) + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }
    srcva = va0 + PGSIZE;
  }
  return got_null ? 0 : -1;
}