#ifndef PPC_CLOSED_DISPATCH_H
#define PPC_CLOSED_DISPATCH_H
typedef struct DispatchChoice { int value; } enum { first, second } const DispatchChoice;
int dispatch_pick<DispatchChoice *p>(int x) = 0;
#endif
