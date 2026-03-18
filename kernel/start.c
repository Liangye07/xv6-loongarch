#include "types.h"
#include "param.h"

void main(void);

// Per-hart boot stacks referenced by entry.S before C is available.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// C-level early boot handoff from entry.S to the regular kernel main path.
void
start(void)
{
  main();

  for(;;)
    ;
}
