#include "kernel/types.h"
#include "user/user.h"

static void
usage(void)
{
  fprintf(2, "usage: sleep <ticks>\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  // check for one argument that is not empty
  if(argc != 2 || argv[1][0] == '\0')
    usage();

  // check for non-numerical characters
  for(char *s = argv[1]; *s; s++)
    if(*s < '0' || *s > '9')
      usage();

  // convert the argument to an integer and pause for that many ticks
  pause(atoi(argv[1]));
  exit(0);
}
