#include <stdarg.h>
#include "floating-abi.h"

#define FP_PREFIX host
#include "floating-abi-functions.h"
#define FP_CHECK host
#define FP_CALLEE emitted
#include "floating-abi-checks.h"

float host_float_value = 1.25f;
double host_double_value = 2.5;
FloatingStorage host_storage = {3.5f, 4.25};

double host_variadic(int count, ...) {
  va_list ap;
  va_start(ap, count);
  double result = 0.0;
  for (int i = 0; i < count; i++)
    result += (i + 1) * va_arg(ap, double);
  va_end(ap);
  return result;
}

int host_calls_floating(void) {
  return host_checks() && host_float_value == 2.25f && host_double_value == 4.5 &&
         host_storage.f == 4.0f && host_storage.d == 5.0;
}
