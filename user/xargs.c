#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define WORDMAX 512  // longest input word, including its '\0'

// Run args[0] with the null-terminated argument list args and wait for it.
// Returns 0 if it exited with status 0, -1 otherwise.
static int
run(char *args[])
{
  int pid, status;

  if ((pid = fork()) < 0) {
    fprintf(2, "xargs: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    exec(args[0], args);
    // exec only returns if it failed
    fprintf(2, "xargs: exec %s failed\n", args[0]);
    exit(1);
  }
  wait(&status);
  return status == 0 ? 0 : -1;
}

static void
usage(void)
{
  fprintf(2, "usage: xargs [-n N] cmd [args...]\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int n = 0;      // words per run with -n; 0 means one run per input line
  int first = 1;  // index of <cmd> in argv
  char *s;

  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    if (argc < 3)
      usage();
    // N must be all digits (this rejects "2x" and "-1") and at least 1
    // (this rejects "" and "0")
    for (s = argv[2]; *s; s++)
      if (*s < '0' || *s > '9')
        usage();
    if ((n = atoi(argv[2])) < 1)
      usage();
    first = 3;
  }
  if (first >= argc)  // no <cmd>
    usage();
  if (argc - first >= MAXARG - 1) {
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }

  // args[] = <cmd> [args...], then the batch of input words, then 0.
  // The kernel's exec() needs that 0 to fit in MAXARG slots too, so a run
  // gets at most MAXARG - 1 arguments.
  // words[] is static because 16 KB would overflow xv6's 4 KB stack.
  static char words[MAXARG][WORDMAX];
  char *args[MAXARG];
  int base = argc - first;        // number of <cmd> [args...]
  int limit = MAXARG - 1 - base;  // most words one run can take
  int nw = 0;                 // words in the current batch
  int len = 0;                // length of the word being read
  int failed = 0;
  int i, r;
  char c = 0;

  if (n > 0 && n < limit)
    limit = n;
  for (i = 0; i < base; i++)
    args[i] = argv[first + i];

  for (;;) {
    r = read(0, &c, 1);
    if (r == 1 && c != ' ' && c != '\t' && c != '\n') {  // part of a word
      if (len == WORDMAX - 1) {
        fprintf(2, "xargs: word too long\n");
        exit(1);
      }
      words[nw][len++] = c;
      continue;
    }
    if (len > 0) {  // whitespace or end of input ends the current word
      words[nw++][len] = '\0';
      len = 0;
    }
    // run when the batch is full, at the end of input, or at the end of
    // a line without -n
    if (nw > 0 && (nw == limit || r != 1 || (c == '\n' && n == 0))) {
      for (i = 0; i < nw; i++)
        args[base + i] = words[i];
      args[base + nw] = 0;
      if (run(args) < 0)
        failed = 1;
      nw = 0;
    }
    if (r != 1)
      break;
  }
  exit(failed);
}
