#ifndef PPC_MULTIFILE_BASE_H
#define PPC_MULTIFILE_BASE_H

typedef struct MFBase { int seed; } case {} MFBase;

int MFValue<MFBase *x>();
int MFUse(MFBase *x);

#endif
