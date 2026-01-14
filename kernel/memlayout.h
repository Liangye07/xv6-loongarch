// kernel/memlayout.h

/**
 * LoongArch 直接映射窗口 (DMW) 掩码
 * 在 entry.S 中配置 CSR_DMWIN0 为 0x9000...11 后，
 * 凡是 0x9000 开头的虚拟地址都会直接映射到物理地址。
 */
#define DMWIN_MASK 0x9000000000000000UL

/**
 * 物理内存布局 (Physical memory layout)
 * 0x00000000 -- 低端保留区/IO寄存器区
 * 0x1fe001e0 -- UART16550 串口 (LS7A 桥片)
 * 0x90000000 -- RAM 起始地址 (物理地址)
 */

// QEMU LoongArch 串口基地址 (虚拟地址)
// 物理地址 0x1fe001e0 通过 DMW1 映射
#define UART0 (0x1fe001e0UL | DMWIN_MASK)
#define UART0_IRQ 2

// LS7A 桥片寄存器基地址 (用于后续中断管理)
#define LS7A_PCH_REG_BASE (0x10000000UL | DMWIN_MASK)
#define LS7A_INT_MASK_REG       (LS7A_PCH_REG_BASE + 0x020)
#define LS7A_INT_EDGE_REG       (LS7A_PCH_REG_BASE + 0x060)
#define LS7A_INT_CLEAR_REG      (LS7A_PCH_REG_BASE + 0x080)
#define LS7A_INT_STATUS_REG     (LS7A_PCH_REG_BASE + 0x3a0)

// 内存配置
// 内核在物理地址 0x90200000 处加载 (对应 kernel.ld 的 2MB 偏移)
// RAMBASE 是物理 RAM 的虚拟映射起点
#define RAMBASE (0x90000000UL | DMWIN_MASK)
#define KERNBASE RAMBASE
#define PHYSTOP (RAMBASE + 128*1024*1024) // 假设分配 128MB 内存

/**
 * 用户态内存布局 (User memory layout)
 * 为后续 Week 4 虚拟内存映射预留
 */

// MAXVA 定义虚拟地址空间上限 (LoongArch64 48位地址线常用配置)
//#define MAXVA (1L << 47)

#define PGSIZE 4096 // 4KB Page Size

// TRAPFRAME 放在最高页，用于存储进程上下文
#define TRAPFRAME (MAXVA - PGSIZE)

// 内核栈 KSTACK 位于 TRAPFRAME 之下
// 每个进程分配 2 页，其中一页作为 Guard Page 防止栈溢出
#define KSTACK(p) (TRAPFRAME - ((p)+1) * 2 * PGSIZE)

/**
 * 差异分析要点：
 * 1. 删除了 RISC-V 的 PLIC 地址，因为 LoongArch 使用 LS7A 桥片。
 * 2. 引入了 DMWIN_MASK，所有硬件访问必须通过 0x9000... 高位地址。
 * 3. 移除了 TRAMPOLINE 宏，LoongArch 移植通常不需要 RISC-V 那样的独立跳板页。
 */