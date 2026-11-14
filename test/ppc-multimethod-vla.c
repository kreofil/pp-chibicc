#include <stdarg.h>
int puts(const char *s);
typedef struct VlaChoice {} enum { one } const VlaChoice;
long vla_sum<VlaChoice *a, VlaChoice *b>(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int *values = va_arg(ap, int *);
  long sum = 0;
  for (int i = 0; i < n; i++) sum += values[i];
  va_end(ap);
  return sum;
}
int main(void) {
  int n = 3;
  int values[n];
  values[0] = 41; values[1] = 42; values[2] = 43;
  VlaChoice a = {0};
  VlaChoice b = VlaChoice.one;
  if (vla_sum<&a, &b>(n, values) != 126) return 1;
  puts("multimethod VLA OK");
  return 0;
}
