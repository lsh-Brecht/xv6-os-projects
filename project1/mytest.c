#include "types.h"
#include "user.h"
#include "stat.h"

int main()
{
  printf(1, "//ps(0)\n");
  ps(0);
    int i;
  printf(1, "\n//for getnice(1~4))\n");
  for (i = 1; i < 5; i++) {
    printf(1, "%d: ", i);
    int temp = getnice(i);
    if (temp == -1)
      printf(1, "wrong pid\n");
    else
      printf(1, "%d\n", temp);
  }
  
  printf(1, "\n//setnice(1,11);setnice(2,22);setnice(3,33);\n");
  printf(1, "//setnice(4,44);\n");
  int arr[5] = {0, 11, 22, 33, 44};
  int j =1;
  for (i = 1; i < 5; i++) {
    printf(1, "%d: ", i);
    int temp = setnice(i, arr[j++]);
    if (temp == -1)
      printf(1, "wrong pid or nice\n");
    else
      printf(1, "return value is %d(0:success)\n", temp);
  }
  
  printf(1, "\n//ps(1)\n");
  ps(1);
  printf(1, "\n//ps(2)\n");
  ps(2);
  exit();
}
