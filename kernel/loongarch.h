#ifndef __ASSEMBLER__
//--------------------------------------------------------------------------------------------------------------------------------------
// CRMD
#define CSR_CRMD_PLV  (3L << 0)  // 权限级位
#define CSR_CRMD_IE   (1L << 2)  // 全局中断使能位
#define CSR_CRMD_DA   (1L << 3)  // 直接映射地址翻译使能
#define CSR_CRMD_PG   (1L << 4)  // 页表映射地址翻译使能

// ECFG 
#define ECFG_LIE_TI   (1L << 11) // 定时器中断使能
#define ECFG_LIE_HWI  (1L << 2)  // 硬件中断0使能 (通常接串口)

// TCFG
#define CSR_TCFG_EN     (1L << 0) // 定时器开启
#define CSR_TCFG_PER    (1L << 1) // 定时器循环模式

// ESTAT 寄存器中的 ECode (位 16:21)
#define ECode_INT     0   // 中断 (Interrupt)
#define ECode_PIL     1   // Load 页面缺失
#define ECode_PIS     2   // Store 页面缺失
#define ECode_PIF     3   // Fetch (取指) 页面缺失
#define ECode_PME     4   // 页面修改例外
#define ECode_PNR     5   // 页面不可读例外
#define ECode_PNX     6   // 页面不可执行例外
#define ECode_PPI     7   // 页面特权级不合规
#define ECode_ADE     8   // 地址对齐错误
#define ECode_SYS     11  // 系统调用 (Syscall)
#define ECode_BRK     12  // 断点 (Breakpoint)
#define ECode_INE     13  // 非法指令 (Instruction Non-existent)

//CRMD：0x0 当前模式信息
static inline uint64 csrrd_crmd()
{
  uint64 x;
  asm volatile("csrrd %0, 0x0" : "=r" (x) );
  return x;
}

static inline void csrwr_crmd(uint64 x)
{
  asm volatile("csrwr %0, 0x0" : : "r" (x));
}
//PRMD：0x1 例外（异常与中断）前模式信息
static inline uint64 csrrd_prmd()
{
  uint64 x;
  asm volatile("csrrd %0, 0x1" : "=r" (x) );
  return x;
}

static inline void csrwr_prmd(uint64 x)
{
  asm volatile("csrwr %0, 0x1" : : "r" (x));
}

//ERA：0x6 异常返回地址，即发生时的PC
static inline uint64 csrrd_era()
{
  uint64 x;
  asm volatile("csrrd %0, 0x6" : "=r" (x) );
  return x;
}

static inline void csrwr_era(uint64 x)
{
  asm volatile("csrwr %0, 0x6" : : "r" (x));
}

//ESTAT：0x5 用于记录本次异常的状态，cause等
static inline uint64 csrrd_estat()
{
  uint64 x;
  asm volatile("csrrd %0, 0x5" : "=r" (x) );
  return x;
}

//BADV：0x7 用于触发地址错误相关异常时，记录出错的虚地址，注意这里是记录出错的地址，例如地位未分配，并非 ERA 的 PC
static inline uint64 csrrd_badv()
{
  uint64 x;
  asm volatile("csrrd %0, 0x7" : "=r" (x) );
  return x;
}
//EENTRY：0xc 中断向量表入口
static inline uint64 csrrd_eentry()
{
  uint64 x;
  asm volatile("csrrd %0, 0xc" : "=r" (x) );
  return x;
}

static inline void csrwr_eentry(uint64 x)
{
  asm volatile("csrwr %0, 0xc" : : "r" (x) );
}

// ECFG：0x4 中断屏蔽字
static inline uint64 csrrd_ecfg()
{
  uint64 x;
  asm volatile("csrrd %0, 0x4" : "=r" (x) ); // ECFG 地址是 0x4
  return x;
}

static inline void csrwr_ecfg(uint64 x)
{
  asm volatile("csrwr %0, 0x4" : : "r" (x));
}
// CRMD_IE 开关中断
static inline void intr_on()
{
  csrwr_crmd(csrrd_crmd() | CSR_CRMD_IE);
}

