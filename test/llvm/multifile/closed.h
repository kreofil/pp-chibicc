#ifndef LLVM_PPC_CLOSED_H
#define LLVM_PPC_CLOSED_H
typedef struct MFClosedCase {} case { int first; double second; } const MFClosedCase;
typedef struct MFClosedChoice { int id; } enum { first, second } const MFClosedChoice;
typedef struct MFClosedPointer {} nullptr { int *first; double *second; } const MFClosedPointer;
typedef struct MFClosedUnion {} union { int first; double second; } const MFClosedUnion;
int closed_pick<MFClosedChoice *x>(void) = 0;
int closed_pair<MFClosedChoice *a, MFClosedUnion *b>(void);
int closed_opaque(struct MFClosedChoice *x);
int closed_sizes(void);
#endif
