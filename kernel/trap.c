/*
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

// 解决隐式声明警告
int devintr(void);

// 解决 cpuid 定义冲突：移除 static，匹配 defs.h
int cpuid() {
  unsigned long x;
  asm volatile("csrrd %0, 0x20" : "=r" (x)); // 直接读取 CPUID 寄存器 (0x20)
  return (int)(x & 0x1ff); // 掩码获取核心 ID
}

extern void kernelvec();

void
trapinit(void)
{
  initlock(&tickslock, "time");
  // 初始化全局异常入口
  csrwr_eentry((uint64)kernelvec);
}

void
trapinithart(void)
{
  // 配置定时器：约 0.1s
  uint64 tcfg = 10000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
  csrwr_tcfg(tcfg);

  // 开启定时器中断使能 (TI)
  uint64 ecfg = ECFG_LIE_TI; 
  csrwr_ecfg(ecfg);

  // 开启全局中断
  intr_on();
}

void 
kerneltrap()
{
  int which_dev = 0;
  uint64 era = csrrd_era();
  uint64 prmd = csrrd_prmd();
  
  if((prmd & CSR_CRMD_PLV) != 0)
    panic("kerneltrap: not from privilege 0");
  
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("Unexpected kernel trap: estat=%lx era=%lx badv=%lx\n", 
           csrrd_estat(), csrrd_era(), csrrd_badv());
    panic("kerneltrap");
  }

  // 恢复状态
  csrwr_era(era);
  csrwr_prmd(prmd);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  if(ticks % 10 == 0){
    printf("."); // 每10次中断打印一个点
  }
  release(&tickslock);
}

int
devintr()
{
  uint64 estat = csrrd_estat();
  uint64 ecfg = csrrd_ecfg();

  if(estat & ecfg & ECFG_LIE_TI){
    // 关键修改：根据你的 loongarch.h 声明，不带参数调用
    csrwr_ticlr(); 

    if(cpuid() == 0){
      clockintr();
    }
    return 2;
  } 
  
  return 0;
}*/
/*
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "defs.h"

// 注意：我们在本周只实现内核态的 trap 处理框架（不涉及进程切换、用户态 trap）。
// 进程相关的逻辑将在下一周实现。

struct spinlock tickslock;
uint ticks;

// kernelvec 汇编入口会调用 kerneltrap()
extern void kernelvec(void);

// 为兼容你现有头文件命名：提供 devintr 原型
int devintr(void);

// 读取 cpuid（核 id）
int cpuid(void) {
  uint64 x;
  asm volatile("csrrd %0, 0x20" : "=r" (x)); // CSR CPUID = 0x20 （与你的 loongarch.h 对应）
  return (int)(x & 0x1ff);
}

// 初始化 trap 子系统（全局一次）
void
trapinit(void)
{
  initlock(&tickslock, "time");

  // 将 EENTRY 指向我们在 kernelvec.S 中实现的异常入口
  // 使用你 header 中的封装函数
  csrwr_eentry((uint64)kernelvec);
}

// 每个 hart 的初始化
void
trapinithart(void)
{
  // 1) 配置/启动定时器：示例值（你可根据 QEMU 仿真器调整）
  //    这里使用你 header 中的 cfg 宏：CSR_TCFG_EN、CSR_TCFG_PER
  //    tcfg 的低位可能包含计数或控制标志；你之前写的是 10000000UL | ...，保持它
  uint64 tcfg = 10000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
  csrwr_tcfg(tcfg);

  // 2) 打开定时器中断（在 ECFG 中使能 TI 比特）
  uint64 ecfg = csrrd_ecfg();
  ecfg |= ECFG_LIE_TI;
  csrwr_ecfg(ecfg);

  // 3) 开启全局中断
  intr_on();
}

// 内核态 trap 入口（由 kernelvec.S 调用）
// 在本阶段不做用户态相关处理，因此不使用 myproc()/schedule()
void
kerneltrap(void)
{
  uint64 era = csrrd_era();
  uint64 prmd = csrrd_prmd();

  // 确保是内核态触发
  if ((prmd & CSR_CRMD_PLV) != 0)
    panic("kerneltrap: not from privilege 0");

  // 内核入口时不应该允许中断标志被置位（根据 xv6 约定）
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  int which_dev = devintr();
  if (which_dev == 0) {
    // 未识别的 trap：打印调试信息并 panic（这里不做进程处理）
    uint64 estat = csrrd_estat();
    uint64 badv  = csrrd_badv();
    printf("Unexpected kernel trap: estat=0x%lx era=0x%lx badv=0x%lx prmd=0x%lx\n",
           estat, era, badv, prmd);
    panic("kerneltrap");
  }

  // 恢复 ERA 和 PRMD（若 kernelvec/S 已保存 PRMD，则此处可选）
  csrwr_era(era);
  csrwr_prmd(prmd);
}

// 时钟中断处理：只维护 ticks 计数并做少量输出，核心处理不涉及进程切换
void
clockintr(void)
{
  acquire(&tickslock);
  ticks++;
  // 每 10 个时钟中断打印一个点，便于在 qemu 控制台观察
  if (ticks % 10 == 0) {
    printf(".");
  }
  release(&tickslock);
}

// 设备中断辨识与处理（返回值语义与 xv6 保持一致）
// return 2: timer interrupt
// return 1: other known device (none implemented here)
// return 0: not a device interrupt / unknown
int
devintr(void)
{
  uint64 estat = csrrd_estat();
  uint64 ecfg  = csrrd_ecfg();

  // 如果 ESTAT 与 ECFG 的 TI 位都被置位 -> 定时器中断
  // 这是基于你 loongarch.h 中对 ECFG_LIE_TI 的定义（1<<11），
  // 并且你之前写过类似的判断： if(estat & ecfg & ECFG_LIE_TI)
  if ((estat & ecfg & ECFG_LIE_TI) != 0) {
    // 清除时钟中断标志（TICLR 写 1）
    // 你 header 中实现的 csrwr_ticlr() 会写入需要的位
    csrwr_ticlr();

    // 仅由 CPU 0 打印/维护 ticks（避免多核重复打印）
    if (cpuid() == 0) {
      clockintr();
    }

    // 返回 2 表示时钟中断（和 xv6 习惯保持一致）
    return 2;
  }

  // 其他设备中断可以在这里分发，例如串口、virtio 等：
  // if ( ... uart 中断检测 ...) { uartintr(); return 1; }
  // if ( ... virtio 中断检测 ...) { virtio_intr(); return 1; }

  // 未知中断
  return 0;
}
*/
// kernel/trap.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "defs.h"

