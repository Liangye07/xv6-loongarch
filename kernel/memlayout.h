// memlayout.h — LoongArch xv6 memory layout (recommended)

// DMW 窗口掩码（entry.S 中也使用了相同掩码）
#define DMWIN0_MASK 0x9000000000000000UL  // DMW0: cached memory (kernel RAM)
#define DMWIN1_MASK 0x9100000000000000UL  // DMW1: device MMIO (UART, PLIC 等)



// QEMU LoongArch 串口基地址 (物理地址)
#define UART0_PHYS 0x1fe001e0UL
// 供内核使用的 UART 虚拟地址（DMW1 映射）
#define UART0 (UART0_PHYS | DMWIN1_MASK)
#define UART0_IRQ 2

// LS7A 桥片寄存器基地址 (物理地址)
#define LS7A_PCH_REG_BASE		(0x10000000UL | DMWIN1_MASK)

#define LS7A_INT_MASK_REG		LS7A_PCH_REG_BASE + 0x020
#define LS7A_INT_EDGE_REG		LS7A_PCH_REG_BASE + 0x060
#define LS7A_INT_CLEAR_REG		LS7A_PCH_REG_BASE + 0x080
#define LS7A_INT_HTMSI_VEC_REG		LS7A_PCH_REG_BASE + 0x200
#define LS7A_INT_STATUS_REG		LS7A_PCH_REG_BASE + 0x3a0
#define LS7A_INT_POL_REG		LS7A_PCH_REG_BASE + 0x3e0

// ----------------------- 物理内存布局 (PHYSICAL) --------------------
// 物理地址 (PA) 语义：
//  - RAMPHYSBASE: 物理 RAM 起点（例：0x80000000）
//  - PHYSTOP:     物理 RAM 结束地址 (exclusive) —— 以字节为单位
//  - KERNBASE:    内核虚拟基址 (DMW 映射下的 VA)
#define RAMBASE (0x200000UL | DMWIN0_MASK)
//#define RAMSTOP (RAMBASE + 128*1024*1024)
#define RAMSTOP (RAMBASE + 128*1024*1024)
// ----------------------- 分页与页面大小 ------------------------------

// MAXVA 指定用户/内核可用的虚拟地址上限（按需调整）
//#define MAXVA ((1UL << 47) - 1)  // 47-bit canonical

// TRAPFRAME / KSTACK 等（如你原来定义）用 MAXVA 和 PGSIZE 计算
#define TRAPFRAME (MAXVA - PGSIZE)
#define KSTACK(p) (TRAPFRAME - ((p)+1)* 2*PGSIZE)

// Physical memory layout

// 0x00200000 -- bios loads kernel here and jumps here
// 0x10000000 -- 
// 0x1c000000 -- reset address
// 0x1fe00000 -- I/O interrupt base address
// 0x1fe001e0 -- uart16550 serial port
// 0x90000000 -- RAM used by user pages
