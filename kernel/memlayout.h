// LoongArch xv6 memory layout.

// Direct-mapped window bases. entry.S programs the same masks during early boot.
#define DMWIN0_MASK 0x9000000000000000UL  // DMW0: cached kernel RAM
#define DMWIN1_MASK 0x9100000000000000UL  // DMW1: device MMIO space

// QEMU LoongArch UART base (physical address).
#define UART0_PHYS 0x1fe001e0UL
#define UART0 (UART0_PHYS | DMWIN1_MASK)
#define UART0_IRQ 2
#define UART0_IRQ_MASK (1UL << UART0_IRQ)

// QEMU virt routes PCI INTx to PCH PIC IRQ 16..19. xv6 currently treats this
// bank as the virtio-blk interrupt source.
#define PCIE_INTX_BASE_IRQ 16
#define PCIE_INTX_IRQ_MASK (0xFUL << PCIE_INTX_BASE_IRQ)

// IRQ lines explicitly used by xv6 right now.
#define KERNEL_EXT_IRQ_MASK (UART0_IRQ_MASK | PCIE_INTX_IRQ_MASK)

// LS7A bridge register base (physical address).
#define LS7A_PCH_REG_BASE                (0x10000000UL | DMWIN1_MASK)

#define LS7A_INT_MASK_REG                LS7A_PCH_REG_BASE + 0x020
#define LS7A_INT_EDGE_REG                LS7A_PCH_REG_BASE + 0x060
#define LS7A_INT_CLEAR_REG               LS7A_PCH_REG_BASE + 0x080
#define LS7A_INT_HTMSI_VEC_REG           LS7A_PCH_REG_BASE + 0x200
#define LS7A_INT_STATUS_REG              LS7A_PCH_REG_BASE + 0x3a0
#define LS7A_INT_POL_REG                 LS7A_PCH_REG_BASE + 0x3e0

// QEMU currently boots with `-m 256M`, but the page allocator only manages the
// first 128 MiB direct-mapped RAM window starting at RAMBASE.
#define RAMBASE (0x200000UL | DMWIN0_MASK)
#define RAMSTOP (RAMBASE + 128*1024*1024)

// User trapframe and per-process kernel stacks live at the top of the
// lower-half virtual address space defined by MAXVA.
#define TRAPFRAME (MAXVA - PGSIZE)
#define KSTACK(p) (TRAPFRAME - ((p)+1)* 2*PGSIZE)
