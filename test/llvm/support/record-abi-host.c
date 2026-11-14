#include "record-abi.h"
#include <stdarg.h>
#define ABI_FN(name) host_##name
#include "record-abi-functions.h"
#define CHECK_FN(name) emitted_##name
#include "record-abi-checks.h"

int host_calls_emitted(void) { return check_record_calls(); }

long host_fixed_variadic(Pair pair, int count, ...) {
  va_list ap;
  va_start(ap, count);
  int a = va_arg(ap, int);
  int b = va_arg(ap, int);
  long c = va_arg(ap, long);
  int d = va_arg(ap, int);
  va_end(ap);
  return pair.a + pair.b + a + b + c + d + count;
}

double host_real_variadic(FloatPair pair, IntDouble mixed, int count, ...) {
  va_list ap;
  va_start(ap, count);
  double result = pair.x + 2*pair.y + 3*mixed.tag + 4*mixed.value;
  for (int i = 0; i < count; i++)
    result += (i + 1) * va_arg(ap, double);
  va_end(ap);
  return result;
}
