// memlayout.h — LoongArch xv6 memory layout (recommended)

// DMW 窗口掩码（entry.S 中也使用了相同掩码）
#define DMWIN0_MASK 0x9000000000000000UL  // DMW0: cached memory (kernel RAM)
#define DMWIN1_MASK 0x9100000000000000UL  // DMW1: device MMIO (UART, PLIC 等)



// QEMU LoongArch 串口基地址 (物理地址)
#define UART0_PHYS 0x1fe001e0UL
// 供内核使用的 UART 虚拟地址（DMW1 映射）
#define UART0 (UART0_PHYS | DMWIN1_MASK)
#define UART0_IRQ 2
#define UART0_IRQ_MASK (1UL << UART0_IRQ)

// QEMU virt machine routes PCI INTx to PCH PIC IRQ 16..19.
// xv6 currently treats this bank as the single virtio-blk disk interrupt
// source; if more PCI INTx devices are enabled later, the dispatch logic
// should be tightened to the exact device IRQ.
#define PCIE_INTX_BASE_IRQ 16
#define PCIE_INTX_IRQ_MASK (0xFUL << PCIE_INTX_BASE_IRQ)

// IRQ lines explicitly used by xv6 kernel right now.
#define KERNEL_EXT_IRQ_MASK (UART0_IRQ_MASK | PCIE_INTX_IRQ_MASK)

// LS7A 桥片寄存器基地址 (物理地址)
#define LS7A_PCH_REG_BASE		(0x10000000UL | DMWIN1_MASK)

#define LS7A_INT_MASK_REG		LS7A_PCH_REG_BASE + 0x020
#define LS7A_INT_EDGE_REG		LS7A_PCH_REG_BASE + 0x060
#define LS7A_INT_CLEAR_REG		LS7A_PCH_REG_BASE + 0x080
#define LS7A_INT_HTMSI_VEC_REG		LS7A_PCH_REG_BASE + 0x200
#define LS7A_INT_STATUS_REG		LS7A_PCH_REG_BASE + 0x3a0
#define LS7A_INT_POL_REG		LS7A_PCH_REG_BASE + 0x3e0

// ----------------------- 物理内存布局 (PHYSICAL) --------------------
// QEMU 当前通过 Makefile 以 `-m 256M` 启动，但内核页分配器目前只管理
// 从 RAMBASE 开始的前 128 MiB DMW0 直映地址窗口。
#define RAMBASE (0x200000UL | DMWIN0_MASK)
#define RAMSTOP (RAMBASE + 128*1024*1024)
// ----------------------- 分页与页面大小 ------------------------------

// TRAPFRAME / KSTACK 基于 loongarch.h 中定义的 47-bit lower-half MAXVA 计算。
#define TRAPFRAME (MAXVA - PGSIZE)
#define KSTACK(p) (TRAPFRAME - ((p)+1)* 2*PGSIZE)

// Physical memory layout

// 0x00200000 -- bios loads kernel here and jumps here
// 0x10000000 -- 
// 0x1c000000 -- reset address
// 0x1fe00000 -- I/O interrupt base address
// 0x1fe001e0 -- uart16550 serial port
// 0x90000000 -- RAM used by user pages
