#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  volatile uint64 sink = (uint64)getpid();
  char *tag = 0;

  if(argc > 1)
    tag = argv[1];

  if(tag)
    printf("burn: pid=%d tag=%s\n", getpid(), tag);
  else
    printf("burn: pid=%d\n", getpid());

  for(;;){
    sink += 1;
    sink ^= (sink << 7);
    sink += (sink >> 3);
  }

  exit(0);
}
