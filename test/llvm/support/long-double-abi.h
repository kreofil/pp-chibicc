#ifndef LLVM_LONG_DOUBLE_ABI_H
#define LLVM_LONG_DOUBLE_ABI_H
#include <stdarg.h>
typedef struct { long double x; } LDBox;
typedef struct { struct { long double items[1]; } inner; } LDNested;
typedef union { long double x, y; } LDX87;
typedef union { long double x; long i; } LDMixed;
typedef union { long double x; double d; } LDFloatUnion;
typedef union { long double x; unsigned long words[2]; } LDIntegerUnion;
typedef struct { long double items[2]; long tag; } LDLarge;
typedef struct __attribute__((packed)) { char tag; long double x; } LDPacked;
typedef struct { double a, b; } LDFpPair;
typedef struct { long a, b; } LDIntPair;
#define LD_DECLARE(p) \
long double p##_scalar(long double x, int bias, double scale); \
long double p##_sse_budget(long double x, double a, double b, double c, double d, double e, double f, LDFpPair pair, double tail); \
long double p##_gp_budget(long double x, long a, long b, long c, long d, LDIntPair pair, long tail); \
long double p##_box_budget(LDBox x, double a, double b, double c, double d, double e, double f, LDFpPair pair); \
LDBox p##_box(LDBox x); \
LDNested p##_nested(LDNested x); \
LDX87 p##_x87(LDX87 x); \
LDMixed p##_mixed(LDMixed x); \
LDFloatUnion p##_float_union(LDFloatUnion x); \
LDIntegerUnion p##_integer_union(LDIntegerUnion x); \
LDLarge p##_large(LDLarge x); \
LDPacked p##_packed(LDPacked x); \
long double p##_consume(int n, va_list ap); \
long double p##_variadic(long double prefix, int n, ...); \
long double p##_forward(int n, ...); \
LDBox p##_variadic_box(int n, ...); \
LDLarge p##_variadic_large(int n, ...);
LD_DECLARE(host)
LD_DECLARE(emitted)
extern long double host_ld_value;
int host_calls_long_double(void);
#define LD_TRIPLES \
  1, 1.25, 1.5L, 2, 2.25, 2.5L, 3, 3.25, 3.5L, 4, 4.25, 4.5L, \
  5, 5.25, 5.5L, 6, 6.25, 6.5L, 7, 7.25, 7.5L, 8, 8.25, 8.5L, \
  9, 9.25, 9.5L, 10, 10.25, 10.5L
#endif
