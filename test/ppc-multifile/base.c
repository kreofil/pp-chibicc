#include "base.h"

int MFValue<MFBase *x>() {
  return x->seed;
}

int MFUse(MFBase *x) {
  return MFValue<x>();
}

int MFPointerValue<MFPointer *x>() { return 1000 + x->seed; }
int MFPointerValue<MFPointer.void *x>() { return x->seed; }
int MFPointerUse(MFPointer *x) { return MFPointerValue<x>(); }
int MFPointerCopy(MFPointer *dst, MFPointer *src) {
  *dst = *src;
  return MFPointerUse(dst);
}
