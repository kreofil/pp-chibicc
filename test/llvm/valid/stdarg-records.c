#include "../../stdarg-records.h"
int puts(const char *);
int printf(const char *,...);
#define VR_FN(name) emitted_##name
#define VR_PEER(name) host_##name
#include "../support/stdarg-record-functions.h"
#define VR_CHECK(name) host_##name
#include "../support/stdarg-record-checks.h"
int main(void) {
  int result=check_record_varargs();
  if(!result) { result=host_checks_record_varargs(); if(result) result+=20; }
  if(result) { printf("record varargs ABI failure: %d\n",result); return result; }
  puts("record varargs ABI OK"); return 0;
}
