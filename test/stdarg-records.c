#include "stdarg-records.h"
int puts(const char *);
int printf(const char *,...);
#define VR_NATIVE_SAFE
#define VR_FN(name) native_##name
#define VR_PEER(name) native_##name
#include "llvm/support/stdarg-record-functions.h"
#define VR_CHECK(name) native_##name
#include "llvm/support/stdarg-record-checks.h"
int main(void) {
  int result=check_record_varargs();
  if(result) { printf("record varargs failure: %d\n",result); return result; }
  puts("record varargs OK"); return 0;
}
