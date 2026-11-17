#include "families.h"
#define PAIR(name, left, right, code, value) \
  unsigned long name<BenchLeft.left *a, BenchRight.right *b>(unsigned long x, BenchStats *s) { \
    s->concrete++; unsigned factor = code; return value; \
  }
#define TRIPLE(name, left, right, third, code, value) \
  unsigned long name<BenchLeft.left *a, BenchRight.right *b, BenchThird.third *c>(unsigned long x, BenchStats *s) { \
    s->concrete++; unsigned factor = code; return value; \
  }
PAIR(bench_short2, void, void, 101, (x + 1) * factor + 13)
PAIR(bench_short2, one, two, 106, (x + 1) * factor + 13)
PAIR(bench_work2, void, void, 101, bench_mix(x, factor))
PAIR(bench_work2, one, two, 106, bench_mix(x, factor))
TRIPLE(bench_short3, void, void, void, 101, (x + 1) * factor + 13)
TRIPLE(bench_short3, one, two, one, 112, (x + 1) * factor + 13)
TRIPLE(bench_work3, void, void, void, 101, bench_mix(x, factor))
TRIPLE(bench_work3, one, two, one, 112, bench_mix(x, factor))
