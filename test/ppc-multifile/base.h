#ifndef PPC_MULTIFILE_BASE_H
#define PPC_MULTIFILE_BASE_H

typedef struct MFBase { int seed; } case {} MFBase;

int MFValue<MFBase *x>();
int MFUse(MFBase *x);

typedef struct MFPointer { int seed; } nullptr {} MFPointer;
int MFPointerValue<MFPointer *x>();
int MFPointerUse(MFPointer *x);
int MFPointerCopy(MFPointer *dst, MFPointer *src);

typedef struct MFEnum { int seed; } enum {} MFEnum;
int MFEnumValue<MFEnum *x>();
void MFEnumSetMonday(MFEnum *x);
void MFEnumSetThursday(MFEnum *x);

typedef struct MFUnion { int seed; } union sizeof(32) {} MFUnion;
int MFUnionValue<MFUnion *x>();
int MFUnionUse(MFUnion *x);
int MFUnionCopy(MFUnion *dst, MFUnion *src);

#endif
