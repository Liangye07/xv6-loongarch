// Mutual exclusion spin locks.
/*
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // switch while using mycpu().
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
*/
// 暂时简化版的修改

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
// #include "proc.h"   // ❌ 暂时注释：未定义 mycpu() 结构
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  // push_off(); // ❌ 暂时注释：依赖中断控制和 mycpu()，未实现
  // if(holding(lk))
  //   panic("acquire");

  // 单核阶段不需要原子指令自旋
  lk->locked = 1;

  // __sync_synchronize();  // ❌ 暂时注释：无并发必要
  // lk->cpu = mycpu();     // ❌ 暂时注释：mycpu() 尚未实现
}

// Release the lock.
void
release(struct spinlock *lk)
{
  // if(!holding(lk))
  //   panic("release");

  lk->cpu = 0;
  lk->locked = 0;

  // __sync_synchronize(); // ❌ 暂时注释：无并发必要
  // __sync_lock_release(&lk->locked); // ❌ 暂时注释：等多核时再启用
  // pop_off(); // ❌ 暂时注释：依赖中断控制
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  // ❌ 原版需要 mycpu()，未定义，临时返回 locked 状态
  return lk->locked;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  // ❌ 暂时空实现：trap/中断系统尚未完成
  // int old = intr_get();
  // intr_off();
  // if(mycpu()->noff == 0)
  //   mycpu()->intena = old;
  // mycpu()->noff += 1;
}

void
pop_off(void)
{
  // ❌ 暂时空实现
  // struct cpu *c = mycpu();
  // if(intr_get())
  //   panic("pop_off - interruptible");
  // if(c->noff < 1)
  //   panic("pop_off");
  // c->noff -= 1;
  // if(c->noff == 0 && c->intena)
  //   intr_on();
}
