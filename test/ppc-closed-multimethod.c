#include "ppc-closed-multimethod.h"
#include <stdarg.h>
int puts(const char *s);
int printf(const char *fmt, ...);
#define CHECK(expr) do { if (!(expr)) { printf("closed multimethod failure: %d\n", __LINE__); return 1; } } while (0)
int multi_override_base;

int multi_early<MultiLeft *a, MultiRight *b>(int x) {
  return 100 + 10 * get_spec_var_index(a) + get_spec_var_index(b) + x;
}
int multi_early<MultiLeft.origin *a, MultiRight.void *b>(int x) { return 200 + x; }
int multi_early<MultiLeft.one *a, MultiRight.two *b>(int x) { return 500 + x; }

// All concrete definitions precede the default, which also follows a caller.
int multi_late<MultiLeft.origin *a, MultiRight.void *b>(int x) { return 200 + x; }
int multi_late<MultiLeft.one *a, MultiRight.two *b>(int x) { return 500 + x; }
int call_multi_late(MultiLeft *a, MultiRight *b) { return multi_late<a, b>(7); }
int multi_late<MultiLeft *a, MultiRight *b>(int x) {
  return 100 + 10 * get_spec_var_index(a) + get_spec_var_index(b) + x;
}

int multi_triple<MultiLeft *a, MultiRight *b, MultiPointer *c>(int x) {
  return 10 + 6 * get_spec_var_index(a) + 2 * get_spec_var_index(b) + get_spec_var_index(c) + x;
}
int multi_triple<MultiLeft.one *a, MultiRight.two *b, MultiPointer.one *c>(int x) { return 900 + x; }
int multi_union<MultiLeft *a, MultiUnion *b>(void) { return 50 + 2 * get_spec_var_index(a) + get_spec_var_index(b); }
int multi_union<MultiLeft.one *a, MultiUnion.one *b>(void) { return 80; }

typedef struct { long id; double value; } MultiSmall;
MultiSmall multi_small<MultiLeft *a, MultiRight *b, MultiUnion *c>(MultiSmall value) {
  value.value += 10;
  return value;
}
MultiSmall multi_small<MultiLeft.one *a, MultiRight.two *b, MultiUnion.one *c>(MultiSmall value) {
  value.value += 100;
  return value;
}
typedef struct { long sum, count, code; } MultiLarge;
static long weighted_multi_sum(int n, va_list ap) {
  long sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, int);
  return sum;
}
MultiLarge multi_varargs<MultiLeft *a, MultiRight *b, MultiPointer *c>(int n, ...) {
  va_list ap; va_start(ap, n);
  MultiLarge result = {weighted_multi_sum(n, ap) + 10, n, 10};
  va_end(ap); return result;
}
MultiLarge multi_varargs<MultiLeft.one *a, MultiRight.two *b, MultiPointer.one *c>(int n, ...) {
  va_list ap; va_start(ap, n);
  MultiLarge result = {weighted_multi_sum(n, ap) + 100, n, 100};
  va_end(ap); return result;
}

int multi_remote<MultiLeft.one *a, MultiRight.two *b>(int x) { return 700 + x; }
#ifndef MULTIMETHOD_EXTERNAL
int multi_remote<MultiLeft *a, MultiRight *b>(int x) { return 600 + x; }
int multi_opaque(MultiLeft *a, MultiRight *b) { return multi_remote<a, b>(7); }
#endif

// Mixed families and a 2*3*2*2 grid exceed the optimization's conditions.
typedef struct MultiOpen {} enum { one } MultiOpen;
int multi_open_left<MultiOpen *a, MultiLeft *b>(void) { return 7; }
int multi_open_right<MultiLeft *a, MultiOpen *b>(void) { return 8; }
int multi_large_grid<MultiLeft *a, MultiRight *b, MultiPointer *c, MultiUnion *d>(void) { return 9; }

int main(void) {
  for (int i = 0; i < 2; i++) {
    MultiLeft *a = get_spec_ptr(MultiLeft, i);
    for (int j = 0; j < 3; j++) {
      MultiRight *b = get_spec_ptr(MultiRight, j);
      int expected = !i && !j ? 207 : i == 1 && j == 2 ? 507 : 107 + i * 10 + j;
      CHECK((multi_early<a, b>(7) == expected));
      int late_expected = multi_override_base && i == 1 && j == 1 ? multi_override_base + 7 : expected;
      CHECK((multi_late<a, b>(7) == late_expected));
      CHECK(call_multi_late(a, b) == late_expected);
      CHECK((multi_remote<a, b>(7) == (i == 1 && j == 2 ? 707 : 607)));
      CHECK(multi_opaque(a, b) == (i == 1 && j == 2 ? 707 : 607));
      for (int k = 0; k < 2; k++) {
        MultiPointer *c = get_spec_ptr(MultiPointer, k);
        int triple_expected = i == 1 && j == 2 && k == 1 ? 905 : 15 + i * 6 + j * 2 + k;
        CHECK((multi_triple<a, b, c>(5) == triple_expected));
      }
    }
    for (int j = 0; j < 2; j++) {
      MultiUnion *b = get_spec_ptr(MultiUnion, j);
      CHECK((multi_union<a, b>() == (i && j ? 80 : 50 + 2 * i + j)));
    }
  }
  MultiLeft.one a;
  MultiRight b = MultiRight.two;
  MultiPointer.one c = 0;
  MultiUnion.one u = 1;
  MultiSmall small = {7, 1.25};
  MultiSmall result = multi_small<&a, &b, &u>(small);
  CHECK(result.id == 7 && result.value == 101.25 && small.value == 1.25);
  b = MultiRight.one;
  result = multi_small<&a, &b, &u>(small);
  CHECK(result.value == 11.25);
  b = MultiRight.two;
  MultiLarge many = multi_varargs<&a, &b, &c>(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
  CHECK(many.sum == 750 && many.count == 12 && many.code == 100);
  b = MultiRight.one;
  many = multi_varargs<&a, &b, &c>(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
  CHECK(many.sum == 660 && many.code == 10);
  int first = 0, second = 0, third = 0;
  CHECK((multi_triple<(first++, &a), (second++, &b), (third++, &c)>(3) == 22));
  CHECK(first == 1 && second == 1 && third == 1);
  MultiOpen.one open;
  CHECK((multi_open_left<&open, &a>() == 7 && multi_open_right<&a, &open>() == 8));
  CHECK((multi_large_grid<&a, &b, &c, &u>() == 9));
  puts("closed multimethod OK");
  return 0;
}
