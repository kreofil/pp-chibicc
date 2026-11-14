#include "../support/bitfield-abi.h"
int puts(const char *);
int printf(const char *, ...);
#define BF_FN(name) emitted_##name
#include "../support/bitfield-abi-functions.h"
#define BF_CHECK(name) host_##name
#include "../support/bitfield-abi-checks.h"
int main(void) {
  int result=check_bitfield_calls();
  if (!result) { result=host_bitfield_calls(); if (result) result+=20; }
  if (result) { printf("bitfield ABI failure: %d\n",result); return result; }
  struct { unsigned long a:31,b:32,c:33; } args={7,0xffffffffU,0x1ffffffffUL};
  if (host_bitfield_variadic(3,args.a,args.b,args.c)!=12) return 50;
  puts("bitfield ABI OK");
  return 0;
}
