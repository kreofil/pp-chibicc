#ifndef STATIC_DISPATCH_BENCH_FAMILIES_H
#define STATIC_DISPATCH_BENCH_FAMILIES_H
#include "api.h"
typedef struct BenchLeft {} enum { one } const BenchLeft;
typedef struct BenchRight {} enum { one, two } const BenchRight;
typedef struct BenchThird {} enum { one } const BenchThird;

#define PAIR_DECL(name) \
  unsigned long name<BenchLeft *a, BenchRight *b>(unsigned long x, BenchStats *s); \
  unsigned long name<BenchLeft.void *a, BenchRight.void *b>(unsigned long x, BenchStats *s); \
  unsigned long name<BenchLeft.one *a, BenchRight.two *b>(unsigned long x, BenchStats *s);
#define TRIPLE_DECL(name) \
  unsigned long name<BenchLeft *a, BenchRight *b, BenchThird *c>(unsigned long x, BenchStats *s); \
  unsigned long name<BenchLeft.void *a, BenchRight.void *b, BenchThird.void *c>(unsigned long x, BenchStats *s); \
  unsigned long name<BenchLeft.one *a, BenchRight.two *b, BenchThird.one *c>(unsigned long x, BenchStats *s);
PAIR_DECL(bench_short2)
PAIR_DECL(bench_work2)
TRIPLE_DECL(bench_short3)
TRIPLE_DECL(bench_work3)

typedef struct { BenchLeft *a; BenchRight *b; BenchThird *c; unsigned long x; } BenchJob;
typedef struct { BenchJob *jobs; int count; } BenchContext;

static inline unsigned long bench_mix(unsigned long x, unsigned code) {
  unsigned long value = x + code;
  for (unsigned i = 0; i < 6; i++) {
    value ^= value >> 13;
    value *= 0x9e3779b185ebca87UL;
    value ^= value >> 17;
    value += code + i;
  }
  return value & 0xffffffUL;
}
#endif
