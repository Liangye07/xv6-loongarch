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

int
main(void)
{
  struct pstat *ps;
  int n;

  ps = malloc(sizeof(struct pstat) * NPROC);
  if(ps == 0){
    fprintf(2, "ps: malloc failed\n");
    exit(1);
  }

  n = getprocs(ps, NPROC);
  if(n < 0){
    fprintf(2, "ps: getprocs failed\n");
    free(ps);
    exit(1);
  }

  printf("PID\tSTATE\tPRI\tMEM\tTICKS\tSCHED\tNAME\n");
  for(int i = 0; i < n; i++){
    printf("%d\t%s\t%d\t%llu\t%llu\t%llu\t%s\n",
           ps[i].pid, state_name(ps[i].state), ps[i].priority,
           ps[i].sz, ps[i].run_ticks, ps[i].sched_count, ps[i].name);
  }

  free(ps);
  exit(0);
}
