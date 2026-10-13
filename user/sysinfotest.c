// Tests for the sysinfo system call. Prints "ok" or "FAIL" for each check
// and exits with status 1 if any check failed.
#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define PAGE 4096

static int failed;

static void
check(int ok, char *what)
{
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    failed = 1;
}

static struct sysinfo
info(void)
{
  struct sysinfo si;

  if (sysinfo(&si) < 0) {
    fprintf(2, "sysinfotest: sysinfo failed\n");
    exit(1);
  }
  return si;
}

static void
freemem(void)
{
  struct sysinfo a, b, c;

  a = info();
  check(a.freemem > 0 && a.freemem % PAGE == 0,
        "freemem is a positive multiple of 4096");
  if (sbrk(10 * PAGE) == SBRK_ERROR) {
    fprintf(2, "sysinfotest: sbrk failed\n");
    exit(1);
  }
  b = info();
  check(b.freemem <= a.freemem - 10 * PAGE,
        "freemem drops by at least 10 pages after sbrk(10 pages)");
  sbrk(-10 * PAGE);
  c = info();
  check(c.freemem >= b.freemem + 10 * PAGE,
        "freemem rises by at least 10 pages after sbrk(-10 pages)");
}

static void
nproc(void)
{
  struct sysinfo a, b;
  int ready[2];
  char ch;

  a = info();
  check(a.nproc >= 2, "nproc counts at least init and this process");
  pipe(ready);
  if (fork() == 0) {
    // wait until the parent closes the write end, then exit
    close(ready[1]);
    read(ready[0], &ch, 1);
    exit(0);
  }
  b = info();
  check(b.nproc == a.nproc + 1, "nproc goes up by 1 after fork");
  close(ready[0]);
  close(ready[1]);
  wait(0);
  b = info();
  check(b.nproc == a.nproc, "nproc goes back down after the child is waited for");
}

static void
nopenfile(void)
{
  struct sysinfo a, b;
  int fd, fd2, fds[2];

  a = info();
  check(a.nopenfile >= 1, "nopenfile counts at least the console");
  fd = open("README", O_RDONLY);
  b = info();
  check(fd >= 0 && b.nopenfile == a.nopenfile + 1, "open adds 1");
  fd2 = dup(fd);
  b = info();
  check(b.nopenfile == a.nopenfile + 1, "dup adds 0 (it shares the entry)");
  pipe(fds);
  b = info();
  check(b.nopenfile == a.nopenfile + 3, "pipe adds 2");
  close(fds[0]);
  close(fds[1]);
  b = info();
  check(b.nopenfile == a.nopenfile + 1, "closing both pipe ends removes 2");
  close(fd2);
  b = info();
  check(b.nopenfile == a.nopenfile + 1, "closing the dup removes 0");
  close(fd);
  b = info();
  check(b.nopenfile == a.nopenfile, "closing the last reference removes 1");
}

static void
pointers(void)
{
  struct sysinfo si;
  char *end;

  check(sysinfo(&si) == 0, "a valid pointer returns 0");
  check(sysinfo((struct sysinfo *)0x3fffffff00L) == -1,
        "a pointer far outside user memory returns -1");
  sbrk(PAGE);  // a fresh last page that is safe to overwrite
  end = sbrk(0);
  check(sysinfo((struct sysinfo *)end) == -1,
        "a pointer just past the end of user memory returns -1");
  check(sysinfo((struct sysinfo *)(end - 8)) == -1,
        "a struct straddling the end of user memory returns -1");
  check(sysinfo((struct sysinfo *)(end - sizeof(si))) == 0,
        "a struct in the last bytes of user memory returns 0");
}

int
main(void)
{
  freemem();
  nproc();
  nopenfile();
  pointers();
  printf(failed ? "sysinfotest: FAILED\n" : "sysinfotest: OK\n");
  exit(failed);
}
