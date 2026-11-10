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
  puts("record ABI OK");
  return 0;
}
