#include <stdio.h>
#include "../support/floating-abi.h"

#define FP_PREFIX emitted
#include "../support/floating-abi-functions.h"
#define FP_CHECK emitted
#define FP_CALLEE host
#include "../support/floating-abi-checks.h"

int main(void) {
  if (!emitted_checks())
    return 1;
  if (host_float_value != 1.25f || host_double_value != 2.5 ||
      host_storage.f != 3.5f || host_storage.d != 4.25)
    return 2;
  host_float_value += 1.0f;
  host_double_value += 2.0;
  host_storage.f += 0.5f;
  host_storage.d += 0.75;
  if (!host_calls_floating())
    return 3;
  float first = 1.0f;
  float ninth = 9.0f;
  if (host_variadic(10, first, 2.0, 3.0f, 4.0, 5.0f,
                    6.0, 7.0f, 8.0, ninth, 10.0) != 385.0)
    return 4;
  printf("floating ABI OK: %.2f %.2f\n", host_float_value, host_double_value);
  return 0;
}
