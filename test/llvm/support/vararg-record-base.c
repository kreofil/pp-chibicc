#include "vararg-record-ppc.h"
long mixed_record_total<MixedRecordChoice *v>(int n,...) {
  va_list ap; va_start(ap,n); long sum=v->bias;
  for(int i=0;i<n;i++) { VrInt pair=va_arg(ap,VrInt); sum+=(i+1)*(pair.a+pair.b); }
  va_end(ap); return sum;
}