// tick 计数及其自旋锁
struct spinlock tickslock;
uint ticks;

// kernelvec 汇编入口（由 entry.S / kernelvec.S 提供）
extern void kernelvec(void);

// 我们不在本周实现进程切换/用户态 trap，这里只处理内核态的中断/异常框架
// devintr 原型（设备中断判别）
int devintr(void);

/*
 * trapinit: 初始化全局 trap（一次性，设置 EENTRY）
 */
void
trapinit(void)
{
  initlock(&tickslock, "time");

  // 将 EENTRY 指向 kernelvec（汇编入口）
  // 读取后可以打印确认（便于调试）
  csrwr_eentry((uint64)kernelvec);
}

/*
 * trapinithart: 每个 hart 的本地中断/定时器初始化
 */
void
trapinithart(void)
{
  // 设置定时器周期（可在需要时调小以便更快看到中断）
  // 该值与你的 QEMU 配置有关；默认使用较大值以免过快打印
  uint64 interval = 10000000UL;
  csrwr_tcfg(interval | CSR_TCFG_EN | CSR_TCFG_PER);

  // 把 ECFG 的 TI（定时器中断使能）打开（在 per-hart 层面）
  uint64 ecfg = csrrd_ecfg();
  ecfg |= ECFG_LIE_TI;
  csrwr_ecfg(ecfg);

  // 开启全局中断位（CRMD.IE）
  intr_on();

  // debug：打印当前 hart 的 CPUID、ECFG/ESTAT（仅用于调试，后续可去掉）
#ifdef TRAP_DEBUG
  printf("trapinithart: hart=%d ECFG=0x%lx ESTAT=0x%lx\n", (int)r_cpuid(), csrrd_ecfg(), csrrd_estat());
#endif
}

