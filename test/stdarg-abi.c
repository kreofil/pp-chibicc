#include <stdio.h>
#include <string.h>
#include "stdarg-abi.h"

#define CHECK(expr) do { if (!(expr)) { \
  printf("stdarg check failed at line %d\n", __LINE__); return 1; \
} } while (0)

long va_sum_ints(int n, ...) {
  if (n < 0) return 0;
  va_list ap;
  va_start(ap, n);
  long sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}

double va_consume_doubles(va_list ap, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, double);
  return sum;
}

double va_sum_doubles(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double sum = va_consume_doubles(ap, n);
  va_end(ap);
  return sum;
}

double va_interleaved(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double sum = 0;
  for (int i = 0; i < n; i++) {
    int integer = va_arg(ap, int);
    double real = va_arg(ap, double);
    sum += (i + 1) * (integer + real);
  }
  va_end(ap);
  return sum;
}

double va_fixed_stack(int a, int b, int c, int d, int e, int f, int g,
                      double x0, double x1, double x2, double x3, double x4,
                      double x5, double x6, double x7, double x8, int marker, ...) {
  va_list ap;
  va_start(ap, marker);
  int integer = va_arg(ap, int);
  double real = va_arg(ap, double);
  va_end(ap);
  return a + g + x0 + x8 + marker + integer * 100 + real;
}

double va_named_record(VaPrefix prefix, int n, ...) {
  va_list ap;
  va_start(ap, n);
  int integer = va_arg(ap, int);
  double real = va_arg(ap, double);
  va_end(ap);
  return prefix.id + prefix.factor + integer + real + n;
}

long va_named_memory(VaLarge prefix, int n, ...) {
  va_list ap;
  va_start(ap, n);
  long sum = prefix.a + prefix.b + prefix.c + n;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  va_end(ap);
  return sum;
}

VaSmall va_small_result(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long integer = va_arg(ap, long);
  double real = va_arg(ap, double);
  va_end(ap);
  return (VaSmall){integer, real + n};
}

VaLarge va_large_result(int n, ...) {
  va_list ap;
  va_start(ap, n);
  long a = va_arg(ap, long);
  long b = va_arg(ap, long);
  double c = va_arg(ap, double);
  va_end(ap);
  return (VaLarge){a, b, (long)c + n};
}

void va_void_result(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int *p = va_arg(ap, int *);
  *p += n;
  va_end(ap);
}

int va_format(char *buf, const char *text, ...) {
  va_list ap, copy;
  va_start(ap, text);
  va_copy(copy, ap);
  char other[128];
  int n = vsnprintf(buf, 128, text, ap);
  int m = vsnprintf(other, 128, text, copy);
  va_end(copy);
  va_end(ap);
  return n == m && !strcmp(buf, other);
}

static int copied_tail(int n, ...) {
  va_list ap, copy, restart;
  va_start(ap, n);
  for (int i = 0; i < 8; i++)
    if (va_arg(ap, int) != i + 1) return 0;
  va_copy(copy, ap);
  for (int i = 8; i < n; i++) {
    if (va_arg(ap, int) != i + 1) return 0;
    if (va_arg(copy, int) != i + 1) return 0;
  }
  va_start(restart, n);
  int first = va_arg(restart, int);
  va_list lists[2];
  va_start(lists[0], n);
  va_copy(lists[1], lists[0]);
  int index = 0;
  int once = va_arg(lists[index++], int);
  va_end(lists[1]);
  va_end(lists[0]);
  va_end(restart);
  va_end(copy);
  va_end(ap);
  return first == 1 && once == 1 && index == 1;
}

static int scalar_promotions(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int a = va_arg(ap, int), b = va_arg(ap, int);
  int c = va_arg(ap, int), d = va_arg(ap, int);
  double e = va_arg(ap, double);
  int *p = va_arg(ap, int *);
  unsigned long wide = va_arg(ap, unsigned long);
  long negative = va_arg(ap, long);
  va_end(ap);
  return a == -7 && b == 250 && c == 65000 && d == 1 && e == 1.25 &&
         *p == n && wide == 0xffffffff00001234UL && negative == -5000000000L;
}

static int pointer_stack(int *base, int n, ...) {
  va_list ap;
  va_start(ap, n);
  for (int i = 0; i < n; i++)
    if (va_arg(ap, int *) != base + i) return 0;
  va_end(ap);
  return 1;
}

static double echo_double(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double value = va_arg(ap, double);
  va_end(ap);
  return value;
}

int main() {
  CHECK(sizeof(va_list) == 24);
  CHECK(va_sum_ints(0) == 0 && va_sum_ints(-1) == 0);
  CHECK(va_sum_ints(12, VA_INTS) == 650);
  CHECK(va_sum_doubles(12, VA_DOUBLES) == 650.0);
  CHECK(va_interleaved(12, VA_MIXED) == 1300.0);
  CHECK(va_fixed_stack(VA_FIXED, 11, 12.5) == 1140.5);
  VaPrefix prefix = {7, 1.5};
  CHECK(va_named_record(prefix, 2, 3, 4.25) == 17.75);
  VaLarge memory = {1, 2, 3};
  CHECK(va_named_memory(memory, 8, 1, 2, 3, 4, 5, 6, 7, 8) == 218);
  VaSmall small = va_small_result(2, 5000000000L, 1.25);
  CHECK(small.n == 5000000000L && small.value == 3.25);
  VaLarge large = va_large_result(3, 5000000000L, -8L, 9.0);
  CHECK(large.a == 5000000000L && large.b == -8 && large.c == 12);
  int value = 42;
  va_void_result(3, &value);
  CHECK(value == 45);
  CHECK(copied_tail(12, VA_INTS));
  CHECK(scalar_promotions(value, (signed char)-7, (unsigned char)250,
                         (unsigned short)65000, (_Bool)1, 1.25f, &value,
                         0xffffffff00001234UL, -5000000000L));
  int array[8];
  CHECK(pointer_stack(array, 8, array, array+1, array+2, array+3,
                      array+4, array+5, array+6, array+7));
  long (*indirect)(int, ...) = va_sum_ints;
  CHECK(indirect(12, VA_INTS) == 650);
  char text[128];
  CHECK(va_format(text, "%d %.2f %.2f %s", 7, 1.25, 2.5, "ok"));
  CHECK(!strcmp(text, "7 1.25 2.50 ok"));
  union { unsigned long bits; double value; } real;
  real.value = echo_double(0, -0.0);
  CHECK(real.bits == 0x8000000000000000UL);
  real.bits = 0x7ff8000000001234UL;
  real.value = echo_double(0, real.value);
  CHECK(real.bits == 0x7ff8000000001234UL);
  puts("stdarg ABI OK");
  return 0;
}
