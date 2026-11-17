#include "../support/record-abi.h"
int puts(const char *s);
#define ABI_FN(name) emitted_##name
#include "../support/record-abi-functions.h"
#define CHECK_FN(name) host_##name
#include "../support/record-abi-checks.h"

int main(void) {
  int result = check_record_calls();
  if (result)
    return result;
  result = host_calls_emitted();
  if (result)
    return 20 + result;
  Pair pair = {7, 11};
  if (host_fixed_variadic(pair, 4, (signed char)-7, (unsigned short)65000,
                          5000000000L, 99) != 5000065114L)
    return 40;
  Pair nested = host_pair(emitted_pair(pair));
  if (nested.a != 13 || nested.b != 7)
    return 41;
  Large value = {1, 2, 3};
  // Call-site argument/result buffers must be reusable across loop iterations.
  for (int i = 0; i < 1000000; i++)
    value = emitted_large(value, 0);
  if (value.a != 1 || value.b != 2 || value.c != 3)
    return 42;
  FloatPair float_pair = {1.5f, 2.5f};
  IntDouble mixed = {7, 11};
  if (host_real_variadic(float_pair, mixed, 10, 1.0f, 2.0, 3.0f, 4.0,
                         5.0f, 6.0, 7.0f, 8.0, 9.0f, 10.0) != 456.5)
    return 43;
  DoublePair double_pair = {7, 11};
  DoublePair real_nested = host_double_pair(emitted_double_pair(double_pair));
  if (real_nested.x != 28.5 || real_nested.y != 10.0)
    return 44;
  puts("record ABI OK");
  return 0;
}
