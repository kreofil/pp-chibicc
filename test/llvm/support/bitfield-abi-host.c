#include "bitfield-abi.h"
#include <stdarg.h>
#define BF_FN(name) host_##name
#include "bitfield-abi-functions.h"
#define BF_CHECK(name) emitted_##name
#include "bitfield-abi-checks.h"
int host_bitfield_calls(void) { return check_bitfield_calls(); }
long host_bitfield_variadic(int count, ...) {
  va_list ap; va_start(ap,count);
  int a=va_arg(ap,int);
  unsigned b=va_arg(ap,unsigned);
  unsigned long c=va_arg(ap,unsigned long);
  va_end(ap);
  return count+a+(b==0xffffffffU)+(c==0x1ffffffffUL);
}