static inline void intr_off()
{
  csrwr_crmd(csrrd_crmd() & ~CSR_CRMD_IE);
}
static inline uint64 intr_get()
{
  uint64 x = csrrd_crmd();
  return (x & CSR_CRMD_IE) != 0;
}

//CPUID ：0x20
static inline uint64 r_cpuid()
{
  uint64 x;
  asm volatile("csrrd %0, 0x20" : "=r" (x) );
  return x;
}

static inline uint64 rd_tp()
{
  uint64 x;
  asm volatile("addi.d %0, $tp, 0" : "=r" (x) );
  return x;
}

//读取栈指针
static inline uint64 rd_sp()
{
  uint64 x;
  asm volatile("addi.d %0, $sp, 0" : "=r" (x) );
  return x;
}

//$ra ($r1) 获取函数调用返回地址 
static inline uint64 rd_ra()
{
  uint64 x;
  asm volatile("or %0, $ra, $r0" : "=r" (x) );
  return x;
}

// cpu上电以来的时间
static inline uint64
r_time()
{
  uint64 val;       //时间
  uint64 coreid;    //cpuid
  asm volatile("rdtime.d %0, %1" : "=r"(val), "=r"(coreid));
  return val;
}

// 设置定时器配置
// 这里的 val 通常包含：计数值 + 循环标志 + 开启位
//TCFG：0x41 定时器配置
static inline void csrwr_tcfg(uint64 val)
{
  asm volatile("csrwr %0, 0x41" : : "r" (val));
}

// TVAL：0x42 读取定时器当前剩余计数值 
static inline uint64 csrrd_tval()
{
  uint64 v;
  asm volatile("csrrd %0, 0x42" : "=r" (v));
  return v;
}

// TICLR：0x44 清除时钟中断标志 
static inline void csrwr_ticlr()
{
  // 往 0x44 (TICLR) 的第 0 位写 1，表示告知硬件“中断已处理”
  asm volatile("csrwr %0, 0x44" : : "r" (0x1));
}
//内存-------------------------------------------------------------------------------------------------------------
//PWCL：0x1c 低位页表
static inline void csrwr_pwcl(uint64 x)
{
  asm volatile("csrwr %0, 0x1c" : : "r" (x) );
}
//PWCH：0x1d 高位页表
static inline void csrwr_pwch(uint64 x)
{
  asm volatile("csrwr %0, 0x1d" : : "r" (x) );
}
//PGDL：0x19 低半地址空间（用户态）页表始址
static inline void csrwr_pgdl(uint64 x)
{
  asm volatile("csrwr %0, 0x19" : : "r" (x) );
}

static inline uint64 csrrd_pgdl()
{
  uint64 x;
  asm volatile("csrrd %0, 0x19" : "=r" (x) );
  return x;
}

// 拼接并写入 PGDL 的宏，类似于 RISC-V 的 MAKE_SATP
#define MAKE_PGDL(pagetable) ((uint64)pagetable)

// invtlb：page95 刷新整个 TLB
static inline void flush_tlb_all()
{
  // 使用 invtlb 类型 0x0：清空所有 TLB 条目
  // 指令格式：invtlb <type>, <asid_reg>, <va_reg>
  // 这里使用 $r0 (常量 0) 表示对所有 ASID 和所有 VA 生效
  asm volatile("invtlb 0x0, $r0, $r0");
}

// 针对特定虚拟地址刷新 TLB
static inline void flush_tlb_one(uint64 va)
{
  asm volatile("invtlb 0x5, $r0, %0" : : "r" (va));
}
//存放 TLB 指令操作时 TLB 表项低位部分物理页号等相关的信息
static inline uint64 csrrd_tlbrelo0()
{
  uint64 x;
  asm volatile("csrrd %0, 0x8c" : "=r" (x) );
  return x;
}

