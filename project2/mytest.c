#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int pid1 = fork();
  int pid2;
  setnice(pid1, 5);
  if(pid1 != 0){
        pid2 = fork();
        setnice(pid2, 0);
  }
  if (pid1 == 0) {
    volatile int temp = 0;
    for(int j=0;j<10000;j++)
    {
      for (int i = 0; i < 100000; i++)
      temp++;
    }
    exit();
  }
  else if (pid2 == 0) {
    volatile int temp = 0;
    for(int j=0;j<10000;j++)
    {
      for (int i = 0; i < 100000; i++)
      temp++;
    }
    ps(0);
    exit();
  }
  wait();
  wait();
  exit();
}
