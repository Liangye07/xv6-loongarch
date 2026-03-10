#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

void extioi_init(void)
{
    // Enable only IRQs currently used by xv6.
    iocsr_writeq(KERNEL_EXT_IRQ_MASK, LOONGARCH_IOCSR_EXTIOI_EN_BASE);

    // Route EXTIOI[31:0] to CPU interrupt pin INT1.
    iocsr_writeq(0x01UL, LOONGARCH_IOCSR_EXTIOI_MAP_BASE);

    // Route only used IRQs to core 0 (node type 0).
    // Each route byte corresponds to one IRQ; value 1 targets core 0.
    for(int i = 0; i < 4; i++)
      iocsr_writeq(0x0UL, LOONGARCH_IOCSR_EXTIOI_ROUTE_BASE + i * 8);

    uint64 route0 = (0x1UL << (UART0_IRQ * 8)); // IRQ2
    iocsr_writeq(route0, LOONGARCH_IOCSR_EXTIOI_ROUTE_BASE + 0 * 8);

    uint64 route2 = 0;
    for(int irq = PCIE_INTX_BASE_IRQ; irq < PCIE_INTX_BASE_IRQ + 4; irq++)
      route2 |= (0x1UL << ((irq - PCIE_INTX_BASE_IRQ) * 8)); // IRQ16..19
    iocsr_writeq(route2, LOONGARCH_IOCSR_EXTIOI_ROUTE_BASE + 2 * 8);

    // Node type 0 -> node 0.
    iocsr_writeq(0x1UL, LOONGARCH_IOCSR_EXRIOI_NODETYPE_BASE);
}

// ask the extioi what interrupt we should serve.
uint64
extioi_claim(void)
{
    return iocsr_readq(LOONGARCH_IOCSR_EXTIOI_ISR_BASE);
}

void extioi_complete(uint64 irq)
{
    iocsr_writeq(irq, LOONGARCH_IOCSR_EXTIOI_ISR_BASE);
}
