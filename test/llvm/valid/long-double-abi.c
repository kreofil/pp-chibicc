#include "../support/long-double-abi.h"
#include <stdio.h>
#define LD_PREFIX emitted
#define LD_OTHER host
#include "../support/long-double-abi-functions.h"
#define LD_CHECKER emitted
#define LD_CALLEE host
#include "../support/long-double-abi-checks.h"
long double host_unprototyped();
int main(void) {
  int result = emitted_checks();
  if (result) { printf("host long double ABI failure: %d\n", result); return 1; }
  result = host_calls_long_double();
  if (result) { printf("emitted long double ABI failure: %d\n", result); return 2; }
  if (host_ld_value - 1.0L != 0x1p-63L) return 3;
  host_ld_value += 0.5L;
  if (host_unprototyped(1.25L, 2.5f, (short)3) != 6.75L) return 4;
  long double (*unspecified)() = host_unprototyped;
  if (unspecified(1.25L, 2.5f, (char)3) != 6.75L) return 5;
  puts("long double ABI OK");
  return 0;
}
