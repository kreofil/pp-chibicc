#include "ppc-closed-dispatch.h"
#include <stdarg.h>
int printf(const char *fmt, ...);
int puts(const char *s);
#define CHECK(expr) do { if (!(expr)) { printf("closed dispatch failure: %d\n", __LINE__); return 1; } } while (0)

int dispatch_second_base = 30;
int dispatch_pick<DispatchChoice.void *p>(int x) { return x + 10; }
int dispatch_pick<DispatchChoice.first *p>(int x) { return x + 20; }
int dispatch_pick<DispatchChoice.second *p>(int x) { return x + 30; }

// The call appears before the concrete handler definitions.
int dispatch_late<DispatchChoice *p>(void) = 0;
int call_late(DispatchChoice *p) { return dispatch_late<p>(); }
int dispatch_late<DispatchChoice.void *p>(void) { return 100; }
int dispatch_late<DispatchChoice.first *p>(void) { return 101; }
int dispatch_late<DispatchChoice.second *p>(void) { return 102; }

void dispatch_void<DispatchChoice *p>(int *result) = 0;
void dispatch_void<DispatchChoice.void *p>(int *result) { *result += 10; }
void dispatch_void<DispatchChoice.first *p>(int *result) { *result += 20; }
void dispatch_void<DispatchChoice.second *p>(int *result) { *result += 30; }

typedef struct { long id; double value; } DispatchSmall;
typedef struct { long first, second, third; } DispatchLarge;
DispatchSmall dispatch_small<DispatchChoice *p>(DispatchSmall previous) = 0;
DispatchSmall dispatch_small<DispatchChoice.void *p>(DispatchSmall previous) { previous.value += 10; return previous; }
DispatchSmall dispatch_small<DispatchChoice.first *p>(DispatchSmall previous) { previous.value += 20; return previous; }
DispatchSmall dispatch_small<DispatchChoice.second *p>(DispatchSmall previous) { previous.value += 30; return previous; }
DispatchLarge dispatch_large<DispatchChoice *p>(DispatchLarge previous) = 0;
DispatchLarge dispatch_large<DispatchChoice.void *p>(DispatchLarge previous) { previous.third += 10; return previous; }
DispatchLarge dispatch_large<DispatchChoice.first *p>(DispatchLarge previous) { previous.third += 20; return previous; }
DispatchLarge dispatch_large<DispatchChoice.second *p>(DispatchLarge previous) { previous.third += 30; return previous; }

static double consume_dispatch(int n, va_list ap) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += (i + 1) * va_arg(ap, double);
  return sum;
}
double dispatch_variadic<DispatchChoice *p>(int n, ...) = 0;
double dispatch_variadic<DispatchChoice.void *p>(int n, ...) {
  va_list ap; va_start(ap, n); double sum = consume_dispatch(n, ap); va_end(ap); return sum + 10;
}
double dispatch_variadic<DispatchChoice.first *p>(int n, ...) {
  va_list ap; va_start(ap, n); double sum = consume_dispatch(n, ap); va_end(ap); return sum + 20;
}
double dispatch_variadic<DispatchChoice.second *p>(int n, ...) {
  va_list ap; va_start(ap, n); double sum = consume_dispatch(n, ap); va_end(ap); return sum + 30;
}
static const char *consume_string(int n, va_list ap) { return va_arg(ap, const char *); }
static int consume_callback(int n, va_list ap) {
  int (*callback)(int) = va_arg(ap, int (*)(int));
  return callback(n);
}
#define VA_HANDLERS(result, name, consume) \
  result name<DispatchChoice *p>(int n, ...) = 0; \
  result name<DispatchChoice.void *p>(int n, ...) { \
    va_list ap; va_start(ap, n); result value = consume(n, ap); va_end(ap); return value; } \
  result name<DispatchChoice.first *p>(int n, ...) { \
    va_list ap; va_start(ap, n); result value = consume(n, ap); va_end(ap); return value; } \
  result name<DispatchChoice.second *p>(int n, ...) { \
    va_list ap; va_start(ap, n); result value = consume(n, ap); va_end(ap); return value; }
VA_HANDLERS(const char *, dispatch_string, consume_string)
VA_HANDLERS(int, dispatch_callback, consume_callback)
static int twice(int n) { return 2 * n; }

long double dispatch_wide<DispatchChoice *p>(long double x) = 0;
long double dispatch_wide<DispatchChoice.void *p>(long double x) { return x + 10; }
long double dispatch_wide<DispatchChoice.first *p>(long double x) { return x + 20; }
long double dispatch_wide<DispatchChoice.second *p>(long double x) { return x + 30; }

