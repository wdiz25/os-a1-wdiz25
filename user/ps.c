#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "user/user.h"

// State names, indexed by the enum procstate values in kernel/proc.h:
// UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE. Keep in that order.
// (getprocs never returns UNUSED processes.)
static char *states[] = {
  [1] = "used",
  [2] = "sleeping",
  [3] = "runnable",
  [4] = "running",
  [5] = "zombie",
};

// static: 64 entries (2.5 KB) is too much for xv6's 4 KB user stack
static struct uproc procs[NPROC];

int
main(void)
{
  int i, n;
  char *state;

  if ((n = getprocs(procs, NPROC)) < 0) {
    fprintf(2, "ps: getprocs failed\n");
    exit(1);
  }
  printf("PID\tPPID\tSTATE\tSIZE\tNAME\n");
  for (i = 0; i < n; i++) {
    state = "?";
    if (procs[i].state >= 1 && procs[i].state <= 5)
      state = states[procs[i].state];
    printf("%d\t%d\t%s\t%lu\t%s\n", procs[i].pid, procs[i].ppid, state,
           procs[i].sz, procs[i].name);
  }
  exit(0);
}
