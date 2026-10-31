#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "spinlock.h"
#include "fs.h"
#include "proc.h"
#include "syscall.h"
void print_hex(uint val) {
    int i; char hex_chars[] = "0123456789ABCDEF";
    char buf[10]; buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        buf[2 + i] = hex_chars[(val >> shift) & 0xF];}
    for (i = 0; i < 10; i++) {printf(1, "%c", buf[i]);}
    printf(1, "\n");
}
int main(void)
{
  int initial_free = freemem();
  printf(1, "<free pages> Initial free pages: %d\n\n", initial_free);
  printf(1, "File mapping: MAP_ANONYMOUS, write hoon\n");
  char *anon = (char*)mmap(0, 4096, PROT_READ|PROT_WRITE , MAP_ANONYMOUS, -1, 0);
  printf(1, "mapping address : "); print_hex((uint)anon);
  if ((int)anon == 0) {printf(1, "anonymous mmap failed\n");exit();}
  printf(1, "<free pages> After MAP_ANONYMOUS: %d\n", freemem());
  anon[0] = 'H';  anon[1] = 'O';  anon[2] = 'O';  anon[3] = 'N';
  printf(1, "[ANONYMOUS] data: %c %c %c %c\n", anon[0], anon[1], anon[2], anon[3]);
  printf(1, "<free pages> After Page Fault Handling: %d\n", freemem());
  
  printf(1, "\nFile mapping: MAP_POPULATE, read README\n");
  int fd = open("README", O_RDWR);
  if (fd < 0) {printf(1, "failed to open README\n");exit();}
  char *filemap = (char*)mmap(4096, 4096, PROT_READ, MAP_POPULATE, fd, 0);
  printf(1, "mapping address: "); print_hex((uint)filemap);
  if ((int)filemap == 0) {printf(1, "filemap mmap failed\n");close(fd);exit();}
  printf(1, "<free pages> After File mapping: %d\n", freemem());
  printf(1, "[FILE MAPING] fd data: %c %c %c %c\n", filemap[0], filemap[1], filemap[2], filemap[3]);
  
  printf(1, "\nFork!\n");
  int pid = fork();
  if (pid < 0) {
    printf(1, "fork failed\n");
  } else if (pid == 0) {
    printf(1, "FORK-CHILD anonymous : %c %c %c %c\n", anon[0], anon[1], anon[2], anon[3]);
    printf(1, "FORK-CHILD filemaping: %c %c %c %c\n", filemap[0], filemap[1], filemap[2], filemap[3]);
    exit();
  } else {wait();}
  
  munmap((uint)anon);
  printf(1, "<free pages> After first unmapping: %d\n", freemem());
  munmap((uint)filemap);
  printf(1, "\n<free pages> After second unmapping: %d\n", freemem());
  close(fd);
  exit();
}
