#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

// tick 计数及其自旋锁
struct spinlock tickslock;
uint ticks;


void kernelvec();
void uservec();
void handle_tlbr();
void handle_merr();
void userret(uint64, uint64);

void usertrap(void);
extern int devintr();

/*
 * trapinit: 初始化全局 trap（一次性，设置 EENTRY）
 */
void
trapinit(void)
{
  initlock(&tickslock, "time");
  uint32 ecfg = ( 0U << CSR_ECFG_VS_SHIFT ) | HWI_VEC | TI_VEC;
  uint64 tcfg = 0x1000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
  csrwr_ecfg(ecfg);
  csrwr_tcfg(tcfg);
  csrwr_eentry((uint64)kernelvec);
  csrwr_tlbrentry((uint64)handle_tlbr);
  csrwr_merrentry((uint64)handle_merr);
  intr_on();
}

void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec.S
  csrwr_eentry((uint64)uservec);  //maybe todo
  // uservec expects SAVE0 to hold TRAPFRAME VA on entry from user.
  csrwr_save0(TRAPFRAME);
  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_pgdl = csrrd_pgdl();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = rd_tp();         // hartid for cpuid()

  // set up the registers that uservec.S's ertn will use
  // to get to user space.
  
  // set Previous Privilege mode to User Privilege3.
  uint32 x = csrrd_prmd();
  x |= PRMD_PPLV; // set PPLV to 3 for user mode
  x |= PRMD_PIE; // enable interrupts in user mode
  csrwr_prmd(x);

  // set S Exception Program Counter to the saved user pc.
  csrwr_era(p->trapframe->era);

  // tell uservec.S the user page table to switch to.
  volatile uint64 pgdl = (uint64)(p->pagetable);

  // jump to uservec.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with ertn.
  userret(TRAPFRAME, pgdl);
}


void
usertrap(void)
{
  int which_dev = 0;

  if((csrrd_prmd() & PRMD_PPLV) == 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  csrwr_eentry((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->era = csrrd_era();
  
  if( ((csrrd_estat() & CSR_ESTAT_ECODE) >> 16) == 0xb){
    // system call
    
    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->era += 4;

    // an interrupt will change crmd & prmd registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected estat=%p pid=%d\n", csrrd_estat(), p->pid);
    printf("            era=%p badv=%p\n", csrrd_era(), csrrd_badv());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}



void 
kerneltrap()
{
  int which_dev = 0;
  uint64 era = csrrd_era();
  uint64 prmd = csrrd_prmd();
  
  if((prmd & PRMD_PPLV) != 0)
    panic("kerneltrap: not from privilege0");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("estat=%p\n", csrrd_estat());
    printf("era=%p eentry=%p\n", csrrd_era(), csrrd_eentry());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's instruction.
  csrwr_era(era);
  csrwr_prmd(prmd);
}

void 
machine_trap()
{
  panic("machine error");
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  static int unexpected_hwi_warn;
  uint32 estat = csrrd_estat();
  uint32 ecfg = csrrd_ecfg();

  if(estat & ecfg & HWI_VEC) {
    // this is a hardware interrupt, via IOCR.

    // irq indicates which device interrupted.
    uint64 irq = extioi_claim();
    uint64 uart_irq = irq & UART0_IRQ_MASK;
    uint64 disk_irq = irq & PCIE_INTX_IRQ_MASK;
    uint64 other_irq = irq & ~(UART0_IRQ_MASK | PCIE_INTX_IRQ_MASK);

    if(uart_irq){
      uartintr();

    // tell the apic the device is
    // now allowed to interrupt again.
      apic_complete(uart_irq);
      extioi_complete(uart_irq);
    }

    if(disk_irq){
      (void)disk_intr();
      apic_complete(disk_irq);
      extioi_complete(disk_irq);
    }

    if(other_irq){
      if(unexpected_hwi_warn < 8){
        printf("devintr: unexpected ext irq=%p\n", other_irq);
        unexpected_hwi_warn++;
      }
      apic_complete(other_irq);
      extioi_complete(other_irq);
    }

    return 1;
  } else if(estat & ecfg & TI_VEC){
    //timer interrupt,

    if(cpuid() == 0){
      clockintr();
      // Safety net for rare lost completion interrupts while requests are pending.
      if(disk_pending())
        (void)disk_intr();
    }
    
    // acknowledge the timer interrupt by clearing
    // the TI bit in TICLR.
    csrwr_ticlr(csrrd_ticlr() | CSR_TICLR_CLR);

    return 2;
  } else {
    return 0;
  }
}
