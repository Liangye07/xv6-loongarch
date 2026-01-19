/*
    DMW0是cached，即为内存空间
    DMW1是外设映射内存
*/
#define DMWIN0_MASK 0x9000000000000000UL
#define DMWIN1_MASK 0x9100000000000000UL


// QEMU LoongArch 串口基地址 (虚拟地址)，使用DMW1
#define UART0 (0x1fe001e0UL | DMWIN1_MASK)
#define UART0_IRQ 2

// LS7A 桥片寄存器基地址 (用于后续中断管理),使用DMW1
#define LS7A_PCH_REG_BASE (0x10000000UL | DMWIN1_MASK)
#define LS7A_INT_MASK_REG       (LS7A_PCH_REG_BASE + 0x020)
#define LS7A_INT_EDGE_REG       (LS7A_PCH_REG_BASE + 0x060)
#define LS7A_INT_CLEAR_REG      (LS7A_PCH_REG_BASE + 0x080)
#define LS7A_INT_STATUS_REG     (LS7A_PCH_REG_BASE + 0x3a0)

// 内存配置
//虚拟的地址
// RAMBASE 是物理 RAM 的虚拟映射起点
#define RAMBASE (0x90000000UL | DMWIN0_MASK)
#define KERNBASE RAMBASE
#define PHYSTOP (RAMBASE + 128*1024*1024) // 假设分配 128MB 内存

// MAXVA 定义虚拟地址空间上限 (LoongArch64 48位地址线常用配置)
//#define MAXVA (1L << 47)

#define PGSIZE 4096 // 4KB Page Size

// TRAPFRAME 放在最高页，用于存储进程上下文
#define TRAPFRAME (MAXVA - PGSIZE)

// 内核栈 KSTACK 位于 TRAPFRAME 之下
// 每个进程分配 2 页，其中一页作为 Guard Page 防止栈溢出
#define KSTACK(p) (TRAPFRAME - ((p)+1) * 2 * PGSIZE)
