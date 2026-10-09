#include "base.h"

int MFValue<MFBase *x>() {
  return x->seed;
}

int MFUse(MFBase *x) {
  return MFValue<x>();
}
