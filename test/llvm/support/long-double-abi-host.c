#include "long-double-abi.h"
#define LD_PREFIX host
#define LD_OTHER emitted
#include "long-double-abi-functions.h"
#define LD_CHECKER host
#define LD_CALLEE emitted
#include "long-double-abi-checks.h"
long double host_ld_value = 0x1.0000000000000002p0L;
long double host_unprototyped(long double x, double y, int z) { return x + y + z; }
int host_calls_long_double(void) { return host_checks(); }
