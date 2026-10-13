#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

static char *pattern;
static int want = 0;  // T_FILE / T_DIR for -type f / -type d

// check if the file type matches and return 1 if it does, 0 otherwise
static int
typeok(short type)
{
  if (want == T_DIR)
    return type == T_DIR;
  if (want == T_FILE)
    return type == T_FILE || type == T_DEVICE;
  return 1;
}

// check if name matches pattern. '*' matches any run of characters, even an empty one
static int
match(char *pat, char *name)
{
  if (*pat == '\0')
    return *name == '\0';
  if (*pat == '*')
    // let '*' match nothing or swallow one more character and try again
    return match(pat + 1, name) || (*name != '\0' && match(pat, name + 1));
  return *pat == *name && match(pat + 1, name + 1);
}

// Search directory dir. Returns -1 if it cannot be opened, 0 otherwise.
static int
find(char *dir)
{
  char buf[MAXPATH], *p; // MAXPATH as used in open()
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(dir, O_RDONLY)) < 0)
    return -1;
  if (fstat(fd, &st) < 0 || st.type != T_DIR) {
    close(fd);
    return 0;
  }
  if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long: %s\n", dir);
    close(fd);
    return 0;
  }
  strcpy(buf, dir);
  p = buf + strlen(buf);
  // Don't double the slash for a dir like "/" or "a/".
  if (p > buf && p[-1] != '/')
    *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;
    if (stat(buf, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }
    if (match(pattern, p) && typeok(st.type))
      printf("%s\n", buf);
    if (st.type == T_DIR && find(buf) < 0)
      fprintf(2, "find: cannot open %s\n", buf);
  }
  close(fd);
  return 0;
}

static void
usage(void)
{
  fprintf(2, "usage: find <dir> <pattern> [-type f|d]\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  if (argc == 5 && strcmp(argv[3], "-type") == 0) { // check if type is specified
    if (strcmp(argv[4], "f") == 0)                  // -type f
      want = T_FILE;
    else if (strcmp(argv[4], "d") == 0)             // -type d
      want = T_DIR;
    else
      usage();                                      // invalid type
  } else if (argc != 3) {                           // check for correct number of arguments w/ unspecified type
    usage();
  }
  pattern = argv[2]; // set the pattern to search for

  if (find(argv[1]) < 0) {
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  exit(0);
}
