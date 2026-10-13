// Tests for the trace system call. Each case runs in its own child, which
// prints a label, turns tracing on, makes some system calls and exits.
// The kernel's trace lines are checked by eye against the expected ones.
#include "kernel/types.h"
#include "kernel/syscall.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BIT(n) (1 << (n))

static void
run(char *label, int mask, void (*f)(void))
{
  int pid = fork();

  if (pid < 0) {
    fprintf(2, "tracetest: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    printf("--- %s\n", label);
    trace(mask);
    f();
    exit(0);
  }
  wait(0);
}

// expect: open(?) -> -1 twice, then open("tracetest") -> fd
static void
badstr(void)
{
  open((char *)0x3fffffff00L, O_RDONLY);  // far outside user memory
  open(sbrk(0), O_RDONLY);                // just past the end of user memory
  close(open("tracetest", O_RDONLY));
}

// expect one line each for the 7 string syscalls, in order
static void
strcalls(void)
{
  char *argv[] = {"nosuch", 0};

  mkdir("tt_d");
  chdir("tt_d");
  chdir("/");
  close(open("tt_f", O_CREATE | O_RDWR));
  link("tt_f", "tt_g");
  mknod("tt_n", 1, 1);
  unlink("tt_g");
  unlink("tt_f");
  unlink("tt_n");
  unlink("tt_d");
  exec("nosuch", argv);  // exec("nosuch") -> -1
}

// expect: getpid() -> pid, uptime() -> ticks, sync() -> 0, fork() -> child
// pid (from the parent only), then the child's getpid() -> its own pid
static void
noargs(void)
{
  getpid();
  uptime();
  sync();
  if (fork() == 0) {
    getpid();  // inherited mask: traced with the child's pid
    exit(0);
  }
  wait(0);
}

// expect signed ints: close(-5) -> -1, read(-1) -> -1, kill(-3) -> -1,
// pause(0) -> 0, dup(1) -> fd; exit is never printed
static void
intargs(void)
{
  char c;

  close(-5);
  read(-1, &c, 1);
  kill(-3);
  pause(0);
  close(dup(1));
  exit(7);
}

// expect: trace(8388608) -> 0 and trace(8390656) -> 0; the final trace(0)
// turns tracing off before the check, so it is not printed
static void
self(void)
{
  trace(BIT(SYS_trace) | BIT(SYS_getpid));
  getpid();
  trace(0);
  getpid();
}

// expect: exec("echo") -> 2 (argc, from the successful exec), and the
// echoed program's own write calls traced under the same pid
static void
execok(void)
{
  char *argv[] = {"echo", "hi", 0};

  exec("echo", argv);
}

int
main(void)
{
  run("bad string pointers", BIT(SYS_open), badstr);
  run("string syscalls", BIT(SYS_mkdir) | BIT(SYS_chdir) | BIT(SYS_open) |
      BIT(SYS_link) | BIT(SYS_mknod) | BIT(SYS_unlink) | BIT(SYS_exec),
      strcalls);
  run("no-argument syscalls and fork", BIT(SYS_getpid) | BIT(SYS_uptime) |
      BIT(SYS_sync) | BIT(SYS_fork), noargs);
  run("int arguments and exit", BIT(SYS_close) | BIT(SYS_read) |
      BIT(SYS_kill) | BIT(SYS_pause) | BIT(SYS_dup) | BIT(SYS_exit), intargs);
  run("trace traces itself", BIT(SYS_trace), self);
  run("successful exec keeps the mask", BIT(SYS_exec) | BIT(SYS_write),
      execok);
  printf("--- done\n");
  exit(0);
}
