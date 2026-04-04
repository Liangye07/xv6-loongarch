#ifndef PROC_H
#define PROC_H

#include "types.h"
#include "spinlock.h"
#include "param.h"
#include "pstat.h"
#include "trapframe.h"

// 保存内核上下文切换时需要记录的寄存器 (Callee-saved)
// 对应 swtch.S 中的保存与恢复逻辑
struct context {
  uint64 ra;
  uint64 sp;
  uint64 fp;

  // callee-saved registers
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
};

// 每个 CPU 的状态结构
struct cpu {
  struct proc *proc;          // 当前在该 CPU 上运行的进程，如果没有则为 null
  struct context context;     // 调用 swtch() 从而进入调度器 (scheduler)
  int noff;                   // push_off() 嵌套深度
  int intena;                 // 在 push_off() 之前，中断是否已经开启
};

extern struct cpu cpus[NCPU];

// 存放用户态寄存器的结构，位于 uservec.S 处理过程中。
// 在页表中映射在 trampoline 页之下。
struct trapframe {
  /* 0 */ uint64 ra;
  /* 8 */ uint64 tp;
  /* 16 */ uint64 sp;
  /* 24 */ uint64 a0;
  /* 32 */ uint64 a1;
  /* 40 */ uint64 a2;
  /* 48 */ uint64 a3;
  /* 56 */ uint64 a4;
  /* 64 */ uint64 a5;
  /* 72 */ uint64 a6;
  /* 80 */ uint64 a7;
  /* 88 */ uint64 t0;
  /* 96 */ uint64 t1;
  /* 104 */ uint64 t2;
  /* 112 */ uint64 t3;
  /* 120 */ uint64 t4;
  /* 128 */ uint64 t5;
  /* 136 */ uint64 t6;
  /* 144 */ uint64 t7;
  /* 152 */ uint64 t8;
  /* 160 */ uint64 r21;
  /* 168 */ uint64 fp;        // r22
  /* 176 */ uint64 s0;        // r23
  /* 184 */ uint64 s1;        // r24
  /* 192 */ uint64 s2;        // r25
  /* 200 */ uint64 s3;        // r26
  /* 208 */ uint64 s4;        // r27
  /* 216 */ uint64 s5;        // r28
  /* 224 */ uint64 s6;        // r29
  /* 232 */ uint64 s7;        // r30
  /* 240 */ uint64 s8;        // r31
  /* 248 */ uint64 kernel_sp;     // 进程内核栈的栈顶地址
  /* 256 */ uint64 kernel_trap;   // usertrap() 的入口地址
  /* 264 */ uint64 era;           // 保存的用户态程序计数器 (EPC)
  /* 272 */ uint64 kernel_hartid; // 保存的内核 tp (CPU ID)
  /* 280 */ uint64 kernel_pgdl;   // 保存的内核页表基址
};

