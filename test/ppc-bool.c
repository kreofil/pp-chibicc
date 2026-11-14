#include <stdarg.h>
int puts(const char *s);
int printf(const char *fmt, ...);
#define CHECK(expr) do { if (!(expr)) { printf("PPC bool failure: %d\n", __LINE__); return 1; } } while (0)

typedef struct BoolChoice { bool enabled; } case { bool flag; } const BoolChoice;
static BoolChoice global_choice = {512};

_Bool bool_pick<BoolChoice *choice>(_Bool gate);
bool bool_pick<BoolChoice *choice>(bool gate) { return gate && choice->enabled; }
bool bool_pick<BoolChoice.flag *choice>(bool gate) { return gate && choice->flag; }

bool bool_pair<BoolChoice *a, BoolChoice *b>(bool x, bool y) { return x && y; }
bool bool_pair<BoolChoice.flag *a, BoolChoice.flag *b>(bool x, bool y) { return x || y; }

typedef struct BoolResult { bool flag; int count; } BoolResult;
BoolResult bool_result<BoolChoice *choice>(bool value) {
  return (BoolResult){value, choice->enabled ? 7 : 3};
}

bool bool_variadic<BoolChoice *choice>(bool initial, int count, ...) {
  va_list ap;
  va_start(ap, count);
  bool result = initial;
  for (int i = 0; i < count; ++i) {
    int value = va_arg(ap, int);
    result = result || value;
  }
  va_end(ap);
  return result;
}

int main(void) {
  BoolChoice base = {true};
  BoolChoice.flag flag = {false, true};
  CHECK(bool_pick<&global_choice>(true));
  CHECK(bool_pick<&base>(true) && !bool_pick<&base>(false));
  CHECK(bool_pick<&flag>(true) && !bool_pick<&flag>(false));
  int calls = 0;
  CHECK(bool_pick<(calls++, &flag)>((bool)256) && calls == 1);
  CHECK((!bool_pair<&base, &flag>(true, false)));
  CHECK((bool_pair<&flag, &flag>(true, false)));
  BoolResult result = bool_result<&base>(false);
  CHECK(!result.flag && result.count == 7);
  CHECK(bool_variadic<&base>(false, 3, false, true, false));
  CHECK(!bool_variadic<&base>(false, 3, false, false, false));
  puts("PPC bool OK");
  return 0;
}
