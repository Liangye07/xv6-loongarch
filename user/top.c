#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

static char *
state_name(int state)
{
  switch(state){
  case PSTAT_USED:
    return "used";
  case PSTAT_SLEEPING:
    return "sleep";
  case PSTAT_RUNNABLE:
    return "runble";
  case PSTAT_RUNNING:
    return "run";
  case PSTAT_ZOMBIE:
    return "zombie";
  default:
    return "unknown";
  }
}

static uint64
previous_ticks(struct pstat *prev, int prevn, int pid)
{
  for(int i = 0; i < prevn; i++){
    if(prev[i].pid == pid)
      return prev[i].run_ticks;
  }
  return 0;
}

int
main(int argc, char **argv)
{
  struct pstat *prev;
  struct pstat *cur;
  int prevn, curn;
  int interval = 10;
  int rounds = 20;

  if(argc >= 2)
    interval = atoi(argv[1]);
  if(argc >= 3)
    rounds = atoi(argv[2]);
  if(argc > 3 || interval <= 0 || rounds <= 0){
    fprintf(2, "usage: top [interval_ticks] [rounds]\n");
    exit(1);
  }

  prev = malloc(sizeof(struct pstat) * NPROC);
  cur = malloc(sizeof(struct pstat) * NPROC);
  if(prev == 0 || cur == 0){
    fprintf(2, "top: malloc failed\n");
    free(prev);
    free(cur);
    exit(1);
  }

  prevn = getprocs(prev, NPROC);
  if(prevn < 0){
    fprintf(2, "top: initial getprocs failed\n");
    free(prev);
    free(cur);
    exit(1);
  }

  for(int round = 0; round < rounds; round++){
    sleep(interval);
    curn = getprocs(cur, NPROC);
    if(curn < 0){
      fprintf(2, "top: getprocs failed\n");
      free(prev);
      free(cur);
      exit(1);
    }

    printf("== top round %d uptime %d ==\n", round + 1, uptime());
    printf("PID\tPRI\t+TICK\tTOTAL\tSCHED\tSTATE\tNAME\n");
    for(int i = 0; i < curn; i++){
      uint64 oldticks = previous_ticks(prev, prevn, cur[i].pid);
      uint64 delta = cur[i].run_ticks - oldticks;
      printf("%d\t%d\t%llu\t%llu\t%llu\t%s\t%s\n",
             cur[i].pid, cur[i].priority, delta, cur[i].run_ticks,
             cur[i].sched_count, state_name(cur[i].state), cur[i].name);
    }
    memmove(prev, cur, sizeof(struct pstat) * NPROC);
    prevn = curn;
  }

  free(prev);
  free(cur);
  exit(0);
}