_Static_assert(__builtin_offsetof(struct trapframe, ra) == TF_RA, "trapframe ra");
_Static_assert(__builtin_offsetof(struct trapframe, tp) == TF_TP, "trapframe tp");
_Static_assert(__builtin_offsetof(struct trapframe, sp) == TF_SP, "trapframe sp");
_Static_assert(__builtin_offsetof(struct trapframe, a0) == TF_A0, "trapframe a0");
_Static_assert(__builtin_offsetof(struct trapframe, a1) == TF_A1, "trapframe a1");
_Static_assert(__builtin_offsetof(struct trapframe, a2) == TF_A2, "trapframe a2");
_Static_assert(__builtin_offsetof(struct trapframe, a3) == TF_A3, "trapframe a3");
_Static_assert(__builtin_offsetof(struct trapframe, a4) == TF_A4, "trapframe a4");
_Static_assert(__builtin_offsetof(struct trapframe, a5) == TF_A5, "trapframe a5");
_Static_assert(__builtin_offsetof(struct trapframe, a6) == TF_A6, "trapframe a6");
_Static_assert(__builtin_offsetof(struct trapframe, a7) == TF_A7, "trapframe a7");
_Static_assert(__builtin_offsetof(struct trapframe, t0) == TF_T0, "trapframe t0");
_Static_assert(__builtin_offsetof(struct trapframe, t1) == TF_T1, "trapframe t1");
_Static_assert(__builtin_offsetof(struct trapframe, t2) == TF_T2, "trapframe t2");
_Static_assert(__builtin_offsetof(struct trapframe, t3) == TF_T3, "trapframe t3");
_Static_assert(__builtin_offsetof(struct trapframe, t4) == TF_T4, "trapframe t4");
_Static_assert(__builtin_offsetof(struct trapframe, t5) == TF_T5, "trapframe t5");
_Static_assert(__builtin_offsetof(struct trapframe, t6) == TF_T6, "trapframe t6");
_Static_assert(__builtin_offsetof(struct trapframe, t7) == TF_T7, "trapframe t7");
_Static_assert(__builtin_offsetof(struct trapframe, t8) == TF_T8, "trapframe t8");
_Static_assert(__builtin_offsetof(struct trapframe, r21) == TF_R21, "trapframe r21");
_Static_assert(__builtin_offsetof(struct trapframe, fp) == TF_FP, "trapframe fp");
_Static_assert(__builtin_offsetof(struct trapframe, s0) == TF_S0, "trapframe s0");
_Static_assert(__builtin_offsetof(struct trapframe, s1) == TF_S1, "trapframe s1");
_Static_assert(__builtin_offsetof(struct trapframe, s2) == TF_S2, "trapframe s2");
_Static_assert(__builtin_offsetof(struct trapframe, s3) == TF_S3, "trapframe s3");
_Static_assert(__builtin_offsetof(struct trapframe, s4) == TF_S4, "trapframe s4");
_Static_assert(__builtin_offsetof(struct trapframe, s5) == TF_S5, "trapframe s5");
_Static_assert(__builtin_offsetof(struct trapframe, s6) == TF_S6, "trapframe s6");
_Static_assert(__builtin_offsetof(struct trapframe, s7) == TF_S7, "trapframe s7");
_Static_assert(__builtin_offsetof(struct trapframe, s8) == TF_S8, "trapframe s8");
_Static_assert(__builtin_offsetof(struct trapframe, kernel_sp) == TF_KERNEL_SP, "trapframe kernel_sp");
_Static_assert(__builtin_offsetof(struct trapframe, kernel_trap) == TF_KERNEL_TRAP, "trapframe kernel_trap");
_Static_assert(__builtin_offsetof(struct trapframe, era) == TF_ERA, "trapframe era");
_Static_assert(__builtin_offsetof(struct trapframe, kernel_hartid) == TF_KERNEL_HARTID, "trapframe kernel_hartid");
_Static_assert(__builtin_offsetof(struct trapframe, kernel_pgdl) == TF_KERNEL_PGDL, "trapframe kernel_pgdl");
_Static_assert(sizeof(struct trapframe) == TF_SIZE, "trapframe size");

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 进程控制块 (Process Control Block)
struct proc {
  struct spinlock lock;

  // 使用以下变量时必须持有 p->lock:
  enum procstate state;        // 进程状态
  void *chan;                  // 如果不为 0，表示在 chan 上睡眠
  int killed;                  // 如果不为 0，表示已被杀死
  int xstate;                  // 返回给父进程 wait 的退出状态
  int pid;                     // 进程 ID

  // 使用这个变量时必须持有 wait_lock:
  struct proc *parent;         // 父进程指针

  // 以下变量是进程私有的，访问时不需要持有 p->lock:
  uint64 kstack;               // 内核栈的虚拟地址
  uint64 sz;                   // 进程占用的内存大小 (字节)
  pagetable_t pagetable;       // 用户态页表 (低半部分地址空间)
  struct trapframe *trapframe; // trapframe 页的地址，建议使用 DMW 地址映射
  struct context context;      // 用于 swtch() 切换到该进程
  struct file *ofile[NOFILE];  // 打开的文件
  struct inode *cwd;           // 当前工作目录
  char name[16];               // 进程名称 (用于调试)

  int priority;                // 用户可见优先级
  uint64 weight;               // 由优先级映射出的调度权重
  uint64 stride;               // stride scheduling 步长
  uint64 pass;                 // stride scheduling 当前虚拟时间
  uint64 run_ticks;            // 累计运行 tick 数
  uint64 sched_count;          // 被调度次数
  uint64 create_ticks;         // 创建时刻对应的全局 ticks
};

#endif
