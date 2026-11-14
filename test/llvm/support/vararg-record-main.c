#include "vararg-record-ppc.h"
int puts(const char *);
int main(void) {
  MixedRecordChoice base={7}; MixedRecordChoice.extra chosen={11,13};
  VrInt pair={3,5};
  if(mixed_record_total<&base>(4,pair,pair,pair,pair)!=87) return 1;
  if(mixed_record_total<&chosen>(8,pair,pair,pair,pair,pair,pair,pair,pair)!=312) return 2;
  puts("mixed record varargs OK"); return 0;
}
