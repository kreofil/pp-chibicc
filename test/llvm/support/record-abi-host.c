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
