#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/pstat.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int pid, priority;

  if(argc != 3){
    fprintf(2, "usage: chpr pid priority\n");
    exit(1);
  }

  pid = atoi(argv[1]);
  priority = atoi(argv[2]);
  if(priority < PROC_PRIO_MIN || priority > PROC_PRIO_MAX){
    fprintf(2, "chpr: priority must be %d..%d\n",
            PROC_PRIO_MIN, PROC_PRIO_MAX);
    exit(1);
  }

  if(setpriority(pid, priority) < 0){
    fprintf(2, "chpr: failed to update pid %d\n", pid);
    exit(1);
  }

  printf("pid %d priority -> %d\n", pid, priority);
  exit(0);
}
