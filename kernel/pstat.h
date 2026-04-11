#ifndef PSTAT_H
#define PSTAT_H

#include "types.h"

#define PSTAT_NAME_LEN 16

enum pstat_state {
  PSTAT_UNUSED = 0,
  PSTAT_USED,
  PSTAT_SLEEPING,
  PSTAT_RUNNABLE,
  PSTAT_RUNNING,
  PSTAT_ZOMBIE,
};

struct pstat {
  int pid;
  int state;
  int level;       // 当前 MLFQ 队列层级 (0=Q0, 1=Q1, 2=Q2)
  uint64 sz;
  uint64 run_ticks;
  uint64 sched_count;
  char name[PSTAT_NAME_LEN];
};

#endif
