#include "../../stdarg-abi.h"
#include <string.h>

long host_vararg_ints(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}
double host_vararg_doubles(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, double);
  va_end(ap);
  return sum;
}
int host_passes_list(int n, ...) {
  va_list ap, copy;
  va_start(ap, n);
  va_copy(copy, ap);
  double a = va_consume_doubles(ap, n);
  double b = va_consume_doubles(copy, n);
  va_end(copy);
  va_end(ap);
  return a == 650.0 && b == 650.0;
}
int host_checks_varargs(void) {
  if (va_sum_ints(12, VA_INTS) != 650 || va_sum_doubles(12, VA_DOUBLES) != 650.0 ||
      va_interleaved(12, VA_MIXED) != 1300.0 ||
      va_fixed_stack(VA_FIXED, 11, 12.5) != 1140.5) return 0;
  VaPrefix prefix = {7, 1.5};
  if (va_named_record(prefix, 2, 3, 4.25) != 17.75) return 0;
  VaLarge memory = {1, 2, 3};
  if (va_named_memory(memory, 8, 1, 2, 3, 4, 5, 6, 7, 8) != 218) return 0;
  VaSmall small = va_small_result(2, 5000000000L, 1.25);
  VaLarge large = va_large_result(3, 5000000000L, -8L, 9.0);
  if (small.n != 5000000000L || small.value != 3.25 ||
      large.a != 5000000000L || large.b != -8 || large.c != 12) return 0;
  int value = 42;
  va_void_result(3, &value);
  char text[128];
  return value == 45 && va_format(text, "%d %.2f", 7, 1.25) && !strcmp(text, "7 1.25");
}
