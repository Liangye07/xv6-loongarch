#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

//
// the loongarch 7A1000 I/O Interrupt Controller Registers.
//

void
apic_init(void)
{
  // Unmask all external lines. Specific dispatch is done in trap.c.
  *(volatile uint64*)(LS7A_INT_MASK_REG) = 0x0UL;

  // Keep UART in level-trigger mode. Edge mode can lose subsequent RX
  // interrupts once the line stays asserted.
  *(volatile uint64*)(LS7A_INT_EDGE_REG) = 0x0UL;

  *(volatile uint8*)(LS7A_INT_HTMSI_VEC_REG + UART0_IRQ) = UART0_IRQ;
  *(volatile uint8*)(LS7A_INT_HTMSI_VEC_REG + 16) = 16;
  *(volatile uint8*)(LS7A_INT_HTMSI_VEC_REG + 17) = 17;
  *(volatile uint8*)(LS7A_INT_HTMSI_VEC_REG + 18) = 18;
  *(volatile uint8*)(LS7A_INT_HTMSI_VEC_REG + 19) = 19;

  *(volatile uint64*)(LS7A_INT_POL_REG) = 0x0UL;

}

// tell the apic we've served this IRQ.
void
apic_complete(uint64 irq)
{
  *(volatile uint64*)(LS7A_INT_CLEAR_REG) = (irq);
}