/*
 * kerneltrap: 由 kernelvec 汇编入口最终调用（仅处理内核态 trap）
 * 我们按照 xv6 的约定：进入 kerneltrap 时应位于内核特权（PLV0）且中断处于关闭状态
 */
void
kerneltrap(void)
{
  uint64 era  = csrrd_era();
  uint64 prmd = csrrd_prmd();

  // 保证是在内核态触发的 trap
  if ((prmd & CSR_CRMD_PLV) != 0)
    panic("kerneltrap: not from privilege 0");

  // 进入 kerneltrap 时中断位应当是关闭的
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  int which_dev = devintr();
  if (which_dev == 0) {
    uint64 estat = csrrd_estat();
    uint64 badv  = csrrd_badv();
    printf("Unexpected kernel trap: estat=0x%lx era=0x%lx badv=0x%lx prmd=0x%lx\n",
           estat, era, badv, prmd);

    // 如果你愿意也可以在这里把更多 CSR 打印出来做排查：
    // printf("CRMD=0x%lx PRMD=0x%lx ECFG=0x%lx EENTRY=0x%lx\n", csrrd_crmd(), csrrd_prmd(), csrrd_ecfg(), csrrd_eentry());

    panic("kerneltrap");
  }

  // 恢复 ERA / PRMD（汇编入口可能已保存或需要 C 层恢复）
  csrwr_era(era);
  csrwr_prmd(prmd);
}

/*
 * clockintr: 仅维护 ticks 且做少量输出（仅 cpuid==0 打印）
 */
void
clockintr(void)
{
  acquire(&tickslock);
  ticks++;
  if (ticks % 10 == 0) {
    // 控制台不会太频繁打印，便于观察
    printf(".");
  }
  release(&tickslock);
}

/*
 * devintr: 识别并处理设备中断
 * 返回值：
 *   2 => timer interrupt
 *   1 => other device interrupt (保留)
 *   0 => not a device interrupt / unknown
 *
 * 说明：
 *   本实现更稳健地从 ESTAT 提取 ECode（ESTAT 中 ECode 位通常在 16:21），
 *   先判断是否为中断（ECode_INT），再根据 pending/enable 位判断哪类中断。
 */
int
devintr(void)
{
  uint64 estat = csrrd_estat();
  uint64 ecfg  = csrrd_ecfg();

  // 从 ESTAT 中提取 ECode（位 16:21）
  uint64 ecode = (estat >> 16) & 0x3f;

  // 如果 ESTAT 表示中断类型（ECode_INT），再细化判断具体哪一位
  if (ecode == ECode_INT) {
    // 如果 ECFG 中的 TI 被使能且 ESTAT 中对应的 pending 位也置位 -> timer
    // 这里用 (estat & ECFG_LIE_TI) 检查 pending（假设 ESTAT 的低位与 ECFG 的位对应）
    if ((ecfg & ECFG_LIE_TI) && (estat & ECFG_LIE_TI)) {
      // 清中断标志（TICLR）
      csrwr_ticlr();

      // 仅在 core0 更新 ticks/打印，避免多核重复输出
      if ((int)r_cpuid() == 0) {
        clockintr();
      }
      return 2;
    }

    // 其他中断位（串口 / virtio / PCH 等）可以在此处扩展检测
    // 例如：
    // if ((ecfg & ECFG_LIE_HWI) && (estat & ECFG_LIE_HWI)) { uartintr(); return 1; }

    // 未能确定具体外设中断，仍返回未知（允许上层打印/排错）
    return 0;
  }

  // 不是 ECode_INT，可能是同步异常（例如页面错误 / 非法指令）
  // 这里不处理用户态异常（下周实现），返回 0 使 kerneltrap() 打印并 panic
  return 0;
}
