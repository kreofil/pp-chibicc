#include "families.h"
#define PAIR_KERNEL(name, method) \
  unsigned long name(BenchContext *context, BenchStats *stats) { \
    unsigned long sum = 0; \
    for (int i = 0; i < context->count; i++) { \
      BenchJob *job = &context->jobs[i]; \
      sum += method<job->a, job->b>(job->x, stats); \
    } \
    return sum; \
  }
#define TRIPLE_KERNEL(name, method) \
  unsigned long name(BenchContext *context, BenchStats *stats) { \
    unsigned long sum = 0; \
    for (int i = 0; i < context->count; i++) { \
      BenchJob *job = &context->jobs[i]; \
      sum += method<job->a, job->b, job->c>(job->x, stats); \
    } \
    return sum; \
  }
PAIR_KERNEL(bench_pair_short, bench_short2)
PAIR_KERNEL(bench_pair_work, bench_work2)
TRIPLE_KERNEL(bench_triple_short, bench_short3)
TRIPLE_KERNEL(bench_triple_work, bench_work3)

unsigned long bench_run(void *pointer, int arity, int work, BenchStats *stats) {
  BenchContext *context = pointer;
  if (arity == 2)
    return work ? bench_pair_work(context, stats) : bench_pair_short(context, stats);
  return work ? bench_triple_work(context, stats) : bench_triple_short(context, stats);
}
