#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
  printf("empty parameter check failed at line %d\n", __LINE__); return 1; \
} } while (0)

int empty_int();
int empty_int() { return 42; }
signed char empty_char() { return -7; }
_Bool empty_bool() { return 1; }
float empty_float() { return 1.25f; }
double empty_double() { return 2.5; }

static int value = 17;
int *empty_pointer() { return &value; }
void empty_touch() { value++; }

int empty_then_void();
int empty_then_void(void) { return 8; }
int void_then_empty(void);
int void_then_empty() { return 9; }

// A call parsed before the prototype must use default argument promotions.
double later_args();
static double call_later() {
  float f = 1.25f;
  signed char c = -7;
  unsigned char u = 250;
  _Bool b = 1;
  return later_args(f, c, u, b);
}
double later_args(double f, int c, int u, int b) { return f + c + u + b; }

// A later unspecified declaration must retain the visible fixed prototype.
int retain_prototype(int);
int retain_prototype();
int retain_prototype(int n) { return n + 10; }

static int countdown() {
  static int left = 3;
  if (left-- == 0) return 0;
  return 1 + countdown();
}

typedef struct { double x; int n; } Small;
typedef struct { long x, y, z; } Large;
Small empty_small() { return (Small){1.5, 7}; }
Large empty_large() { return (Large){5000000000L, 8, 9}; }

int main() {
  CHECK(empty_int() == 42 && empty_char() == -7 && empty_bool());
  CHECK(empty_float() == 1.25f && empty_double() == 2.5);
  CHECK(*empty_pointer() == 17);
  void (*touch)() = empty_touch;
  touch();
  CHECK(*empty_pointer() == 18);
  CHECK(empty_then_void() == 8 && void_then_empty() == 9);
  CHECK(call_later() == 245.25 && retain_prototype(5) == 15);
  CHECK(countdown() == 3);

  int (*unspecified)() = empty_then_void;
  int (*fixed)(void) = empty_int;
  typedef int Empty();
  Empty *alias = fixed;
  CHECK(unspecified() == 8 && fixed() == 42 && alias() == 42);
  double (*promoted)() = later_args;
  CHECK(promoted(1.25f, (signed char)-7, (unsigned char)250, (_Bool)1) == 245.25);

  Small small = empty_small();
  Large large = empty_large();
  Large (*make_large)() = empty_large;
  CHECK(small.x == 1.5 && small.n == 7);
  CHECK(large.x == 5000000000L && large.y == 8 && large.z == 9);
  CHECK(make_large().x == 5000000000L);

  CHECK(__builtin_types_compatible_p(int (*)(), int (*)(void)));
  CHECK(__builtin_types_compatible_p(int (*)(), int (*)(int, double)));
  CHECK(!__builtin_types_compatible_p(int (*)(), int (*)(float)));
  CHECK(!__builtin_types_compatible_p(int (*)(), int (*)(char)));
  CHECK(!__builtin_types_compatible_p(int (*)(), int (*)(int, ...)));
  puts("empty parameters OK");
  return 0;
}
