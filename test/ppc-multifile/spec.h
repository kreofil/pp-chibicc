#ifndef PPC_MULTIFILE_SPEC_H
#define PPC_MULTIFILE_SPEC_H

#include "base.h"

typedef struct MFRect {
  int x;
  int y;
} MFRect;

struct MFBase + { MFRect rect; };

int MFValue<MFBase.rect *r>();

MFPointer + { MFRect *rect; };
int MFPointerValue<MFPointer.rect *r>();

#endif
