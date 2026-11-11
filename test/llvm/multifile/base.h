#ifndef LLVM_PPC_BASE_H
#define LLVM_PPC_BASE_H
typedef struct Figure { int id; } case {} Figure;
int perimeter<Figure *f>();
long variable_sum<Figure *f>(int n, ...);
long double extended<Figure *f>(long double extra);
int pair<Figure *first, Figure *second>(int extra);
typedef struct Report { long id, perimeter, count; } Report;
Report report<Figure *f>(Report previous);
typedef struct Dimensions { double width, height; } Dimensions;
typedef struct Measurement { long id; double area; } Measurement;
Measurement measure<Figure *f>(Dimensions extra);
#endif
