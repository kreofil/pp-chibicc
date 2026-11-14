#ifndef STDARG_RECORDS_H
#define STDARG_RECORDS_H
#include <stdarg.h>
typedef struct { long a,b; } VrInt;
typedef struct { double a,b; } VrReal;
typedef struct { int a; double b; } VrMixed;
typedef struct { double a; long b; } VrReverse;
typedef struct { float a,b; } VrFloats;
typedef struct { float a,b,c; } VrFloat3;
typedef struct { unsigned char a,b,c; } VrBytes;
typedef struct { unsigned a:3; int b:5; _Bool c:1; } VrBits;
typedef struct { struct { int a,b; } inner; double real; } VrNested;
typedef union { double real; unsigned long bits; } VrUnion;
typedef union { long double real; unsigned long bits[2]; } VrWideUnion;
typedef struct { long a,b,c; } VrLarge;
typedef struct __attribute__((packed)) { char tag; double real; } VrPacked;
typedef struct __attribute__((packed)) { unsigned head:7; unsigned long value:64; } VrCrossBits;
typedef struct __attribute__((aligned(32))) { long a,b,c; } VrAligned;
typedef struct { long double real; } VrX87;
typedef struct __attribute__((aligned(16))) { double real; } VrPadding;
#define VR_DECL(P) \
  long P##_ints(int,...); \
  double P##_reals(int,...); \
  double P##_mixed(int,...); \
  int P##_shapes(int,...); \
  int P##_bits(int,...); \
  long P##_gp_boundary(long,long,long,long,long,...); \
  double P##_fp_boundary(double,double,double,double,double,double,double,int,...); \
  double P##_gp_rollback(long,long,long,long,long,long,...); \
  long P##_fp_rollback(double,double,double,double,double,double,double,double,...); \
  double P##_named(VrLarge,VrInt,double,int,...); \
  VrInt P##_return_int(int,...); \
  VrLarge P##_return_large(long,long,long,long,long,...); \
  int P##_copies(int,...); \
  double P##_consume_mixed(va_list,int); \
  double P##_consume(va_list); \
  double P##_bridge(int,...); \
  long P##_loop(int,...); \
  int P##_extended(int,...);
VR_DECL(native)
VR_DECL(host)
VR_DECL(emitted)
int host_checks_record_varargs(void);
#endif
