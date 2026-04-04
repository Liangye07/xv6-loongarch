#ifndef PSTAT_H
#define PSTAT_H

#include "types.h"

#define PSTAT_NAME_LEN 16

#define PROC_PRIO_MIN 1
#define PROC_PRIO_MAX 5
#define PROC_PRIO_DEFAULT 3

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
  int priority;
  uint64 sz;
  uint64 run_ticks;
  uint64 sched_count;
  char name[PSTAT_NAME_LEN];
};

#endif
