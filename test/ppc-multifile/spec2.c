#include "spec2.h"

int MFValue<MFBase.trian *t>() {
  return t->trian.a * 100 + t->trian.b * 10 + t->trian.c;
}

int MFPointerValue<MFPointer.trian *t>() {
  return t->seed + t->trian->a * 100 + t->trian->b * 10 + t->trian->c;
}

int MFEnumValue<MFEnum.MFThursday *x>() { return 20 + x->seed; }
void MFEnumSetThursday(MFEnum *x) { *x = MFEnum.MFThursday; }
