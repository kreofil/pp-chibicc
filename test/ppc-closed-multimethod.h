#ifndef PPC_CLOSED_MULTIMETHOD_H
#define PPC_CLOSED_MULTIMETHOD_H
typedef struct MultiLeft {} case { default void origin; void one; } const MultiLeft;
typedef struct MultiRight {} enum { one, two } const MultiRight;
typedef struct MultiPointer {} nullptr { int *one; } const MultiPointer;
typedef struct MultiUnion {} union { int one; } const MultiUnion;
int multi_remote<MultiLeft *a, MultiRight *b>(int x) = 0;
int multi_opaque(MultiLeft *a, MultiRight *b);
#endif
