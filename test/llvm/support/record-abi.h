#ifndef LLVM_RECORD_ABI_H
#define LLVM_RECORD_ABI_H
typedef struct { char value; } One;
typedef struct { unsigned char data[3]; } Three;
typedef struct { int a, b, c; } Twelve;
typedef struct { long a, b; } Pair;
typedef struct { long a, b, c; } Large;
typedef struct { char tag; long value; } __attribute__((packed)) Packed;
typedef struct { int value; } __attribute__((aligned(16))) Aligned;
typedef struct { short head; struct { int x; short y; } inner; char tail; } Nested;
typedef union { long number; unsigned char bytes[12]; } Value;
typedef struct { int *pointer; long offset; } PointerPair;
typedef struct { float x; } FloatOne;
typedef struct { float x, y; } FloatPair;
typedef struct { float x, y, z; } FloatTriple;
typedef struct { float v[4]; } FloatQuad;
typedef struct { double x; } DoubleOne;
typedef struct { double x, y; } DoublePair;
typedef struct { float x; double y; } FloatDouble;
typedef struct { double x; float y; } DoubleFloat;
typedef struct { int tag; float value; } IntFloat;
typedef struct { float value; int tag; } FloatInt;
typedef struct { long tag; double value; } IntDouble;
typedef struct { double value; long tag; } DoubleInt;
typedef struct { struct { float v[2]; } parts[2]; } NestedReal;
typedef union { float f[2]; double d; } RealUnion;
typedef union { double d; long bits; } MixedUnion;
typedef union { float f[3]; double d; } WideUnion;
typedef struct { char tag; double value; } __attribute__((packed)) PackedReal;
typedef struct { double v[3]; } RealTriple;
typedef struct { float value; } __attribute__((aligned(16))) AlignedReal;

#define RECORD_ABI_PROTOTYPES(prefix) \
  One prefix##_one(One x); \
  Three prefix##_three(Three x); \
  Twelve prefix##_twelve(Twelve x); \
  Pair prefix##_pair(Pair x); \
  Large prefix##_large(Large x, long n); \
  Packed prefix##_packed(Packed x, long n); \
  Aligned prefix##_aligned(Aligned x, long n); \
  Nested prefix##_nested(Nested x); \
  Value prefix##_value(Value x); \
  PointerPair prefix##_pointer(PointerPair x); \
  long prefix##_pressure(long a, long b, long c, long d, long e, Pair p, long last); \
  long prefix##_boundary(long a, long b, long c, long d, Pair p, long last); \
  Large prefix##_sret_pressure(long a, long b, long c, long d, Pair p, long last); \
  long prefix##_many(Pair a, Pair b, Pair c, Pair d, long last); \
  Pair prefix##_select(int which, Pair a, Pair b); \
  Large prefix##_compose(Pair a, Large b, long n); \
  FloatOne prefix##_float_one(FloatOne x); \
  FloatPair prefix##_float_pair(FloatPair x); \
  FloatTriple prefix##_float_triple(FloatTriple x); \
  FloatQuad prefix##_float_quad(FloatQuad x); \
  DoubleOne prefix##_double_one(DoubleOne x); \
  DoublePair prefix##_double_pair(DoublePair x); \
  FloatDouble prefix##_float_double(FloatDouble x); \
  DoubleFloat prefix##_double_float(DoubleFloat x); \
  IntFloat prefix##_int_float(IntFloat x); \
  FloatInt prefix##_float_int(FloatInt x); \
  IntDouble prefix##_int_double(IntDouble x); \
  DoubleInt prefix##_double_int(DoubleInt x); \
  NestedReal prefix##_nested_real(NestedReal x); \
  RealUnion prefix##_real_union(RealUnion x); \
  RealUnion prefix##_real_union_double(RealUnion x); \
  MixedUnion prefix##_mixed_union(MixedUnion x); \
  WideUnion prefix##_wide_union(WideUnion x); \
  PackedReal prefix##_packed_real(PackedReal x, double n); \
  RealTriple prefix##_real_triple(RealTriple x, double n); \
  AlignedReal prefix##_aligned_real(AlignedReal x, float n); \
  double prefix##_sse_pressure(double a, double b, double c, double d, double e, \
                               double f, double g, DoublePair p, double last); \
  double prefix##_sse_boundary(double a, double b, double c, double d, double e, \
                               double f, DoublePair p, double last); \
  double prefix##_vector_boundary(float a, float b, float c, float d, float e, \
                                  float f, float g, FloatPair p, float last); \
  double prefix##_vector_pressure(float a, float b, float c, float d, float e, \
                                  float f, float g, FloatTriple p, float last); \
  double prefix##_fp_rollback(double a, double b, double c, double d, double e, \
      double f, double g, double h, IntDouble spill, long w, long x, long y, \
      long z, Pair tail); \
  double prefix##_gp_rollback(long a, long b, long c, long d, long e, long f, \
      IntDouble spill, double u, double v, double w, double x, double y, double z, \
      DoublePair tail); \
  double prefix##_many_real(DoublePair a, DoublePair b, DoublePair c, \
                            DoublePair d, DoublePair e, long last); \
  RealTriple prefix##_real_sret(long a, long b, long c, long d, \
                               IntDouble p, DoubleInt q, double last); \
  double prefix##_aligned_sse(double a, double b, double c, double d, double e, \
                              double f, double g, AlignedReal x, float last);
RECORD_ABI_PROTOTYPES(host)
RECORD_ABI_PROTOTYPES(emitted)
int host_calls_emitted(void);
long host_fixed_variadic(Pair pair, int count, ...);
double host_real_variadic(FloatPair pair, IntDouble mixed, int count, ...);
#endif
