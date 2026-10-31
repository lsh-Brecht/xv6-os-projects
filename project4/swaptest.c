#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int main () {
  int a, b;
  swapstat(&a, &b);
  printf(1, "first print Swap space: %d/%d\n", a, b);
  for (int i = 0; i < 4000; i++) {
    sbrk(4096);
  }
  swapstat(&a, &b);
  printf(1, "second print Swap space: %d/%d\n", a, b);
  int pid = fork();
  if(pid == 0) {
    for (int i = 0; i < 4000; i++) {
      sbrk(4096);
    }
    swapstat(&a, &b);
    printf(1, "child Swap space: %d/%d\n", a, b);
    exit();
  }else{
    wait();
  }
  swapstat(&a, &b);
  printf(1, "last space: %d/%d\n", a, b);
  exit();
}
