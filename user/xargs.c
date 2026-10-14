#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define WORDMAX 512  // longest input word, including its '\0'

// The words of the batch being collected. static because 32 x 512 bytes
// (16 KB) would overflow xv6's 4 KB user stack.
static char words[MAXARG][WORDMAX];

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

// Spaces, tabs and newlines separate words.
static int
is_blank(char c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

static void
usage(void)
{
  fprintf(2, "usage: xargs [-n N] cmd [args...]\n");
  exit(1);
}

// Return N from "-n N", or print the usage message and exit if N is not
// all digits (this rejects "2x" and "-1") or is less than 1 (this rejects
// "" and "0").
static int
parse_n(char *s)
{
  char *p;
  int n;

  for (p = s; *p; p++)
    if (*p < '0' || *p > '9')
      usage();
  if ((n = atoi(s)) < 1)
    usage();
  return n;
}

int
main(int argc, char *argv[])
{
  int perrun = 0;  // N from -n N; 0 means one run per input line
  int cmdidx = 1;  // index of <cmd> in argv

  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    if (argc < 3)
      usage();
    perrun = parse_n(argv[2]);
    cmdidx = 3;
  }
  if (cmdidx >= argc)  // no <cmd>
    usage();
  if (argc - cmdidx >= MAXARG - 1) {
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }

  // args[] = <cmd> [args...], then the batch of input words, then 0.
  // The kernel's exec() needs that 0 to fit in MAXARG slots too, so a run
  // gets at most MAXARG - 1 arguments.
  char *args[MAXARG];
  int nfixed = argc - cmdidx;          // <cmd> [args...], always passed
  int maxwords = MAXARG - 1 - nfixed;  // most input words one run can take
  int nwords = 0;                      // words in the current batch
  int wordlen = 0;                     // length of the word being read
  int failed = 0;
  int i, eof, endline;
  char c = 0;

  if (perrun > 0 && perrun < maxwords)
    maxwords = perrun;
  for (i = 0; i < nfixed; i++)
    args[i] = argv[cmdidx + i];

  for (;;) {
    eof = read(0, &c, 1) != 1;
    if (!eof && !is_blank(c)) {  // part of a word
      if (wordlen == WORDMAX - 1) {
        fprintf(2, "xargs: word too long\n");
        exit(1);
      }
      words[nwords][wordlen++] = c;
      continue;
    }
    if (wordlen > 0) {  // whitespace or end of input ends the current word
      words[nwords++][wordlen] = '\0';
      wordlen = 0;
    }
    endline = !eof && c == '\n';
    // run when the batch is full, at the end of input, or at the end of
    // a line without -n
    if (nwords > 0 && (nwords == maxwords || eof || (endline && perrun == 0))) {
      for (i = 0; i < nwords; i++)
        args[nfixed + i] = words[i];
      args[nfixed + nwords] = 0;
      if (run(args) < 0)
        failed = 1;
      nwords = 0;
    }
    if (eof)
      break;
  }
  exit(failed);
}
