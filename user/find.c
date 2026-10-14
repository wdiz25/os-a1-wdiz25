#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

static char *pattern;  // the <pattern> argument
static int want = 0;   // -type filter: 0 for any type, or T_FILE / T_DIR

// check if a file of this type passes the -type filter (1 if it does)
static int
typeok(short type)
{
  if (want == T_DIR)
    return type == T_DIR;
  if (want == T_FILE)
    return type == T_FILE || type == T_DEVICE;
  return 1;
}

// check if name matches pattern. '*' matches any run of characters, even
// an empty one
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
  char buf[MAXPATH], *p; // MAXPATH: the longest path open() accepts
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
    exit(1);
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
      exit(1);
    }
    if (match(pattern, p) && typeok(st.type))
      printf("%s\n", buf);
    if (st.type == T_DIR && find(buf) < 0) {
      fprintf(2, "find: cannot open %s\n", buf);
      exit(1);
    }
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
  // find <dir> <pattern> -type f|d
  if (argc == 5 && strcmp(argv[3], "-type") == 0) {
    if (strcmp(argv[4], "f") == 0)
      want = T_FILE;
    else if (strcmp(argv[4], "d") == 0)
      want = T_DIR;
    else
      usage();  // invalid type
  } else if (argc != 3) {  // otherwise it must be find <dir> <pattern>
    usage();
  }
  pattern = argv[2];

  if (find(argv[1]) < 0) {
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  exit(0);
}
