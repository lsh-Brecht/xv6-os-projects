#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "proc.h"
#include "syscall.h"

int
main(int argc, char *argv[])
{
  printf(1, "-TEST START-\n");
  int fd;
  int blocks = 0;
  char buf[BSIZE];
  fd = open("maximum file size", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "file open failed\n");
    exit();
  }
  while (1) {
    *(int *)buf = blocks;
    int cc = write(fd, buf, sizeof(buf));
    if (cc <= 0)
      break;
    blocks++;
    if (blocks % 100 == 0)
      printf(1, ".");
  }
  printf(1, "\nwrote %d blocks\n", blocks);
  if (blocks != 16523) {
    printf(1, "bigfile: file is too small\n");
    exit();
  }
  printf(1, "-TEST FINISHED-\n");
  close(fd);
  exit();
}
