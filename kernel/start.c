#include "types.h"
#include "param.h"
#include "loongarch.h"

void main(void);

// Per-hart boot stacks referenced by entry.S before C is available.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// C-level early boot handoff from entry.S to the regular kernel main path.
void
start(void)
{
  uint64 id = r_cpuid();

  asm volatile("move $tp, %0" : : "r" (id));
  main();

  for(;;)
    ;
}
