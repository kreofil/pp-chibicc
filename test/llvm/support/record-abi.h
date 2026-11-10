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
  Large prefix##_compose(Pair a, Large b, long n);
RECORD_ABI_PROTOTYPES(host)
RECORD_ABI_PROTOTYPES(emitted)
int host_calls_emitted(void);
long host_fixed_variadic(Pair pair, int count, ...);
#endif
