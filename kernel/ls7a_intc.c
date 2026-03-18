#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "defs.h"

//
// the loongarch 7A1000 I/O Interrupt Controller Registers.
//

void
ls7a_intc_init(void)
{
  // Only unmask IRQs currently used by xv6.
  *(volatile uint64*)(LS7A_INT_MASK_REG) = ~KERNEL_EXT_IRQ_MASK;

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

uint64
ls7a_intc_pending(void)
{
  return *(volatile uint64*)(LS7A_INT_STATUS_REG) & KERNEL_EXT_IRQ_MASK;
}

// Tell the LS7A interrupt controller these IRQ lines are now served.
void
ls7a_intc_complete(uint64 irq)
{
  *(volatile uint64*)(LS7A_INT_CLEAR_REG) = (irq);
}
