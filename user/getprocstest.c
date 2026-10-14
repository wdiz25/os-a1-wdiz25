// Tests for the getprocs system call. Prints "ok" or "FAIL" for each check
// and exits with status 1 if any check failed.
#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

#define PAGE 4096
#define SLEEPING 2  // enum procstate values from kernel/proc.h
#define RUNNING 4
#define ZOMBIE 5

static int failed;
static struct uproc all[NPROC], few[NPROC];

static void
check(int ok, char *what)
{
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    failed = 1;
}

// Index of pid in procs[0..n), or -1.
static int
find(struct uproc *procs, int n, int pid)
{
  int i;

  for (i = 0; i < n; i++)
    if (procs[i].pid == pid)
      return i;
  return -1;
}

// Does every entry look like a real process?
static int
sane(struct uproc *procs, int n)
{
  int i, j;

  if (n < 1 || n > NPROC)
    return 0;
  for (i = 0; i < n; i++) {
    if (procs[i].pid <= 0 || procs[i].ppid < 0)
      return 0;
    if (procs[i].state < 1 || procs[i].state > ZOMBIE)
      return 0;
    for (j = 0; j < 16 && procs[i].name[j]; j++)
      ;
    if (j == 16)  // name not NUL-terminated
      return 0;
  }
  return 1;
}

// Checks that compare two separate calls can see different process tables
// if other processes start or exit in between (for example when this test
// runs in the background), so those checks retry until two calls agree.
#define RETRIES 20

static void
basics(void)
{
  struct sysinfo si;
  int n, i, tries;

  n = getprocs(all, NPROC);
  check(sane(all, n), "a full listing has sane entries");
  for (tries = 0; tries < RETRIES; tries++) {
    n = getprocs(all, NPROC);
    sysinfo(&si);
    if (n == si.nproc)
      break;
  }
  check(n == si.nproc, "the count matches sysinfo's nproc");
  i = find(all, n, getpid());
  check(i >= 0 && all[i].state == RUNNING &&
        strcmp(all[i].name, "getprocstest") == 0,
        "this process is listed as running, with its name");
  check(all[0].pid == 1 && all[0].ppid == 0 && strcmp(all[0].name, "init") == 0,
        "init is first, with ppid 0");
}

static void
maxarg(void)
{
  int n, m, ok, tries;

  for (tries = 0, ok = 0; tries < RETRIES && !ok; tries++) {
    n = getprocs(all, NPROC);
    m = getprocs(few, 2);
    ok = m == 2 && few[0].pid == all[0].pid && few[1].pid == all[1].pid;
  }
  check(ok, "max 2 returns the first 2 entries in table order");
  for (tries = 0, ok = 0; tries < RETRIES && !ok; tries++) {
    n = getprocs(all, NPROC);
    m = getprocs(few, 1);
    ok = m == 1 && few[0].pid == all[0].pid;
  }
  check(ok, "max 1 returns the first entry");
  check(getprocs(few, 0) == 0, "max 0 returns 0");
  check(getprocs(few, -1) == -1, "max -1 returns -1");
  for (tries = 0, ok = 0; tries < RETRIES && !ok; tries++) {
    n = getprocs(all, NPROC);
    ok = getprocs(few, 1000) == n;
  }
  check(ok, "a max above the count returns the count");
}

static void
pointers(void)
{
  char *end;
  struct uproc *last;

  check(getprocs((struct uproc *)0x3fffffff00L, 1) == -1,
        "a buffer far outside user memory returns -1");
  sbrk(PAGE);  // a fresh last page that is safe to overwrite
  end = sbrk(0);
  last = (struct uproc *)(end - sizeof(struct uproc));
  check(getprocs((struct uproc *)end, 1) == -1,
        "a buffer just past the end of user memory returns -1");
  check(getprocs(last, 1) == 1,
        "one entry in the last bytes of user memory works");
  check(getprocs(last, 2) == -1,
        "a second entry past the end of user memory returns -1");
}

static void
children(void)
{
  int ready[2], pid, n, i, tries;
  char ch;

  pipe(ready);
  if ((pid = fork()) == 0) {
    close(ready[1]);
    read(ready[0], &ch, 1);  // wait until the parent closes the pipe
    exit(0);
  }
  pause(1);  // let the child reach read() and sleep
  n = getprocs(all, NPROC);
  i = find(all, n, pid);
  check(i >= 0 && all[i].ppid == getpid() &&
        strcmp(all[i].name, "getprocstest") == 0,
        "a forked child is listed with ppid = its parent's pid");
  check(i >= 0 && all[i].state == SLEEPING,
        "a child blocked in read is sleeping");

  close(ready[0]);
  close(ready[1]);  // the child's read returns and it exits
  for (tries = 0; tries < 100; tries++) {
    n = getprocs(all, NPROC);
    i = find(all, n, pid);
    if (i >= 0 && all[i].state == ZOMBIE)
      break;
    pause(1);
  }
  check(i >= 0 && all[i].state == ZOMBIE,
        "an exited child that is not waited for is a zombie");
  wait(0);
  n = getprocs(all, NPROC);
  check(find(all, n, pid) < 0, "a waited-for child is gone");
}

// Fork children that fork a grandchild and exit at once, so grandchildren
// are reparented to init while getprocs reads p->parent.
static void
churn(int rounds)
{
  int i, pid;

  for (i = 0; i < rounds; i++) {
    if ((pid = fork()) < 0)
      continue;  // process table full for a moment
    if (pid == 0) {
      if (fork() == 0)
        exit(0);  // grandchild
      exit(0);
    }
    wait(0);
  }
  exit(0);
}

static void
stress(void)
{
  int i, k, n, bad = 0;

  for (k = 0; k < 4; k++)
    if (fork() == 0)
      churn(200);
  for (i = 0; i < 2000; i++) {
    n = getprocs(all, (i % 3 == 0) ? 3 : NPROC);
    if (!sane(all, n))
      bad++;
  }
  for (k = 0; k < 4; k++)
    wait(0);
  check(bad == 0, "2000 calls during fork/exit stress all return sane entries");
}

int
main(void)
{
  basics();
  maxarg();
  pointers();
  children();
  stress();
  printf(failed ? "getprocstest: FAILED\n" : "getprocstest: OK\n");
  exit(failed);
}
