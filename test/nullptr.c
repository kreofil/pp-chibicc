#include "test.h"
#include <stddef.h>
#include <stdarg.h>

#define IS_NULLPTR(x) _Generic((x), nullptr_t: 1, default: 0)

static nullptr_t global_null;
static nullptr_t global_init = nullptr;
static nullptr_t global_zero = 0;
static nullptr_t global_void = (void *)0;
static int *global_ptr = nullptr;
static int (*global_fn)(void) = nullptr;
static _Bool global_bool = nullptr;
static struct { int *p; nullptr_t n; } global_struct = {nullptr, nullptr};

static nullptr_t identity(nullptr_t n) { return n; }
static nullptr_t return_null(void) { return nullptr; }
static nullptr_t return_zero(void) { return 0; }
static int *return_pointer(void) { return nullptr; }
static int takes_pointer(int *p) { return p == nullptr; }
static int takes_function(int (*p)(void)) { return p == nullptr; }
static int takes_bool(_Bool b) { return b; }
static int function(void) { return 42; }

static int varargs(int count, ...) {
  va_list ap;
  va_start(ap, count);
  int ok = 1;
  // Read nullptr_t in both registers and stack slots, interleaved with
  // floating arguments, then read it through the permitted pointer types.
  for (int i = 0; i < count; i++) {
    ok = ok && va_arg(ap, nullptr_t) == nullptr;
    ok = ok && va_arg(ap, double) == i + 0.5;
  }
  ok = ok && va_arg(ap, void *) == nullptr;
  ok = ok && va_arg(ap, char *) == nullptr;
  va_end(ap);
  return ok;
}

int main(void) {
  ASSERT(sizeof(void *), sizeof(nullptr));
  ASSERT(sizeof(char *), sizeof(nullptr_t));
  ASSERT(_Alignof(char *), _Alignof(nullptr_t));
  ASSERT(1, IS_NULLPTR(nullptr));
  ASSERT(1, __builtin_types_compatible_p(typeof(nullptr), nullptr_t));
  ASSERT(0, __builtin_types_compatible_p(nullptr_t, void *));
  ASSERT(0, __builtin_types_compatible_p(nullptr_t, long));

  ASSERT(1, global_null == nullptr);
  ASSERT(1, global_init == nullptr);
  ASSERT(1, global_zero == nullptr);
  ASSERT(1, global_void == nullptr);
  ASSERT(1, global_ptr == nullptr);
  ASSERT(1, global_fn == nullptr);
  ASSERT(0, global_bool);
  ASSERT(1, global_struct.p == nullptr && global_struct.n == nullptr);

  nullptr_t n = nullptr;
  nullptr_t zero = 0;
  nullptr_t empty = {};
  nullptr_t brace = {nullptr};
  nullptr_t values[3] = {nullptr, 0, (void *)0};
  ASSERT(1, IS_NULLPTR(n));
  ASSERT(1, IS_NULLPTR(*&n));
  ASSERT(1, n == zero && empty == brace);
  ASSERT(1, values[0] == values[1] && values[1] == values[2]);
  ASSERT(1, identity(n) == nullptr);
  ASSERT(1, return_null() == nullptr);
  ASSERT(1, return_zero() == nullptr);
  ASSERT(1, return_pointer() == nullptr);

  enum { ZERO = 0 };
  n = zero;
  n = ZERO;
  n = 2 - 2;
  n = (int)0.0;
  n = (_Bool)0.0;
  n = 0 && (1 / 0);
  n = 1 ? 0 : (1 / 0);
  n = sizeof(int) - sizeof(int);
  n = (void *)0;
  n = (nullptr_t)0;
  n = (nullptr_t)(void *)0;
  ASSERT(1, n == nullptr);
  ASSERT(1, nullptr == 0);
  ASSERT(1, 0L == nullptr);
  ASSERT(1, n == ZERO);
  ASSERT(1, nullptr == (void *)0);
  ASSERT(0, nullptr != n);

  int x = 42;
  int *p = nullptr;
  void *v = n;
  int (*fp)(void) = n;
  ASSERT(1, p == n && v == n && fp == n);
  p = &x;
  ASSERT(0, p == nullptr);
  ASSERT(1, nullptr != p);
  ASSERT(1, (void *)0x100000000UL != nullptr);
  ASSERT(1, nullptr != (void *)0x100000000UL);
  p = n;
  ASSERT(1, p == nullptr);
  ASSERT(1, takes_pointer(nullptr));
  ASSERT(1, takes_pointer(n));
  ASSERT(1, takes_function(nullptr));
  ASSERT(1, takes_function(n));
  ASSERT(1, (int *)n == nullptr);
  ASSERT(1, (int (*)(void))nullptr == nullptr);

  ASSERT(1, IS_NULLPTR(1 ? n : nullptr));
  ASSERT(1, _Generic((0 ? nullptr : &x), int *: 1, default: 0));
  ASSERT(1, _Generic((1 ? &x : nullptr), int *: 1, default: 0));
  ASSERT(42, *(0 ? nullptr : &x));
  ASSERT(42, *(1 ? &x : nullptr));
  ASSERT(1, (1 ? nullptr : &x) == nullptr);
  ASSERT(1, (0 ? &x : nullptr) == nullptr);
  int array[2] = {7, 8};
  ASSERT(7, *(0 ? nullptr : array));
  ASSERT(8, (1 ? array : nullptr)[1]);
  ASSERT(42, (0 ? nullptr : function)());
  ASSERT(42, (1 ? function : nullptr)());

  _Bool b = n;
  ASSERT(0, b);
  ASSERT(0, (_Bool)nullptr);
  ASSERT(0, takes_bool(nullptr));
  ASSERT(1, !nullptr);
  ASSERT(0, nullptr && 1);
  ASSERT(1, nullptr || 1);
  ASSERT(2, nullptr ? 1 : 2);
  int count = 0;
  if (n) count++;
  while (nullptr) count++;
  for (; n;) count++;
  do { count++; } while (n);
  ASSERT(1, count);
  ASSERT(0, nullptr && ++count);
  ASSERT(1, count);
  ASSERT(1, nullptr || ++count);
  ASSERT(2, count);
  (void)nullptr;
  (void)n;
  ASSERT(1, (count++, n) == nullptr);
  ASSERT(3, count);
  ASSERT(1, takes_pointer((count++, n)));
  ASSERT(4, count);
  ASSERT(1, (1 ? (count++, n) : nullptr) == nullptr);
  ASSERT(5, count);

  ASSERT(1, varargs(7, nullptr, 0.5, n, 1.5, nullptr, 2.5, n, 3.5,
                    nullptr, 4.5, n, 5.5, nullptr, 6.5, nullptr, n));
  printf("OK\n");
  return 0;
}
