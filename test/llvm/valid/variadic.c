#include <stdio.h>

int main(void) {
  signed char c = -7;
  unsigned short s = 65000;
  _Bool b = 1;
  int (*printer)(const char *, ...) = printf;
  printer("varargs %d %d %d %ld %s\n", c, s, b, 5000000000L, "OK");
  return 0;
}