static inline uint64 csrrd_tlbrelo1()
{
  uint64 x;
  asm volatile("csrrd %0, 0x8d" : "=r" (x) );
  return x;
}
//TLB 重填例外入口地址
static inline void csrwr_tlbrentry(uint64 x)
{
  asm volatile("csrwr %0, 0x88" : : "r" (x) );
}
//二级页表
static inline void csrwr_stlbps(uint32 x)
{
  asm volatile("csrwr %0, 0x1e" : : "r" (x) );
}
//TLB 重填例外表项高位
static inline void csrwr_tlbrehi(uint64 x)
{
  asm volatile("csrwr %0, 0x8e" : : "r" (x) );
}

//ASID：0x18
static inline void csrwr_asid(uint64 x)
{
  asm volatile("csrwr %0, 0x18" : : "r" (x) );
}

static inline uint64 csrrd_asid()
{
  uint64 x;
  asm volatile("csrrd %0, 0x18" : "=r" (x) );
  return x;
}

typedef uint64 pte_t;        //页表项
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__
//-------------------------------------------------------------------------------------------------------------------------------------
//IOCSR 访问函数，riscv里是io映射内存，因此需要新增这一部分

// 读取 32 位 (Word)
static inline uint32 iocsr_readw(uint32 addr)
{
  uint32 val;
  asm volatile("iocsrrd.w %0, %1" : "=r"(val) : "r"(addr));
  return val;
}

// 写入 32 位 (Word)
static inline void iocsr_writew(uint32 val, uint32 addr)
{
  asm volatile("iocsrwr.w %0, %1" : : "r"(val), "r"(addr));
}

// 读取 64 位 (Double-word) 
static inline uint64 iocsr_readq(uint32 addr)
{
  uint64 val;
  asm volatile("iocsrrd.d %0, %1" : "=r"(val) : "r"(addr));
  return val;
}

// 写入 64 位 (Double-word)
static inline void iocsr_writeq(uint64 val, uint32 addr)
{
  asm volatile("iocsrwr.d %0, %1" : : "r"(val), "r"(addr));
}

//--------------------------------------------------------------------------------------------------------------------------------------
// 定义页表配置参数 (以 Sv39 为准：9-9-9 索引)
#define PTBASE    12U
#define PTWIDTH   9U
#define DIR1BASE  21U 
#define DIR1WIDTH 9U
#define DIR2BASE  30U
#define DIR2WIDTH 9U

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page
//边界对齐
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// 页表标志位修改
#define PTE_V     (1L << 0)    // 有效位 (Valid)
#define PTE_D     (1L << 1)    // 脏位 (Dirty)，在 LoongArch 中用于表示“已修改”，通常配合 W 位使用
#define PTE_PLV   (3L << 2)    // 权限级 (Privilege Level)，3L 代表用户态
#define PTE_MAT   (1L << 4)    // 存储访问类型 (Memory Access Type)，通常设为 1L (Cached)
#define PTE_P     (1L << 7)    // 物理页存在位 (Present)
#define PTE_W     (1L << 8)    // 可写位 (Writable)
#define PTE_NX    (1UL << 62)  // 不可执行 (Non-executable)
#define PTE_NR    (1L << 61)   // 不可读 (Non-readable)

// 对应 RISC-V 的 PTE_U (User)，LoongArch 使用 PLV=3
#define PTE_U     (3L << 2)    // 用户可访问权限

// --- 地址转换宏修改 ---
// LoongArch 的物理地址在 PTE 中是直接 4K 对齐的（从第 12 位开始）
#define PAMASK          (0xFFFFFFFFFUL << PGSHIFT) // 36位物理页号掩码
#define PA2PTE(pa)      (((uint64)pa) & PAMASK)    // 物理地址转 PTE (保持 12 位对齐)
#define PTE2PA(pte)     ((pte) & PAMASK)           // PTE 转物理地址

// 提取标志位：LoongArch 的标志位包含低 12 位和最高几位（NX, NR 等）
#define PTE_FLAGS(pte)  ((pte) & 0xE0000000000001FFUL)

// --- 多级页表索引修改 ---
// 龙芯 LA64 的 Sv39 模式同样使用 9-9-9 结构
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va)   ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// 虚拟地址空间定义
// LoongArch 用户空间通常在地址空间的低半部分
#define MAXVA           (1L << (9 + 9 + 9 + 12 - 1)) // 256GB