int dispatch_transition<DispatchChoice *p>(void) = 0;
int dispatch_transition<DispatchChoice.void *p>(void) { return 10; }
int dispatch_transition<DispatchChoice.first *p>(void) { *(DispatchChoice *)p = DispatchChoice.second; return 20; }
int dispatch_transition<DispatchChoice.second *p>(void) { *(DispatchChoice *)p = DispatchChoice.first; return 30; }

// These families must retain their runtime path.
int dispatch_incomplete<DispatchChoice *p>(int x) { return x + 100; }
int dispatch_incomplete<DispatchChoice.first *p>(int x) { return x + 101; }
typedef struct DispatchOpen {} enum { first } DispatchOpen;
int dispatch_mixed<DispatchChoice *p, DispatchOpen *q>(void) { return 7; }
int dispatch_pair<DispatchChoice *p, DispatchChoice *q>(void) { return 8; }

// The representation and eligibility test are shared by all four forms.
typedef struct DispatchCase {} case { int first; int second; } const DispatchCase;
typedef struct DispatchPointer {} nullptr { int *first; int *second; } const DispatchPointer;
typedef struct DispatchUnion {} union { int first; int second; } const DispatchUnion;
#define TRIPLE(name, T) \
  int name<T *p>(void) = 0; \
  int name<T.void *p>(void) { return 0; } \
  int name<T.first *p>(void) { return 1; } \
  int name<T.second *p>(void) { return 2; }
TRIPLE(dispatch_case, DispatchCase)
TRIPLE(dispatch_pointer, DispatchPointer)
TRIPLE(dispatch_union, DispatchUnion)

int main(void) {
  DispatchChoice zero = {0};
  DispatchChoice first = DispatchChoice.first;
  DispatchChoice second = DispatchChoice.second;
  CHECK(dispatch_pick<&zero>(2) == 12 && dispatch_pick<&first>(3) == 23);
  CHECK(dispatch_pick<&second>(4) == dispatch_second_base + 4);
  CHECK(dispatch_pick<&second>(dispatch_pick<&first>(1)) == dispatch_second_base + 21);
  CHECK(call_late(&zero) == 100 && call_late(&first) == 101 && call_late(&second) == 102);
  int sum = 0;
  dispatch_void<&zero>(&sum); dispatch_void<&first>(&sum); dispatch_void<&second>(&sum);
  CHECK(sum == 60);
  DispatchSmall small = {7, 1.25};
  DispatchSmall small_result = dispatch_small<&first>(small);
  CHECK(small_result.id == 7 && small_result.value == 21.25 && small.value == 1.25);
  DispatchLarge large = {1, 2, 3};
  DispatchLarge large_result = dispatch_large<&second>(large);
  CHECK(large_result.first == 1 && large_result.second == 2 && large_result.third == 33);
  CHECK(large.third == 3);
  DispatchLarge nested = dispatch_large<&second>(dispatch_large<&first>(large));
  CHECK(nested.third == 53 && large.third == 3);
  CHECK(dispatch_variadic<&first>(10, 1.0, 2.0, 3.0, 4.0, 5.0,
                                6.0, 7.0, 8.0, 9.0, 10.0) == 405.0);
  CHECK(dispatch_string<&first>(1, "abc")[2] == 'c');
  CHECK(dispatch_callback<&second>(7, twice) == 14);
  CHECK(dispatch_wide<&second>(1.25L) == 31.25L);
  DispatchChoice changing = DispatchChoice.first;
  CHECK(dispatch_transition<&changing>() == 20 && get_spec_var_index(&changing) == 2);
  CHECK(dispatch_transition<&changing>() == 30 && get_spec_var_index(&changing) == 1);
  int effects = 0;
  CHECK(dispatch_pick<(effects++, &second)>(effects++) == dispatch_second_base + 1);
  CHECK(effects == 2);
  CHECK(dispatch_incomplete<&zero>(1) == 101 && dispatch_incomplete<&first>(1) == 102);
  DispatchOpen.first open;
  CHECK((dispatch_mixed<&first, &open>() == 7 && dispatch_pair<&first, &second>() == 8));
  DispatchCase.second c = 7;
  DispatchPointer.first p = 0;
  DispatchUnion.second u = 8;
  CHECK(dispatch_case<&c>() == 2 && dispatch_pointer<&p>() == 1 && dispatch_union<&u>() == 2);
  puts("closed dispatch OK");
  return 0;
}
