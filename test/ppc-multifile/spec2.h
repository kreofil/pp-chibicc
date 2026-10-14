#ifndef PPC_MULTIFILE_SPEC2_H
#define PPC_MULTIFILE_SPEC2_H

#include "base.h"

typedef struct MFTri {
  int a;
  int b;
  int c;
} MFTri;

struct MFBase + { MFTri trian; };

int MFValue<MFBase.trian *t>();

MFPointer + { MFTri *trian; };
int MFPointerValue<MFPointer.trian *t>();

#endif
