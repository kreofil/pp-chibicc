#include <stdio.h>

static volatile int status;

int sample(volatile int *address) {
  *address = 7;
  int first = *address;
  int second = *address;
  return first + second;
}

int main(void) {
  printf("volatile sum=%d\n", sample(&status));
  return 0;
}
